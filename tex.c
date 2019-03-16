#include "tex.h"


/*--------------------------------------------------------------------------
                                TERMINAL
--------------------------------------------------------------------------*/

void die(const char *s) {
    perror(s);  // Print descriptive error message
    exit(EXIT_FAILURE); // Exit
}


void disableRawMode() {
    // Error Handling
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); // Set terminal attributes back to default
}


void enableRawMode() {
    // Error Handling
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
        die("tcgetattr");

    tcgetattr(STDIN_FILENO, &orig_termios);         // Read terminal attributes into termios, raw
    atexit(disableRawMode); // Call disableRawMode automatically on program exit

    struct termios raw = orig_termios;  // Make a copy of original terminal attributes

    // Disable ctrl-(s&z&m) signals & other misc flags incase they're on by default
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_iflag &= ~(OPOST);    // Disable all output processing

    // Disable user input echo, turn off canonical mode, disable ctrl-(c&z&v) signals
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    
    // read() timeout
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    // Error Handling
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);   // Apply terminal attributes
}



/*--------------------------------------------------------------------------
                                   INIT
--------------------------------------------------------------------------*/

int main() {
    enableRawMode();

    while (1) {
        char c = '\0';
        // Error Handling
        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
            die("read");

        // Read byte by byte until EOF or user enters q
        read(STDIN_FILENO, &c, 1);

        // Check if char is a control character (is non-printable)
        if (iscntrl(c)) {
            printf("%d\r\n, c");  // Print ASCII value
        } else {
            printf("%d ('%c')\r\n", c, c);    // Print ASCII value & character
        }
        if (c == 'q') break;
    }

    return 0;
}
