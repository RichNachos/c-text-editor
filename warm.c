/*** Includes ***/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>

/*** Data ***/
struct editorConfig {
    int screen_rows;
    int screen_cols;
    struct termios ORIGINAL_TERMIOS;
};
struct editorConfig E;

/*** Defines ***/
#define CTRL_KEY(c) ((c) & 0x1f)

/*** Terminal ***/
void die(const char* s);
void enableRawTerminalMode();
void disableRawTerminalMode();

/*** Append Buffer ***/
struct append_buffer {
    char *buf;
    int len;
};

#define ABUF_INIT {NULL, 0}
void buffer_append(struct append_buffer *ab, const char *s, int len);
void buffer_free(struct append_buffer *ab);

/*** Output ***/
char editorReadKey();
void editorProcessKeypress();
void editorRefreshScreen();
void editorDrawRows(struct append_buffer *ab);
int getWindowSize(int*, int*);

/*** Init ***/
void initEditor();

void initEditor() {
    if (getWindowSize(&E.screen_cols, &E.screen_rows) == -1) {
        die("getWindowSize failed");
    }
}

int main(void) {
    enableRawTerminalMode();
    initEditor();

    // Read until 'q' char or EOF
    while(1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    
    return 0;
}

/*** Terminal ***/

// Error handling
void die(const char* s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

// Disables "Cooked" mode of the terminal
void enableRawTerminalMode() {
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &raw) == -1) {
        die("tcgetattr failed");
    }
    E.ORIGINAL_TERMIOS = raw;
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
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.ORIGINAL_TERMIOS) == -1) {
        die("tcsetattr failed");
    }
}

/*** Input ***/
char editorReadKey() {
    int nread;
    char c = '\0';
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) {
            die("read failed");
        }
    }
    return c;
}

void editorProcessKeypress() {
    char c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
    }
}

void editorRefreshScreen() {
    struct append_buffer ab = ABUF_INIT;

    buffer_append(&ab, "\x1b[?25l", 6);
    buffer_append(&ab, "\x1b[2J", 4);
    buffer_append(&ab, "\x1b[H", 3);
    editorDrawRows(&ab);
    buffer_append(&ab, "\x1b[H", 3);
    buffer_append(&ab, "\x1b[?25l", 6);

    write(STDOUT_FILENO, ab.buf, ab.len);
    buffer_free(&ab);
}

void editorDrawRows(struct append_buffer* ab) {

    for (int i = 0; i < E.screen_rows; i++) {
        buffer_append(ab, "~", 1);

        if (i < E.screen_rows - 1) {
            buffer_append(ab, "\r\n", 2);
        }
    }
}

int getWindowSize(int* cols, int* rows) {
    struct winsize window_size;

    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &window_size) == -1 || window_size.ws_col == 0) {
        return -1;
    }

    *cols = window_size.ws_col;
    *rows = window_size.ws_row;
    return 0;
}

void buffer_append(struct append_buffer *ab, const char *s, int len) {
    char* new = realloc(ab->buf, ab->len + len);

    if (new == NULL) {
        die("buffer_append realloc failed");
        return;
    }

    memcpy(&new[ab->len], s, len);
    ab->buf = new;
    ab->len += len;
}

void buffer_free(struct append_buffer *ab) {
    free(ab->buf);
}
