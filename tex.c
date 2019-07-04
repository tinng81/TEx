#include <unistd.h>
#include <termios.h>

int main(int argc, char const *argv[])
{

    char c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q');

    return 0;
}
