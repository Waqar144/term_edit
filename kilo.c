#include<stdlib.h>
#include<stdio.h>
#include<ctype.h>
#include<unistd.h>
#include<termios.h>

struct termios term;

void disable_raw_mode() {
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);
}

void enable_raw_mode()
{
	tcgetattr(STDIN_FILENO, &term);
	atexit(disable_raw_mode);

	struct termios raw = term;

	//turn off output processing
	raw.c_oflag &= ~(OPOST);
	//turn off Ctrl+S/Ctrl+Q
	raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP | ICRNL | IXON);
	//turn off canonical mode
	//turn off echo
	//turn off SIGINT and SIGTSTP
	raw.c_lflag &= ~(ECHO | IEXTEN | ICANON | ISIG);
	raw.c_cflag |= CS8;
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}


int main()
{
	enable_raw_mode();
	while(1) {
		char c = '\0';
		read(STDIN_FILENO, &c, 1);
		if (iscntrl(c)) {
			printf("%d\r\n", c);
		} else {
			printf("%d - %c\r\n", c, c);
		}

		if (c == 'q') break;
	}
	return 0;
}
