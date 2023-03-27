#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "printfmt.h"

// this size can be updated
// for larger outputs
#define MAX_SIZE 2048

static char buf[MAX_SIZE];
static int fd = -1;

static void close_logfile(){
	close(fd);
}

// prints formatted data to stdout
// without using `printf(3)` function
//
// it prevents indirect calls to `malloc(3)`
//
int
printfmt(char* format, ...)
{
	int r;
	va_list ap;

	memset(buf, 0, MAX_SIZE);

	va_start(ap, format);
	vsnprintf(buf, MAX_SIZE, format, ap);
	va_end(ap);

	r = write(STDOUT_FILENO, buf, MAX_SIZE);

	return r;
}

int
logfmt(char* format, ...)
{
	int r;
	va_list ap;

	memset(buf, 0, MAX_SIZE);

	va_start(ap, format);
	vsnprintf(buf, MAX_SIZE, format, ap);
	va_end(ap);

#ifdef DEBUG
	r = write(STDOUT_FILENO, buf, MAX_SIZE);
#else
	if (fd < 0){
		fd = open("logfile.log", O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
		if (fd < 0){
			perror("Error");
			exit(-1);
		}
		atexit(close_logfile);
	}
	r = write(fd, buf, strlen(buf));
	
#endif
	return r;
}
