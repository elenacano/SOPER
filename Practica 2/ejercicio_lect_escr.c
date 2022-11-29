/**
 * @file ejercicio_prottemp.c
 * @author Rubén García de la Fuente ruben.garciadelafuente@estudiante.uam.es
 * @author Elena Cano Castillejo, elena.canoc@estudiante.uam.es
 * @group 2202
 * @date 26-03-2020
 *
 * @brief Este programa crea un número de procesos que se le pasa como argumento.
 * Los hijos harán de lectores y el padre de escritor. Se creará un bucle
 * infinito en el que se pretende controlar (por medio de semáforos) la lectura
 * y la escritura de los procesos y que ésta no se realice de manera simultánea.
 * Varios lectores podrán leer al mismo tiempo pero no puede haber lectores y
 * escritores.
 * Tras terminar de leer o escribir, los procesos esperarán un tiempo que es
 * pasado como argumento por el usuario.
 * El bucle infinito termina cuando el padre recibe la señal SIGINT, pues este
 * enviará la señal SIGTERM a todos sus hijos (lo que les permite terminar), y
 * finalizará liberando todos los recuros asociados.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <semaphore.h>

#define SEM1 "sem_lectura"
#define SEM2 "sem_escritura"
#define SEM3 "sem_lectores"

static volatile sig_atomic_t got_signal_padre = 0;
static volatile sig_atomic_t got_signal_hijo = 0;

void manejador_SIGINT(int sig) {
    got_signal_padre = 1;
}

void manejador_SIGTERM(int sig) {
    got_signal_hijo = 1;
}

int main(int argc, char** argv) {
    sem_t *sem1 = NULL;
    sem_t *sem2 = NULL;
    sem_t *sem3 = NULL;
    struct sigaction act;
    sigset_t mask;
    int N_READ;
    int SECS;
    pid_t pid;
    pid_t *cpid;
    int i;
    int lectores;

    /* COMPORBAMOS EL NÚMERO DE ARGUMENTOS */

    if (argc < 3) {
        fprintf(stdout, "Se esperaban 2 argumentos de entrada\n");
        exit(EXIT_FAILURE);
    }

    N_READ = atoi(argv[1]);
    SECS = atoi(argv[2]);
    
    if ((cpid = malloc(N_READ*sizeof(pid_t))) == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    /* CREAMOS LOS MANEJADORES */

    sigemptyset(&(act.sa_mask));
    act.sa_flags = 0;
    act.sa_handler = manejador_SIGINT;
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    act.sa_handler = manejador_SIGTERM;
    if (sigaction(SIGTERM, &act, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    /* CREAMOS LOS SEMÁFOROS */

    if ((sem1 = sem_open(SEM1, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED) {
        perror("sem_open1");
        exit(EXIT_FAILURE);
    }
    if ((sem2 = sem_open(SEM2, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED) {
        perror("sem_open2");
        exit(EXIT_FAILURE);
    }
    if ((sem3 = sem_open(SEM3, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED) {
        perror("sem_open3");
        exit(EXIT_FAILURE);
    }

    /* CREAMOS LOS HIJOS */

    for (i=0; i<N_READ; i++) {
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
        while(got_signal_hijo == 0) {

            /* ENTRADA A ZONA CRÍTICA 1 */
            /* ENTRADA A ZONA CRÍTICA 2 */

            sem_wait(sem1);
            sem_post(sem3);
            sem_getvalue(sem3, &lectores);
            if (lectores == 1)
                sem_wait(sem2);
            sem_post(sem1);

            /* SALIDA DE ZONA CRÍTICA 1 */

            fprintf(stdout, "R-INI <%d>\n", getpid());
            sleep(1);
            fprintf(stdout, "R-FIN <%d>\n", getpid());

            /* ENTRADA A ZONA CRÍTICA 1 */

            sem_wait(sem1);
            sem_wait(sem3);
            sem_getvalue(sem3, &lectores);
            if (lectores == 0){
                sem_post(sem2);
            }
            sem_post(sem1);

            /* SALIDA DE ZONA CRÍTICA 1 */
            /* SALIDA DE ZONA CRÍTICA 2 */

            sleep(SECS);
        }
        free(cpid);
        sem_close(sem1);
        sem_close(sem2);
        sem_close(sem3);
        exit(EXIT_SUCCESS);
    }

    /* CÓDIGO DEL PADRE */

    else {
        while(got_signal_padre == 0) {

            /* ENTRADA A ZONA CRÍTICA */

            sem_wait(sem2);

            fprintf(stdout, "W-INI <%d>\n", getpid());
            sleep(1);
            fprintf(stdout, "W-FIN <%d>\n", getpid());

            sem_post(sem2);

            /* SALIDA DE ZONA CRÍTICA */

            sleep(SECS);
        }

        for (i=0; i<N_READ; i++) {
            kill(cpid[i], 15);
        }

        for (i=0; i<N_READ; i++) {
            wait(NULL);
        }

        free(cpid);
        sem_close(sem1);
        sem_close(sem2);
        sem_close(sem3);
        sem_unlink(SEM1);
        sem_unlink(SEM2);
        sem_unlink(SEM3);
        exit(EXIT_SUCCESS);
    }
}
