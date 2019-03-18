#include "tex.h"


/*--------------------------------------------------------------------------
                                TERMINAL
--------------------------------------------------------------------------*/

void die(const char *s) {
    // Clear screen
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);  // Print descriptive error message
    exit(EXIT_FAILURE); // Exit
}


void disableRawMode() {
    // Error Handling
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios); // Set terminal attributes back to default
}


void enableRawMode() {
    // Error Handling
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");

    atexit(disableRawMode); // Call disableRawMode automatically on program exit

    struct termios raw = E.orig_termios;  // Make a copy of original terminal attributes

    // Disable ctrl-(s&z&m) signals & other misc flags incase they're on by default
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_iflag &= ~(OPOST);    // Disable all output processing

    // Disable user input echo, turn off canonical mode, disable ctrl-(c&z&v) signals
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    
    // read() timeout
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);   // Apply terminal attributes
}


int editorReadKey() {
    int nRead;
    char c;
    // Read until ctrl-q
    while ((nRead = read(STDIN_FILENO, &c, 1)) != 1) {
        // Handle Errors
        if (nRead == -1 && errno != EAGAIN)
            die("read");
    }

    // Handle multi-byte keys as input
    if (c == '\x1b') {
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return '\x1b';
        
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1)
                    return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1':
                            return HOME_KEY;
                        case '3':
                            return DEL_KEY;
                        case '4':
                            return END_KEY;
                        case '5':
                            return PAGE_UP;
                        case '6':
                            return PAGE_DOWN;
                        case '7':
                            return HOME_KEY;
                        case '8':
                            return END_KEY;
                    }
                }
            } else {
            switch (seq[1]) {
                    // Arrow key Input
                    case 'A':
                        return ARROW_UP;
                    case 'B':
                        return ARROW_DOWN;
                    case 'C':
                        return ARROW_RIGHT;
                    case 'D':
                        return ARROW_LEFT;
                    case 'H':
                        return HOME_KEY;
                    case 'F':
                        return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1])
            {
                case 'H':
                    return HOME_KEY;
                case 'F':
                    return END_KEY;
            }
        }
        return '\x1b';
        
    } else {
        return c;
    }
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];   // 32 Character buffer
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;


    while (i < (sizeof(buf) - 1)) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;
        
        if (buf[i] == 'R')  //Break when R is read into buffer
            break;

        i++;
    }

    buf[i] = '\0';  // Add escape char

    if (buf[0] != '\x1b' || buf[1] != '[')
        return -1;

    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return -1;

    return 0;
}


int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    // On success, ioctl wil lpalce the num of cols and rows into the given winsize struct
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        /*  Fallback for ioctl get window size
            Position cursor at bottom right then use escape  sequences to quesry cursor position
            Finds the number of rows and columns in the terminal window
        */
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;

        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}


/*--------------------------------------------------------------------------
                            ROW OPERATIONS
--------------------------------------------------------------------------*/

void editorAppendRow(char *s, size_t len) {
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1)); // Reallocate space for new row

    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);

    if (E.row[0].chars == NULL)
        die("E.row.chars malloc failed");

    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.numrows++;
}


/*--------------------------------------------------------------------------
                                 FILE IO
--------------------------------------------------------------------------*/

void editorOpen(char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp)
        die("fopen");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    // Read until EOFs
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
            linelen--;
        
        editorAppendRow(line, linelen);   
    }
    free(line);
    fclose(fp);
}


/*--------------------------------------------------------------------------
                               APPEND BUFFER
--------------------------------------------------------------------------*/

void abAppend(struct aBuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);  // Realloc for new string length

    if (new == NULL)
        return;
    
    // Assign struct fields
    memcpy(&new[ab->len], s, len);  // Copy string s after end of current buffer
    ab->b = new;    // Update pointer
    ab->len += len; // Update length
}

void abFree(struct aBuf *ab) {
    free(ab->b);    // Free char pointer
}


/*--------------------------------------------------------------------------
                                   OUTPUT
--------------------------------------------------------------------------*/

void editorScroll() {
    // Cursor is above visible window
    if (E.cy < E.rowOff) {
        E.rowOff = E.cy;    // Scroll to where cursor is
    }

    // Cursor is past bottom of visible window
    if (E.cy >= E.rowOff + E.screenRows) {
        E.rowOff = E.cy - E.screenRows + 1; // Scroll just past bottom of screen, but not past EOF
    }

    // Cursor is left of visible window
    if (E.cx < E.colOff) {
        E.colOff = E.cx;
    }

    // Cursor is right of visible window
    if (E.cx >= E.colOff + E.screenCols) {
        E.colOff = E.cx - E.screenCols + 1;
    }
}

void editorDrawRows(struct aBuf *ab) {
    int y;  // Terminal height
    // Draw rows of ~ for entire terminal window
    for(y = 0; y < E.screenRows; y++) {
        int fileRow = y + E.rowOff;
        if (fileRow >= E.numrows) {
            if (E.numrows == 0 && y == E.screenRows / 3) {
                // Display welcome message
                char welcome[80];
                int welcomeLen = snprintf(welcome, sizeof(welcome), "\tTex Editor -- version: %s", TEX_VERSION);
                if (welcomeLen > E.screenCols)
                    welcomeLen = E.screenCols;

                // Add welcome message padding
                int padding = (E.screenCols - welcomeLen) / 2;  // Centre
                if (padding) {
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while (padding--)
                    abAppend(ab, " ", 1);

                abAppend(ab, welcome, welcomeLen);
            } else {
                abAppend(ab, "~", 1);   //Append line tildes
            }
        } else {
            int len = E.row[fileRow].size - E.colOff;
            // Set len to 0 incase its negative
            if (len < 0)
                len = 0;
            if (len > E.screenCols)
                len = E.screenCols;
            
            abAppend(ab, &E.row[fileRow].chars[E.colOff], len);
        }

        abAppend(ab, "\x1b[K", 3);  // Escaape K sequence at end of each line
        // Make the last line an exception for the carriage return and newline
        if (y < E.screenRows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}


void editorRefreshScreen() {
    editorScroll();

    struct aBuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);  // Show Mouse Cursor
    abAppend(&ab, "\x1b[H", 3);  // Reposition cursor to top left

    editorDrawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowOff) + 1, (E.cx - E.colOff) + 1);  // Add 1 to convert from 0 based C to 1 based terminal
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);  // Hide Mouse Cursor

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}


/*--------------------------------------------------------------------------
                                   INPUT
--------------------------------------------------------------------------*/

void editorMoveCursor(int key) {
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

    switch (key)
    {
        case ARROW_LEFT:
            // Move cursor left
            if (E.cx != 0) {
                E.cx--;
            // If at start of line, go to end of prev line
            } else if (E.cy > 0) {
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            // Check if cursor is to the left of the end of the line
            // Move cursor right
            if (row && E.cx < row->size) {
                E.cx++;
            // Move cursor to beginning of next line
            } else if (row && E.cx == row->size) {
                E.cy++;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            // Move cursor up a line
            if (E.cy != 0) {
                E.cy--;
            }
            break;
        case ARROW_DOWN:
            // Move cursor down a line
            if (E.cy < E.numrows)
                E.cy++;
            break;
    }

    // Snap cursor to end of line in case it ends up past it
    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowLen = row ? row->size : 0;
    if (E.cx > rowLen)
        E.cx = rowLen;

}


void editorProcessKeyPress() {
    int c = editorReadKey();

    switch (c)
    {
        // CTRL-Q sucessful exit
        case CTRL_KEY('q'):
            // Clear screen
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(EXIT_SUCCESS);
            break;

        case HOME_KEY:
            E.cx = 0;   // Move cursor to left edge
            break;
        case END_KEY:
            E.cx = E.screenCols - 1;
            break;

        case PAGE_UP:   // Move cursor to top edge
        case PAGE_DOWN: // Move cursor to bottom edge
            {
                int times = E.screenRows;
                while (times--)
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
    }
}


/*--------------------------------------------------------------------------
                                   INIT
--------------------------------------------------------------------------*/

void initEditor() {
    // Set Cursor pos to top left
    E.cx = 0;
    E.cy = 0;
    
    // Default scroll to top left
    E.rowOff = 0;
    E.colOff = 0;

    E.numrows = 0;
    E.row = NULL;

    // Error Handling
    if (getWindowSize(&E.screenRows, &E.screenCols) == -1)
        die("getWindowSize");
}

int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    if (argc >= 2) {
        editorOpen(argv[1]);
    }

    while (1) {
        editorRefreshScreen();
        editorProcessKeyPress();
    }

    return 0;
}
