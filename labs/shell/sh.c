#include "defs.h"
#include "types.h"
#include "readline.h"
#include "runcmd.h"
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>

char prompt[PRMTLEN] = { 0 };
char *histfile;
extern struct histbuf histbuf;
struct termios oldtc;
struct winsize wsize;

static void
cleanup()
{
	free(histfile);
	if (histbuf.buf) {
		for (size_t i = 0; i < histbuf.n; i++) {
			free(histbuf.buf[i]);
		}
		free(histbuf.buf);
	}
	tcsetattr(STDIN_FILENO, TCSANOW, &oldtc);
}

int
on_window_resize(int sig)
{
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &wsize) < 0) {
		perror("Error requesting terminal size");
		return -1;
	}
	return 0;
}


// runs a shell command
static void
run_shell()
{
	char *cmd;

	while ((cmd = read_line(prompt)) != NULL)
		if (run_cmd(cmd) == EXIT_SHELL)
			return;
}

// initializes the shell
// with the "HOME" directory
static int
init_shell()
{
	char buf[BUFLEN] = { 0 };
	char *home = getenv("HOME");

	histfile = getenv("HISTFILE");
	if (histfile)
		histfile = strdup(histfile);
	else {
		histfile = malloc(strlen(home) + 20);
		sprintf(histfile, "%s/.fisop_history", home);
	}

	printf("HISTFILE: %s\n", histfile);

	if (chdir(home) < 0) {
		snprintf(buf, sizeof buf, "cannot cd to %s ", home);
		perror(buf);
	} else {
		snprintf(prompt, sizeof prompt, "(%s)", home);
	}

	// Read terminal size
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &wsize) < 0) {
		perror("Error requesting terminal size");
		return -1;
	}

	// Set noncanonical mode with no echo
	struct termios newtc;
	tcgetattr(STDIN_FILENO, &oldtc);
	newtc = oldtc;
	newtc.c_lflag &= ~(ICANON | ECHO);
	newtc.c_cc[VMIN] = 1;
	// newtc.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSANOW, &newtc);

	// EVENTS
	// register cleanup function
	atexit(cleanup);
	// register window resize event
	struct sigaction sa_winch;
	sa_winch.sa_handler = on_window_resize;
	sigaction(SIGWINCH, &sa_winch, NULL);
}

int
main(void)
{
	if ((status = init_shell()) < 0)
		return status;

	run_shell();

	return 0;
}
