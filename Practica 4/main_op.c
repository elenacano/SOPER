/**
 * @file main.c
 * @author Rubén García de la Fuente, ruben.garciadelafuente@estudiante.uam.es
 * @author Elena Cano Castillejo, elena.canoc@estudiante.uam.es
 * @group 2202
 * @date 29-04-2020
 *
 * @brief
 * Este programa se encarga de ordenar una lista de números con una mezcla del
 * algoritmo de mergesort y bubblesort creando tantos hijos trabajadores como lo
 * inidque el parametro de entrada además de un proceso ilustrador.
 * El padre se encargará de gestionar el trabajo que deberán hacer los hijos,
 * indicándolo a través de una cola de mensajes. Terminará ordenadamente si
 * recibe la señal SIGINT.
 * Los hijos se encargarán de realizar las tareas de ordenación, además de
 * notificar su estado al proceso ilustrador cada segundo a través de una señal
 * de alarma.
 * El ilustrador se encargará de imprimir la lista de números además del estado
 * de los procesos trabajadores junto con su PID, el nivel y número de tarea que
 * están realizando (si están realizando alguna), y el incio y final de la parte.
 * Al terminar el padre enviará la señal SIGTERM a todos sus hijos liberando los
 * recursos y saliendo de forma ordenada.
 */

#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "global.h"
#include "sort.h"
#include "utils.h"


/* Constantes */
#define SEM_NAME "sem_proyecto"
#define SHM_NAME "/shm_proyecto"
#define MQ_NAME "/mq_proyecto"

#define READ 0
#define WRITE 1


/* Estructura utilizada para enviar mensajes en la cola */
typedef struct {
    int n_level;
    int n_part;
} Message;


/* Estructura utilizada para enviar mensajes en los pipes */
typedef struct {
    pid_t pid;
    Completed completed;
    int n_level;
    int n_part;
} Pipe;


/* Variables globales que serán utilizadas por otras rutinas además del main */
sem_t *sem = NULL;
int i, j, n_processes;
int flag;
char buffer[20];
char ilustracion[2048];
mqd_t queue = -1;
int fd1[MAX_PARTS][2];
int fd2[MAX_PARTS][2];
Message message;
Pipe pipemsg;
pid_t *cpid = NULL;
Sort *sort = NULL;


/**
 * Libera todos los recursos cuya memoria haya sido reservada en algún momento.
 * Método utilizado cada vez que un programa termine, ya sea en caso de error
 * o tras finalizar correctamente.
 *
 * @author Rubén García de la Fuente, ruben.garciadelafuente@estudiante.uam.es
 * @author Elena Cano Castillejo, elena.canoc@estudiante.uam.es
 * @group 2202
 * @date 29-04-2020
 */
void freeAll() {
    if (cpid != NULL)
        free(cpid);
    if (sort != NULL) {
        munmap(sort, sizeof(*sort));
        shm_unlink(SHM_NAME);
    }
    if (queue > -1) {
        mq_close(queue);
        mq_unlink(MQ_NAME);
    }
    if (sem != NULL) {
        sem_close(sem);
        sem_unlink(SEM_NAME);
    }
}


/**
 * Rutina manejadora de la señal SIGTERM. Cuando es recibida libera todos los
 * recursos asociados al proceso y termina correctamente.
 *
 * @author Rubén García de la Fuente, ruben.garciadelafuente@estudiante.uam.es
 * @author Elena Cano Castillejo, elena.canoc@estudiante.uam.es
 * @group 2202
 * @date 29-04-2020
 *
 * @param sig   Número de señal asociada a SIGTERM.
 */
void manejador_SIGTERM(int sig) {
    freeAll();
    exit(EXIT_SUCCESS);
}


/**
 * Rutina manejadora de la señal SIGUSR1. Cuando es recibida comprueba que el
 * la última de las tareas ha sido terminada. Esta información será utilizada
 * para determinar si puede o no terminar el programa.
 *
 * @author Rubén García de la Fuente, ruben.garciadelafuente@estudiante.uam.es
 * @author Elena Cano Castillejo, elena.canoc@estudiante.uam.es
 * @group 2202
 * @date 29-04-2020
 *
 * @param sig   Número de señal asociada a SIGUSR1.
 */
 void manejador_SIGUSR1(int sig) {
     if (i == sort->n_levels && j == get_number_parts(sort->n_levels-1, sort->n_levels)) {
         sem_wait(sem);
         flag = 1;
         if(sort->tasks[i - 1][j - 1].completed != COMPLETED)
            flag = 0;
         sem_post(sem);
     }
 }


/**
 * Rutina manejadora de la señal SIGINT. Cuando es recibida envía la señal
 * SIGTERM a todos sus hijos, espera a que finalicen correctamente y termina
 * liberando todos los recursos.
 *
 * @author Rubén García de la Fuente, ruben.garciadelafuente@estudiante.uam.es
 * @author Elena Cano Castillejo, elena.canoc@estudiante.uam.es
 * @group 2202
 * @date 29-04-2020
 *
 * @param sig   Número de señal asociada a SIGINT.
 */
void manejador_SIGINT(int sig) {
    for (i = 0; i < n_processes+1; i++) {
        if (kill(cpid[i], SIGTERM) == -1) {
            perror("kill");
            freeAll();
            exit(EXIT_FAILURE);
        }
    }

    for (i = 0; i < n_processes+1; i++) {
        wait(NULL);
    }

    munmap(sort, sizeof(*sort));
    freeAll();
    exit(EXIT_SUCCESS);
}


/**
 * Rutina manejadora de la señal SIGALRM. Cuando es recibida prepara el mensaje
 * que será enviado al proceso ilustrador para que este pueda realizar su
 * trabajo correctamente. Tras ello espera la confirmación del ilustrador de
 * poder continuar su ejecución y vuelve a establecer otra alarma para que se
 * ejecute al cabo de un segundo.
 *
 * @author Rubén García de la Fuente, ruben.garciadelafuente@estudiante.uam.es
 * @author Elena Cano Castillejo, elena.canoc@estudiante.uam.es
 * @group 2202
 * @date 29-04-2020
 *
 * @param sig   Número de señal asociada a SIGALRM.
 */
void manejador_SIGALRM(int sig) {

    /* Preparamos el envío del mensaje */
    pipemsg.pid = getpid();
    pipemsg.n_level = message.n_level;
    pipemsg.n_part = message.n_part;
    if (pipemsg.n_level != -1 && pipemsg.n_part != -1)
        pipemsg.completed = sort->tasks[message.n_level][message.n_part].completed;
    else
        pipemsg.completed = INCOMPLETE;

    /* Enviamos la información de estado al ilustrador a través de la pipe */
    if (write(fd1[i][WRITE], &pipemsg, sizeof(pipemsg)) == -1) {
        perror("write1");
        freeAll();
        exit(EXIT_FAILURE);
    }

    /* Esperamos confirmación de ejecución del ilustrador */
    if (read(fd2[i][READ], buffer, sizeof(buffer)) == -1) {
        perror("read");
        freeAll();
        exit(EXIT_FAILURE);
    }

    /* Establecemos la próxima alarma */
    alarm(1);
}


/**
 * Función main. Será la rutina princpal que se ejecutará al comienzo del
 * programa.
 *
 * @author Rubén García de la Fuente, ruben.garciadelafuente@estudiante.uam.es
 * @author Elena Cano Castillejo, elena.canoc@estudiante.uam.es
 * @group 2202
 * @date 29-04-2020
 *
 * @param argc  Número de argumentos de entrada del programa.
 * @param argv  Puntero a los string de los correspondientes argumentos de
 *              entrada.
 * @return  EXIT_SUCCESS si el programa ha finalizado correctamente.
 *          EXIT_FAILURE en caso contrario.
 */
int main(int argc, char **argv) {

    /* Variables locales */
    struct mq_attr attributes = {
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_curmsgs = 0,
        .mq_msgsize = sizeof(Message)
    };
    struct sigaction act;
    sigset_t set;
    sigset_t setsuspend;
    int n_levels, delay;
    int fd_shm;
    pid_t pid;
    pid_t ppid;

    /* Comprobamos los arguentos de entrada e inicializamos algunos valores */
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <FILE> <N_LEVELS> <N_PROCESSES> [<DELAY>]\n", argv[0]);
        fprintf(stderr, "    <FILE> :        Data file\n");
        fprintf(stderr, "    <N_LEVELS> :    Number of levels (1 - %d)\n", MAX_LEVELS);
        fprintf(stderr, "    <N_PROCESSES> : Number of processes (1 - %d)\n", MAX_PARTS);
        fprintf(stderr, "    [<DELAY>] :     Delay (ms)\n");
        exit(EXIT_FAILURE);
    }

    n_levels = atoi(argv[2]);
    if (n_levels > 10)
        n_levels = 10;
    n_processes = atoi(argv[3]);
    if (n_processes > 512)
        n_processes = 512;
    if (argc > 4) {
        delay = 1e6 * atoi(argv[4]);
    }
    else {
        delay = 1e8;
    }

    cpid = malloc((n_processes+1)*sizeof(pid_t));
    if (cpid == NULL) {
        perror("malloc");
        freeAll();
        exit(EXIT_FAILURE);
    }

    message.n_level = -1;
    message.n_part = -1;

    /* Creamos las tuberías */
    for (i = 0; i < n_processes; i++) {
        if (pipe(fd1[i]) == -1) {
            perror("pipe");
            freeAll();
            exit(EXIT_FAILURE);
        }
        if (pipe(fd2[i]) == -1) {
            perror("pipe");
            freeAll();
            exit(EXIT_FAILURE);
        }
    }

    /* Creamos las máscaras necesarias y los manejadores */
    sigemptyset(&(act.sa_mask));
    act.sa_flags = 0;
    act.sa_handler = manejador_SIGTERM;
    if (sigaction(SIGTERM, &act, NULL) < 0) {
        perror("sigaction");
        freeAll();
        exit(EXIT_FAILURE);
    }
    act.sa_handler = manejador_SIGUSR1;
    if (sigaction(SIGUSR1, &act, NULL) < 0) {
        perror("sigaction");
        freeAll();
        exit(EXIT_FAILURE);
    }
    act.sa_handler = manejador_SIGALRM;
    if (sigaction(SIGALRM, &act, NULL) < 0) {
        perror("sigaction");
        freeAll();
        exit(EXIT_FAILURE);
    }

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
        perror("sigprocmask");
        freeAll();
        exit(EXIT_FAILURE);
    }

    sigfillset(&setsuspend);
    sigdelset(&setsuspend, SIGUSR1);

    /* Creamos el semáforo */
    if ((sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED) {
        perror("sem_open");
        freeAll();
        exit(EXIT_FAILURE);
    }

    /* Inicializamos la memoria compartida */
    fd_shm = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd_shm == -1) {
        perror("shm_open");
        freeAll();
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd_shm, sizeof(Sort)) == -1) {
        perror("ftruncate");
        freeAll();
        exit(EXIT_FAILURE);
    }

    sort = mmap(NULL, sizeof(*sort), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);

    if (sort == MAP_FAILED) {
        perror("map failed");
        freeAll();
        exit(EXIT_FAILURE);
    }

    if (init_sort(argv[1], sort, n_levels, n_processes, delay) == ERROR) {
        perror("init_sort");
        freeAll();
        exit(EXIT_FAILURE);
    }

    /* Creamos la cola de mensajes */
    queue = mq_open(MQ_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attributes);
    if (queue == (mqd_t)-1) {
        perror("mq_open");
        freeAll();
        exit(EXIT_FAILURE);
    }

    /* Creamos los procesos trabajadores y el ilustrador */
    ppid = getpid();

    for (i = 0; i < n_processes+1; i++) {
        if ((pid = fork()) == -1) {
            perror("fork");
            freeAll();
            exit(EXIT_FAILURE);
        }
        if (!pid)
            break;
        cpid[i] = pid;
    }

    /* Código del trabajador */
    if(i < n_processes && !pid) {

        /* Ignoramos la señal SIGINT, cerramos los descriptores de fichero de
           las tuberías que no vayamos a utilizar y establecemos la primera
           alarma */
        act.sa_handler = SIG_IGN;
        if (sigaction(SIGINT, &act, NULL) < 0) {
            perror("sigaction");
            freeAll();
            exit(EXIT_FAILURE);
        }

        close(fd1[i][READ]);
        close(fd2[i][WRITE]);

        alarm(1);

        /* El bucle se ejecutará hasta la llegada de la señal SIGTERM */
        while(1) {

            /* Leemos el mensaje de la cola asegurándonos de volver a hacerlo
               si se recibe alguna interrupción de señal */
            while (mq_receive(queue, (char*)&message, sizeof(message), NULL) == -1) {
                if (errno != EINTR) {
                    perror("mq_receive");
                    freeAll();
                    exit(EXIT_FAILURE);
                }
            }

            /* Marcamos la parte como PROCESSING asegurando la exclusión mutua */
            while(sem_wait(sem) == -1 && errno == EINTR);
            sort->tasks[message.n_level][message.n_part].completed = PROCESSING;
            sem_post(sem);

            /* Resolvemos la parte asignada */
            solve_task(sort, message.n_level, message.n_part);

            /* Marcamos la parte como COMPLETED asegurando la exclusión mutua */
            while(sem_wait(sem) == -1 && errno == EINTR);
            sort->tasks[message.n_level][message.n_part].completed = COMPLETED;
            sem_post(sem);

            /* Avisamos al padre de que debe revisar si se han completado las
               tareas del nivel */
            if (kill(ppid, SIGUSR1) == -1) {
                perror("kill");
                freeAll();
                exit(EXIT_FAILURE);
            }
        }
    }

    /* Código del ilustrador */
    else if (!pid) {

        /* Ignoramos la señal SIGINT, cerramos los descriptores de fichero de
           las tuberías que no vayamos a utilizar y pintamos el vector inicial */
        act.sa_handler = SIG_IGN;
        if (sigaction(SIGINT, &act, NULL) < 0) {
            perror("sigaction");
            freeAll();
            exit(EXIT_FAILURE);
        }

        for (i = 0; i < n_processes; i++) {
            close(fd1[i][WRITE]);
            close(fd2[i][READ]);
        }

        plot_vector(sort->data, sort->n_elements);
        printf("\nStarting algorithm with %d levels and %d processes...\n", sort->n_levels, sort->n_processes);

        /* El bucle se ejecutará hasta la llegada de la señal SIGTERM */
        while(1) {
            sprintf(ilustracion, "\n     %-10s%-10s     %-10s%-10s%-10s%-10s\n\n",
                    "PID", "STATUS", "LEVEL", "PART", "INI", "END");

            /* Leemos todos los estados de los trabajadores preparando la impresión */
            for(i = 0; i < n_processes; i++) {
                if (read(fd1[i][READ], &pipemsg, sizeof(pipemsg)) == -1) {
                    perror("read");
                    freeAll();
                    exit(EXIT_FAILURE);
                }
                if (pipemsg.completed == PROCESSING)
                    sprintf(ilustracion + strlen(ilustracion), "     %-10ld%-10s     %-10d%-10d%-10d%-10d\n",
                            (long)pipemsg.pid, "PROCESSING", pipemsg.n_level, pipemsg.n_part,
                            sort->tasks[pipemsg.n_level][pipemsg.n_part].ini, sort->tasks[pipemsg.n_level][pipemsg.n_part].end);
                else {
                    if (pipemsg.completed == INCOMPLETE)
                        strncpy(buffer, "INCOMPLETE", sizeof("INCOMPLETE"));
                    else if (pipemsg.completed == COMPLETED)
                        strncpy(buffer, "COMPLETED", sizeof("COMPLETED"));
                    else if (pipemsg.completed == SENT)
                        strncpy(buffer, "SENT", sizeof("SENT"));
                    sprintf(ilustracion + strlen(ilustracion), "     %-10ld%-10s     %-10s%-10s%-10s%-10s\n",
                            (long)pipemsg.pid, buffer, "-", "-", "-", "-");
                }
            }

            /* Imprimimos el vector por pantalla junto con el estado de todos los
               trabajadores */
            plot_vector(sort->data, sort->n_elements);
            fprintf(stdout, "%s", ilustracion);
            fflush(stdout);

            /* Avisamos a todos los trabajadores de que pueden continuar con su
               ejecución */
            for(i = 0; i < n_processes; i++) {
                strncpy(buffer, "Continue", sizeof("Continue"));
                if (write(fd2[i][WRITE], buffer, strlen(buffer) + 1) == -1) {
                    perror("write2");
                    freeAll();
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

    /* Código del padre */
    else {
        /* Establecemos el manejador de la señal SIGINT */
        act.sa_handler = manejador_SIGINT;
        if (sigaction(SIGINT, &act, NULL) < 0) {
            perror("sigaction");
            freeAll();
            exit(EXIT_FAILURE);
        }

        /* Anidación de bucles que recorrerá cada nivel y, dentro del mismo,
           cada parte */
        for (i = 0; i < sort->n_levels; i++) {
            for (j = 0; j < get_number_parts(i, sort->n_levels); j++) {

                sem_wait(sem);
                if (check_task_ready(sort, i, j)) {

                    /* Marcamos una tarea como enviada asegurando la exclusión mutua
                       y la enviamos a través de la cola de mensajes */
                    sem_post(sem);

                    message.n_level = i;
                    message.n_part = j;
                    sem_wait(sem);
                    sort->tasks[i][j].completed = SENT;
                    sem_post(sem);

                    if (mq_send(queue, (char*)&message, sizeof(message), 1) == -1) {
                        perror("mq_send");
                        freeAll();
                        exit(EXIT_FAILURE);
                    }
                }

                else {
                    sem_post(sem);
                    j--;
                    sigsuspend(&setsuspend);
                }
            }
        }

        /* Suspendemos el programa a la espera de la señal SIGUSR1 tras la cual
           comprobomas se haya completado la última tarea */
        do {
            sigsuspend(&setsuspend);
        } while(flag != 1);

        /* Imprimimos el vector ordenado y finalizamos */
        plot_vector(sort->data, sort->n_elements);
        printf("\nAlgorithm completed\n");

        for (i = 0; i < n_processes+1; i++) {
            if (kill(cpid[i], SIGTERM) == -1) {
                perror("kill");
                freeAll();
                exit(EXIT_FAILURE);
            }
        }

        for (i = 0; i < n_processes+1; i++) {
            wait(NULL);
        }
    }

    freeAll();
    exit(EXIT_SUCCESS);
}
