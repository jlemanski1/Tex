#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

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


// ------------Structs------------
struct termios orig_termios;


// ------------Function Prototypes------------

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
    Waits for a keypress, then handles it
*/
void editorProcessKeyPress();

/*

*/
void editorRefreshScreen();