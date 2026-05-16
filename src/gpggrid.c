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
    
    for (int i = 0; i < GRID_ROWS; i++) {
        for (int j = 0; j < GRID_COLS; j++) {
            read(fd, &buf, 1);
            grid[i][j] = charset[buf % charset_len];
        }
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

int main(int argc, char *argv[]) {
    char grid[GRID_ROWS][GRID_COLS];
    char passphrase[MAX_PASS];
    char input[16];
    int pass_len = 0;
    
    printf("GPGgrid - Anti-keylogger passphrase entry\n");
    printf("Select characters by row (A-F) and column (1-6)\n");
    printf("Press ENTER alone to confirm passphrase\n\n");
    
    while (1) {
        fill_grid(grid);
        print_grid(grid);
        
        printf("Select [RowCol] (e.g. A3) or ENTER to confirm: ");
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin))
            break;
        
        input[strcspn(input, "\n")] = 0;
        
        if (strlen(input) == 0)
            break;
        
        if (strlen(input) != 2) {
            printf("Invalid input. Use RowCol format (e.g. A3)\n");
            continue;
        }
        
        int row = input[0] - 'A';
        int col = input[1] - '1';
        
        if (row < 0 || row >= GRID_ROWS || col < 0 || col >= GRID_COLS) {
            printf("Invalid coordinates.\n");
            continue;
        }
        
        if (pass_len < MAX_PASS - 1) {
            passphrase[pass_len++] = grid[row][col];
            printf("*");
            fflush(stdout);
        }
    }
    
    passphrase[pass_len] = '\0';
    
    fprintf(stderr, "\n");
    printf("%s", passphrase);
    
    memset(passphrase, 0, sizeof(passphrase));
    
    return 0;
}
