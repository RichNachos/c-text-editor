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

    // Read until 'q' char or EOF
    while(1) {
        char c = '\0';
        read(STDIN_FILENO, &c, 1);

        if (iscntrl(c)) {
            printf("%d\r\n", c);
        } else {
            printf("%d ('%c')\r\n", c, c);
        }

        if (c == QUIT_KEY) {
            break;
        }
    }
    
    return 0;
}


// Disables "Cooked" mode of the terminal
void enableRawTerminalMode() {
    struct termios raw;

    tcgetattr(STDIN_FILENO, &raw);
    ORIGINAL_TERMIOS = raw;
    atexit(disableRawTerminalMode);

    raw.c_iflag &= ~(ICRNL | IXON | BRKINT | ISTRIP | INPCK);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag &= ~(CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Enables the original configuration of the terminal
void disableRawTerminalMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &ORIGINAL_TERMIOS);
}
