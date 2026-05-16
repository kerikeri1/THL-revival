#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define GRID_ROWS 6
#define GRID_COLS 6
#define MAX_PASS 256

static const char charset[] = 
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    "!@#$%^&*()";

#define CHARSET_LEN (sizeof(charset) - 1)

// Fill grid with unique characters from charset
void fill_grid(char grid[GRID_ROWS][GRID_COLS], int page) {
    int fd = open("/dev/urandom", O_RDONLY);
    unsigned char buf;
    
    // Copy charset and shuffle it
    char pool[CHARSET_LEN];
    memcpy(pool, charset, CHARSET_LEN);
    
    // Fisher-Yates shuffle using urandom
    for (int i = CHARSET_LEN - 1; i > 0; i--) {
        read(fd, &buf, 1);
        int j = buf % (i + 1);
        char tmp = pool[i];
        pool[i] = pool[j];
        pool[j] = tmp;
    }
    close(fd);
    
    // Fill grid with chars from current page
    int start = page * GRID_ROWS * GRID_COLS;
    for (int i = 0; i < GRID_ROWS; i++)
        for (int j = 0; j < GRID_COLS; j++) {
            int idx = start + i * GRID_COLS + j;
            if (idx < (int)CHARSET_LEN)
                grid[i][j] = pool[idx];
            else
                grid[i][j] = ' ';
        }
}

void print_grid(char grid[GRID_ROWS][GRID_COLS], int page, int total_pages) {
    printf("\n  Page %d/%d — press 'n' for next page, ENTER to confirm\n", 
           page + 1, total_pages);
    printf("\n    ");
    for (int j = 0; j < GRID_COLS; j++)
        printf(" %d  ", j + 1);
    printf("\n");
    for (int i = 0; i < GRID_ROWS; i++) {
        printf(" %c  ", 'A' + i);
        for (int j = 0; j < GRID_COLS; j++) {
            if (grid[i][j] == ' ')
                printf("[ ] ");
            else
                printf("[%c] ", grid[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}

char *get_passphrase() {
    char grid[GRID_ROWS][GRID_COLS];
    static char passphrase[MAX_PASS];
    char input[16];
    int pass_len = 0;
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

        // ENTER = confirm
        if (strlen(input) == 0)
            break;

        // 'n' = next page, regenerate grid
        if (strcmp(input, "n") == 0) {
            page = (page + 1) % total_pages;
            fill_grid(grid, page);
            continue;
        }

        if (strlen(input) != 2) {
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

            // Regenerate grid after each character
            page = 0;
            fill_grid(grid, page);
        }
    }

    passphrase[pass_len] = '\0';
    fprintf(stderr, "\n");
    return passphrase;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: gpggrid <gpg arguments>\n");
        fprintf(stderr, "Example: gpggrid --encrypt -r user@example.com file.txt\n");
        return 1;
    }

    char *passphrase = get_passphrase();

    // Create pipe to pass passphrase to GPG
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        fprintf(stderr, "Error creating pipe\n");
        return 1;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child process — execute GPG
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);

        // Build argv for GPG
        char **gpg_argv = malloc((argc + 5) * sizeof(char *));
        gpg_argv[0] = "gpg";
        gpg_argv[1] = "--passphrase-fd";
        gpg_argv[2] = "0";
        gpg_argv[3] = "--pinentry-mode";
        gpg_argv[4] = "loopback";
        for (int i = 1; i < argc; i++)
            gpg_argv[i + 4] = argv[i];
        gpg_argv[argc + 4] = NULL;

        execvp("gpg", gpg_argv);
        fprintf(stderr, "Error executing gpg\n");
        exit(1);
    } else {
        // Parent process — write passphrase to pipe
        close(pipefd[0]);
        write(pipefd[1], passphrase, strlen(passphrase));
        write(pipefd[1], "\n", 1);
        close(pipefd[1]);

        // Clear passphrase from memory
        memset(passphrase, 0, MAX_PASS);

        int status;
        waitpid(pid, &status, 0);
        return WEXITSTATUS(status);
    }
}
