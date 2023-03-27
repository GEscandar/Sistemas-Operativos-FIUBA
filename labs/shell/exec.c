#include "exec.h"
#include "parsing.h"
#include "printstatus.h"

extern int status;

// sets "key" with the key part of "arg"
// and null-terminates it
//
// Example:
//  - KEY=value
//  arg = ['K', 'E', 'Y', '=', 'v', 'a', 'l', 'u', 'e', '\0']
//  key = "KEY"
//
static void
get_environ_key(char *arg, char *key)
{
	int i;
	for (i = 0; arg[i] != '='; i++)
		key[i] = arg[i];

	key[i] = END_STRING;
}

// sets "value" with the value part of "arg"
// and null-terminates it
// "idx" should be the index in "arg" where "=" char
// resides
//
// Example:
//  - KEY=value
//  arg = ['K', 'E', 'Y', '=', 'v', 'a', 'l', 'u', 'e', '\0']
//  value = "value"
//
static void
get_environ_value(char *arg, char *value, int idx)
{
	size_t i, j;
	for (i = (idx + 1), j = 0; i < strlen(arg); i++, j++)
		value[j] = arg[i];

	value[j] = END_STRING;
}

// sets the environment variables received
// in the command line
//
// Hints:
// - use 'block_contains()' to
// 	get the index where the '=' is
// - 'get_environ_*()' can be useful here
static void
set_environ_vars(char **eargv, int eargc)
{
	for (int i = 0; i < eargc; i++) {
		char *value = split_line(eargv[i], '=');
		if (setenv(eargv[i], value, 1) < 0)
			perror("Error setting env variable");
	}
}

// opens the file in which the stdin/stdout/stderr
// flow will be redirected, and returns
// the file descriptor
//
// Find out what permissions it needs.
// Does it have to be closed after the execve(2) call?
//
// Hints:
// - if O_CREAT is used, add S_IWUSR and S_IRUSR
// 	to make it a readable normal file
static int
open_redir_fd(char *file, int flags)
{
	int fd = -1;

	if ((flags & O_CREAT) == O_CREAT) {
		fd = open(file, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
	} else {
		fd = open(file, O_RDONLY);
	}

	return fd;
}


void
redir_fd(char *file, int flags, int fd)
{
	int filefd = open_redir_fd(file, flags);
	if (filefd < 0) {
		perror("Error opening file");
		_exit(-1);
	}

	int dupfd = dup2(filefd, fd);
	if (dupfd < 0) {
		perror("Error duplicating file descriptor");
		_exit(-1);
	}

	close(filefd);
}

// executes a command - does not return
//
// Hint:
// - check how the 'cmd' structs are defined
// 	in types.h
// - casting could be a good option
void
exec_cmd(struct cmd *cmd)
{
	// To be used in the different cases
	struct execcmd *e;
	struct backcmd *b;
	struct execcmd *r;
	struct pipecmd *p;

	switch (cmd->type) {
	case EXEC:
		// spawns a command
		//
		e = (struct execcmd *) cmd;
		if (e->eargc > 0)
			set_environ_vars(e->eargv, e->eargc);
		if (execvp(e->argv[0], e->argv) < 0) {
			perror(e->argv[0]);
			free_command(cmd);
			_exit(-1);
		}

		break;

	case BACK: {
		// runs a command in background
		//
		b = (struct backcmd *) cmd;
		exec_cmd(b->c);
		break;
	}

	case REDIR: {
		// changes the input/output/stderr flow
		//
		// To check if a redirection has to be performed
		// verify if file name's length (in the execcmd struct)
		// is greater than zero
		//

		r = (struct execcmd *) cmd;

		for (int i = 0; r->scmd[i] != END_STRING; i++) {
			if (r->scmd[i] == '<' && strlen(r->in_file) > 0) {
				redir_fd(r->in_file, O_RDONLY, STDIN_FILENO);
			}
			if (r->scmd[i] == '>') {
				if (r->scmd[i - 1] == '2' &&
				    strlen(r->err_file) > 0) {
					if (strstr(r->err_file, "&1")) {
						if (dup2(STDOUT_FILENO,
						         STDERR_FILENO) < 0) {
							perror("Error "
							       "duplicating "
							       "file "
							       "descriptor");
							_exit(-1);
						}
					} else {
						redir_fd(r->err_file,
						         O_CREAT,
						         STDERR_FILENO);
					}
				} else if (strlen(r->out_file) > 0) {
					redir_fd(r->out_file,
					         O_CREAT,
					         STDOUT_FILENO);
				}
			}
		}


		cmd->type = EXEC;
		exec_cmd(cmd);

		break;
	}

	case PIPE: {
		// pipes two commands
		//
		p = (struct pipecmd *) cmd;

		int pipefd[2];
		if (pipe(pipefd) < 0) {
			perror("Error creating pipe");
			_exit(-1);
		}

		int pid_left, pid_right;
		if ((pid_left = fork()) == 0) {
			close(pipefd[0]);
			if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
				perror("Error duplicating file descriptor");
				_exit(-1);
			}
			close(pipefd[1]);
			exec_cmd(p->leftcmd);
		} else if ((pid_right = fork()) == 0) {
			close(pipefd[1]);
			if (dup2(pipefd[0], STDIN_FILENO) < 0) {
				perror("Error duplicating file descriptor");
				_exit(-1);
			}
			close(pipefd[0]);

			struct cmd *right = parse_line(p->rightcmd->scmd);
			exec_cmd(right);
		} else {
			close(pipefd[0]);
			close(pipefd[1]);

			waitpid(pid_left, &status, 0);
			print_status_info(p->leftcmd);
			waitpid(pid_right, &status, 0);
			print_status_info(p->rightcmd);
		}


		// free the memory allocated
		// for the pipe tree structure
		free_command(parsed_pipe);
		_exit(status);
		break;
	}
	}
}
