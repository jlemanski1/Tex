#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>

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


/*--------------------------------------------------------------------------
                                   DATA
--------------------------------------------------------------------------*/
struct editorConfig {
    // Terminal Size
    int screenRows;
    int screenCols;

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
char editorReadKey();


/*
    Gets the size of the terminal window
*/
int getWindowSize(int *rows, int *cols);


/*--------------------------------------------------------------------------
                                   OUTPUT
--------------------------------------------------------------------------*/

/*

*/
void editorRefreshScreen();

/*
    Handles drawing each row of the buffer of text being edited.
    Current fraw a tilde ~ in each row, that row is not part of the file and can't contain text
*/
void editorDrawRows();


/*--------------------------------------------------------------------------
                                   INPUT
--------------------------------------------------------------------------*/

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