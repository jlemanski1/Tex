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
                          SYNTAX HIGHLIGHTING
--------------------------------------------------------------------------*/

int isSeparator(int c) {
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}


void editorUpdateSyntax(erow *row) {
    // Realloc for new row
    row->highlight = realloc(row->highlight, row->rSize);

    // Set all characters to HL_NORMAL by default
    memset(row->highlight, HL_NORMAL, row->rSize);

    // Return if no filetype is detecting
    if (E.syntax == NULL)
        return;

    // Make keywords an alias for readability
    char **keywords = E.syntax->keywords;
    
    // Make aliases for readability
    char *scs = E.syntax->singleline_comment_start;
    char *mcs = E.syntax->multiline_comment_start;
    char *mce = E.syntax->multiline_comment_end;

    // Used to determine whether to highlight or not
    int scs_len = scs ? strlen(scs) : 0;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;;

    int prev_sep = 1;   // Mark beginning of line to be a separator
    int in_string = 0;  // Keep track of whether char is part of a string or not
    // Keep track of whether char is part of a multiline comment
    int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);

    // Loop through the characters and set digits to HL_NUMBER
    int i = 0;
    while (i < row->rSize) {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->highlight[i - 1] : HL_NORMAL;

        // Check if single line comment should be highlighted (not in a string)
        if (scs_len && !in_string && !in_comment) {
            if (!strncmp(&row->render[i], scs, scs_len)) {
                memset(&row->highlight[i], HL_COMMENT, row->rSize - i);
                break;
            }
        }

        // Check if multi line comment should be highlighted or not
        if (mcs_len && mce_len && !in_string) {
            if (in_comment) {
                row->highlight[i] = HL_MLCOMMENT;
                // Check for the end of a ML_Comment
                if (!strncmp(&row->render[i], mce, mce_len)) {
                    memset(&row->highlight[i], HL_MLCOMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                } else {
                    i++;
                    continue;
                }
            // Check for start of a ML_Comment
            } else if (!strncmp(&row->render[i], mcs, mcs_len)) {
                memset(&row->highlight[i], HL_MLCOMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }

        // Check if strings should be highlighted for the current filetype
        if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
            // String is set, highlight current character
            if (in_string) {
                row->highlight[i] = HL_STRING;
                // Take escape quotes into account, highlight char after backslash then iterate over both
                if (c == '\\' && i + 1 < row->rSize) {
                    row->highlight[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }

                // Reset in_string once character is highlighted
                if (c == in_string)
                    in_string = 0;
                i++;
                prev_sep = 1;
                continue;
            } else {
                // Check for the beginning of a string
                if (c == '"' || c == '\'') {
                    // store the quote in in_string, highlight it, then iterate over it
                    in_string = c;
                    row->highlight[i] = HL_STRING;
                    i++;
                    continue;
                }
            }
        }

        // Check if numbers should be highlighted for the current filetype
        if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
            // Highlight Numbers
            if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) || (c == '.' && prev_hl == HL_NUMBER)) {
                row->highlight[i] = HL_NUMBER;
                i++;
                prev_sep = 0;
                continue;
            }
        }

        // Check if keyword should be highlighted
        if (prev_sep) {
            int j;
            for (j = 0; keywords[j]; j++) {
                int klen = strlen(keywords[j]);
                int types = keywords[j][klen - 1] == '|';
                if (types)
                    klen--;
                // Check if there's a keyword to highlight
                if (!strncmp(&row->render[i], keywords[j], klen) && isSeparator(row->render[i + klen])) {
                    // Highlight the whole keyword/type at once
                    memset(&row->highlight[i], types ? HL_TYPE : HL_KEYWORD, klen);
                    i += klen;
                    break;
                }
            }
            if (keywords[j] != NULL) {
                prev_sep = 0;
                continue;
            }
        }

        prev_sep = isSeparator(c);
        i++;
    }

    int changed = (row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment;
    if (changed && row->idx + 1 < E.numrows)
        editorUpdateSyntax(&E.row[row->idx + 1]);
}


int editorSyntaxToColour(int highlight) {
    switch (highlight) {
        case HL_COMMENT:
        case HL_MLCOMMENT:
            return 36;  // Cyan
        
        case HL_KEYWORD:
            return 33;  // Yellow
        
        case HL_TYPE:
            return 32;  // Green

        case HL_STRING:
            return 35;  // Magenta

        case HL_NUMBER:
            return 31;

        case HL_MATCH:
            return 34;  // Blue
    
        default:
            return 37;
    }
}


void editorSelectSyntaxHighlight() {
    // Set to NULL, so that if nothing matches or there's no name, there is no filetype
    E.syntax = NULL;
    if (E.filename == NULL)
        return;

    char *ext = strchr(E.filename, '.');    // Get a pointer to the extension part of the filename

    // Loop through HLDB
    for (unsigned int i = 0; i < HLDB_ENTRIES; i ++) {
        struct editorSyntax *s = &HLDB[i];
        unsigned int j = 0;
        // For each entrie, loop through each pattern in it's filematch array
        while (s->filematch[j]) {
            int is_ext = (s->filematch[j][0] == '.');

            // If it ends with . check if filename ends with that extension
            if ((is_ext && ext && !strcmp(ext, s->filematch[j])) ||
            (!is_ext && strstr(E.filename, s->filematch[j]))) {
                E.syntax = s;   // Set E.syntax to the current editor syntax struct and return
                
                // Loop through each row in the file and call editorUpdateSyntax on it
                for (int filerow = 0; filerow < E.numrows; filerow++) {
                    editorUpdateSyntax(&E.row[filerow]);
                }
                
                return;
            }
            j++;
        }
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


int editorRowRxToCx(erow *row, int rx) {
    int cur_rx = 0;
    int cx;

    for (cx = 0; cx < row->size; cx++) {
        // Stop when cur_rx hits the given rx value
        if (row->chars[cx] == '\t')
            cur_rx += (TEX_TAB_STOP - 1) - (cur_rx % TEX_TAB_STOP);
        cur_rx++;

        if (cur_rx > rx)
            return cx;
    }
    return cx;
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

    editorUpdateSyntax(row);
}


void editorInsertRow(int at, char *s, size_t len) {
    // Validate at before inserting row
    if (at < 0 || at > E.numrows)
        return;

    // Reallocate space for new row
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    // Make room at the specified index for the new row
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));

    for (int j = at + 1; j <= E.numrows; j++)
        E.row[j].idx++;

    E.row[at].idx = at;
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
    E.row[at].highlight = NULL;
    E.row[at].hl_open_comment = 0;
    editorUpdateRow(&E.row[at]);    // Update render & rSize fields with the new row content

    E.numrows++;
    E.dirty++;  // Increment dirty after changing text
}


void editorFreeRow(erow *row) {
    free(row->render);
    free(row->chars);
    free(row->highlight);
}


void editorDelRow(int at) {
    // Return if row is undeletable (after EOF)
    if (at < 0 || at >= E.numrows)
        return;

    editorFreeRow(&E.row[at]);  // Free memory used by the row
    // Overwrite the deleted row struct with the rest of the rows that come after it
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
    
    for (int j = at; j < E.numrows - 1; j++)
        E.row[j].idx--;

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
        editorInsertRow(E.numrows, "", 0); // Append new row to file before inserting char
    
    editorRowInsertChar(&E.row[E.cy], E.cx, c);
    E.cx++; // Increment cursor after inserted char
}


void editorInsertNewLine(){
    // If cursor is at begining of line, insert a new black row before that current line
    if (E.cx == 0) {
        editorInsertRow(E.cy, "", 0);
    } else {
        // Split the current line into two rows
        erow *row = &E.row[E.cy];
        // Create a new row after the current one, with the correct contents
        editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);

        row = &E.row[E.cy]; // Reassign row ptr, since editorInsertRow() calls realloc()
        row->size = E.cx;   // Truncate row, set size to cursor pos
        row->chars[row->size] = '\0';   // Add NULL terminator
        editorUpdateRow(row);   // Update the new row
    }
    // Move cursor to the start of the new line
    E.cy++;
    E.cx = 0;
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

    editorSelectSyntaxHighlight();  // Detect filetype

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
        
        editorInsertRow(E.numrows, line, linelen);   
    }
    free(line);
    fclose(fp);
    E.dirty = 0;    // Reset flag so user isn't alerted after opening file
}


void editorSave() {
    // New file, prompt user for a filename
    if (E.filename == NULL) {
        E.filename = editorPrompt("Save as %s (ESC to cancel)", NULL);
        // User presses ESC, abort save
        if (E.filename == NULL) {
            editorSetStatusMessage("Save aborted!");
            return;
        }
        editorSelectSyntaxHighlight();
    }
    
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
        // Close file after succesfully saving
        close(fp);
    }
    // Unsuccessful save
    free(buf);
    editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}


void editorFindCallback(char *query, int key) {
    static int lastMatch = -1;  // -1 when there is no last match
    static int direction = 1;   // 1 Seach forward, -1 seach backward

    static int saved_hl_line;
    static char *saved_hl = NULL;

    // Save syntax highlights pripr to search
    if (saved_hl) {
        memcpy(E.row[saved_hl_line].highlight, saved_hl, E.row[saved_hl_line].rSize);
        free(saved_hl); // Free incase user cancels search
        saved_hl = NULL;
    }

    // Exit and reset lastMatch & dir for next search
    if (key == '\r' || key == '\x1b') {
        lastMatch = -1;
        direction = 1;
        return;
    } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {   // Jump to next match found
        direction = 1;
    } else if (key == ARROW_LEFT || key == ARROW_UP) {      // Jump to prev match
        direction = -1;
    } else {
        lastMatch = -1;
        direction = 1;
    }
    
    if (lastMatch == -1)
        direction = 1;

    int current = lastMatch;

    // Loop through rows in the file
    for (int i = 0; i < E.numrows; i++) {
        current += direction;

        if (current == -1)
            current = E.numrows - 1;
        else if (current == E.numrows)
            current = 0;
        

        erow *row = &E.row[current];
        // Check if query is found in file, return pointer to the matching substring
        char *match = strstr(row->render, query);

        // String is found in file
        if (match) {
            lastMatch = current;
            E.cy = current; // Jump cursor to next match row
            // Move cursor to the substring on the row
            E.cx = editorRowRxToCx(row, match - row->render);
            E.rowOff = E.numrows;   // Update row offset
            
            // Restore previous syntax highlighting
            saved_hl_line = current;
            saved_hl = malloc(row->rSize);
            memcpy(saved_hl, row->highlight, row->rSize);

            memset(&row->highlight[match - row->render], HL_MATCH, strlen(query));
            break;
        }
    }
}


void editorFind() {
    // Save cursor location prior to search
    int save_cx = E.cx;
    int saved_cy = E.cy;
    int saved_colOff = E.colOff;
    int saved_rowOff = E.rowOff;

    // Query user for string to search for, returns NULL on ESC key press
    char *query = editorPrompt("Search: %s (Use Arrow keys goto next match, and ESC to exit find mode)", editorFindCallback);

    if (query) {
        free(query);
    } else {    // Restore cursor to previous position
        E.cx = save_cx;
        E.cy = saved_cy;
        E.colOff = saved_colOff;
        E.rowOff = saved_rowOff;
    }
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
            
            char *c = &E.row[fileRow].render[E.colOff];
            unsigned char *highlight = &E.row[fileRow].highlight[E.colOff];
            int current_colour = -1;

            for (int j = 0; j < len; j++) {
                // Check if input is a control character
                if (iscntrl(c[j])) {
                    // Make ctrl letters Capital and nonAlpha ?
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    abAppend(ab, "\x1b[7m", 4);
                    abAppend(ab, &sym, 1);
                    abAppend(ab, "\x1b[m", 3);
                    if (current_colour != -1) {
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_colour);
                        abAppend(ab, buf, clen);
                    }
                }
                // Print normal characters without highlighting
                else if (highlight[j] == HL_NORMAL) {
                    if (current_colour != -1) {
                        abAppend(ab, "\x1b[39m", 5);
                        current_colour = -1;
                    }
                    abAppend(ab, &c[j], 1);
                } else {
                    int colour = editorSyntaxToColour(highlight[j]);

                    if (colour != current_colour) {
                        current_colour = colour;
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", colour);
                        abAppend(ab, buf, clen);
                    }
                    abAppend(ab, &c[j], 1);
                }
            }
            abAppend(ab, "\x1b[39m", 5);
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
    
    int rLen = snprintf(rStatus, sizeof(rStatus), "%s | %d/%d",
        E.syntax ? E.syntax->filetype : "no ft", E.cy + 1, E.numrows);

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

char *editorPrompt(char *prompt, void (*callback)(char *, int)) {
    size_t bufsize = 128;
    char *buf = malloc(bufsize);    // Stores user input

    size_t buflen = 0;
    buf[0] = '\0';  // Initialize buf with a NULL terminator

    // Repeatedly set the status message, refresh the screen, and wait for a keypress to handle
    while (1) {
        editorSetStatusMessage(prompt, buf);
        editorRefreshScreen();

        int c = editorReadKey();
        // Allow removing chars in prompt input
        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
            if (buflen != 0)
                buf[--buflen] = '\0';
        } else if (c == '\x1b') {   // User presses Escape key, cancel input prompt
            editorSetStatusMessage("");
            if (callback)
                callback(buf, c);
            free(buf);
            return NULL;
        }
        // User presses Enter key
        if (c == '\r') {
            // Input is not empty
            if (buflen != 0) {
                // Clear status message and return the user's input
                editorSetStatusMessage("");
                if (callback)
                    callback(buf, c);
                return buf;
            }
        } else if(!iscntrl(c) && c < 128) {     // Ensure key isn't a special key defined in editorKey enum
            // buflen has reached the maximum capacity
            if (buflen == bufsize - 1) {
                bufsize *= 2;   // double the bufsize
                buf = realloc(buf, bufsize);    // realloc to account for new size
            }
            buf[buflen++] = c;  // Append char to buf
            buf[buflen] = '\0'; // Add NULL terminator
        }
        if (callback)
            callback(buf, c);
    }
}


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

    switch (c) {
        // ENTER key is pressed, insert newline
        case '\r':
            editorInsertNewLine();
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
        
        // CTRL-f Search Feature
        case CTRL_KEY('f'):
            editorFind();
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

    E.syntax = NULL;    // No current filetype, no highlighting

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
    editorSetStatusMessage("HELP: CTRL-S 'save' | CTRL-F 'find' | Ctrl-Q 'quit'");

    while (1) {
        editorRefreshScreen();
        editorProcessKeyPress();
    }

    return 0;
}
