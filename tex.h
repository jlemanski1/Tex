#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

/*
    *NOTE*:
        - Because of disabled output processing, when printf-ing a new line, carriage return first
        "\r\n"
*/


/*--------------------------------------------------------------------------
                                DEFINTIONS
--------------------------------------------------------------------------*/

//0001 1111, Mirrors ctrl key in terminal, strips bits 5&6 from key pressed with ctrl, and sends that
#define CTRL_KEY(k) ((k) &0x1f)

// Version number
#define TEX_VERSION "0.06"

// Tab Stop Constant
#define TEX_TAB_STOP 8

// Arrow Key constants
enum editorKey {
    BACKSPACE = 127,
    ARROW_LEFT = 1000,  //out of range of a char so arrows don't conflict with ordinary keys
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

/*--------------------------------------------------------------------------
                                   DATA
--------------------------------------------------------------------------*/

// Stors a row of text in the editor
typedef struct erow {
    int size;
    int rSize;  // Size of the contents of render
    char *chars;
    char *render;
} erow;

struct editorConfig {
    // Cursor Pos
    int cx, cy;
    // When no tabs on current line, rx = cx, when line has tabs, rx will be greater by the num of spaces the tabs take up
    int rx;
    // Terminal Size
    int screenRows;
    int screenCols;
    // Row & Column Offset
    int rowOff;
    int colOff;
    // Editor Rows
    int numrows;
    erow *row;
    // Status Bar
    char *filename; // Filename, for status bar
    char statusmsg[80];
    time_t statusmsgTime;

    struct termios orig_termios;
};

struct editorConfig E;


/*--------------------------------------------------------------------------
                                  TERMINAL
--------------------------------------------------------------------------*/

/*
    Set terminal to raw mode from canonical by disabling the required flags. Terminal won't
    echo user input.
*/
void enableRawMode();


/*
    Return Terminal to it's default, canonical state
*/
void disableRawMode();


/*
    Prints an error message and exits the program
*/
void die(const char *s);

/*
    Waits for a keypress, and returns it
*/
int editorReadKey();


/*
    Gets the cursor position in the terminal window, used for the fallback method of
    getting the window size
*/
int getCursorPosition(int *rows, int *cols);


/*
    Gets the size of the terminal window
*/
int getWindowSize(int *rows, int *cols);


/*--------------------------------------------------------------------------
                            ROW OPERATIONS
--------------------------------------------------------------------------*/

/*
    Converts a chars index to a render index, loops through all chars to the left of cx,
    to figure out how many spaces each tab takes up
*/
int editorRowCxToRx(erow *row, int cx);

/*
    Uses the chars string of an erow to fill in the contents of the render string
*/
void editorUpdateRow(erow *row);

/*

*/
void editorAppendRow(char *s, size_t len);

/*
    Inserts a single character into an erow, at a given position
*/
void editorRowInsertChar(erow *row, int at, int c);


/*--------------------------------------------------------------------------
                            EDITOR OPERATIONS
--------------------------------------------------------------------------*/

/*
    Takes a character and uses editorRowInsertChar() to insert that character into the
    cursor's position.
*/
void editorInsertChar(int c);


/*--------------------------------------------------------------------------
                                 FILE IO
--------------------------------------------------------------------------*/

/*
    Opens and reads a file from disk
*/
void editorOpen(char *filename);


/*--------------------------------------------------------------------------
                               APPEND BUFFER
--------------------------------------------------------------------------*/

/*
    Append buffer consisting of a pointer to the buffer, and a length
*/
struct aBuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0} // Empty buffer

/*
    Append a string to the append buffer
*/
void abAppend(struct aBuf *ab, const char *s, int len);

/*
    Free the append buffer
*/
void abFree(struct aBuf *ab);

/*--------------------------------------------------------------------------
                                   OUTPUT
--------------------------------------------------------------------------*/

/*
    Checks if the cursor has moved outside of the visible window, and adjusts E.rowOff so
    that the cursor is just inside the visible window
*/
void editorScroll();

/*
    Handles drawing each row of the buffer of text being edited.
    Current fraw a tilde ~ in each row, that row is not part of the file and can't contain text
*/
void editorDrawRows(struct aBuf *ab);

/*
    Draws the status bar to show useful info like the filename, the line count, the current
    line number, a modified-since-last-saved marker, and the filetype (FUTURE)
*/
void editorDrawStatusBar(struct aBuf *ab);

/*

*/
void editorRefreshScreen();

/*
    Variadic function(takes any number of args), Prints a stauts message below the status
    bar notifying users of saves, erros, etc.
*/
void editorSetStatusMessage(const char *fmt, ...);

/*
    Updates the status bar message
*/
void editorDrawMessageBar(struct aBuf *ab);


/*--------------------------------------------------------------------------
                                   INPUT
--------------------------------------------------------------------------*/

/*
    Allows the user move the curor using WASD keys
*/
void editorMoveCursor(int key);


/*
    Waits for a keypress, then handles it
*/
void editorProcessKeyPress();


/*--------------------------------------------------------------------------
                                   INIT
--------------------------------------------------------------------------*/

/*
    Initialize all the fields in the E struct
*/
void initEditor();
