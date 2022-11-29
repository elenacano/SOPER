/**
 * @file ejercicio_prottemp.c
 * @author Rubén García de la Fuente ruben.garciadelafuente@estudiante.uam.es
 * @author Elena Cano Castillejo, elena.canoc@estudiante.uam.es
 * @group 2202
 * @date 26-03-2020
 *
 * @brief Este programa crea un número de procesos que se le pasa como argumento.
 * Todos los hijos creados realizarán la suma desde 1 hasta su pid/10, enviarán
 * la señal SIGUSR2 al padre y se suspenderán hasta que reciban la señal SIGTERM.
 * El padre llamará a la función alarm y se suspenderá hasta recibir la señal
 * SIGALRM, tras T segundos se recibirá y enviará la señal SIGTERM a todos sus
 * hijos, y finalizará liberando todos los recursos asociados.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

void manejador_SIGNAL(int sig) {
    return;
}

int main(int argc, char** argv) {
    struct sigaction act;
    sigset_t mask;
    int N;
    int T;
    pid_t pid;
    pid_t *cpid;
    int i, suma;

    /* COMPORBAMOS EL NÚMERO DE ARGUMENTOS */

    if (argc < 3) {
        fprintf(stdout, "Se esperaban 2 argumentos de entrada\n");
        exit(EXIT_FAILURE);
    }

    N = atoi(argv[1]);
    T = atoi(argv[2]);

    if ((cpid = malloc(N*sizeof(pid_t))) == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    /* CREAMOS LAS MÁSCARAS */

    sigemptyset(&(act.sa_mask));
    act.sa_flags = 0;
    act.sa_handler = manejador_SIGNAL;
    if (sigaction(SIGALRM, &act, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGUSR2, &act, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &act, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    /* CREAMOS LOS HIJOS */

    for (i=0; i<N; i++) {
        if ((pid = fork()) == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (!pid)
            break;
        else
            cpid[i] = pid;
    }

    /* CÓDIGO DEL HIJO */

    if (pid == 0) {
        sigfillset(&mask);
        sigdelset(&mask, SIGTERM);

        for (i=1, suma=0; i<getpid()/10; i++) {
            suma += i;
        }
        fprintf(stdout, "(PID: %d) = %d\n", getpid(), suma);

        if (kill(getppid(), 12) == -1) {
            perror("kill");
            exit(EXIT_FAILURE);
        }
        sigsuspend(&mask);

        fprintf(stdout, "Finalizado %d\n", getpid());
        free(cpid);
        exit(EXIT_SUCCESS);
    }

    /* CÓDIGO DEL PADRE */

    else {
        sigfillset(&mask);
        sigdelset(&mask, SIGALRM);

        alarm(T);
        sigsuspend(&mask);

        for (i=0; i<N; i++) {
            if (kill(cpid[i], 15) == -1) {
                perror("kill");
                exit(EXIT_FAILURE);
            }
        }

        fprintf(stdout, "Finalizado Padre\n");
        for (i=0; i<N; i++) {
            wait(NULL);
        }
        free(cpid);
        exit(EXIT_SUCCESS);
    }
}
