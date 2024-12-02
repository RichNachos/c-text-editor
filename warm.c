/*** Includes ***/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>

/*** Data ***/
char QUIT_KEY = 'q';
struct termios ORIGINAL_TERMIOS;

/*** Header Functions ***/
void die(const char* s);
void enableRawTerminalMode();
void disableRawTerminalMode();

/*** Init ***/
int main(void) {
    enableRawTerminalMode();

    // Read until 'q' char or EOF
    while(1) {
        char c = '\0';
        
        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) {
            die("read failed");
        }

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

/*** Terminal ***/

// Error handling
void die(const char* s) {
    perror(s);
    exit(1);
}

// Disables "Cooked" mode of the terminal
void enableRawTerminalMode() {
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &raw) == -1) {
        die("tcgetattr failed");
    }
    ORIGINAL_TERMIOS = raw;
    atexit(disableRawTerminalMode);

    raw.c_iflag &= ~(ICRNL | IXON | BRKINT | ISTRIP | INPCK);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag &= ~(CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr failed");
    }
}

// Enables the original configuration of the terminal
void disableRawTerminalMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &ORIGINAL_TERMIOS) == -1) {
        die("tcsetattr failed");
    }
}
