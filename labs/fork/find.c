// Necesario para usar O_DIRECTORY, DT_DIR y strcasestr
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <dirent.h>


#define CURRENT_DIR "."
#define PARENT_DIR ".."
#define ERROR -1

// Devolver si str1 es o no exactamente igual a str2.
bool streq(char *str1, char *str2);

// Encontrar y mostrar por pantalla todos los archivos (o directorios)
// cuyos nombres contengan al string target, buscando a partir del directorio actual.
void find(char *target, bool ignore_case);


bool
streq(char *str1, char *str2)
{
	return strcmp(str1, str2) == 0;
}


// Funcion auxiliar
void
_find(char *dirname,
      char *target,
      bool ignore_case,
      DIR *relative_to,
      char *relative_dirname)
{
	DIR *dir;
	if (relative_to == NULL) {
		dir = opendir(dirname);
	} else {
		int relative_fd =
		        openat(dirfd(relative_to), dirname, O_DIRECTORY);
		dir = fdopendir(relative_fd);
	}

	if (dir == NULL) {
		perror("Error al abrir directorio: ");
		return;
	}

	struct dirent *entry;
	char _relative_dirname[PATH_MAX];

	if (relative_dirname && !streq(relative_dirname, CURRENT_DIR)) {
		strcpy(_relative_dirname, relative_dirname);
		strcat(strcat(_relative_dirname, "/"), dirname);
	} else {
		strcpy(_relative_dirname, dirname);
	}

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_type == DT_DIR && !streq(entry->d_name, CURRENT_DIR) &&
		    !streq(entry->d_name, PARENT_DIR)) {
			_find(entry->d_name,
			      target,
			      ignore_case,
			      dir,
			      _relative_dirname);
		}
		char *substring = ignore_case ? strcasestr(entry->d_name, target)
		                              : strstr(entry->d_name, target);
		if (substring != NULL) {
			if (streq(_relative_dirname, CURRENT_DIR)) {
				printf("%s\n", entry->d_name);
			} else {
				char complete_name[PATH_MAX];
				strcpy(complete_name, _relative_dirname);
				strcat(strcat(complete_name, "/"), entry->d_name);
				printf("%s\n", complete_name);
			}
		}
	}

	closedir(dir);
}

void
find(char *target, bool ignore_case)
{
	_find(CURRENT_DIR, target, ignore_case, NULL, NULL);
}


int
main(int argc, char *argv[])
{
	if (argc < 2) {
		printf("Tenes que pasar por lo menos un argumento!\n");
		exit(ERROR);
	}

	char *target_str;
	bool ignore_case = false;

	if (strcmp(argv[1], "-i") == 0) {
		ignore_case = true;
		target_str = argv[2];
	} else {
		target_str = argv[1];
	}

	find(target_str, ignore_case);

	return 0;
}