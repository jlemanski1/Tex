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


char editorReadKey() {
    int nRead;
    char c;
    // Read until EOF or ctrl-q, includes error handing
    while ((nRead = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nRead == -1 && errno != EAGAIN)
            die("read");
    }
    return c;
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
    free(ab->b);
}


/*--------------------------------------------------------------------------
                                   OUTPUT
--------------------------------------------------------------------------*/

void editorDrawRows(struct aBuf *ab) {
    int y;  // Terminal height
    // Draw 24 rows of ~
    for(y = 0; y < E.screenRows; y++) {
        // Display welcome message
        if (y == E.screenRows / 3) {
            char welcome[80];
            int welcomeLen = snprintf(welcome, sizeof(welcome), "\tTex Editor\n\t\tversion: %s", TEX_VERSION);
            if (welcomeLen > E.screenCols)
                welcomeLen = E.screenCols;
            
            abAppend(ab, welcome, welcomeLen);
        } else {
            abAppend(ab, "~", 1);   //Append line tildes
        }

        abAppend(ab, "\x1b[K", 3);  // Escaape K sequence at end of each line
        // Make the last line an exception for the carriage return and newline
        if (y < E.screenRows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}


void editorRefreshScreen() {
    struct aBuf ab = ABUF_INIT;

    /*
        \x1b - escape character, decimal: 27
        J    - clear the screen
        2    - specifically entire screen

    abAppend(&ab, "\x1b[2J", 4);
    */

    abAppend(&ab, "\x1b[?25l", 6);  // Show Cursor
    abAppend(&ab, "\x1b[H", 3);  // Reposition cursor to top left

    editorDrawRows(&ab);

    abAppend(&ab, "\x1b[H", 3);
    abAppend(&ab, "\x1b[?25h", 6);  // Hide Cursor

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}


/*--------------------------------------------------------------------------
                                   INPUT
--------------------------------------------------------------------------*/

void editorProcessKeyPress() {
    char c = editorReadKey();

    switch (c)
    {
        // CTRL-Q sucessful exit
        case CTRL_KEY('q'):
            // Clear screen
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(EXIT_SUCCESS);
            break;
    }
}


/*--------------------------------------------------------------------------
                                   INIT
--------------------------------------------------------------------------*/

void initEditor() {
    if (getWindowSize(&E.screenRows, &E.screenCols) == -1)
        die("getWindowSize");
}

int main() {
    enableRawMode();
    initEditor();

    while (1) {
        editorRefreshScreen();
        editorProcessKeyPress();
    }

    return 0;
}
