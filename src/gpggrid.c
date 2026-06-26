#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#define GRID_ROWS 6
#define GRID_COLS 6
#define MAX_PASS 256

static const char charset[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    "!@#$%^&*()";

#define CHARSET_LEN (sizeof(charset) - 1)

/*
 * Security rationale
 *
 * The passphrase never resides in global writable storage.
 * Its lifetime is:
 *
 *   stack of main() --> pipe to gpg --> immediate wipe
 *
 * The process retains no usable copy after the handoff.
 * g_secret points into the stack of main() so the signal handler
 * can reach it from any execution context.
 */

static int urandom_fd = -1;

static volatile unsigned char *g_secret = NULL;
static size_t g_secret_len = 0;

/*
 * secure_bzero() is intentionally called from the signal handler.
 * Although POSIX does not specify arbitrary memory writes as an
 * async-signal-safe primitive, on Linux this is simply a sequence
 * of stores and is preferable to leaving key material in RAM.
 */
static inline void secure_bzero(void *ptr, size_t len)
{
    volatile unsigned char *p = (volatile unsigned char *)ptr;
    while (len--)
        *p++ = 0;
}

static void wipe_and_exit(int sig)
{
    if (g_secret)
        secure_bzero((void *)g_secret, g_secret_len);
    _exit(128 + sig);
}

static void install_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = wipe_and_exit;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT,  &sa, NULL) < 0) { perror("sigaction SIGINT");  exit(EXIT_FAILURE); }
    if (sigaction(SIGTERM, &sa, NULL) < 0) { perror("sigaction SIGTERM"); exit(EXIT_FAILURE); }
    if (sigaction(SIGHUP,  &sa, NULL) < 0) { perror("sigaction SIGHUP");  exit(EXIT_FAILURE); }
    if (sigaction(SIGQUIT, &sa, NULL) < 0) { perror("sigaction SIGQUIT"); exit(EXIT_FAILURE); }
}

static ssize_t read_random(void *buf, size_t count)
{
    ssize_t n;
    do {
        n = read(urandom_fd, buf, count);
    } while (n < 0 && errno == EINTR);
    return n;
}

static int write_all(int fd, const void *buf, size_t len)
{
    const unsigned char *p = (const unsigned char *)buf;
    size_t remaining = len;
    while (remaining > 0) {
        ssize_t n = write(fd, p, remaining);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            return -1;
        p += n;
        remaining -= n;
    }
    return 0;
}

static void fill_grid(char grid[GRID_ROWS][GRID_COLS], int page)
{
    unsigned char buf;
    char pool[CHARSET_LEN];
    memcpy(pool, charset, CHARSET_LEN);

    for (int i = CHARSET_LEN - 1; i > 0; i--) {
        if (read_random(&buf, 1) != 1) {
            perror("read /dev/urandom");
            exit(EXIT_FAILURE);
        }
        int j = buf % (i + 1);
        char tmp = pool[i];
        pool[i] = pool[j];
        pool[j] = tmp;
    }

    int start = page * GRID_ROWS * GRID_COLS;
    for (int i = 0; i < GRID_ROWS; i++)
        for (int j = 0; j < GRID_COLS; j++) {
            int idx = start + i * GRID_COLS + j;
            grid[i][j] = (idx < (int)CHARSET_LEN) ? pool[idx] : ' ';
        }

    secure_bzero(pool, sizeof(pool));
}

static void print_grid(const char grid[GRID_ROWS][GRID_COLS], int page, int total_pages)
{
    printf("\n  Page %d/%d -- press 'n' for next page, ENTER to confirm\n",
           page + 1, total_pages);
    printf("\n    ");
    for (int j = 0; j < GRID_COLS; j++)
        printf(" %d  ", j + 1);
    printf("\n");
    for (int i = 0; i < GRID_ROWS; i++) {
        printf(" %c  ", 'A' + i);
        for (int j = 0; j < GRID_COLS; j++)
            printf(grid[i][j] == ' ' ? "[ ] " : "[%c] ", grid[i][j]);
        printf("\n");
    }
    printf("\n");
}

static void get_passphrase(char *passphrase)
{
    char grid[GRID_ROWS][GRID_COLS];
    char input[16];
    size_t pass_len = 0;
    int total_pages = (CHARSET_LEN + GRID_ROWS * GRID_COLS - 1) / (GRID_ROWS * GRID_COLS);
    int page = 0;

    fprintf(stderr, "GPGgrid - Anti-keylogger passphrase entry\n");
    fprintf(stderr, "Select characters by row (A-F) and column (1-6)\n");
    fprintf(stderr, "Press 'n' for next page, ENTER alone to confirm\n\n");

    fill_grid(grid, page);

    while (1) {
        print_grid(grid, page, total_pages);
        fprintf(stderr, "Select [RowCol] (e.g. A3), 'n' for next page, or ENTER to confirm: ");
        fflush(stderr);

        if (!fgets(input, sizeof(input), stdin))
            break;

        input[strcspn(input, "\n")] = 0;
        size_t len = strlen(input);

        if (len == 0)
            break;

        if (len == 1 && input[0] == 'n') {
            page = (page + 1) % total_pages;
            fill_grid(grid, page);
            continue;
        }

        if (len != 2) {
            fprintf(stderr, "Invalid input. Use RowCol format (e.g. A3)\n");
            continue;
        }

        int row = input[0] - 'A';
        int col = input[1] - '1';

        if (row < 0 || row >= GRID_ROWS || col < 0 || col >= GRID_COLS) {
            fprintf(stderr, "Invalid coordinates.\n");
            continue;
        }

        if (grid[row][col] == ' ') {
            fprintf(stderr, "Empty cell. Press 'n' for next page.\n");
            continue;
        }

        if (pass_len < MAX_PASS - 1) {
            passphrase[pass_len++] = grid[row][col];
            fprintf(stderr, "*");
            fflush(stderr);
            page = 0;
            fill_grid(grid, page);
        }
    }

    passphrase[pass_len] = '\0';
    fprintf(stderr, "\n");

    secure_bzero(grid, sizeof(grid));
    secure_bzero(input, sizeof(input));
}

int main(int argc, char *argv[])
{
    install_signal_handlers();

    if (argc < 2) {
        fprintf(stderr, "Usage: gpggrid <gpg arguments>\n");
        fprintf(stderr, "Example: gpggrid --encrypt -r user@example.com file.txt\n");
        return EXIT_FAILURE;
    }

    urandom_fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (urandom_fd < 0) {
        perror("open /dev/urandom");
        return EXIT_FAILURE;
    }

    char passphrase[MAX_PASS];
    g_secret = (volatile unsigned char *)passphrase;
    g_secret_len = sizeof(passphrase);

    get_passphrase(passphrase);

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        secure_bzero(passphrase, sizeof(passphrase));
        return EXIT_FAILURE;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        secure_bzero(passphrase, sizeof(passphrase));
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        (void)close(pipefd[1]);
        (void)close(urandom_fd);

        if (dup2(pipefd[0], STDIN_FILENO) < 0) {
            perror("dup2");
            secure_bzero(passphrase, sizeof(passphrase));
            _exit(EXIT_FAILURE);
        }
        (void)close(pipefd[0]);

        char *gpg_argv[argc + 5];
        gpg_argv[0] = "gpg";
        gpg_argv[1] = "--passphrase-fd";
        gpg_argv[2] = "0";
        gpg_argv[3] = "--pinentry-mode";
        gpg_argv[4] = "loopback";
        for (int i = 1; i < argc; i++)
            gpg_argv[i + 4] = argv[i];
        gpg_argv[argc + 4] = NULL;

        execvp("gpg", gpg_argv);

        secure_bzero(passphrase, sizeof(passphrase));
        perror("execvp");
        _exit(EXIT_FAILURE);
    }

    (void)close(pipefd[0]);

    size_t plen = strlen(passphrase);
    if (write_all(pipefd[1], passphrase, plen) < 0)
        fprintf(stderr, "Warning: failed to write passphrase to pipe\n");
    if (write_all(pipefd[1], "\n", 1) < 0)
        fprintf(stderr, "Warning: failed to write newline to pipe\n");

    (void)close(pipefd[1]);

    secure_bzero(passphrase, sizeof(passphrase));
    passphrase[0] = 0;
    g_secret = NULL;
    g_secret_len = 0;

    int status;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return EXIT_FAILURE;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    return EXIT_FAILURE;
}
