#include<stdlib.h>
#include<stdio.h>
#include<ctype.h>
#include<unistd.h>
#include<termios.h>
#include<errno.h>
#include<sys/ioctl.h>
#include<string.h>

#define CTRL_KEY(key) (key & 0x1f)
struct editor_config {
	int rows;
	int cols;
	struct termios term;
};

/**
 * Buffer
 */
typedef struct buffer {
	char *buf;
	int len;
}buffer;
#define BUF_INIT {NULL, 0}

void buffer_append(buffer *s, const char *b, int len) {
	char *new = realloc(s->buf, s->len + len);

	if (new == NULL) return;

	memcpy(&new[s->len], b, len);
	s->buf = new;
	s->len += len;
}

void buffer_free(buffer *s) {
	free(s->buf);
}
/* String end */

struct editor_config e;

void die(const char *s) 
{
	perror(s); 
	exit(1); 
} 


void disable_raw_mode() {
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &e.term) == -1) {
		die ("tcsetattr(&term): failed");
	}
}

void enable_raw_mode()
{
	if (tcgetattr(STDIN_FILENO, &e.term) == -1) {
		die("tcgetattr(&term): failed");
	}
	atexit(disable_raw_mode);

	struct termios raw = e.term;

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




void editor_draw_tildes(buffer *b) {
	for (int i = 0; i<e.rows; i++) {
		buffer_append(b, "~", 1);
		if (i < e.rows -1)
			buffer_append(b, "\r\n", 2);
	}
}

void editor_refresh_screen() {
	buffer b = BUF_INIT;
	buffer_append(&b, "\x1b[2J", 4);
	buffer_append(&b, "\x1b[H", 3);

	editor_draw_tildes(&b);

	buffer_append(&b, "\x1b[1;2f", 6);

	write(STDOUT_FILENO, b.buf, b.len);
	buffer_free(&b);
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

int get_cursor_position(int *rows, int *cols) {
	char buf[32];
	unsigned int i = 0;
	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
	
	size_t bufsize = sizeof(buf) - 1;
	while (i < bufsize) {
		if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
		if(buf[i] == 'R') break;
		i++;
	}
	buf[i] = '\0';

	if (buf[0] != '\x1b' && buf[1] != '[') return -1;
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

	return 0;
}

int get_window_size(int *rows, int *cols){
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
		return get_cursor_position(rows, cols);
		return -1;
	}else {
		*rows = ws.ws_row;
		*cols = ws.ws_col;
		return 0;
	}
}

void init_editor() {
	if (get_window_size(&e.rows, &e.cols) == -1) {
		die("init_editor()");
	}
}

int main()
{
	enable_raw_mode();
	init_editor();
	while(1) {
		editor_refresh_screen();
		editor_process_keypress();
	}
	return 0;
}
