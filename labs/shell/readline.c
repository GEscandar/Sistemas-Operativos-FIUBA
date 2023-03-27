#include "defs.h"
#include "readline.h"
#include "builtin.h"
#include <ctype.h>
#include <termios.h>
#include <limits.h>
#include <sys/ioctl.h>

extern struct histbuf histbuf;
extern struct winsize wsize;

static char buffer[BUFLEN];
static char line[BUFLEN];
static int cpos;     // cursor position
static int coffset;  //  cursor position at buffer start
size_t hindex;
static char escseq[20];


#define EOT (char) 4  // end of transmission

#define ESC_START '\33'  // Escape sequence start character

// ESCAPE SEQUENCES

// Arrow Keys
#define KEY_UP "[A"
#define KEY_DOWN "[B"
#define KEY_RIGHT "[C"
#define KEY_LEFT "[D"

// Control keys
#define CTL_HOME "[H"
#define CTL_ERASE "[J"
#define CTL_ERASE_LINE "[2K"

// Device Status
#define STAT_CURSOR "[6n"

int
putch(char ch)
{
	return write(STDOUT_FILENO, &ch, 1);
}

int putstr(char *str){
	return write(STDOUT_FILENO, str, strlen(str));
}

void
write_seq(char *seq)
{
	putch(ESC_START);
	for (; *seq; seq++)
		putch(*seq);
}

void
backspace()
{
	if (cpos > coffset) {
		if (cpos % wsize.ws_col == 0) {
			write_seq(KEY_UP);
			memset(escseq, 0, sizeof escseq);
			sprintf(escseq, "[%dC", wsize.ws_col);
			write_seq(escseq);
			putch(' ');
		} else {
			putch('\b');
			putch(' ');
			putch('\b');
		}
		if (cpos - coffset == strlen(buffer)) {
			buffer[--cpos - coffset] = '\0';
		} else {
			buffer[--cpos - coffset] = ' ';
		}
	}
}

void
write_inline(char *cmd)
{
	int buflen = strlen(buffer);
	int lines = buflen / wsize.ws_col;
	char tmp[30] = { 0 };

	// printf("x: %ld, y: %ld, start x: %ld, cpos: %d\n", coords[0], coords[1], start_coords[0], cpos);

	if (buflen > 0) {
		sprintf(tmp,
		        "[%ldD",
		        buflen > wsize.ws_col ? buflen % wsize.ws_col : buflen);
		write_seq(tmp);
	}
	if (lines) {
		for (int i = 0; i < lines; i++) {
			write_seq(KEY_UP);
		}
	}
	write_seq(CTL_ERASE);

	cpos = coffset;
	memset(buffer, 0, sizeof buffer);
	for (; *cmd && *cmd != '\n'; cmd++) {
		putch(*cmd);
		buffer[cpos++ - coffset] = *cmd;
	}
}


int
getch()
{
	struct termios tc;
	int bytes;
	char ch;

	tcgetattr(STDIN_FILENO, &tc);

	if ((bytes = read(STDIN_FILENO, &ch, 1)) == -1) {
		perror("Error reading character");
		return 0;
	} else if (bytes == 0) {
		return 0;
	}

	// printf("%d\n", ch);

	if (ch == tc.c_cc[VERASE]) {
		ch = '\b';
		backspace();
	} else if (ch == ESC_START) {
		fd_set set;
		struct timeval timeout = { 0, 0 };
		FD_ZERO(&set);
		FD_SET(STDIN_FILENO, &set);
		int selret = select(1, &set, NULL, NULL, &timeout);

		memset(escseq, 0, sizeof escseq);

		if (selret == 1) {
			if ((bytes = read(STDIN_FILENO, escseq, sizeof escseq)) ==
			    -1) {
				// perror("Error reading character");
				return 0;
			}

			int up = strcmp(escseq, KEY_UP) == 0,
			    down = strcmp(escseq, KEY_DOWN) == 0;
			if (up || down) {
				if (histbuf.n == hindex) {
					// read the next 10 history entries
					hist2str(10);
					// printf("Reading history entries: n=%lu\n", histbuf.n);
				}

				int i = 0;
				if (up && histbuf.n > hindex) {
					if (hindex == 0) {
						memcpy(line, buffer, cpos);
						line[cpos] = '\0';
					}
					i = histbuf.n - 1 - hindex++;
					write_inline(histbuf.buf[i]);
				} else if (down) {
					if (hindex > 0)
						hindex--;
					if (hindex > 0) {
						i = histbuf.n - hindex;
						write_inline(histbuf.buf[i]);
					} else {
						write_inline(line);
					}
				}
			} else if (strcmp(escseq, KEY_RIGHT) == 0) {
				if (cpos - coffset < strlen(buffer)) {
					if (++cpos % wsize.ws_col == 0) {
						write_seq(KEY_DOWN);
						memset(escseq, 0, sizeof escseq);
						sprintf(escseq,
						        "[%dD",
						        wsize.ws_col);
						write_seq(escseq);
					} else {
						write_seq(escseq);
					}
				}
			} else if (strcmp(escseq, KEY_LEFT) == 0) {
				if (cpos > coffset) {
					if (cpos % wsize.ws_col == 0) {
						write_seq(KEY_UP);
						memset(escseq, 0, sizeof escseq);
						sprintf(escseq,
						        "[%dC",
						        wsize.ws_col);
						write_seq(escseq);
					} else {
						write_seq(escseq);
					}
					cpos--;
				}
			}
			return 0;
		}
	} else {
		putch(ch);
	}

	return ch;
}

// reads a line from the standard input
// and prints the prompt
char *
read_line(const char *prompt)
{
	int c = 0;
	hindex = 0;

#ifndef SHELL_NO_INTERACTIVE
	fprintf(stdout, "%s %s %s\n", COLOR_RED, prompt, COLOR_RESET);
	fprintf(stdout, "%s", "$ ");
	fflush(stdout);
#endif
	coffset = 2;  // length of "$ "
	cpos = coffset;

	memset(buffer, 0, BUFLEN);

	c = getch();

	while (c != END_LINE && c != EOF && c != EOT) {
		if (c) {
			if (c != '\b')
				buffer[cpos++ - coffset] = c;
		}
		c = getch();
	}

	// if the user press ctrl+D
	// just exit normally
	if (c == EOF || c == EOT)
		return NULL;

	return buffer;
}