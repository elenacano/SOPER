/**
 * @file mq_workers_pool_timed.c
 * @author Rubén García de la Fuente, ruben.garciadelafuente@estudiante.uam.es
 * @author Elena Cano Castillejo, elena.canoc@estudiante.uam.es
 * @group 2202
 * @date 13-04-2020
 *
 * @brief
 * Este programa se encarga de crear tantos hijos como lo indique dicho
 * parámetro y abrir una cola de mensajes cuyo nombre también se le pasará
 * como argumento. Los hijos se encargarán de solicitar mensajes de dicha cola
 * los cuales almacenarán en un buffer del cual contarán cuantas veces aparece
 * el caracter pasado como argumento. Si tras 100ms no han recibido el mensaje
 * que estaban esperando imprimirán que no se les necesita y finalizarán. Una
 * vez lean el mensaje de finalización enviarán la señal SIGUSR2 al padre.
 * Al recibirla enviará una señal SIGTERM a todos sus hijos los cuales
 * ejecutarán el manejador e imprimiran sus datos por pantalla.
 */

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <mqueue.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define TAM_MSG 2048

int cuentamensaje, cuentacaracter;
char caracter;
mqd_t queue;
pid_t *cpid;
char *buffer;
char *name;

void manejador_SIGUSR2(int sig) {
    return;
}

void manejador_SIGTERM(int sig) {
    fprintf(stdout, "<%ld>: %d mensajes, %d contador\n", (long)getpid(), cuentamensaje, cuentacaracter);
    fflush(stdout);
    free(buffer);
    free(cpid);
    mq_close(queue);
    mq_unlink(name);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    struct mq_attr attributes = {
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_msgsize = TAM_MSG,
        .mq_curmsgs = 0
    };
    struct timespec time;
    struct sigaction act;
    sigset_t set;
    sigset_t setsuspend;
    pid_t pid;
    pid_t ppid;
    int i, N;

    time.tv_nsec = 100000000;

    buffer = malloc(TAM_MSG);
    if (buffer == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    if (argc < 4) {
        fprintf(stdout, "Se esperaban 3 argumentos de entrada\n");
        free(buffer);
        exit(EXIT_FAILURE);
    }

    N = atoi(argv[1]);
    name = argv[2];
    caracter = argv[3][0];

    cpid = malloc(N*sizeof(pid_t));
    if (cpid == NULL) {
        perror("malloc");
        free(buffer);
        exit(EXIT_FAILURE);
    }

    /* DEFINIMOS LAS MÁSCARAS Y EL MANEJADOR */

    sigemptyset(&(act.sa_mask));
    act.sa_flags = 0;
    act.sa_handler = manejador_SIGUSR2;
    if (sigaction(SIGUSR2, &act, NULL) < 0) {
        perror("sigaction");
        free(buffer);
        free(cpid);
        exit(EXIT_FAILURE);
    }
    act.sa_handler = manejador_SIGTERM;
    if (sigaction(SIGTERM, &act, NULL) < 0) {
        perror("sigaction");
        free(buffer);
        free(cpid);
        exit(EXIT_FAILURE);
    }

    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);
    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
        perror("sigprocmask");
        free(buffer);
        free(cpid);
        exit(EXIT_FAILURE);
    }

    sigfillset(&setsuspend);
    sigdelset(&setsuspend, SIGUSR2);

    /*ABRIMOS LA COLA DE MENSAJES*/

    queue = mq_open(name, O_CREAT | O_RDONLY , S_IRUSR | S_IWUSR, &attributes);
    if(queue == (mqd_t)-1) {
        perror("mq_open");
        free(buffer);
        free(cpid);
        exit(EXIT_FAILURE);
    }

    /* CREAMOS LOS HIJOS */

    ppid = getpid();

    for(i = 0; i < N; i++) {
        if ((pid = fork()) == -1) {
            perror("fork");
            free(buffer);
            free(cpid);
            exit(EXIT_FAILURE);
        }
        if (!pid)
            break;
        cpid[i] = pid;
    }

    /* CÓDIGO DEL HIJO */

    if(!pid) {
        cuentacaracter = 0;
        cuentamensaje = 0;

        /* SOLICITUD DE MENSAJE */

        while(1) {
            if (mq_timedreceive(queue, buffer, TAM_MSG, NULL, &time) == -1) {
                if (errno == ETIMEDOUT) {
                    fprintf(stdout, "<%ld>: No se me necesita\n", (long)getpid());
                    fflush(stdout);
                    free(buffer);
                    free(cpid);
                    mq_close(queue);
                    mq_unlink(name);
                    exit(EXIT_SUCCESS);
                }
                perror("mq_receive");
                free(buffer);
                free(cpid);
                mq_close(queue);
                mq_unlink(name);
                exit(EXIT_FAILURE);
            }

            cuentamensaje++;

            if(buffer[0] != '\r') {
                for (i = 0; i < strlen(buffer); i++) {
                    if(buffer[i] == caracter)
                        cuentacaracter++;
                }
            }

            else {
                if (kill(ppid, SIGUSR2) == -1) {
                    perror("kill");
                    free(buffer);
                    free(cpid);
                    mq_close(queue);
                    mq_unlink(name);
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

    /* CÓDIGO DEL PADRE */

    else {
        sigsuspend(&setsuspend);

        for (i=0; i<N; i++) {
            if (kill(cpid[i], SIGTERM) == -1) {
                perror("kill");
                free(buffer);
                free(cpid);
                mq_close(queue);
                mq_unlink(name);
                exit(EXIT_FAILURE);
            }
        }
        for (i=0; i<N; i++) {
            wait(NULL);
        }
    }

    exit(EXIT_SUCCESS);
}
