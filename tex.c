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


int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    // On success, ioctl wil lpalce the num of cols and rows into the given winsize struct
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*--------------------------------------------------------------------------
                                   OUTPUT
--------------------------------------------------------------------------*/

void editorDrawRows() {
    int y;  // Terminal height
    // Draw 24 rows of ~
    for(y = 0; y < E.screenRows; y++)
    {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}


void editorRefreshScreen() {
    /*
        \x1b - escape character, decimal: 27
        J    - clear the screen
        2    - specifically entire screen

    */
    write(STDOUT_FILENO, "\x1b[2J", 4); // Write 4 bytes
    write(STDOUT_FILENO, "\x1b[H", 3);  // Reposition cursor to top left

    editorDrawRows();

    write(STDOUT_FILENO, "\x1b[H", 3);
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
