/**
 * @file ejercicio_arbol.c
 * @author Rubén García de la Fuente ruben.garciadelafuente@estudiante.uam.es
 * @author Elena Cano Castillejo, elena.canoc@estudiante.uam.es
 * @group 2202
 * @date 21-02-2020
 *
 * @brief Este programa crea un número de procesos que se le pasa como argumento y que
 * generará el arbol de procesos del ejercicio 8, cada padre esperará a su hijo.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char** argv) {
    int num_proc;
    int i;
	pid_t pid;

    if(argv[1]==NULL) {
        fprintf(stdout, "Se esperaba al menos un argumento de entrada\n");
        exit(EXIT_FAILURE);
    }
    num_proc=atoi(argv[1]);

    /*Creamos tantos procesos como arguemntos de entrada nos hallan pasado*/
    for(i=0; i<num_proc-1; i++) {
		pid = fork();
		if(pid <  0) {
			perror("fork");
			exit(EXIT_FAILURE);
		}
        /*Imprimimos por pantalla qué proceso es*/
		else if(pid ==  0 && i==num_proc-2) {
			printf("Soy el último proceso\n");
		}
		else if(pid >  0) {
			printf("Proceso %d\n", i+1);
            wait(NULL);
            exit(EXIT_SUCCESS);
		}
	}
	exit(EXIT_SUCCESS);
}
