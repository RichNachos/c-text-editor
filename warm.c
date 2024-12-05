/*** Includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

/*** Data ***/

struct editorSyntax {
    char* filetype;
    char** filematch;
    char** keywords;
    char* single_comment_start;
    int flags;
};

typedef struct editorRow {
    int size;
    int render_size;
    char *line;
    char *render_line;
    unsigned char* highlight;
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
    char* filename;
    int dirty;

    char status_message[80];
    time_t status_message_time;

    struct editorSyntax* syntax;

    struct termios ORIGINAL_TERMIOS;
};
struct editorConfig E;

/*** Defines ***/
#define WARM_VERSION "0.1.0"

#define CTRL_KEY(c) ((c) & 0x1f)

enum editorKey {
    BACKSPACE = 127,
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

enum editorHighlight {
    HIGHLIGHT_NORMAL = 0,
    HIGHLIGHT_COMMENT,
    HIGHLIGHT_KEYWORD1,
    HIGHLIGHT_KEYWORD2,
    HIGHLIGHT_STRING,
    HIGHLIGHT_NUMBER,
    HIGHLIGHT_MATCH
};

#define HIGHLIGHT_NUMBERS (1<<0)
#define HIGHLIGHT_STRINGS (1<<1)

const int TAB_SIZE = 8;
const int QUIT_TIMES = 3;

/*** Filetypes ***/
char* C_HIGHLIGHT_EXTENSIONS[] = { ".c", ".h", ".cpp", NULL };
char* C_HIGHLIGHT_KEYWORDS[] = {
    "switch", "if", "while", "for", "break", "continue", "return", "else", "struct",
    "union", "typedef", "static", "enum", "class", "case",

    "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|", "void|", NULL
};

struct editorSyntax HIGHLIGHT_DB[] = {
    {
        "c",
        C_HIGHLIGHT_EXTENSIONS,
        C_HIGHLIGHT_KEYWORDS,
        "//",
        HIGHLIGHT_NUMBERS | HIGHLIGHT_STRINGS
    }
};

#define HIGHLIGHT_DB_ENTRIES (sizeof(HIGHLIGHT_DB) / sizeof(HIGHLIGHT_DB[0]))

/*** Terminal ***/
void die(const char* s);
void enableRawTerminalMode();
void disableRawTerminalMode();

/*** Syntax Highlighting ***/
void editorUpdateSyntax(editorRow* row);
int editorSyntaxToColor(int highlight);
void editorSelectSyntaxHighlight();
int isSeparator(int c);

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
void editorDrawStatusBar(struct append_buffer *ab);
void editorDrawMessageBar(struct append_buffer *ab);
void editorSetStatusMessage(const char *fmt, ...);

/*** Input ***/
char *editorPrompt(char *prompt, void(*callback)(char*, int));
void editorMoveCursor(int key);

/*** Row Operations ***/
void editorInsertRow(int at, char *s, size_t len);
void editorDeleteRow(int at);
void editorFreeRow(editorRow* row);
void editorUpdateRow(editorRow* row);
int editorCursorxToRenderx(editorRow* row, int cursor_x);
int editorRenderxToCursorx(editorRow* row, int render_x);
void editorRowInsertChar(editorRow *row, int at, int c);
void editorRowDeleteChar(editorRow *row, int at);
void editorRowAppendString(editorRow *row, char* s, size_t length);

/*** Editor Operations ***/
void editorInsertNewline();
void editorInsertChar(int c);
void editorDeleteChar();

/*** File I/O ***/
void editorOpen(char* filename);
char *editorRowsToString(int *buffer_length);
void editorSave();

/*** Find ***/
void editorFindCallback(char* query, int key);
void editorFind();

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
    E.filename = NULL;
    E.dirty = 0;
    E.status_message[0] = '\0';
    E.status_message_time = 0;
    E.syntax = NULL;
    if (getWindowSize(&E.screen_cols, &E.screen_rows) == -1) {
        die("getWindowSize failed");
    }
    E.screen_rows -= 2; // For status bar
}

int main(int argc, char* argv[]) {
    enableRawTerminalMode();
    initEditor();
    if (argc >= 2) {
        editorOpen(argv[1]);
    }

    editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

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

void editorUpdateSyntax(editorRow* row) {
    row->highlight = realloc(row->highlight, row->render_size);
    memset(row->highlight, HIGHLIGHT_NORMAL, row->render_size);

    if (E.syntax == NULL) return;

    char** keywords = E.syntax->keywords;
    char* single_comment_start = E.syntax->single_comment_start;
    int length = single_comment_start ? strlen(single_comment_start) : 0;

    int prev_separation = 1;
    int in_string = 0;

    for (int i = 0; i < row->render_size; i++) {
        char c = row->render_line[i];
        unsigned char prev_highlight = (i > 0) ? row->highlight[i-1] : HIGHLIGHT_NORMAL;

        if (length && !in_string) {
            if (!strncmp(&row->render_line[i], single_comment_start, length)) {
                memset(&row->highlight[i], HIGHLIGHT_COMMENT, row->render_size - i);
                break;
            }
        }

        if (E.syntax->flags & HIGHLIGHT_STRINGS) {
            if (in_string) {
                row->highlight[i] = HIGHLIGHT_STRING;
                
                if (c == '\\' && i + 1 == row->render_size) {
                    row->render_line[i + 1] = HIGHLIGHT_STRING;
                    i++;
                    continue;
                }
                if (c == in_string)
                    in_string = 0;
                prev_separation = 1;
                continue;
            } else {
                if (c == '"' || c == '\'') {
                    in_string = c;
                    row->highlight[i] = HIGHLIGHT_STRING;
                    continue;
                }
            }
        }

        if (E.syntax->flags & HIGHLIGHT_NUMBERS) {    
            if ((isdigit(c) && (prev_separation || prev_highlight == HIGHLIGHT_NUMBER)) || ((c == '.') && (prev_highlight == HIGHLIGHT_NUMBER))) {
                row->highlight[i] = HIGHLIGHT_NUMBER;
                prev_separation = 0;
                continue;
            }
        }

        if (prev_separation) {
            int j = 0;
            for (j = 0; keywords[j]; j++) {
                int keyword_length = strlen(keywords[j]);
                int keyword2 = keywords[j][keyword_length - 1] == '|';
                if (keyword2)
                    keyword_length--;

                if (!strncmp(&row->render_line[i], keywords[j], keyword_length) && isSeparator(row->render_line[i + keyword_length])) {
                    memset(&row->highlight[i], keyword2 ? HIGHLIGHT_KEYWORD2 : HIGHLIGHT_KEYWORD1, keyword_length);
                    i += keyword_length;
                    break;
                }
            }

            if (keywords[j] != NULL) {
                prev_separation = 0;
                continue;
            } 
        }

        prev_separation = isSeparator(c);
    }
}

int editorSyntaxToColor(int highlight) {
    switch (highlight) {
        case HIGHLIGHT_COMMENT:
            return 36;
        case HIGHLIGHT_KEYWORD1:
            return 33;
        case HIGHLIGHT_KEYWORD2:
            return 32;
        case HIGHLIGHT_STRING:
            return 35;
        case HIGHLIGHT_NUMBER:
            return 31;
        case HIGHLIGHT_MATCH:
            return 34;
        default:
            return 37;
    }
}

void editorSelectSyntaxHighlight() {
    E.syntax = NULL;
    if (E.filename == NULL) return;

    char* extension = strchr(E.filename, '.');

    for (unsigned int j = 0; j < HIGHLIGHT_DB_ENTRIES; j++) {
        struct editorSyntax* s = &HIGHLIGHT_DB[j];
        
        unsigned int i = 0;
        while (s->filematch[i]) {
            int is_extension = (s->filematch[i][0] == '.');

            if ((is_extension && extension && !strcmp(extension, s->filematch[i])) || (!is_extension && strstr(E.filename, s->filematch[i]))) {
                E.syntax = s;

                for (int file_row = 0; file_row < E.num_rows; file_row++) {
                    editorUpdateSyntax(&E.row[file_row]);
                }
                return;
            }
            i++;
        }
    }
}

int isSeparator(int c) {
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
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
    static int quit_times = QUIT_TIMES;
    int c = editorReadKey();

    switch (c) {
        case '\r':
            editorInsertNewline();
            break;
        
        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
            if (c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
            editorDeleteChar();
            break;

        case CTRL_KEY('l'):
        case '\x1b':
            //TODO
            break;
        
        case CTRL_KEY('s'):
            editorSave();
            break;
        
        case CTRL_KEY('f'):
            editorFind();
            break;

        case CTRL_KEY('q'):
            if (E.dirty && quit_times > 0) {
                editorSetStatusMessage("WARNING! File has unsaved changes. "
                                        "Press Ctrl-Q %d more times to quit.", quit_times);
                quit_times--;
                return;
            }
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
            if (E.cursor_y < E.num_rows)
                E.cursor_x = E.row[E.cursor_y].size;
            break;
        case ARROW_UP:
        case ARROW_LEFT:
        case ARROW_DOWN:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
        default:
            editorInsertChar(c);
            break;
    }

    quit_times = QUIT_TIMES;
}

void editorRefreshScreen() {
    editorScroll();

    struct append_buffer ab = ABUF_INIT;

    buffer_append(&ab, "\x1b[?25l", 6);
    buffer_append(&ab, "\x1b[H", 3);
    
    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);
    
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
            
            char* c = &E.row[file_row].render_line[E.col_offset];
            unsigned char* highlight = &E.row[file_row].highlight[E.col_offset];
            int current_color = -1;

            for (int j = 0; j < length; j++) {
                if (highlight[j] == HIGHLIGHT_NORMAL) {
                    if (current_color != -1) {
                        buffer_append(ab, "\x1b[39m", 5);
                        current_color = -1;
                    }
                    buffer_append(ab, &c[j], 1);
                } else {
                    int color = editorSyntaxToColor(highlight[j]);
                    
                    if (color != current_color) {
                        current_color = color;
                        char buffer[16];
                        int c_length = snprintf(buffer, sizeof(buffer), "\x1b[%dm", color);
                        buffer_append(ab, buffer, c_length);
                    }
                    
                    buffer_append(ab, &c[j], 1);
                }
            }

            buffer_append(ab, "\x1b[39m", 5);
        }

        buffer_append(ab, "\x1b[K", 3);
        buffer_append(ab, "\r\n", 2);
    }
}

void editorDrawStatusBar(struct append_buffer *ab) {
    buffer_append(ab, "\x1b[7m", 4);
    char status[80];
    char render_status[80];

    int length = snprintf(
        status,
        sizeof(status),
        "%.20s - %d lines %s",
        E.filename ? E.filename : "[No Name]",
        E.num_rows, 
        E.dirty ? "(modified)" : ""
        );
    
    int render_length = snprintf(
        render_status,
        sizeof(render_status),
        "%s | %d/%d",
        E.syntax ? E.syntax->filetype : "no file type",
        E.cursor_y + 1,
        E.num_rows
        );

    if (length > E.screen_cols)
        length = E.screen_cols;
    buffer_append(ab, status, length);

    while (length < E.screen_cols) {
        if (E.screen_cols - length == render_length) {
            buffer_append(ab, render_status, render_length);
            break;
        }
        buffer_append(ab, " ", 1);
        length++;
    }
    buffer_append(ab, "\x1b[m", 3);
    buffer_append(ab, "\r\n", 2);
}

void editorSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.status_message, sizeof(E.status_message), fmt, ap);
    va_end(ap);
    E.status_message_time = time(NULL);
}

void editorDrawMessageBar(struct append_buffer *ab) {
    buffer_append(ab, "\x1b[K", 3);

    int message_length = strlen(E.status_message);
    if (message_length > E.screen_cols)
        message_length = E.screen_cols; 
    if (message_length && time(NULL) - E.status_message_time < 5) {
        buffer_append(ab, E.status_message, message_length);
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

char *editorPrompt(char *prompt, void(*callback)(char*, int)) {
    size_t buffer_size = 128;
    char* buffer = malloc(buffer_size);

    size_t length = 0;
    buffer[0] = '\0';

    while(1) {
        editorSetStatusMessage(prompt, buffer);
        editorRefreshScreen();

        int c = editorReadKey();

        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
            if (length != 0) {
                buffer[--length] = '\0';
            }
        } else if (c == '\x1b') {
            editorSetStatusMessage("");
            if (callback) callback(buffer, c);
            free(buffer);
            return NULL;
        } else if (c == '\r') {
            editorSetStatusMessage("");
            if (callback) callback(buffer, c);
            return buffer;
        }
        else if (!iscntrl(c) && c < 128) {
            if (length == buffer_size - 1) {
                buffer_size *= 2;
                buffer = realloc(buffer, buffer_size);
            }
            buffer[length++] = c;
            buffer[length] = '\0'; 
        }
        if (callback) callback(buffer, c);
    }
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
    free(E.filename);
    E.filename = strdup(filename);

    editorSelectSyntaxHighlight();

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
        editorInsertRow(E.num_rows, line, line_length);
    }

    free(line);
    fclose(fp);
    E.dirty = 0;
}

char *editorRowsToString(int *buffer_length) {
    int length = 0;
    for (int i = 0; i < E.num_rows; i++) {
        length += E.row[i].size + 1;
    }
    *buffer_length = length;

    char* buffer = malloc(length);
    if (!buffer) die("editorRowsToString failed");

    char* p = buffer;

    for (int i = 0; i < E.num_rows; i++) {
        memcpy(p, E.row[i].line, E.row[i].size);
        p += E.row[i].size;
        *p = '\n';
        p++;
    }

    return buffer;
}

void editorSave() {
    if (E.filename == NULL) {
        E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
        if (E.filename == NULL) {
            editorSetStatusMessage("Save aborted");
            return;
        }
        editorSelectSyntaxHighlight();
    }
    int length;
    char *buffer = editorRowsToString(&length);

    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
    if (fd != -1) {
        if (ftruncate(fd, length) != -1) {
        if (write(fd, buffer, length) == length) {
            close(fd);
            free(buffer);
            E.dirty = 0;
            editorSetStatusMessage("%d bytes written to disk", length);
            return;
        }
        }
        close(fd);
    }
    free(buffer);
    editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

void editorFindCallback(char* query, int key) {
    static int last_match = -1;
    static int direction = 1;

    static int saved_highlight_line;
    static char* saved_highlight = NULL;

    if (saved_highlight) {
        memcpy(E.row[saved_highlight_line].highlight, saved_highlight, E.row[saved_highlight_line].render_size);
        free(saved_highlight);
        saved_highlight = NULL;
    }

    if (key == '\r' || key == '\x1b') {
        last_match = -1;
        direction = 1;
        return;
    } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
        direction = 1;
    } else if (key == ARROW_LEFT || key == ARROW_UP) {
        direction = -1;
    } else {
        last_match = -1;
        direction = 1;
    }


    if (last_match == -1) {
        direction = 1;
    }

    int current_row = last_match;

    for (int i = 0; i < E.num_rows; i++) {
        current_row += direction;
        if (current_row == -1) {
            current_row = E.num_rows - 1;
        } else if (current_row == E.num_rows) {
            current_row = 0;
        }
        
        editorRow* row = &E.row[current_row];
        char* match = strstr(row->render_line, query);

        if (match) {
            last_match = current_row;
            E.cursor_y = current_row;
            E.cursor_x = editorRenderxToCursorx(row, match - row->render_line);
            E.row_offest = E.num_rows;

            saved_highlight_line = current_row;
            saved_highlight = malloc(row->render_size);
            memcpy(saved_highlight,row->highlight, row->render_size);
            memset(&row->highlight[match - row->render_line], HIGHLIGHT_MATCH, strlen(query));
            break;
        }
    }
}

void editorFind() {
    int saved_x = E.cursor_x;
    int saved_y = E.cursor_y;
    int saved_col_offset = E.col_offset;
    int saved_row_offset = E.row_offest;
    
    char* query = editorPrompt("Search: %s (Use ESC/Arrows/Enter)", editorFindCallback);
    
    if (query)
        free(query);
    
    if (!query) {
        E.cursor_x = saved_x;
        E.cursor_y = saved_y;
        E.col_offset = saved_col_offset;
        E.row_offest = saved_row_offset;
    }
}

void editorInsertRow(int at, char *s, size_t len) {
    if (at < 0 || at > E.num_rows) return;

    E.row = realloc(E.row, sizeof(editorRow) * (E.num_rows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(editorRow) * (E.num_rows - at));

    E.row[at].size = len;
    E.row[at].line = malloc(len + 1);
    memcpy(E.row[at].line, s, len);
    E.row[at].line[len] = '\0';

    E.row[at].render_size = 0;
    E.row[at].render_line = NULL;
    E.row[at].highlight = NULL;

    editorUpdateRow(&E.row[at]);

    E.num_rows++;
    E.dirty++;
}

void editorDeleteRow(int at) {
    if (at < 0 || at >= E.num_rows) return;
    editorFreeRow(&E.row[at]);
    
    memmove(&E.row[at], &E.row[at + 1], sizeof(editorRow) * (E.num_rows - at - 1));
    E.num_rows--;
    E.dirty++;
}

void editorFreeRow(editorRow* row) {
    free(row->render_line);
    free(row->line);
    free(row->highlight);
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

    editorUpdateSyntax(row);
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

int editorRenderxToCursorx(editorRow* row, int render_x) {
    int curr = 0;
    int cursor_x;
    for (cursor_x = 0; cursor_x < row->size; cursor_x++) {
        if (row->line[cursor_x] == '\t')
            curr += (TAB_SIZE - 1) - (curr % TAB_SIZE);
        curr++;

        if (curr > render_x) return cursor_x;
    }

    return cursor_x;
}

void editorRowInsertChar(editorRow *row, int at, int c) {
    if (at < 0 || at > row->size)
        at = row->size;

    row->line = realloc(row->line, row->size + 2);
    memmove(&row->line[at + 1], &row->line[at], row->size - at + 1);
    row->size++;
    row->line[at] = c;

    editorUpdateRow(row);
    E.dirty++;
}

void editorRowDeleteChar(editorRow *row, int at) {
    if (at < 0 || at >= row->size)
        return;

    memmove(&row->line[at], &row->line[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowAppendString(editorRow *row, char* s, size_t length) {
    row->line = realloc(row->line, row->size + length + 1);

    memcpy(&row->line[row->size], s, length);
    row->size += length;
    row->line[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}

void editorInsertNewline() {
    if (E.cursor_x == 0) {
        editorInsertRow(E.cursor_y, "", 0);
    } else {
        editorRow* row = &E.row[E.cursor_y];

        editorInsertRow(E.cursor_y + 1, &row->line[E.cursor_x], row->size - E.cursor_x);
        row = &E.row[E.cursor_y];
        row->size = E.cursor_x;
        row->line[row->size] = '\0';
        editorUpdateRow(row);
    }

    E.cursor_y++;
    E.cursor_x = 0;
}

void editorInsertChar(int c) {
    if (E.cursor_y == E.num_rows) {
        editorInsertRow(E.num_rows, "", 0);
    }
    editorRowInsertChar(&E.row[E.cursor_y], E.cursor_x, c);
    E.cursor_x++;
}

void editorDeleteChar() {
    if (E.cursor_y == E.num_rows) return;
    if (E.cursor_x == 0 && E.cursor_y == 0) return;

    editorRow* row = &E.row[E.cursor_y];
    if (E.cursor_x > 0) {
        editorRowDeleteChar(row, E.cursor_x - 1);
        E.cursor_x--;
    } else {
        E.cursor_x = E.row[E.cursor_y - 1].size;
        editorRowAppendString(&E.row[E.cursor_y - 1], row->line, row->size);
        editorDeleteRow(E.cursor_y);
        E.cursor_y--;
    }
}
