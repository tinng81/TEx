/**
 * @brief File Headers
 * @details Terminal, I/O, Error
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

/**
 * @brief Define Keys
 * @details <Ctrl-k>
*/
#define CTRL_KEY(k) ( k & 0x1f )

/**
 * @brief Terminal Struct
 * @details Configuration Data
 */
struct texConfig{
    struct termios orig_termios;
};
struct texConfig conf; // Global scope

/**
 * @brief Function Prototypes
 */
void enableRawMode();
void disableRawMode();
void terminate(const char *);
char texReadKey();
void texProcessKey();
void texDispRefresh();
void texVimTildes();

/**
 * @brief main
 * @details int main
 */
int main(int argc, char const *argv[])
{

    enableRawMode();

    while(1){
        texDispRefresh();
        texProcessKey();
    }

    return 0;
}

/**
 * @brief Terminal API
 * @details Enable Raw Input
 */
void enableRawMode() {
    // tcgetattr(STDIN_FILENO, &orig_termios);
    if (tcgetattr(STDIN_FILENO, &conf.orig_termios) == -1)
    {
        terminate("tcgetattr");
    }

    atexit(disableRawMode);

    struct termios raw = conf.orig_termios;
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);  
    raw.c_oflag &= ~(OPOST); 
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cflag |= (CS8);

    // raw.c_cc[VMIN] = 0;
    // raw.c_cc[VTIME] = 10; // 10 * 100 msec = 10s timeout

    // tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    {
        terminate("tcsetattr");
    }
}

/**
 * @brief Terminal API
 * @details Disable Raw Input
 */
void disableRawMode() {
  // tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &conf.orig_termios) == -1)
  {
      terminate("tcsetattr");
  }
}

/**
 * @brief Terminal API
 * @details Error Handler
 * 
 * @param s Error Message
 */
void terminate(const char *s){
    write(STDIN_FILENO,"\x1b[2J",4);
    write(STDIN_FILENO,"\x1b[1;1H",3);

    perror(s);
    exit(1);
}

/**
 * @brief Terminal API
 * @details Read Input
 * @return Byte char
 */ 
char texReadKey(){
    int nChar;
    char c;
    while(nChar = read(STDIN_FILENO, &c, 1) != 1 ){
        if (nChar == -1 && errno != EAGAIN) // Again, Cygwin compatibility
        {
            terminate("read");
        }
    }
    return c;
}

/**
 * @brief Input Handling
 * @details Comprise keystrokes
 */
void texProcessKey(){
    char c = texReadKey();

    switch(c){
        case CTRL_KEY('q'):
            write(STDIN_FILENO,"\x1b[2J",4);
            write(STDIN_FILENO,"\x1b[1;1H",3);

            exit(0);
            break;
        default:
            printf("%d [%c]\r\n", c, c);
            break;
    }
}

/**
 * @brief Output Handling
 * @details Refresh  Display
 * @args Escape <\x1b> + <[>: <esc> sequence
 * @args Erase-in-Display <2J>: clear all mode (2)
 * @args Cursor Position <1;1H>: Row 1 ; Col 1
 */
void texDispRefresh(){
    write(STDIN_FILENO,"\x1b[2J",4);
    write(STDIN_FILENO,"\x1b[1;1H",3);

    texVimTildes();

    write(STDIN_FILENO,"\x1b[1;1H",3);
}

/**
 * @brief Output Handling
 * @details Vimify with tildes at each line
 * @args nRows: Arbitrary no. of tildes
 */
void texVimTildes(){
    int i, nRows = 24;
    for (i = 0; i < nRows; ++i)
    {
        write(STDIN_FILENO,"~\r\n",3);
    }
}
