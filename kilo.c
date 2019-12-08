#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#include<stdlib.h>
#include<stdio.h>
#include<ctype.h>
#include<unistd.h>
#include<termios.h>
#include<errno.h>
#include<sys/ioctl.h>
#include<sys/types.h>
#include<string.h>
#include<time.h>
#include<stdarg.h>

#define VERSION "0.1"
#define TAB_STOP 4

#define CTRL_KEY(key) (key & 0x1f)

enum KEYS {
	KEY_UP = 500,
	KEY_DOWN, 
	KEY_LEFT,
	KEY_RIGHT,
	DEL_KEY,
	PAGE_UP,
	PAGE_DOWN,
	HOME_KEY,
	END_KEY
};

typedef struct editor_row {
	int size;
	int r_size;
	char *chars;
	char *render;
}editor_row;

struct editor_config {
	int cursorX;
	int cursorY;
	int rx;
	int rows;
	int row_offset;
	int col_offset;
	int cols;
	int numrows;
	editor_row *erow;
	char *filename;
	char statusmsg[80];
	time_t statusmsg_time;
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

int editor_row_cx_to_rx(editor_row *row, int cx) {
	int rx = 0;
	int j;
	for (j = 0; j < cx; j++) {
		if (row->chars[j] == '\t')
			rx += (TAB_STOP - 1) - (rx % TAB_STOP);
		rx++;
	}
	return rx;
}

void editor_update_row (editor_row *r) {
	int tabs = 0;
	int j;
	for (j = 0; j < r->size; j++) {
		if (r->chars[j] == '\t')
			tabs++;
	}
	free(r->render);
	r->render = malloc(r->size + tabs * (TAB_STOP - 1)  + 1);

	int idx = 0;
	for (j = 0; j < r->size; j++) {
		if (r->chars[j] == '\t') {
			r->render[idx++] = ' ';
			while (idx % TAB_STOP != 0) r->render[idx++] = ' ';
		} else {
			r->render[idx++] = r->chars[j];
		}
	}

	r->render[idx] = '\0';
	r->r_size = idx;
}

void editor_append_row(char *row, size_t len) {
	e.erow = realloc(e.erow, sizeof(editor_row) * (e.numrows+1));

	int at = e.numrows;
	e.erow[at].size = len;
	e.erow[at].chars = malloc(len + 1);
	memcpy(e.erow[at].chars, row, len);
	e.erow[at].chars[len] = '\0';

	e.erow[at].r_size = 0;
	e.erow[at].render = NULL;
	editor_update_row(&e.erow[at]);

	e.numrows++;
}

void editor_scroll() {
	e.rx = 0;
	if (e.cursorY < e.numrows) {
		e.rx = editor_row_cx_to_rx(&e.erow[e.cursorY], e.cursorX);
	}

	if (e.cursorY < e.row_offset) {
		e.row_offset = e.cursorY;
	}
	if(e.cursorY >= e.row_offset + e.rows){
		e.row_offset = e.cursorY - e.rows + 1;
	}
	if (e.rx < e.col_offset) {
		e.col_offset = e.rx;
	}
	if (e.rx >= e.col_offset + e.cols) {
		e.col_offset = e.rx - e.cols + 1;
	}
}

void editor_open(char *filename) {
	if (filename) {
		free(e.filename);
	}
	e.filename = strdup(filename);

	FILE *fp = fopen(filename, "r");
	if (!fp) die("Fail to open");

	char *line = NULL; 
	size_t linecap = 0;
	ssize_t linelen;
	
	while ((linelen = getline(&line, &linecap, fp)) != -1) {
		while (linelen > 0 && (line[linelen - 1] == '\n' || 
							   line[linelen - 1] == '\r')) {
			linelen--;
		}
		editor_append_row(line, linelen);
	}
	free(line);
	fclose(fp);
}



void editor_draw_tildes(buffer *b) {
	for (int i = 0; i<e.rows; i++) {
		int filerow = i + e.row_offset;
		if (filerow >= e.numrows) {
			if (e.numrows == 0 && i == e.rows / 3) {
				char welcome[32];
				int len = snprintf(welcome, sizeof(welcome), 
							"RePico Version - %s", VERSION);
				if (len > e.cols) {
					len = e.cols;
				}
				int padding = (e.cols - len)/2;
				if (padding) {
					buffer_append(b, "~", 1);
					padding--;
				}
				//insert spaces to center welcome message
				while(padding--) buffer_append(b, " ", 1);

				buffer_append(b, welcome, len);
			} else {
				buffer_append(b, "~", 1);
			}
		} else {
			int len = e.erow[filerow].r_size - e.col_offset;
			if (len < 0) len = 0;
			if (len > e.cols) len = e.cols;
			buffer_append(b, &e.erow[filerow].render[e.col_offset], len);
		}
		//clear the line
		buffer_append(b, "\x1b[K", 4);
			buffer_append(b, "\r\n", 2);
	}
}

void editor_draw_statusbar(buffer *b) {
	buffer_append(b, "\x1b[7m", 4);
	char status[80], rstatus[80];
	int len = snprintf(status, sizeof(status), "%.20s - %d lines",
		e.filename ? e.filename : "[No name]", e.numrows);
	int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", e.cursorY + 1, e.numrows);
	if (len > e.cols)
		len = e.cols;
	buffer_append(b, status, len);
	while (len < e.cols) {
		if (e.cols - len == rlen) {
			buffer_append(b, rstatus, rlen);
			break;
		} else {
			buffer_append(b, " ", 1);
			len++;
		}
	}
	buffer_append(b, "\x1b[m", 3);
	buffer_append(b, "\r\n", 2);
}

void editor_set_status_msg(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(e.statusmsg, sizeof(e.statusmsg), fmt, ap);
	va_end(ap);
	e.statusmsg_time = time(NULL);
}

void editor_draw_message_bar(buffer *b) {
	buffer_append(b, "\x1b[K", 3);
	int msglen = strlen(e.statusmsg);
	if (msglen > e.cols)
		msglen = e.cols;
	if (msglen && time(NULL) - e.statusmsg_time < 5)
		buffer_append(b, e.statusmsg, msglen);
}

void editor_refresh_screen() {
	editor_scroll();

	buffer b = BUF_INIT;
	//hide the cursor
	buffer_append(&b, "\x1b[?25l", 6);
	//clear screen
	// buffer_append(&b, "\x1b[2J", 4);
	//go to top left
	buffer_append(&b, "\x1b[H", 3);

	//draw the tildes
	editor_draw_tildes(&b);

	//draw the statusbar
	editor_draw_statusbar(&b);

	//draw the messagebar
	editor_draw_message_bar(&b);

	//position cursor at top left
	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (e.cursorY - e.row_offset)+ 1, (e.rx-e.col_offset) + 1);
	buffer_append(&b, buf, strlen(buf));

	//show the cursor again
	buffer_append(&b, "\x1b[?25h", 6);

	write(STDOUT_FILENO, b.buf, b.len);
	buffer_free(&b);
}

int editor_read_key() {
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN)
			die("read()");
	}

	if (c == '\x1b') {
		char seq[3];
		if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
		if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

		if (seq[0] == '[') {
			if (seq[1] >= '0' && seq[1] <= '9') {
				if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
				if (seq[2] == '~') {
					switch(seq[1]) {
						case '1': return HOME_KEY;
						case '3': return DEL_KEY;
						case '4': return END_KEY;
						case '5': return PAGE_UP;
						case '6': return PAGE_DOWN;
						case '7': return HOME_KEY;
						case '8': return END_KEY;
					}
				}
			}
			switch (seq[1])
			{
				case 'A' : return KEY_UP;
				case 'B' : return KEY_DOWN;
				case 'C' : return KEY_RIGHT;
				case 'D' : return KEY_LEFT;
				case 'H' : return HOME_KEY;
				case 'F' : return END_KEY;
			}
		} else if (seq[0] == '0') {
			switch (seq[1]) {
				case 'H' : return HOME_KEY;
				case 'F' : return END_KEY;
			}
		}
		return '\x1b';
	} else {
		return c;
	}
}

void editor_move_cursor(int key) {
	editor_row *row = (e.cursorY >= e.numrows) ? NULL : &e.erow[e.cursorY];
	switch (key)
	{
	case KEY_LEFT: 
		if (e.cursorX != 0) {
			e.cursorX--;
		} else if (e.cursorY > 0) {
			e.cursorY--;
			e.cursorX = e.erow[e.cursorY].size;
		}
		break;
	case KEY_RIGHT: 
		if (row && e.cursorX < row->size) {
			e.cursorX++;
		} else if (row && e.cursorX == row->size) {
			e.cursorY++;
			e.cursorX = 0;
		}
		break;
	case KEY_DOWN: 
		if(e.cursorY < e.numrows)
			e.cursorY++;
		break;
	case KEY_UP: 
		if(e.cursorY != 0) e.cursorY--;
		break;
	}

	row = (e.cursorY >= e.numrows) ? NULL : &e.erow[e.cursorY];
	int rowlen = row ? row->size : 0;
	if (e.cursorX > rowlen) {
		e.cursorX = rowlen;
	}
}

void editor_process_keypress(){
	int c = editor_read_key();
	switch (c) {
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;

		case HOME_KEY:
			e.cursorX = 0;
			break;
		case END_KEY:
			if (e.cursorY < e.numrows) {
				e.cursorX = e.erow[e.cursorY].size;
			}
			break;
		case KEY_LEFT:
		case KEY_RIGHT:
		case KEY_UP:
		case KEY_DOWN:
			editor_move_cursor(c);
			break;
		case PAGE_UP:
		case PAGE_DOWN:
		{
			if (c == PAGE_UP) {
				e.cursorY = e.row_offset;
			} else if (c == PAGE_DOWN) {
				e.cursorY = e.row_offset + e.rows - 1;
				if (e.cursorY > e.numrows) {
					e.cursorY = e.numrows;
				}
			}
			int times = e.rows;
			while(times--)
				editor_move_cursor(c == PAGE_UP ? KEY_UP : KEY_DOWN);
			break;
		}
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
	e.cursorX = 0;
	e.cursorY = 0;
	e.numrows = 0;
	e.row_offset = 0;
	e.col_offset = 0;
	e.rx = 0;
	e.erow = NULL;
	e.filename = NULL;
	e.statusmsg[0] = '\0';
	e.statusmsg_time = 0;
	if (get_window_size(&e.rows, &e.cols) == -1) {
		die("init_editor()");
	}
	e.rows -= 2;
}

int main(int argc, char *argv[])
{
	enable_raw_mode();
	init_editor();
	if (argc >= 2) {
		editor_open(argv[1]);
	}

	editor_set_status_msg("HELP: CTRL + Q to quit");
	while(1) {
		editor_refresh_screen();
		editor_process_keypress();
	}
	return 0;
}
