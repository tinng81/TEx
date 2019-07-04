#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;

void enableRawMode();
void disableRawMode();

int main(int argc, char const *argv[])
{

    enableRawMode();

    while(1){
        char c = "\0";
        read(STDIN_FILENO, &c, 1) == 1;

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
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disableRawMode);

  struct termios raw = orig_termios;
  raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);  
  raw.c_oflag &= ~(OPOST); 
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  raw.c_cflag |= (CS8);

  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 10; // 10 * 100 msec = 10s timeout
  
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}
