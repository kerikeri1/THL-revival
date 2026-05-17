#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/kd.h>

#define DOT_MS  100
#define DASH_MS 300
#define GAP_MS  100
#define CHAR_GAP_MS 300

// Morse code table A-Z, 0-9
static const char *morse[] = {
    ".-",   "-...", "-.-.", "-..",  ".",    // A-E
    "..-.", "--.",  "....", "..",   ".---", // F-J
    "-.-",  ".-..", "--",   "-.",   "---",  // K-O
    ".--.", "--.-", ".-.",  "...",  "-",    // P-T
    "..-",  "...-", ".--",  "-..-", "-.--", // U-Y
    "--..",                                  // Z
    "-----", ".----", "..---", "...--", "....-", // 0-4
    ".....", "-....", "--...", "---..", "----."   // 5-9
};

void set_leds(int fd, int state) {
    ioctl(fd, KDSETLED, state);
}

void ms_sleep(int ms) {
    usleep(ms * 1000);
}

void blink_morse(int fd, const char *code) {
    for (int i = 0; code[i]; i++) {
        if (code[i] == '.') {
            set_leds(fd, 0x07); // all LEDs on
            ms_sleep(DOT_MS);
        } else if (code[i] == '-') {
            set_leds(fd, 0x07);
            ms_sleep(DASH_MS);
        }
        set_leds(fd, 0x00); // all LEDs off
        ms_sleep(GAP_MS);
    }
    ms_sleep(CHAR_GAP_MS);
}

char random_char() {
    int fd = open("/dev/urandom", O_RDONLY);
    unsigned char buf;
    read(fd, &buf, 1);
    close(fd);
    // Return A-Z or 0-9
    int n = buf % 36;
    if (n < 26) return 'A' + n;
    return '0' + (n - 26);
}

int char_to_morse_idx(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '0' && c <= '9') return 26 + (c - '0');
    return -1;
}

int main() {
    int fd = open("/dev/console", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Cannot open /dev/console\n");
        return 1;
    }

    while (1) {
        // Blink 4 random characters in morse
        for (int i = 0; i < 4; i++) {
            char c = random_char();
            int idx = char_to_morse_idx(c);
            if (idx >= 0)
                blink_morse(fd, morse[idx]);
        }
        ms_sleep(500);
    }

    set_leds(fd, 0xFF); // restore LEDs
    close(fd);
    return 0;
}
