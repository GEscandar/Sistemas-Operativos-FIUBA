#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#ifndef NARGS
#define NARGS 4
#endif

#ifndef NPROCS
#define NPROCS 4
#endif

#define ERROR -1
#define BUFFSIZE 1024

// Particionar al string src, dividiéndolo segun el delimitador
// delim y guardar el resultado en dest. Se devuelve la cantidad
// de elementos de dest.
int strsplit(char *src, char *dest[], char delim);

// Ejecutar un programa con los argumentos especificados.
// El primer argumento es la ruta del programa a ejecutar,
// o su nombre si dicha ruta está guardada en la variable
// PATH del sistema.
void run(char *args[]);

// Leer stdin cuando es parte de un pipe, y guardar el resultado
// en buffer, asignandole más memoria de ser necesario.
// Devolver la capacidad del buffer después de realizar
// la lectura.
size_t read_piped_stdin(char **buffer);

// Version reducida de xargs que toma argumentos linea a linea.
void xargs(char *command, char *flag);

int
strsplit(char *src, char *dest[], char delim)
{
	int copystart = 0, copysize = 0, argcount = 0;

	for (int i = 0; src[i] != '\0'; i++) {
		if (src[i] == delim) {
			if (copysize > 0) {
				dest[argcount] =
				        strndup(src + copystart, copysize);

				copysize = 0;
				copystart = i + 1;
				argcount++;
			}
		} else {
			copysize++;
		}
	}
	if (copysize > 0) {
		// Si el ultimo caracter de src no es delim, agregar la ultima particion
		dest[argcount] = strndup(src + copystart, copysize);
		argcount++;
	}

	return argcount;
}

void
run(char *args[])
{
	int pid_hijo = fork();
	if (pid_hijo == 0) {
		int status = execvp(args[0], args);
		if (status == ERROR) {
			perror("Error al ejecutar "
			       "execvp: ");
			exit(ERROR);
		}
	}
}


size_t
read_piped_stdin(char **buffer)
{
	size_t size = BUFFSIZE;
	*buffer = (char *) malloc(size);
	if (*buffer == NULL) {
		perror("Error en malloc: ");
		exit(ERROR);
	}
	// Asumo que el pipe fue escrito
	int bytes_read = read(STDIN_FILENO, *buffer, size);

	while (bytes_read == BUFFSIZE) {
		size += bytes_read;
		*buffer = realloc(*buffer, size);

		bytes_read = read(STDIN_FILENO,
		                  *buffer + (size - bytes_read),
		                  BUFFSIZE);
	}

	return size;
}


void
xargs(char *command, char *flag)
{
	char *buffer;
	size_t size = read_piped_stdin(&buffer);

	char **tokens = malloc(
	        (size / 2) *
	        sizeof(char *));  // puede haber como máximo size/2 strings divididos por el delimitador
	if (tokens == NULL) {
		perror("Error en malloc: ");
		exit(ERROR);
	}

	bool run_with_parallelism = flag && strcmp(flag, "-P") == 0 && NPROCS > 0;
	int split_size = strsplit(buffer, tokens, '\n');
	free(buffer);

	char *exec_args[NARGS + 2];
	exec_args[0] = command;

	int exec_count = split_size % NARGS == 0 ? (split_size / NARGS)
	                                         : (split_size / NARGS) + 1;
	int total_procs = 0;
	for (int i = 0; i < exec_count; i++) {
		for (int j = 0; j < NARGS && (i * NARGS + j) < split_size; j++) {
			exec_args[j + 1] = tokens[i * NARGS + j];
			exec_args[j + 2] = NULL;
		}

		if (run_with_parallelism) {
			if (total_procs == NPROCS) {
				// Maximo de procesos paralelos alcanzado: Esperar
				// a que alguno de los procesos hijos termine
				wait(NULL);
				total_procs--;
			}
			run(exec_args);
			total_procs++;
		} else {
			run(exec_args);
			wait(NULL);
		}
	}
	for (int i = 0; i < total_procs; i++) {
		// Esperar a que el resto de los procesos hijos terminen (maximo NPROCS procesos a esperar)
		wait(NULL);
	}

	for (int i = 0; i < split_size; i++) {
		free(tokens[i]);
	}
	free(tokens);
}


int
main(int argc, char *argv[])
{
	char *command;
	char *flag;

	if (argc < 2) {
		printf("Tenes que pasar por lo menos un argumento!\n");
		exit(ERROR);
	} else if (argc == 2) {
		command = argv[1];
		flag = NULL;
	} else {
		flag = argv[1];
		command = argv[2];
	}

	xargs(command, flag);

	return 0;
}