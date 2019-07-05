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
#include <sys/ioctl.h>
#include <string.h>

/**
 * @brief Define Keys
 * @details <Ctrl-k>
*/
#define CTRL_KEY(k) ( k & 0x1f )
#define BUF_INIT {NULL, 0}
#define TEx_VERSION "0.0.1"
/**
 * @brief Terminal Struct
 * @details Configuration Data
 */
struct texConfig{
    int dispRows;
    int dispCols;
    struct termios orig_termios;
};
struct texConfig conf; // Global scope
struct memBuf{
    char *b;
    int len;
};

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
int getWindowSize(int *, int *);
void texDispSize();
int getCursorPosition(int *, int *);
void memBufAppend(struct memBuf *, const char *, int );
void memBufFree(struct memBuf *);

/**
 * @brief main
 * @details int main
 */
int main(int argc, char const *argv[])
{

    enableRawMode();
    texDispSize();

    while(1){
        texDispRefresh();
        texProcessKey();
    }

    return 0;
}

/**
 * @brief Initialize
 * @details Invoke size info and adjust display
 */
void texDispSize(){
    if (getWindowSize(&conf.dispRows, &conf.dispCols) == -1)
    {
        terminate("getWindowSize");
    }
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
 * @brief Terminal API
 * @details Call Window Size
 * 
 * @param rs Rows
 * @param cs Columns
 * 
 * @return struct winsize {row, col}
 */
int getWindowSize(int *rs, int *cs){
    struct winsize sWinSize;

    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &sWinSize) == -1 || sWinSize.ws_col == 0)
    {
        if (write(STDIN_FILENO,"\x1b[999C\x1b[999B",12) != 12)
        {
            return -1;
        }
        return getCursorPosition(rs, cs);
    }
    else {
        *cs = sWinSize.ws_col;
        *rs = sWinSize.ws_row;
        return 0;
    }
}

/**
 * @brief Terminal API
 * @details Find Cursor Position
 * 
 * @param rs Rows
 * @param cs Columns
 * 
 * @return valid/invalid: 0/-1
 */
int getCursorPosition(int *rs, int *cs){
    char buffer[32];
    unsigned int i = 0;

    if (write(STDIN_FILENO,"\x1b[6n",4) != 4 )
    {
        return -1;
    }

    while(i < sizeof(buffer) - 1){
        if (read(STDIN_FILENO, &buffer[i], 1) != 1) 
        {
            break;
        }

        if (buffer[i] == 'R')
        {
            break;
        }
        ++i;
    }

    buffer[i] = '\0';

    if (buffer[0] != '\x1b' || buffer[1] != '[')    
    {
        return -1;
    }

    if (sscanf(&buffer[2], "%d;%d", rs, cs) != 2)
    {
        return -1;
    }

    return 0;
}

/**
 * @brief Parser
 * @details Append String
 * 
 * @param memBuf Dynamic-string Struct
 * @param s Input String
 * @param len String Length
 */
void memBufAppend(struct memBuf *abuf, const char *s, int len){
    char *new = realloc(abuf -> b, abuf -> len + len);

    if (new == NULL)
    {
        return;
    }

    memcpy(&new[abuf->len], s, len);
    abuf -> b = new;
    abuf -> len += len;
}

/**
 * @brief Parser
 * @details Append String
 * 
 * @param memBuf Dynamic-string Struct
 */
void memBufFree(struct memBuf *abuf){
    free(abuf->b);
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
    struct memBuf ab = BUF_INIT;

    memBufAppend(&ab,"\x1b[?25l",4);
    memBufAppend(&ab,"\x1b[1;1H",3);

    texVimTildes(&ab);

    memBufAppend(&ab,"\x1b[1;1H",3);
    memBufAppend(&ab,"\x1b[?25l",4);

    write(STDIN_FILENO, ab.b, ab.len);
    memBufFree(&ab);
}

/**
 * @brief Output Handling
 * @details Vimify with tildes at each line
 * @args nRows: Arbitrary no. of tildes
 */
void texVimTildes(struct memBuf *ab){
      int i;
  for (i = 0; i < conf.dispRows; ++i) {
    if (i == conf.dispRows / 3) {
      char wlcMsg[80];

      int wlcLen = snprintf(wlcMsg, sizeof(wlcMsg),
        "TEx Editor –– Version %s", TEx_VERSION);

    if (wlcLen > conf.dispCols) 
    {
        wlcLen = conf.dispCols;
    }

    int padding = (conf.dispCols - wlcLen) / 2;
    if (padding)
    {
        memBufAppend(ab, "~", 1);
        --padding;
    }
    while(--padding) {
        memBufAppend(ab, " ", 1);
    }

    memBufAppend(ab, wlcMsg, wlcLen);

    } else {
      memBufAppend(ab, "~", 1);
    }
    
    memBufAppend(ab, "\x1b[K", 3);
    if (i < conf.dispRows - 1) 
    {
      memBufAppend(ab, "\r\n", 2);
    }
  }
}
