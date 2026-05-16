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

void fill_grid(char grid[GRID_ROWS][GRID_COLS]) {
    int fd = open("/dev/urandom", O_RDONLY);
    unsigned char buf;
    int charset_len = strlen(charset);
    for (int i = 0; i < GRID_ROWS; i++)
        for (int j = 0; j < GRID_COLS; j++) {
            read(fd, &buf, 1);
            grid[i][j] = charset[buf % charset_len];
        }
    close(fd);
}

void print_grid(char grid[GRID_ROWS][GRID_COLS]) {
    printf("\n    ");
    for (int j = 0; j < GRID_COLS; j++)
        printf(" %d  ", j + 1);
    printf("\n");
    for (int i = 0; i < GRID_ROWS; i++) {
        printf(" %c  ", 'A' + i);
        for (int j = 0; j < GRID_COLS; j++)
            printf("[%c] ", grid[i][j]);
        printf("\n");
    }
    printf("\n");
}

char *get_passphrase() {
    char grid[GRID_ROWS][GRID_COLS];
    static char passphrase[MAX_PASS];
    char input[16];
    int pass_len = 0;

    fprintf(stderr, "GPGgrid - Anti-keylogger passphrase entry\n");
    fprintf(stderr, "Select characters by row (A-F) and column (1-6)\n");
    fprintf(stderr, "Press ENTER alone to confirm\n\n");

    while (1) {
        fill_grid(grid);
        print_grid(grid);

        fprintf(stderr, "Select [RowCol] (e.g. A3) or ENTER to confirm: ");
        fflush(stderr);

        if (!fgets(input, sizeof(input), stdin))
            break;

        input[strcspn(input, "\n")] = 0;

        if (strlen(input) == 0)
            break;

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

        if (pass_len < MAX_PASS - 1) {
            passphrase[pass_len++] = grid[row][col];
            fprintf(stderr, "*");
            fflush(stderr);
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
        char **gpg_argv = malloc((argc + 3) * sizeof(char *));
        gpg_argv[0] = "gpg";
        gpg_argv[1] = "--passphrase-fd";
        gpg_argv[2] = "0";
        for (int i = 1; i < argc; i++)
            gpg_argv[i + 2] = argv[i];
        gpg_argv[argc + 2] = NULL;

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
