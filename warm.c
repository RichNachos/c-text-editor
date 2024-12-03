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
    int cursor_x;
    int cursor_y;
    int screen_rows;
    int screen_cols;
    struct termios ORIGINAL_TERMIOS;
};
struct editorConfig E;

/*** Defines ***/
#define WARM_VERSION "0.1.0"

#define CTRL_KEY(c) ((c) & 0x1f)

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN
};

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
int editorReadKey();
void editorProcessKeypress();
void editorRefreshScreen();
void editorDrawRows(struct append_buffer *ab);
int getWindowSize(int*, int*);

/*** Input ***/
void editorMoveCursor(int key);

/*** Init ***/
void initEditor();

void initEditor() {
    E.cursor_x = 0;
    E.cursor_y = 0;
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
int editorReadKey() {
    int nread;
    char c = '\0';
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) {
            die("read failed");
        }
    }
    if (c == '\x1b') {
        char sequence[3];

        if (read(STDIN_FILENO, &sequence[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &sequence[1], 1) != 1) return '\x1b';

        if (sequence[0] == '[') {
            switch (sequence[1]) {
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN;
                case 'C': return ARROW_RIGHT;
                case 'D': return ARROW_LEFT;
            }
        }
    }

    return c;
}

void editorProcessKeypress() {
    int c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case ARROW_UP:
        case ARROW_LEFT:
        case ARROW_DOWN:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
    }
}

void editorRefreshScreen() {
    struct append_buffer ab = ABUF_INIT;

    buffer_append(&ab, "\x1b[?25l", 6);
    buffer_append(&ab, "\x1b[H", 3);
    
    editorDrawRows(&ab);
    
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cursor_y + 1, E.cursor_x + 1);
    buffer_append(&ab, buf, strlen(buf));

    buffer_append(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.buf, ab.len);
    buffer_free(&ab);
}

void editorDrawRows(struct append_buffer* ab) {

    for (int i = 0; i < E.screen_rows; i++) {
        if (i == E.screen_rows / 3) {
            char welcome[80];

            int welcome_length = snprintf(welcome, sizeof(welcome), "Warm Editor -- version %s", WARM_VERSION);
            if (welcome_length > E.screen_cols) {
                welcome_length = E.screen_cols;
            }
            int padding = (E.screen_cols - welcome_length) / 2;
            if (padding) {
                buffer_append(ab, "~", 1);
                padding--;
            }
            while (padding--) {
                buffer_append(ab, " ", 1);
            }

            buffer_append(ab, welcome, welcome_length);
        } else {
            buffer_append(ab, "~", 1);
        }

        buffer_append(ab, "\x1b[K", 3);
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

void editorMoveCursor(int key) {
    switch (key) {
        case ARROW_UP:
            E.cursor_y--;
            break;
        case ARROW_LEFT:
            E.cursor_x--;
            break;
        case ARROW_DOWN:
            E.cursor_y++;
            break;
        case ARROW_RIGHT:
            E.cursor_x++;
            break;
    }
    if (E.cursor_x < 0) E.cursor_x = 0;
    if (E.cursor_y < 0) E.cursor_y = 0;
    if (E.cursor_x == E.screen_cols) E.cursor_x = E.screen_cols - 1;
    if (E.cursor_y == E.screen_rows) E.cursor_y = E.screen_rows - 1;
}