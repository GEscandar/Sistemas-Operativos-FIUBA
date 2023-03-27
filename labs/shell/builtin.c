#include "builtin.h"
#include "parsing.h"

extern char prompt[PRMTLEN];
extern int status;
extern char *histfile;
struct histbuf histbuf = { NULL, 0, 0 };
// returns true if the 'exit' call
// should be performed
//
// (It must not be called from here)
int
exit_shell(char *cmd)
{
	return strcmp(cmd, "exit") == 0;
}

// returns true if "chdir" was performed
//  this means that if 'cmd' contains:
// 	1. $ cd directory (change to 'directory')
// 	2. $ cd (change to $HOME)
//  it has to be executed and then return true
//
//  Remember to update the 'prompt' with the
//  	new directory.
//
// Examples:
//  1. cmd = ['c','d', ' ', '/', 'b', 'i', 'n', '\0']
//  2. cmd = ['c','d', '\0']
int
cd(char *cmd)
{
	bool changed = false;
	int size = cmd ? strlen(cmd) : 0;

	if (size >= 2 && cmd[0] == 'c' && cmd[1] == 'd' &&
	    (cmd[2] == END_STRING || cmd[2] == SPACE)) {
		if (cmd[2] == SPACE) {
			int i = 3;
			while (i < size && cmd[i] == SPACE)
				i++;

			status = i < size ? chdir(cmd + i)
			                  : chdir(getenv("HOME"));
		} else
			status = chdir(getenv("HOME"));

		if (status == 0) {
			char dir[PRMTLEN - 2] = { 0 };
			getcwd(dir, sizeof dir);
			snprintf(prompt, sizeof prompt, "(%s)", dir);
			changed = true;
		} else
			perror("sh: cd");
	}

	return changed;
}

// returns true if 'pwd' was invoked
// in the command line
//
// (It has to be executed here and then
// 	return true)
int
pwd(char *cmd)
{
	if (strcmp(cmd, "pwd") == 0) {
		char cwd[ARGSIZE];
		getcwd(cwd, ARGSIZE);
		printf("%s\n", cwd);
		status = 0;

		return true;
	}

	return false;
}


size_t
hist2str(size_t n)
{
	char line[BUFLEN];
	size_t buff_grow = 1000;

	if (n > histbuf.n && n > histbuf.len) {
		histbuf.len += buff_grow;
		if (!(histbuf.buf = realloc(histbuf.buf,
		                            histbuf.len * sizeof(char *)))) {
			perror("Error allocating buffer");
			return -1;
		}
	}


	FILE *history = fopen(histfile, "r");
	if (!history) {
		history = fopen(histfile, "w");
		if (!history) {
			perror("Error opening histfile");
			return -1;
		}
	}

	size_t i;
	for (i = 0; fgets(line, sizeof line, history); i++) {
		if (i < histbuf.n)
			continue;
		if (i % buff_grow == 0) {
			if (!(histbuf.buf = realloc(histbuf.buf,
			                            histbuf.n + buff_grow))) {
				perror("Error allocating memory");
				return -1;
			}
			histbuf.len += buff_grow;
		}
		histbuf.buf[histbuf.n++] = strdup(line);
	}

	fclose(history);

	return histbuf.n;
}


int
history(char *cmd)
{
	if (strstr(cmd, "history") == cmd) {
		size_t n = 1000;

		if (strstr(cmd, "-n")) {
			char *tmp = strrchr(cmd, ' ');
			if (!tmp) {
				perror("Error, missing argument");
				return -1;
			}
			if (!(n = strtoul(tmp + 1, NULL, 10))) {
				perror("Error parsing argument");
				return -1;
			}
		}

		size_t i = hist2str(n);
		printf("history entries: %lu\n", histbuf.n);
		for (size_t j = i > n ? i - n : 0; j < i; j++) {
			printf("%5lu %s", j, histbuf.buf[j]);
		}

		return 1;
	}

	return 0;
}
