#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#define ERROR -1

// Funcion auxiliar
void
cerrar_fds(int pipe1[2], int pipe2[2])
{
	// Cerrar file descriptors
	close(pipe1[0]);
	close(pipe1[1]);
	close(pipe2[0]);
	close(pipe2[1]);
}

int
main(void)
{
	// Inicializacion del PRNG, solo se hace una vez
	srand(time(NULL));

	/*
	 * Arrays para guardar file descriptors abiertos por pipe():
	 * - Uno que se usará para leer en el proceso hijo y escribir en el proceso padre
	 * - Uno que se usará para leer en el proceso padre y escribir en el proceso hijo
	 */
	int hin_pout[2];
	int pin_hout[2];

	if (pipe(hin_pout) == ERROR || pipe(pin_hout) == ERROR) {
		perror("Error en pipe: ");
		exit(ERROR);
	}

	int pid_padre = getpid();

	printf("Hola, soy PID %d:\n", pid_padre);
	printf("  - primer pipe me devuelve: [%d, %d]\n", hin_pout[0], hin_pout[1]);
	printf("  - segundo pipe me devuelve: [%d, %d]\n\n",
	       pin_hout[0],
	       pin_hout[1]);

	int pid_hijo = fork();

	if (pid_hijo == ERROR) {
		perror("Error al ejecutar fork: ");
		cerrar_fds(hin_pout, pin_hout);
		exit(ERROR);
	}

	if (pid_hijo == 0) {
		// Proceso hijo
		int recv = 0;
		int bytes_read = read(hin_pout[0], &recv, sizeof(recv));
		if (bytes_read == ERROR) {
			perror("Error en read: ");
			exit(ERROR);
		}

		printf("Donde fork me devuelve 0:\n");
		printf("  - getpid me devuelve: %d\n", getpid());
		printf("  - getppid me devuelve: %d\n", getppid());
		printf("  - recibo valor %d vía fd=%d\n", recv, hin_pout[0]);
		printf("  - reenvío valor en fd=%d y termino\n\n", pin_hout[1]);

		int bytes_written = write(pin_hout[1], &recv, sizeof(recv));
		if (bytes_written == ERROR) {
			perror("Error en write: ");
			exit(ERROR);
		}

		cerrar_fds(hin_pout, pin_hout);
	} else {
		// Proceso padre
		int randnum = random();
		printf("Donde fork me devuelve %d:\n", pid_hijo);
		printf("  - getpid me devuelve: %d\n", getpid());
		printf("  - getppid me devuelve: %d\n", getppid());
		printf("  - random me devuelve: %d\n", randnum);
		printf("  - envío valor %d a través de fd=%d\n\n",
		       randnum,
		       hin_pout[1]);

		int bytes_written = write(hin_pout[1], &randnum, sizeof(randnum));
		if (bytes_written == ERROR) {
			perror("Error en write: ");
			exit(ERROR);
		}

		int recv = 0;
		int bytes_read = read(pin_hout[0], &recv, sizeof(recv));
		if (bytes_read == ERROR) {
			perror("Error en read: ");
			exit(ERROR);
		}

		printf("Hola, de nuevo PID %d:\n", pid_padre);
		printf("  - recibí valor %d vía fd=%d\n", recv, pin_hout[0]);

		// Espero a que el proceso hijo termine, si es que aún no lo hizo
		wait(NULL);

		cerrar_fds(hin_pout, pin_hout);
	}

	return 0;
}