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
#include <sys/types.h>
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>

/**
 * @brief Define Buffer
 * @details For input Keystroke
*/
#define CTRL_KEY(k) ( k & 0x1f )
#define BUF_INIT {NULL, 0}

/**
 * @brief Define relevant params
 * @details VER, TTS, Quit mode
*/
#define TEx_VERSION "1.0.2"
#define TEx_VERSION_LAYOUT 3
#define TABS_TO_SPACES 8
#define FORCE_QUIT 2

/**
 * @brief Define Compiler FLAG
 * @details Cross-platform compatibility
*/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

/**
 * @brief Terminal Struct
 * @details Line Data Structure
 */
typedef struct erow {
    int size;
    int ren_sz;
    char *chars;
    char *render;
} erow;

/**
 * @brief Terminal Struct
 * @details Configuration
*/
struct texConfig {
    int dispRows;
    int dispCols;
    int cur_x;
    int cur_y;
    int ren_x;
    int n_rows;
    int off_row;
    int off_col;
    int mod;
    char *file_name;
    char *stt_msg[80];
    time_t msg_time;
    erow *row;
    struct termios orig_termios;
};
struct texConfig conf; // Global scope

/**
 * @brief Define memory struct
 * @details Memory Buffer for input
*/
struct memBuf {
    char *b;
    int len;
};

/**
 * @brief Control Key Enumerator
 * @details Mgmt. & Navigation keystrokes
*/
enum navKey {
    BKSP_KEY = 127,
    ARR_UP = 1000, // arbitrary, out of ASCII & CTRL range
    ARR_DOWN, // incremental
    ARR_LEFT,
    ARR_RIGHT,
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY,
    END_KEY,
    DEL_KEY,
};

/**
 * @brief Function Prototypes
 * @details TEx general API
 */
void texDispInit();
void texRawEnable();
void texRawDisable();
void texTerminate(const char *);
int texReadKey();
int texGetWindowsSize(int *, int *);
int texGetCursorPosition(int *, int *);
void texProcessKey();
void texNavCursor(int );
void texDispRefresh();
void texDrawLine();
void texDrawStatusBar(struct memBuf *);
void texDrawStatusMsg(struct memBuf *);
char *texUserPrompt(char *);
void texSetStatusMessage(const char *, ...);

/**
 * @brief Function Prototypes
 * @details TEx - editor API
*/
void editorOpen(char *);
void editorSave();
void editorAppendChar(int , char *, size_t );
void editorAppendString(erow *, char *, size_t );
void editorInsertNewLine();
void editorScroll();
void editorUpdateRow(erow *);
void editorInputChar(int );
void editorRemoveChar();
void editorRemoveRow(int );

/**
 * @brief Function Prototypes
 * @details TEx - Memory control
*/
void memBufAppend(struct memBuf *, const char *, int );
void memBufFree(struct memBuf *);
void memFreeRow(erow *);

/**
 * @brief Function Prototypes
 * @details TEx - Miscellaneous Utilities
*/
int utilCur2Ren(erow *, int );
void utilCharInsert(erow *, int , int );
void utilCharDel(erow *, int );
char *utilRow2Str(int *);


/**
 * @brief main
 * @details int main
 */
int main(int argc, char const *argv[]){

    texRawEnable();
    texDispInit();
    if (argc >= 2)
    {
        editorOpen( (char *) argv[1]);
    }

    texSetStatusMessage("HELP: Ctrl-S to save | Ctrl-Q to quit");

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
void texDispInit(){
    conf.cur_x = 0;
    conf.cur_y = 0;
    conf.ren_x = 0;
    conf.n_rows = 0;
    conf.row = NULL;
    conf.file_name = NULL;
    conf.off_row = 0;
    conf.off_col = 0;
    conf.stt_msg[0] = '\0';
    conf.msg_time = 0;
    conf.mod = 0;

    if (texGetWindowsSize(&conf.dispRows, &conf.dispCols) == -1)
    {
        texTerminate("texGetWindowsSize");
    }

    conf.dispRows -= 2;
}

/**
 * @brief Terminal API
 * @details Enable Raw Input
 */
void texRawEnable() {
    // tcgetattr(STDIN_FILENO, &orig_termios);
    if (tcgetattr(STDIN_FILENO, &conf.orig_termios) == -1)
    {
        texTerminate("tcgetattr");
    }

    atexit(texRawDisable);

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
        texTerminate("tcsetattr");
    }
}

/**
 * @brief Terminal API
 * @details Disable Raw Input
 */
void texRawDisable() {
  // tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &conf.orig_termios) == -1)
  {
      texTerminate("tcsetattr");
  }
}

/**
 * @brief Terminal API
 * @details Error Handler
 * 
 * @param s Error Message
 */
void texTerminate(const char *s){
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
int texReadKey(){
    int nChar;
    char c;
    while( (nChar = read(STDIN_FILENO, &c, 1) ) != 1 ){
        if (nChar == -1 && errno != EAGAIN) // Again, Cygwin compatibility
        {//
            texTerminate("read");
        }
    }

    if (c == '\x1b')
    {
        char kNav[3];

        if (read(STDIN_FILENO, &kNav[0], 1) != 1)
        {
            return '\x1b';
        }
        if (read(STDIN_FILENO, &kNav[1], 1) != 1)
        {
            return '\x1b';
        }

        if (kNav[0] == '[')
        {

            if (kNav[1] >= '0' && kNav[1] <= '9')
            {
                if (read(STDIN_FILENO, &kNav[2], 1) != 1)
                {
                    return '\x1b';
                }

                if (kNav[2] == '~')
                {
                    switch(kNav[1]){
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;

                        /* 
                            NOTE: OSes, Terminal emulators compatibility
                        */
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            }
            else {
                switch(kNav[1]){
                    case 'A':
                        return ARR_UP;
                    case 'B':
                        return ARR_DOWN;
                    case 'D':
                        return ARR_LEFT;
                    case 'C':
                        return ARR_RIGHT;
                    case 'H':
                        return HOME_KEY;
                    case 'F':
                        return END_KEY;
                }
            }
        }
        else if (kNav[0] == 'O') {
            switch (kNav[1]){
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }

        return '\x1b';
    }
    else {
        return c;
    }
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
int texGetWindowsSize(int *rs, int *cs){
    struct winsize sWinSize;

    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &sWinSize) == -1 || sWinSize.ws_col == 0)
    {
        if (write(STDIN_FILENO,"\x1b[999C\x1b[999B",12) != 12)
        {
            return -1;
        }
        return texGetCursorPosition(rs, cs);
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
int texGetCursorPosition(int *rs, int *cs){
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
 * @brief Input Handling
 * @details Comprise keystrokes
 */
void texProcessKey(){
    static int confirm_exit = FORCE_QUIT;
    int c = texReadKey();

    switch(c){
        case CTRL_KEY('q'):
            if (conf.mod && confirm_exit > 0)
            {
                texSetStatusMessage("WARNING ! File has unsaved changes. Press Ctrl-Q again (%d) to confirm quit", confirm_exit);
                --confirm_exit;
                return;
            }

            write(STDIN_FILENO,"\x1b[2J",4);
            write(STDIN_FILENO,"\x1b[1;1H",3);

            exit(0);
            break;

        case CTRL_KEY('s'):
            editorSave();
            break;

        case ARR_UP:
        case ARR_DOWN:
        case ARR_LEFT:
        case ARR_RIGHT:
            texNavCursor(c);
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            {
                if (c == PAGE_UP)
                {
                    conf.cur_y = conf.off_row;
                }
                else if (c == PAGE_DOWN) {
                    conf.cur_y = conf.off_row + conf.dispRows - 1;
                    if (conf.cur_y > conf.n_rows)
                    {
                        conf.cur_y = conf.n_rows;
                    }
                }

                int times = conf.dispRows;
                while (--times){
                    texNavCursor(c == PAGE_UP ? ARR_UP : ARR_DOWN);
                }
            }
            break;

        case HOME_KEY:
            conf.cur_x = 0;
            break;

        case END_KEY:
            if (conf.cur_y < conf.n_rows)
            {
                conf.cur_x = conf.row[conf.cur_y].size;                
            }
            break;

        case BKSP_KEY:
        case CTRL_KEY('h'):
        case DEL_KEY:
            if (c == DEL_KEY)
            {
                texNavCursor(ARR_RIGHT);
            }
            editorRemoveChar();
            break;

        case CTRL_KEY('l'):
        case '\x1b':
            break;

        case '\r':
            editorInsertNewLine();
            break;

        default:
            editorInputChar(c);
            break;
    }
    confirm_exit = FORCE_QUIT; // re-initialize
}

/**
 * @brief Input Handling
 * @details Navigate Cursor position
 * 
 * @param key Input keystroke (arrow)
 */
void texNavCursor(int key){
    erow *row = (conf.cur_y >= conf.n_rows) ? NULL : &conf.row[conf.cur_y];

    switch(key){
        case ARR_UP:
            if (conf.cur_y != 0)
            {
                --conf.cur_y;
            }
            break;

        case ARR_DOWN:
            if (conf.cur_y < conf.n_rows)
            {
                ++conf.cur_y;                
            }
            break;

        case ARR_LEFT:
            if (conf.cur_x != 0)
            {
                --conf.cur_x;
            }         
            else if (conf.cur_y > 0) {
                --conf.cur_y;
                conf.cur_x = conf.row[conf.cur_y].size;
            }   
            break;

        case ARR_RIGHT:
            if (row && conf.cur_x < row -> size)
            {
                conf.cur_x++;
            }
            else if (row && conf.cur_x == row->size) {
                ++conf.cur_y;
                conf.cur_x = 0;
            }
            break;

    }

    row = (conf.cur_y >= conf.n_rows) ? NULL : &conf.row[conf.cur_y];
    int row_len = row ? row->size : 0;
    if (conf.cur_x > row_len)
    {
        conf.cur_x = row_len;
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
    editorScroll();

    struct memBuf ab = BUF_INIT;

    memBufAppend(&ab,"\x1b[?25l",6);
    // FIXME: [1;1H format error bug for all previous versions
    memBufAppend(&ab,"\x1b[H",3);

    texDrawLine(&ab);
    texDrawStatusBar(&ab);
    texDrawStatusMsg(&ab);

    char cur_buf[64];
    snprintf(cur_buf, sizeof(cur_buf), "\x1b[%d;%dH", (conf.cur_y - conf.off_row) + 1,
                                            (conf.ren_x - conf.off_col) + 1);
    memBufAppend(&ab, cur_buf, strlen(cur_buf));

    memBufAppend(&ab,"\x1b[?25h",6);

    write(STDIN_FILENO, ab.b, ab.len);
    memBufFree(&ab);
}

/**
 * @brief Output Handling
 * @details Vimify with tildes at each line
 * @args nRows: Arbitrary no. of tildes
 */
void texDrawLine(struct memBuf *ab){
  int i;
  for (i = 0; i < conf.dispRows; ++i) {
    int fp_row = i + conf.off_row;

    if (fp_row >= conf.n_rows)
    {
        if (conf.n_rows == 0 && i == conf.dispRows / TEx_VERSION_LAYOUT) {
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
    }
    else {
        int len = conf.row[fp_row].ren_sz - conf.off_col;

        if (len < 0)
        {
            len = 0;
        }

        if (len > conf.dispCols)
        {
            len = conf.dispCols;
        }
        memBufAppend(ab, &conf.row[fp_row].render[conf.off_col], len);
    }

    memBufAppend(ab, "\x1b[K", 3);
    memBufAppend(ab, "\r\n", 2);
  }
}

/**
 * @brief Draw Status Bar
 * @details STT at end of window
 * 
 * @param memBuf memory buffer for Status Bar
 */
void texDrawStatusBar(struct memBuf *ab) {
    memBufAppend(ab, "\x1b[7m", 4);
    char stt[80], cur_stt[80];

    int len = snprintf(stt, sizeof(stt), "%.20s - %d lines %s",
    conf.file_name ? conf.file_name : "[No Name]", conf.n_rows,
    conf.mod ? "(modified)" : "");

    int cur_len = snprintf(cur_stt, sizeof(cur_stt), "%d/%d", 
       conf.cur_y + 1, conf.n_rows );

    if (len > conf.dispCols)
    {
        len = conf.dispCols;
    }

    memBufAppend(ab, stt, len);

    while (len < conf.dispCols) {
        if (conf.dispCols - len == cur_len)
        {
            memBufAppend(ab, cur_stt, cur_len);
            break;
        }
        else {
            memBufAppend(ab, " ", 1);
            ++len;
        }
    }
    memBufAppend(ab, "\x1b[m", 3);
    memBufAppend(ab, "\r\n", 2);
}

/**
 * @brief Draw Message Bar
 * @details STT msg below STT bar
 * 
 * @param memBuf memory buffer for Status Message
 */
void texDrawStatusMsg(struct memBuf *ab) {
    memBufAppend(ab, "\x1b[K", 3);
    int msg_len = strlen(conf.stt_msg);

    if (msg_len > conf.dispCols)
    {
        msg_len = conf.dispCols;
    }

    if (msg_len && (time(NULL) - conf.msg_time) < 5)
    {
        memBufAppend(ab, conf.stt_msg, msg_len);
    }
}

/**
 * @brief Draw User-input Prompt
 * @details Prompt in STT bar
 * 
 * @param prompt content
 * @return buffer
 */
char *texUserPrompt(char *prompt) {
    size_t buf_sz = 128;
    char *buffer = malloc(buf_sz);

    size_t buf_len = 0;
    buffer[0] = '\0';

    while(1) {
        texSetStatusMessage(prompt, buffer);
        texDispRefresh();

        int c = texReadKey();

        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BKSP_KEY)
        {
            if (buf_len != 0)
            {
                buffer[--buf_len] = '\0';
            }
        }
        else if (c == '\x1b')
        {
            texSetStatusMessage("");
            free(buffer);
            return NULL;
        }
        else if (c == '\r') {
            if (buf_len != 0)
            {
                texSetStatusMessage("");
                return buffer;
            }
        }
        else if (!iscntrl(c) && c < 128) {
            if (buf_len == buf_sz - 1)
            {
                buf_sz *= 2;
                buffer = realloc(buffer, buf_sz);
            }

            buffer[buf_len++] = c;
            buffer[buf_len] = '\0';
        }
    }
}

/**
 * @brief Set Status Message
 * @details stt_msg below Status Bar
 * 
 * @param fmt variadic func
 */
void texSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(conf.stt_msg, sizeof(conf.stt_msg), fmt, ap);
    va_end(ap);
    conf.msg_time = time(NULL);
}

/**
 * @brief High-level Editor handling
 * @details Allocate memory, dynamic string for input
 */
void editorOpen(char *file_name){
    free(conf.file_name);
    conf.file_name = strdup(file_name);

    FILE *fp = fopen(file_name, "r");
    if (!fp)
    {
        texTerminate("fopen");
    }

    char *line = NULL;
    size_t line_cap = 0;
    ssize_t line_len;

    while ((line_len = getline(&line, &line_cap, fp)) != -1) {
        while (line_len > 0 && (line[line_len - 1] == '\n' ||
                               line[line_len - 1] == '\r'))
          line_len--;
        editorAppendChar(conf.n_rows, line, line_len);
    }

    free(line);
    fclose(fp);
    conf.mod = 0;
}

/**
 * @brief File I/O Handling
 * @details Save any changes
 */
void editorSave() {
    if (conf.file_name == NULL)
    {
        conf.file_name = texUserPrompt("Save as: %s (<ESC> to cancel)");

        if (conf.file_name == NULL)
        {
            texSetStatusMessage("Save cancelled");
            return;
        }
    }

    int len;
    char *buffer = utilRow2Str(&len);

    int fp = open(conf.file_name, O_RDWR | O_CREAT, 0644);

    if (fp != -1)
    {
        if (ftruncate(fp, len) != -1)
        {
            if (write(fp, buffer, len) == len)
            {
                close(fp);
                free(buffer);
                conf.mod = 0;
                texSetStatusMessage("%d bytes written to file", len);
                return;
            }
        }
        close(fp);
    }
    free(buffer);
    texSetStatusMessage("Cannot save ! I/O Error: %s", strerror(errno));
}

/**
 * @brief High-level Editor handling
 * @details Read each line from file input
 * 
 * @param s Line from file
 * @param len Line Length
 */
void editorAppendChar(int at, char *s, size_t len){
    if (at < 0 || at > conf.n_rows)
    {
        return;
    }

    conf.row = realloc(conf.row, sizeof(erow) * (conf.n_rows + 1) );
    memmove(&conf.row[at + 1], &conf.row[at], sizeof(erow) * (conf.n_rows - at) );

    conf.row[at].size = len;
    conf.row[at].chars = malloc (len + 1);
    memcpy(conf.row[at].chars, s, len);
    conf.row[at].chars[len] = '\0';
    
    conf.row[at].ren_sz = 0;
    conf.row[at].render = NULL;
    editorUpdateRow(&conf.row[at]);

    conf.n_rows++;
    conf.mod++;
}

/**
 * @brief High-level Editor Handling
 * @details Append entire row of char, i.e. String
 * 
 * @param erow Current Row Struct
 * @param s Append String Pointer
 * @param len String Length
 */
void editorAppendString(erow *row, char *s, size_t len) {
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    conf.mod++;
}

/**
 * @brief High-level Editor Handling
 * @details Insert a new line or break into 2 lines
 */
void editorInsertNewLine() {
    if (conf.cur_x == 0)
    {
        editorAppendChar(conf.cur_y, "", 0);
    }
    else {
        erow *row = &conf.row[conf.cur_y];
        editorAppendChar(conf.cur_y + 1, &row->chars[conf.cur_x], row->size - conf.cur_x);
        row = &conf.row[conf.cur_y];
        row->size = conf.cur_x;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }
    conf.cur_y++;
    conf.cur_x = 0;
}

/**
 * @brief High-level Editor handling
 * @details Scrolling feature
 */
void editorScroll(){
    conf.ren_x = 0;

    if (conf.cur_y < conf.n_rows)
    {
        conf.ren_x = utilCur2Ren(&conf.row[conf.cur_y], conf.cur_x);
    }


    if (conf.cur_y < conf.off_row)
    {
        conf.off_row = conf.cur_y;
    }

    if (conf.cur_y >= conf.off_row + conf.dispRows)
    {
        conf.off_row = conf.cur_y - conf.dispRows + 1;
    }

    if (conf.ren_x < conf.off_col)
    {
        conf.off_col = conf.ren_x;
    }

    if (conf.ren_x >= conf.off_col + conf.dispCols)
    {
        conf.off_col = conf.ren_x - conf.dispCols + 1;
    }
}

/**
 * @brief High-level Editor handling
 * @details Rendering each char, watch for tab
 * 
 * @param row File Input line
 */
void editorUpdateRow(erow *row) {
    int tabs = 0;
    int i;

    for (i = 0; i < row->size; ++i)
    {
        if (row->chars[i] == '\t')
        {
            ++tabs;
        }
    }

    free(row ->render);
    row->render = malloc(row->size + tabs * (TABS_TO_SPACES - 1) + 1);

    int idx = 0;
    for (i = 0; i < row->size; ++i)
    {

        if (row->chars[i] == '\t')
        {
            row->render[idx++] = ' ';
            while (idx % TABS_TO_SPACES != 0) {
                row->render[idx++] = ' ';
            }
        }
        else {
            row->render[idx++] = row->chars[i];
        }
    }
    row->render[idx] = '\0';
    row->ren_sz = idx;
}

/**
 * @brief User Input Handling
 * @details Invoke util to insert Char to row
 * 
 * @param c Input Character
 */
void editorInputChar(int c) {
    if (conf.cur_y == conf.n_rows)
    {
        editorAppendChar(conf.n_rows,"", 0);
    }
    utilCharInsert(&conf.row[conf.cur_y], conf.cur_x, c);
    ++conf.cur_x;
}

/**
 * @brief User Input Handling
 * @details Invoke util to delete Char from row
 */
void editorRemoveChar() {
    if (conf.cur_y == conf.n_rows)
    {
        return;
    }

    if (conf.cur_x == 0 && conf.cur_y == 0)
    {
        return;
    }

    erow *row = &conf.row[conf.cur_y];

    if (conf.cur_x > 0)
    {
        utilCharDel(row, conf.cur_x - 1);
        --conf.cur_x;
    }
    else {
        conf.cur_x = conf.row[conf.cur_y - 1].size;
        editorAppendString(&conf.row[conf.cur_y - 1], row->chars, row->size);
        editorRemoveRow(conf.cur_y);
        --conf.cur_y;
    }
}

/**
 * @brief User Input Handling
 * @details Invoke util to delete entire Row
 * 
 * @param at Current Row
 */
void editorRemoveRow(int at) {
    if (at < 0 || at >= conf.n_rows)
    {
        return;
    }

    memFreeRow(&conf.row[at]);
    memmove(&conf.row[at], &conf.row[at + 1], sizeof(erow) * (conf.n_rows - at - 1) );
    --conf.n_rows;
    conf.mod++;
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
 * @brief Row control
 * @details Free Memory at Delete
 * 
 * @param row Current Row
 */
void memFreeRow(erow *row) {
    free(row->render);
    free(row->chars);
}

/**
 * @brief Utility for Screen Rending
 * @details Cursor to Render char count
 * 
 * @param row Input Row
 * @param cur_x Cursor Column
 * 
 * @return Render equivalent Column
 */
int utilCur2Ren(erow *row, int cur_x) {
    int ren_x = 0;
    int i;
    for (i = 0; i < cur_x; ++i)
    {
        if (row->chars[i] == '\t')
        {
            ren_x += (TABS_TO_SPACES - 1) - (ren_x % TABS_TO_SPACES);
        }
        ++ren_x;
    }
    return ren_x;
}

/**
 * @brief Utility for Row Rending
 * @details Insert input char
 * 
 * @param row Current Row
 * @param at Cursor Position
 * @param c ASCII coded char
 */ 
void utilCharInsert(erow *row, int at, int c) {
    if (at < 0 || at > row->size)
    {
        at = row->size;
    }
    
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    ++row->size;
    row->chars[at] = c;
    editorUpdateRow(row);
    conf.mod++;
}

/**
 * @brief Utility for Row Rending
 * @details Remove char at buffer
 * 
 * @param row Current Row
 * @param at Cursor Position
 */
void utilCharDel(erow *row, int at) {
    if (at < 0 || at > row->size)
    {
        return;
    }

    memmove(&row->chars[at], &row->chars[at + 1], row->size -at);
    row->size--;
    editorUpdateRow(row);
    conf.mod++;
}

/**
 * @brief Utility for File I/O
 * @details Convert before write to file
 * 
 * @param buf_len Row Buffer length
 * @return Pointer to converted String
 */
char *utilRow2Str(int *buf_len) {
    int tot_len = 0;
    int i;

    for (i = 0; i < conf.n_rows; ++i)
    {
        tot_len += conf.row[i].size + 1;
    }

    *buf_len = tot_len;

    char *buffer = malloc(tot_len);
    char *buf_ptr = buffer;

    for (i = 0; i < conf.n_rows; ++i)
    {
        memcpy(buf_ptr, conf.row[i].chars, conf.row[i].size);
        buf_ptr += conf.row[i].size;
        *buf_ptr = '\n';
        ++buf_ptr;
    }
    return buffer;
}
