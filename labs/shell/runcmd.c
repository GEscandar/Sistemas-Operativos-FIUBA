#include "runcmd.h"

#ifndef MAX_BACKGROUND_PROCS
#define MAX_BACKGROUND_PROCS 1024
#endif

int status = 0;
struct cmd *parsed_pipe;
struct cmd *background[1024];
size_t bprocs = 0;
extern char *histfile;

// Agregar cmd a la lista de procesos en segundo plano
void add_background_process(struct cmd *cmd);

// Quitar el struct cmd con cmd->pid == pid de la lista
// de procesos en segundo plano y liberar la memoria
// del comando
void finish_background_process(pid_t pid);

void
add_background_process(struct cmd *cmd)
{
	background[bprocs++] = cmd;
}

void
finish_background_process(pid_t pid)
{
	size_t i;
	for (i = 0; i < bprocs; i++) {
		if (background[i]->pid == pid) {
			print_status_info(background[i]);
			free_command(background[i]);
			break;
		}
	}
	for (; i < bprocs; i++) {
		background[i] = i + 1 == bprocs ? NULL : background[i + 1];
	}
	bprocs--;
}

// runs the command in 'cmd'
int
run_cmd(char *cmd)
{
	pid_t p;
	struct cmd *parsed;

	p = waitpid(-1, &status, WNOHANG);
	if (p > 0) {
		finish_background_process(p);
	}

	// if the "enter" key is pressed
	// just print the prompt again
	if (cmd[0] == END_STRING)
		return 0;

	FILE *history_file = fopen(histfile, "a+");
	if (!history_file) {
		perror("Error opening histfile");
		return -1;
	}

	if (fprintf(history_file, "%s\n", cmd) == -1) {
		perror("Error writing histfile");
		return -1;
	}
	fclose(history_file);
	hist2str(1);  // update the in-memory buffer

	// "cd" built-in call
	if (cd(cmd))
		return 0;

	// "exit" built-in call
	if (exit_shell(cmd))
		return EXIT_SHELL;

	// "pwd" built-in call
	if (pwd(cmd))
		return 0;

	if (history(cmd))
		return 0;

	// parses the command line
	parsed = parse_line(cmd);

	// forks and run the command
	if ((p = fork()) == 0) {
		// keep a reference
		// to the parsed pipe cmd
		// so it can be freed later
		if (parsed->type == PIPE)
			parsed_pipe = parsed;

		exec_cmd(parsed);
	}

	// stores the pid of the process
	parsed->pid = p;

	// background process special treatment
	// Hint:
	// - check if the process is
	//		going to be run in the 'back'
	// - print info about it with
	// 	'print_back_info()'
	//
	if (parsed->type == BACK) {
		print_back_info(parsed);
		add_background_process(parsed);
	}

	else {
		// waits for the process to finish
		waitpid(p, &status, 0);

		print_status_info(parsed);

		free_command(parsed);
	}

	return 0;
}
