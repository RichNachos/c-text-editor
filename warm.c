/*** Includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <string.h>

/*** Data ***/
typedef struct editorRow {
    int size;
    int render_size;
    char *line;
    char *render_line;
} editorRow;

struct editorConfig {
    int cursor_x;
    int cursor_y;
    int render_x;
    int col_offset;
    int row_offest;
    int screen_rows;
    int screen_cols;
    int num_rows;
    struct editorRow* row;
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
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

int TAB_SIZE = 8;

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
void editorScroll();

/*** Input ***/
void editorMoveCursor(int key);

/*** Row Operations ***/
void editorAppendRow(char *s, size_t len);
void editorUpdateRow(editorRow* row);
int editorCursorxToRenderx(editorRow* row, int cursor_x);

/*** File I/O ***/
void editorOpen(char* filename);

/*** Init ***/
void initEditor();

void initEditor() {
    E.cursor_x = 0;
    E.cursor_y = 0;
    E.render_x = 0; 
    E.col_offset = 0;
    E.row_offest = 0;
    E.num_rows = 0;
    E.row = NULL;
    if (getWindowSize(&E.screen_cols, &E.screen_rows) == -1) {
        die("getWindowSize failed");
    }
}

int main(int argc, char* argv[]) {
    enableRawTerminalMode();
    initEditor();
    if (argc >= 2) {
        editorOpen(argv[1]);
    }

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

        if (read(STDIN_FILENO, &sequence[0], 1) != 1) return c;
        if (read(STDIN_FILENO, &sequence[1], 1) != 1) return c;

        if (sequence[0] == '[') {
            if (sequence[1] >= '0' && sequence[1] <= '9') {
                if (read(STDIN_FILENO, &sequence[2], 1) != 1) return c;
                if (sequence[2] == '~') {
                    switch (sequence[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
                switch (sequence[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        }
        if (sequence[0] == 'O') {
            switch (sequence[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
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
        case PAGE_UP:
            E.cursor_y = E.row_offest;
            break;
        case PAGE_DOWN:
            E.cursor_y = E.row_offest + E.screen_rows - 1;
            if (E.cursor_y > E.num_rows) E.cursor_y = E.num_rows;
            break;
        case HOME_KEY:
            E.cursor_x = 0;
            break;
        case END_KEY:
            E.cursor_x = E.screen_cols - 1;
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
    editorScroll();

    struct append_buffer ab = ABUF_INIT;

    buffer_append(&ab, "\x1b[?25l", 6);
    buffer_append(&ab, "\x1b[H", 3);
    
    editorDrawRows(&ab);
    
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cursor_y - E.row_offest + 1, E.render_x - E.col_offset + 1);
    buffer_append(&ab, buf, strlen(buf));

    buffer_append(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.buf, ab.len);
    buffer_free(&ab);
}

void editorDrawRows(struct append_buffer* ab) {

    for (int i = 0; i < E.screen_rows; i++) {
        int file_row = i + E.row_offest;
        if (file_row >= E.num_rows) {
            if (i == E.screen_rows / 3 && E.num_rows == 0) {
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
        } else {
            int length = E.row[file_row].render_size - E.col_offset;
            if (length < 0) length = 0;
            if (length > E.screen_cols) length = E.screen_cols;
            buffer_append(ab, &E.row[file_row].render_line[E.col_offset], length);
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
    editorRow* row = (E.cursor_y >= E.num_rows) ? NULL : &E.row[E.cursor_y];

    switch (key) {
        case ARROW_UP:
            if (E.cursor_y != 0)
                E.cursor_y--;
            break;
        case ARROW_LEFT:
            if (E.cursor_x != 0)
                E.cursor_x--;
            else if (E.cursor_y > 0) {
                E.cursor_y--;
                E.cursor_x = E.row[E.cursor_y].size;
            }
            break;
        case ARROW_DOWN:
            if (E.cursor_y < E.num_rows)
                E.cursor_y++;
            break;
        case ARROW_RIGHT:
            if (row && E.cursor_x < row->size)
                E.cursor_x++;
            else if (row && E.cursor_x == row->size) {
                E.cursor_x = 0;
                E.cursor_y++;
            }
            break;
    }

    row = (E.cursor_y >= E.num_rows) ? NULL : &E.row[E.cursor_y];

    int row_length = row ? row->size : 0;
    if (E.cursor_x > row_length) {
        E.cursor_x = row_length;
    }
}

void editorOpen(char* filename) {
    FILE* fp = fopen(filename, "r");
    
    if (!fp) {
        die("fopen failed");
    }

    char* line = NULL;
    size_t line_cap = 0;
    ssize_t line_length;

    while ((line_length = getline(&line, &line_cap, fp)) != -1) {
        while (line_length > 0 && (line[line_length - 1] == '\n' || line[line_length - 1] == '\r')) {
            line_length--;
        }
        editorAppendRow(line, line_length);
    }

    free(line);
    fclose(fp);
}

void editorAppendRow(char *s, size_t len) {
    E.row = realloc(E.row, sizeof(editorRow) * (E.num_rows + 1));

    int at = E.num_rows;

    E.row[at].size = len;
    E.row[at].line = malloc(len + 1);
    memcpy(E.row[at].line, s, len);
    E.row[at].line[len] = '\0';

    E.row[at].render_size = 0;
    E.row[at].render_line = NULL;

    editorUpdateRow(&E.row[at]);

    E.num_rows++;
}

void editorScroll() {
    E.render_x = 0;
    if (E.cursor_y < E.num_rows) {
        E.render_x = editorCursorxToRenderx(&E.row[E.cursor_y], E.cursor_x);
    }

    if (E.cursor_y < E.row_offest) {
        E.row_offest = E.cursor_y;
    }
    if (E.cursor_y >= E.row_offest + E.screen_rows) {
        E.row_offest = E.cursor_y - E.screen_rows + 1;
    }
    if (E.cursor_x < E.col_offset) {
        E.col_offset = E.render_x;
    }
    if (E.cursor_x >= E.col_offset + E.screen_cols) {
        E.col_offset = E.render_x - E.screen_cols + 1;
    }
}

void editorUpdateRow(editorRow* row) {
    if (row->render_line)
        free(row->render_line);

    int tabs = 0;
    for (int j = 0; j < row->size; j++) {
        if (row->line[j] == '\t')
            tabs++;
    }

    row->render_line = malloc(row->size + (tabs * (TAB_SIZE - 1)) + 1);

    int idx = 0;
    for (int j = 0; j < row->size; j++) {
        if (row->line[j] == '\t') {
            row->render_line[idx++] = ' ';
            while (idx % TAB_SIZE != 0)
                row->render_line[idx++] = ' ';
        } else {
            row->render_line[idx++] = row->line[j];
        }
    }
    row->render_line[idx] = '\0';
    row->render_size = idx;
}

int editorCursorxToRenderx(editorRow* row, int cursor_x) {
    int render_x = 0;
    for (int j = 0; j < cursor_x; j++) {
        if (row->line[j] == '\t')
            render_x += (TAB_SIZE - 1) - (render_x % TAB_SIZE);
        render_x++;
    }

    return render_x;
}