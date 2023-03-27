#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#define ERROR -1
#define READ_EOF 0


void filtrar_primos(int fd_lectura);

void
filtrar_primos(int fd_lectura)
{
	// Leer valor
	int primo;
	int status = read(fd_lectura, &primo, sizeof(primo));

	if (status > READ_EOF) {  // condicion de corte del ultimo filtro

		// Asumo que 'primo' es primo y lo imprimo por pantalla
		printf("primo %d\n", primo);
		fflush(NULL);  // limpiar stdout para que no se impriman lineas en exceso

		int fd[2];
		if (pipe(fd) == ERROR) {
			perror("Error en pipe: ");
		}

		int pid_hijo = fork();

		if (pid_hijo == 0) {
			// cerrar fds que no se vuelven a usar
			close(fd[1]);
			close(fd_lectura);
			filtrar_primos(fd[0]);
			close(fd[0]);
		} else {
			close(fd[0]);
			int numero;
			while (read(fd_lectura, &numero, sizeof(numero)) >
			       READ_EOF) {
				if (numero % primo != 0) {
					int bytes_written = write(
					        fd[1], &numero, sizeof(numero));
					if (bytes_written == ERROR) {
						perror("Error en write: ");
						exit(ERROR);
					}
				}
			}
			close(fd[1]);
			close(fd_lectura);
			wait(NULL);
		}
	}
}

int
main(int argc, char *argv[])
{
	if (argc < 2) {
		printf("Tenes que pasar un numero!\n");
		return ERROR;
	}

	int top = strtol(argv[1], NULL, 10);
	int fd[2];
	if (pipe(fd) == ERROR) {
		perror("Error en pipe: ");
		exit(ERROR);
	}

	int pid_hijo = fork();

	if (pid_hijo == 0) {
		close(fd[1]);
		filtrar_primos(fd[0]);
		// fd[0] se cierra en la funcion filtrar_primos
	} else {
		close(fd[0]);
		for (int i = 2; i <= top; i++) {
			int bytes_written = write(fd[1], &i, sizeof(i));
			if (bytes_written == ERROR) {
				perror("Error en write: ");
				exit(ERROR);
			}
		}
		close(fd[1]);
		wait(NULL);
	}

	return 0;
}