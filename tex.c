#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

struct termios orig_termios;

void enableRawMode();
void disableRawMode();
void terminate(const char *);

int main(int argc, char const *argv[])
{

    enableRawMode();

    while(1){
        char c = "\0";
        // read(STDIN_FILENO, &c, 1) == 1;
        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) // Cygwin compatibility
        {
            terminate("read");
        }        

        if (iscntrl(c))
        {
            printf("%d\r\n", c);
        }
        else {
            printf("%d ('%c')\r\n", c, c);
        }
        if (c == 'q')
        {
            break;
        }
    }

    return 0;
}

void enableRawMode() {
    // tcgetattr(STDIN_FILENO, &orig_termios);
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
    {
        terminate("tcgetattr");
    }

    atexit(disableRawMode);

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);  
    raw.c_oflag &= ~(OPOST); 
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cflag |= (CS8);

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 10; // 10 * 100 msec = 10s timeout

    // tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    {
        terminate("tcsetattr");
    }
}

void disableRawMode() {
  // tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
  {
      terminate("tcsetattr");
  }

}

void terminate(const char *s){
    perror(s);
    exit(1);
}
