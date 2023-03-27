#ifndef BUILTIN_H
#define BUILTIN_H

#include "defs.h"

extern char prompt[PRMTLEN];
struct histbuf {
	char **buf;
	size_t len;
	size_t n;
};

int cd(char *cmd);

int exit_shell(char *cmd);

int pwd(char *cmd);

size_t hist2str(size_t n);
int history(char *cmd);

#endif  // BUILTIN_H
