/**
 * @file ejercicio_pipes.c
 * @author Rubén García de la Fuente ruben.garciadelafuente@estudiante.uam.es
 * @author Elena Cano Castillejo, elena.canoc@estudiante.uam.es
 * @group 2202
 * @date 21-02-2020
 *
 * @brief Este programa crea dos hijos, el primero de ellos generará un número aleatorio y
 * se lo pasará a su padre a través de una tubería, el padre lo recogerá y lo enviará al
 * segundo hijo a través de otra tubería. El segundo hijo creará un fichero llamado "numero_leido.txt"
 * y escribirá en ese fichero el número pasado.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

int main() {
    int fd1[2];
    int fd2[2];
    int aleatnum;
    pid_t pid;
    ssize_t nbytes;
    char buffer[80];
    int filedesc;

    srand(time(NULL));
	if(pipe(fd1) == -1)
	{
		perror("pipe 1");
		exit(EXIT_FAILURE);
	}
	if(pipe(fd2) == -1)
	{
		perror("pipe 2");
		exit(EXIT_FAILURE);
	}

    pid=fork();
    if (pid == -1) {
        perror("fork 1");
        exit(EXIT_FAILURE);
    }

    if(pid == 0) {
        /*Hijo 1*/
        close(fd1[0]);

        aleatnum=rand()%11;
        sprintf(buffer, "%d", aleatnum);

        /*Escribimos en la tubería 1*/
        nbytes=write(fd1[1], buffer, strlen(buffer)+1);
        if (nbytes == -1) {
            perror("write 1");
            exit(EXIT_FAILURE);
        }
        fprintf(stdout, "He escrito el número %d\n", aleatnum);

        exit(EXIT_SUCCESS);
    }
    else {
        pid=fork();
        if (pid == -1) {
            perror("fork 2");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {
            /*Hijo 2*/
            close(fd2[1]);

            /*Abrimos un nuevo fichero*/
            filedesc=open("numero_leido.txt", O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
            if (filedesc == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }

            /*Leemos de la segunda tubería*/
            nbytes=read(fd2[0], buffer, sizeof(buffer));
            if (nbytes == -1) {
                perror("read 2");
                exit(EXIT_FAILURE);
            }

            /*Escribimos en el fichero creado*/
            dprintf(filedesc, "%s", buffer);

            close(filedesc);

            exit(EXIT_SUCCESS);
        }
        else {
            /*Padre*/
            close(fd1[1]);
            close(fd2[0]);

            /*Leemos de la primera tubería*/
            nbytes=read(fd1[0], buffer, sizeof(buffer));
            if (nbytes == -1) {
                perror("read 1");
                exit(EXIT_FAILURE);
            }

            /*Escribimos en la segunda tubería*/
            nbytes=write(fd2[1], buffer, strlen(buffer)+1);
            if (nbytes == -1) {
                perror("write 2");
                exit(EXIT_FAILURE);
            }

            wait(NULL);
            wait(NULL);
        }
    }
    exit(EXIT_SUCCESS);
}
