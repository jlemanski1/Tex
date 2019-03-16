#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

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