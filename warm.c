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
        write(STDOUT_FILENO, &c, 1);
    }
    
    return 0;
}


// Disables "Cooked" mode of the terminal
void enableRawTerminalMode() {
    struct termios raw;

    tcgetattr(STDIN_FILENO, &raw);
    ORIGINAL_TERMIOS = raw;
    atexit(disableRawTerminalMode);

    raw.c_lflag &= ~(ECHO);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Enables the original configuration of the terminal
void disableRawTerminalMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &ORIGINAL_TERMIOS);
}
