#include "tex.h"

int main() {
    enableRawMode();

    char c;
    
    // Read byte by byte until EOF or user enters q
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
        // Check if char is a control character (is non-printable)
        if (iscntrl(c)) {
            printf("%d\n, c");  // Print ASCII value
        } else {
            printf("%d ('%c')\n", c, c);    // Print ASCII value & character
        }
    }

    return 0;
}


void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); // Set terminal attributes back to default
}


void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);         // Read terminal attributes into termios, raw
    atexit(disableRawMode); // Call disableRawMode automatically on program exit

    struct termios raw = orig_termios;  // Make a copy of original terminal attributes
    raw.c_lflag &= ~(ECHO | ICANON); // Disable Echo-ing of user input, turn off canonical mode
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);   // Apply terminal attributes
}

