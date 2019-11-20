#include<stdlib.h>
#include<stdio.h>
#include<ctype.h>
#include<unistd.h>
#include<termios.h>
#include<errno.h>

#define CTRL_KEY(key) (key & 0x1f)

void die(const char *s) 
{
	perror(s); 
	exit(1); 
} 

struct termios term;

void disable_raw_mode() {
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &term) == -1) {
		die ("tcsetattr(&term): failed");
	}
}

void enable_raw_mode()
{
	if (tcgetattr(STDIN_FILENO, &term) == -1) {
		die("tcgetattr(&term): failed");
	}
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

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
		die ("tcsetattr(&raw): failed");
	}
}

void editor_refresh_screen() {
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
}

char editor_read_key() {
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN)
			die("read()");
	}
	return c;
}

void editor_process_keypress(){
	char c = editor_read_key();
	switch (c) {
	case CTRL_KEY('q'):
		write(STDOUT_FILENO, "\x1b[2J", 4);
		write(STDOUT_FILENO, "\x1b[H", 3);
		exit(0);
		break;
	}
}

int main()
{
	enable_raw_mode();
	while(1) {
		editor_refresh_screen();
		editor_process_keypress();
	}
	return 0;
}
