/**
 * @file ejercicio_prottemp.c
 * @author Rubén García de la Fuente ruben.garciadelafuente@estudiante.uam.es
 * @author Elena Cano Castillejo, elena.canoc@estudiante.uam.es
 * @group 2202
 * @date 26-03-2020
 *
 * @brief Este programa crea un número de procesos que se le pasa como argumento.
 * Todos los hijos creados realizarán la suma desde 1 hasta su pid/10 y
 * escribirán en el fichero data.txt la suma más lo que había almacenado en la
 * segunda línea del mismo. Además sumarán uno a la primera línea del fichero.
 * Los hijos, enviarán la señal SIGUSR2 al padre y se suspenderán hasta que
 * reciban la señal SIGTERM.
 * El padre abrirá el fichero data.txt, creándolo si no existía anteriormente e
 * inicializará los valores a 0. A continuación llamará a la función alarm y se
 * suspenderá hasta recibir la señal SIGALRM o SIGUSR2, si recibe la segunda
 * realizará una comprobará que todos los hijos han escrito en el fichero por
 * medio de un semáforo, enviará la señal SIGTERM a todos sus hijos, y
 * finalizará liberando todos los recursos asociados.
 * El acceso al fichero se controla por medio de semáforos.
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

#define SEM1 "lectura_escritura"
#define SEM2 "contador"

static volatile sig_atomic_t got_signal = 0;

void manejador_SIGALRM(int sig) {
    got_signal = 1;
}

void manejador_SIGNAL(int sig) {
    return;
}

int main(int argc, char** argv) {
    sem_t *sem1 = NULL;
    sem_t *sem2 = NULL;
    struct sigaction act;
    sigset_t mask;
    int N;
    int T;
    pid_t pid;
    pid_t *cpid;
    int i, suma;
    FILE *f;
    int ln1, ln2;
    int semvalue;

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
    act.sa_handler = manejador_SIGALRM;
    if (sigaction(SIGALRM, &act, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    act.sa_handler = manejador_SIGNAL;
    if (sigaction(SIGUSR2, &act, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &act, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }


    /* CREAMOS LOS SEMÁFOROS */

    if ((sem1 = sem_open(SEM1, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED) {
        perror("sem_open1");
        exit(EXIT_FAILURE);
    }
    if ((sem2 = sem_open(SEM2, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED) {
        perror("sem_open2");
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

        /* ENTRADA A ZONA CRÍTICA */

        sem_wait(sem1);

        if ((f = fopen("data.txt", "r+")) == NULL) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
        fscanf(f, "%d\n%d", &ln1, &ln2);
        ln1++;
        ln2+=suma;
        fseek(f, 0, SEEK_SET);
        fprintf(f, "%d\n%d", ln1, ln2);
        fclose(f);

        sem_post(sem2);
        sem_post(sem1);

        /* SALIDA DE ZONA CRÍTICA */

        if (kill(getppid(), 12) == -1) {
            perror("kill");
            exit(EXIT_FAILURE);
        }
        sigsuspend(&mask);

        free(cpid);
        sem_close(sem1);
        sem_close(sem2);
        exit(EXIT_SUCCESS);
    }

    /* CÓDIGO DEL PADRE */

    else {
        sigfillset(&mask);
        sigdelset(&mask, SIGALRM);
        sigdelset(&mask, SIGUSR2);

        if ((f = fopen("data.txt", "w+")) == NULL) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
        fprintf(f, "%d\n%d", 0, 0);
        fclose(f);

        alarm(T);

        sem_post(sem1);

        sem_unlink(SEM1);
        sem_unlink(SEM2);

        /* SUSPENDEMOS HASTA QUE SE RECIBA SIGALRM O SIGUSR2 */
        /* SI SE RECIBE SIGALRM SALIMOS DEL BUCLE Y TERMINAMOS CON LOS PROCESOS
           HIJOS */
        /* SI SE RECIBE SIGUSR2 COMPROBAMOS QUE TODOS LOS HIJOS HAYAN ESCRITO EN
           EL FICHERO */

        do {
            sigsuspend(&mask);
            if (got_signal == 1)
                break;

            /* ENTRADA A ZONA CRÍTICA */

            sem_wait(sem1);

            sem_getvalue(sem2, &semvalue);

            sem_post(sem1);

            /* SALIDA DE ZONA CRÍTICA */

        } while(semvalue != N);

        if (got_signal == 1)
            fprintf(stdout, "Falta trabajo\n");
        else {
            if ((f = fopen("data.txt", "r")) == NULL) {
                perror("fopen");
                exit(EXIT_FAILURE);
            }
            fscanf(f, "%d\n%d", &ln1, &ln2);
            fclose(f);
            fprintf(stdout, "Han acabado todos, resultado: %d\n", ln2);
        }

        for (i=0; i<N; i++) {
            if (kill(cpid[i], 15) == -1) {
                perror("kill");
                exit(EXIT_FAILURE);
            }
        }

        for (i=0; i<N; i++) {
            wait(NULL);
        }

        free(cpid);
        sem_close(sem1);
        sem_close(sem2);
        exit(EXIT_SUCCESS);
    }
}
