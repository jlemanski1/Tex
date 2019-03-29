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
#include <fcntl.h>


// Version number
#define TEX_VERSION "1.0.0"

/*--------------------------------------------------------------------------
                                DEFINTIONS
--------------------------------------------------------------------------*/

//0001 1111, Mirrors ctrl key in terminal, strips bits 5&6 from key pressed with ctrl, and sends that
#define CTRL_KEY(k) ((k) &0x1f)

// Tab Stop Constant
#define TEX_TAB_STOP 8

// Quit confirmation, Force user to press CTRL-Q 3 times to quit with unsaved changes
#define TEX_QUIT_AMOUNT 3

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

enum editorHighlight {
    HL_NORMAL = 0,  // Hightlight keywords
    HL_COMMENT,     // Highlight comments
    HL_MLCOMMENT,   // Highlight multiline comments
    HL_KEYWORD,     // Highlight keywords
    HL_TYPE,        // Highlight common types
    HL_STRING,      // Highlight strings
    HL_NUMBER,      // Highlight numbers
    HL_MATCH        // Highlights search results
};

// Highlight bitflags
#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

/*--------------------------------------------------------------------------
                                   DATA
--------------------------------------------------------------------------*/

// Contains highlighting information for a particular filetype
struct editorSyntax {
    char *filetype;     // Name of the full displayed in the status bar
    char **filematch;   // array of strings that contain a pattern to match against
    char **keywords;    // array of strings that contain a language's keywords
    char *singleline_comment_start; // string to hold the start of single line comments, since they differ in langs
    char *multiline_comment_start;  // string to hold the start of a multiline comment identifer
    char *multiline_comment_end;    // string to hold the end of a multiline comment identifer
    int flags;          // bitfield to flag whether to highlight numbers and strings for that filetype
};


// Stors a row of text in the editor
typedef struct erow {
    int idx;    // Index within the file
    int size;
    int rSize;  // Size of the contents of render
    char *chars;
    char *render;
    unsigned char *highlight;
    int hl_open_comment;
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
    int dirty;   // modified since opening flag
    // Status Bar
    char *filename; // Filename, for status bar
    char statusmsg[80];
    time_t statusmsgTime;

    struct editorSyntax *syntax;    // Ptr to current editorSyntax struct

    struct termios orig_termios;
};

struct editorConfig E;


/*--------------------------------------------------------------------------
                                 FILETYPES
--------------------------------------------------------------------------*/

// Filematch extensions for C language
char *C_HL_extensions[] = { ".c", ".h", ".cpp", NULL };

// Keywords for C language
char *C_HL_keywords[] = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "struct", "union", "typedef", "static", "enum", "class", "case",

    "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
    "void|", NULL
};

// Syntax Highlight database
struct editorSyntax HLDB[] = {
    {
        "c",
        C_HL_extensions,
        C_HL_keywords,
        "//",
        "/*",
        "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

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
                          SYNTAX HIGHLIGHTING
--------------------------------------------------------------------------*/

/*
    Takes in a character and returns true if it's considered a separator character
*/
int isSeparator(int c);


/*

*/
void editorUpdateSyntax(erow *row);


/*
    Maps values in highlight to the ANSI colour code to draw them with
*/
int editorSyntaxToColour(int highlight);


/*
    Mathces the current filename to one of the filematch fields in the HLDB. If one matches,
    it'll set E.syntax to that filetype
*/
void editorSelectSyntaxHighlight();


/*--------------------------------------------------------------------------
                            ROW OPERATIONS
--------------------------------------------------------------------------*/

/*
    Converts a chars index to a render index, loops through all chars to the left of cx,
    to figure out how many spaces each tab takes up
*/
int editorRowCxToRx(erow *row, int cx);


/*
    Coverts a render index to a char index, essentially does the opposite of editorRowCxToRx()
*/
int editorRowRxToCx(erow *row, int rx);


/*
    Uses the chars string of an erow to fill in the contents of the render string
*/
void editorUpdateRow(erow *row);


/*

*/
void editorInsertRow(int at, char *s, size_t len);


/*
    Frees an erow, used when deleting an erow
*/
void editorFreeRow(erow *row);


/*
    Deletes an erow
*/
void editorDelRow(int at);


/*
    Inserts a single character into an erow, at a given position
*/
void editorRowInsertChar(erow *row, int at, int c);


/*
    Appends a string to the end of a row, used primarily the user backspaces at the
    start of a line, appending the row to the previous one.
*/
void editorRowAppendString(erow *row, char *s, size_t len);


/*
    Deletes a char in an erow
*/
void editorRowDelChar(erow *row, int at);


/*--------------------------------------------------------------------------
                            EDITOR OPERATIONS
--------------------------------------------------------------------------*/

/*
    Takes a character and uses editorRowInsertChar() to insert that character into the
    cursor's position.
*/
void editorInsertChar(int c);


/*
    Handles Enter keypresses by inserting a new line
*/
void editorInsertNewLine();


/*
    Uses editorRowDelChar() to delete the character to the left of the cursor
*/
void editorDelChar();




/*--------------------------------------------------------------------------
                                 FILE IO
--------------------------------------------------------------------------*/

/*
    Converts array of erow structs into a single string ready to be written to a file.
    Returns: buf - char *buf - Caller expected to free
*/
char *editorRowsToString(int *buflen);


/*
    Opens and reads a file from disk
*/
void editorOpen(char *filename);


/*
    Writes the string returned by editorRowsToString() to disk
*/
void editorSave();


/*
    When the user types a search query and presses Enter, loops through all the fows in the file,
    and if a row contains their query string, the cursor is moved to the match
*/
void editorFindCallback(char *query, int key);


/*
    Get query from the user  from and then return the cursor to its previous position before the search
*/
void editorFind();

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
    Displays a prompt in the status bar, and returns the user input a line of text after the prompt
*/
char *editorPrompt(char *prompt, void (*callback)(char *, int));


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
