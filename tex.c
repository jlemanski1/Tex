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

int editorRowCxToRx(erow *row, int cx) {
    int rx = 0;
    int j;

    for (j = 0; j < cx; j++) {
        if (row->chars[j] == '\t')
            // How many columns cursor is to the left of the tab stop, add to rx
            rx += (TEX_TAB_STOP - 1) - (rx % TEX_TAB_STOP);
        rx++;
    }
    return rx;
}


void editorUpdateRow(erow *row) {
    int tabs = 0;
    int j;

    // Count the tabs to calc the memory required for render
    for (j = 0; j< row->size; j++) {
        if (row->chars[j] == '\t')
            tabs++;
    }

    free(row->render);  // Free any previous renders
    row->render = malloc(row->size + tabs * (TEX_TAB_STOP - 1) + 1); //Allocate memory for new render
    
    int idx = 0;    // Number of characters copied into row->render

    for (j = 0; j < row->size; j++) {
        if (row->chars[j] == '\t') {
            row->render[idx++] = ' ';
            while (idx % TEX_TAB_STOP != 0)
                row->render[idx++] = ' ';

        } else {
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->rSize = idx;
}


void editorAppendRow(char *s, size_t len) {
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1)); // Reallocate space for new row

    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);

    if (E.row[0].chars == NULL)
        die("E.row.chars malloc failed");

    // Copy string into row's char array
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';    // Add NULL terminator

    // Reset rSize & render
    E.row[at].rSize = 0;
    E.row[at].render = NULL;
    editorUpdateRow(&E.row[at]);    // Update render & rSize fields with the new row content

    E.numrows++;
    E.dirty++;  // Increment dirty after changing text
}


void editorFreeRow(erow *row) {
    free(row->render);
    free(row->chars);
}

void editorDelRow(int at) {
    // Return if row is undeletable (after EOF)
    if (at < 0 || at >= E.numrows)
        return;

    editorFreeRow(&E.row[at]);  // Free memory used by the row
    // Overwrite the deleted row struct with the rest of the rows that come after it
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
    E.numrows--;    // Decrement numrows after deletion
    E.dirty++;      // Mark as modified
} 


void editorRowInsertChar(erow *row, int at, int c) {
    // Validate at before assignment
    if (at < 0 || at > row->size)
        at = row->size;
    
    // Allocate extra byte for char and NULL byte
    row->chars = realloc(row->chars, row->size + 2);
    // Make room for new char
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;        // Increment size
    row->chars[at] = c; // Assign char to position in the array
    editorUpdateRow(row);   // Update render & rSize fields with the new row content
    E.dirty++;  // Increment dirty after changing text
}


void editorRowAppendString(erow *row, char *s, size_t len) {
    // Realloc enough memory for the new string
    row->chars = realloc(row->chars, row->size + len + 1);
    // memcpy the string to the end of the contents of row->chars
    memcpy(&row->chars[row->size], s, len);
    row->size += len;   // Increment size with length of new string
    row->chars[row->size] = '\0';   // Add Null terminator to end
    editorUpdateRow(row);   // Update row
    E.dirty++;  // Mark as modified
}


void editorRowDelChar(erow *row, int at) {
    // Return if undeletable
    if (at < 0 || at >= row->size)
        return;

    // Overwrite the char to delete with the chars that come after it
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);

    row->size--;    // Decrement size after removing char
    editorUpdateRow(row);   // Update row
    E.dirty++;  // Mark as modified
}


/*--------------------------------------------------------------------------
                            EDITOR OPERATIONS
--------------------------------------------------------------------------*/

void editorInsertChar(int c) {
    // Cursor is on the tilde line after EOF
    if (E.cy == E.numrows)
        editorAppendRow("", 0); // Append new row to file before inserting char
    
    editorRowInsertChar(&E.row[E.cy], E.cx, c);
    E.cx++; // Increment cursor after inserted char
}

void editorDelChar() {
    // Return if cursor is past EOF
    if (E.cy == E.numrows)
        return;
        
    // If cursor is at top left corner, there's nothing to delete, return
    if (E.cx == 0 && E.cy == 0)
        return;
    
    erow *row = &E.row[E.cy];

    if (E.cx > 0) {
        editorRowDelChar(row, E.cx - 1);
        E.cx--;
    } else {
        /*
            Set cx to the end of the contents of the previous row so the cursor ends
            up where the two lines joined up
        */
        E.cx = E.row[E.cy - 1].size;
        // Append the current row to the previous row
        editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
        editorDelRow(E.cy); // Delete the row being pointed to
        E.cy--;
    }
}


/*--------------------------------------------------------------------------
                                 FILE IO
--------------------------------------------------------------------------*/

char *editorRowsToString(int *buflen) {
    int totlen = 0;
    int j;

    // Get total length, from lengths of each row of text
    for (j = 0; j < E.numrows; j++)
        totlen += E.row[j].size + 1;
    
    *buflen = totlen;   // Save total length into buflen

    char *buf = malloc(totlen); // Allocate memory for the string
    char *p = buf;

    for (j = 0; j < E.numrows; j++) {
        // Copy contents of each row to the end of the buffer
        memcpy(p, E.row[j].chars, E.row[j].size);
        p += E.row[j].size; // Move pointer to end of line
        *p = '\n';  // Append newline after each row
        p++;        // Increment pointer
    }

    return buf;
}


void editorOpen(char *filename) {
    free(E.filename);   // Free prev filename
    E.filename = strdup(filename);  // Duplicate filename, returns identical malloc-ed string

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
    E.dirty = 0;    // Reset flag so user isn't alerted after opening file
}


void editorSave() {
    // New file, don't know where to save so just return
    if (E.filename == NULL)
        return;
    
    int len;
    char *buf = editorRowsToString(&len);

    // Open/Create new file if it doesn't exist, for read&write, and with proper permissions
    int fp = open(E.filename, O_RDWR | O_CREAT, 0644);

    // Error Handling
    if (fp != -1) {
        if (ftruncate(fp, len) != -1){
            if (write(fp, buf, len) == len) {
                // Successful save
                close(fp);
                free(buf);
                E.dirty = 0;    // Reset flag after saving
                // Notify user on sucessful save
                editorSetStatusMessage("%d bytes written to disk", len);
                return;
            }
        }
        close(fp);
    }
    // Unsuccessful save
    free(buf);
    editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
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
    E.rx = 0;
    // Set rx
    if (E.cy < E.numrows) {
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
    }

    // Cursor is above visible window
    if (E.cy < E.rowOff) {
        E.rowOff = E.cy;    // Scroll to where cursor is
    }

    // Cursor is past bottom of visible window
    if (E.cy >= E.rowOff + E.screenRows) {
        E.rowOff = E.cy - E.screenRows + 1; // Scroll just past bottom of screen, but not past EOF
    }

    // Cursor is left of visible window
    if (E.rx < E.colOff) {
        E.colOff = E.rx;
    }

    // Cursor is right of visible window
    if (E.rx >= E.colOff + E.screenCols) {
        E.colOff = E.rx - E.screenCols + 1;
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
            int len = E.row[fileRow].rSize - E.colOff;
            // Set len to 0 incase its negative
            if (len < 0)
                len = 0;
            if (len > E.screenCols)
                len = E.screenCols;
            
            abAppend(ab, &E.row[fileRow].render[E.colOff], len);
        }

        abAppend(ab, "\x1b[K", 3);  // Escaape K sequence at end of each line
        // Make the last line an exception for the carriage return and newline
        
        abAppend(ab, "\r\n", 2);
        
    }
}


void editorDrawStatusBar(struct aBuf *ab) {
    abAppend(ab, "\x1b[7m", 4);     // Invert status bar colours [7m
    char status[80], rStatus[80];
    // Cut string short if it doesn't fit, display [No Name] if there's no filename
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
        E.filename ? E.filename: "[No Name]", E.numrows,
        E.dirty ? "(modified)" : "");   // Alert user when file is modified since last save
    
    int rLen = snprintf(rStatus, sizeof(rStatus), "%d/%d", E.cy + 1, E.numrows);

    if (len > E.screenCols)
        len = E.screenCols;
    abAppend(ab, status, len);
    
    while (len < E.screenCols) {
        // Display current line number on right edge
        if (E.screenCols - len == rLen) {
            abAppend(ab, rStatus, rLen);
            break;
        } else {
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);    // Add space for status bar message
}


void editorSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsgTime = time(NULL);
}


void editorDrawMessageBar(struct aBuf *ab) {
    abAppend(ab, "\x1b[K", 3);  // Clear the message bar
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screenCols)
        msglen = E.screenCols;
    // Display msg to bar if its less than 5 secs old
    if (msglen && time(NULL) - E.statusmsgTime < 5)
        abAppend(ab, E.statusmsg, msglen);
}


void editorRefreshScreen() {
    editorScroll();

    struct aBuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);  // Show Mouse Cursor
    abAppend(&ab, "\x1b[H", 3);  // Reposition cursor to top left

    editorDrawRows(&ab);        // Draw Rows
    editorDrawStatusBar(&ab);   // Draw Status bar
    editorDrawMessageBar(&ab);  // Update status bar message

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowOff) + 1, (E.rx - E.colOff) + 1);  // Add 1 to convert from 0 based C to 1 based terminal
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
    static int quitCount = TEX_QUIT_AMOUNT;    // Track amount of quit keypresses
    int c = editorReadKey();

    switch (c)
    {
        case '\r':
            // TODO
            break;

        // CTRL-Q sucessful exit
        case CTRL_KEY('q'):
            // Require confimation to exit with unsaved changes
            if (E.dirty && quitCount > 0) {
                // Warn User
                editorSetStatusMessage("WARNING! %s has unsaved changes."
                    "Press Ctrl-Q %d more times to quit.", E.filename, quitCount);
                quitCount--;
                return;
            }
            // Clear screen
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(EXIT_SUCCESS);
            break;

        // CTRL-S Save editor
        case CTRL_KEY('s'):
            editorSave();
            break;

        case HOME_KEY:
            E.cx = 0;   // Move cursor to left edge
            break;
        case END_KEY:
            if (E.cy < E.numrows)
                E.cx = E.row[E.cy].size;
            break;
        
        case BACKSPACE:         // Delete char to the left of the cursor
        case CTRL_KEY('h'):     // Delete char to the left of the cursor
        case DEL_KEY:
            // Delete the character to the right of the cursor
            if (c == DEL_KEY)
                editorMoveCursor(ARROW_RIGHT);
            editorDelChar();
            break;

        case PAGE_UP:   // Move cursor to top edge
        case PAGE_DOWN: // Move cursor to bottom edge
            {
                // Scroll Entire Page
                if (c == PAGE_UP) {
                    E.cy = E.rowOff;
                } else if (c == PAGE_DOWN) {
                    E.cy = E.rowOff + E.screenRows - 1;
                    if (E.cy > E.numrows)
                        E.cy = E.numrows;
                }

                // Scroll to bottom of page
                int times = E.screenRows;
                while (times--)
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;

        // Arrow navigation
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;

        case CTRL_KEY('l'):
        case '\x1b':
            break;
        
        // Allow any unmapped keypress to be inserted directly into the text being edited. 
        default:
            editorInsertChar(c);
            break;
    }
    quitCount = TEX_QUIT_AMOUNT;   // Rest quit counter
}


/*--------------------------------------------------------------------------
                                   INIT
--------------------------------------------------------------------------*/

void initEditor() {
    // Set Cursor pos to top left
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;

    // Default scroll to top left
    E.rowOff = 0;
    E.colOff = 0;

    E.numrows = 0;
    E.row = NULL;
    E.dirty = 0;

    // Initialize Status bar
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsgTime = 0;

    // Error Handling
    if (getWindowSize(&E.screenRows, &E.screenCols) == -1)
        die("getWindowSize");

    E.screenRows -= 2;  // Make room for the status bar & status message
}

int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    if (argc >= 2) {
        editorOpen(argv[1]);
    }

    // Set initial status message
    editorSetStatusMessage("HELP: CTRL-S 'save' | Ctrl-Q 'quit'");

    while (1) {
        editorRefreshScreen();
        editorProcessKeyPress();
    }

    return 0;
}
