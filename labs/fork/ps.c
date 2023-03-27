#include <stdio.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define FORMAT "%5s %s\t%8s %s\n"
#define BODY_FORMAT "%5d %s\t\t%8s %s\n"
#define NAMELEN 256
#define PATHLEN 1024
#define PROCPATHLEN 64
#define BUFF_GROW 1024

// dynamic buffer for file2str calls
struct dbuf {
	char *buf;  // dynamically grown buffer
	int size;   // current len of the above
};

struct proc {
	pid_t pid;
	char comm[NAMELEN];
};

static int
file2str(const char *directory, const char *what, struct dbuf *strbuf)
{
	char path[PROCPATHLEN];
	int fd, num, tot_read = 0, len;

	if (!strbuf->buf) {
		strbuf->buf = calloc(1, (strbuf->size = BUFF_GROW));
		if (!strbuf->buf)
			// error allocating buffer
			return -1;
	}

	len = snprintf(path, sizeof path, "%s/%s", directory, what);
	if (len <= 0 || (size_t) len >= sizeof path)
		// error forming full path
		return -1;

	if ((fd = open(path, O_RDONLY, 0)) == -1)
		// error opening file
		return -1;

	while ((num = read(fd, strbuf->buf + tot_read, strbuf->size - tot_read)) >
	       0) {
		tot_read += num;
		if (tot_read < strbuf->size)
			break;
		if (strbuf->size >= INT_MAX - BUFF_GROW) {
			tot_read--;
			break;
		}
		if (!(strbuf->buf = realloc(strbuf->buf,
		                            (strbuf->size += BUFF_GROW)))) {
			close(fd);
			return -1;
		}
	};
	strbuf->buf[tot_read] = '\0';
	close(fd);
	if (tot_read < 1)
		return -1;
	return tot_read;
}


static int
stat2proc(struct proc *proc, struct dbuf *strbuf)
{
	int read;
	char *curr = strbuf->buf;

	if ((read = sscanf(curr, "%d", &proc->pid)) <= 0)
		return -1;

	curr = strchr(curr, '(');
	curr++;
	char *end = strrchr(curr, ')');
	int len = end - curr;
	memcpy(proc->comm, curr, len);
	proc->comm[len] = '\0';

	return 0;
}


int
main(int argc, char **argv)
{
	DIR *dir;
	struct dirent *ent;

	char path[PATHLEN];
	FILE *file;

	dir = opendir("/proc");
	if (!dir) {
		perror("Error opening /proc");
		return -1;
	}

	printf(FORMAT, "PID", "TTY", "TIME", "CMD");

	// Read all entries of the /proc directory that have a number as their name
	while ((ent = readdir(dir)) != NULL) {
		char *tty;
		char is_number = 1;

		int i;
		for (i = 0; ent->d_name[i]; i++) {
			if (!isdigit(ent->d_name[i])) {
				is_number = 0;
				break;
			}
		}

		if (is_number) {
			sprintf(path, "/proc/%s", ent->d_name);

			struct dbuf dbuf = { NULL, 0 };
			if (file2str(path, "stat", &dbuf) == -1) {
				fprintf(stderr, "Error reading %s\n", path);
			}

			struct proc proc;
			stat2proc(&proc, &dbuf);
			free(dbuf.buf);

			printf(BODY_FORMAT, proc.pid, "?", "00:00:00", proc.comm);
		}
	}
	return 0;
}