#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

char QUIT_KEY = 'q';

struct termios ORIGINAL_TERMIOS;

void enableRawTerminalMode();
void disableRawTerminalMode();

int main(void) {
    enableRawTerminalMode();

    char c;

    // Read until 'q' char or EOF
    while(read(STDIN_FILENO, &c, 1) && c != QUIT_KEY) {
        if (iscntrl(c)) {
            printf("%d\r\n", c);
        } else {
            printf("%d ('%c')\r\n", c, c);
        }
    }
    
    // Write a line after quitting program
    printf("\r\n");
    return 0;
}


// Disables "Cooked" mode of the terminal
void enableRawTerminalMode() {
    struct termios raw;

    tcgetattr(STDIN_FILENO, &raw);
    ORIGINAL_TERMIOS = raw;
    atexit(disableRawTerminalMode);

    raw.c_iflag &= ~(ICRNL | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Enables the original configuration of the terminal
void disableRawTerminalMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &ORIGINAL_TERMIOS);
}
