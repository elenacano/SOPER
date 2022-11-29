/**
 * @file shm_concurrence_solved.c
 * @author Rubén García de la Fuente, ruben.garciadelafuente@estudiante.uam.es
 * @author Elena Cano Castillejo, elena.canoc@estudiante.uam.es
 * @group 2202
 * @date 13-04-2020
 *
 * @brief
 * En este programa el proceso padre se encarga de reservar un fragmento de
 * memoria compartida, truncarlo para poder almacenar la estructura deseada y
 * enlazarlo con esta. Después generará tantos hijos como se le pase como
 * argumento. Cada hijo repetira un bucle m veces, que es el segundo argumento
 * del programa. Dormirá un tiempo aleatrio e irá rellenando los datos de la
 * estructura. Cuando acabe el bucle enviará la señal SIGUSR1 al padre y
 * finalizará. Cuando el proceso padre reciba la señal SIGUSR1 leerá de la
 * zona de memoria compartida e imprimirá su contenido. Cada vez que un
 * proceso, ya sea padre o cualquiera de los hijos, quiera acceder a la
 * estructura para leer o escribir un dato, tendrá que hacer un down de un
 * semáforo. De esta forma nos aseguraremos de que solo un proceso este
 * accediendo a la zona crítica para que se pueda dar asi la concurrencia
 * deseada para que el programa funcione correctamente.
 */

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <semaphore.h>
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

#define SHM_NAME "/shm_eje3"
#define MAX_MSG 2000

static void getMilClock(char *buf) {
    int millisec;
	char aux[100];
    struct tm* tm_info;
    struct timeval tv;

    gettimeofday(&tv, NULL);
	millisec = lrint(tv.tv_usec/1000.0); // Round to nearest millisec
    if (millisec>=1000) { // Allow for rounding up to nearest second
        millisec -=1000;
        tv.tv_sec++;
    }
    tm_info = localtime(&tv.tv_sec);
    strftime(aux, 10, "%H:%M:%S", tm_info);
	sprintf(buf, "%s.%03d", aux, millisec);
}

typedef struct {
	pid_t processid;       /* Logger process PID */
	long logid;            /* Id of current log line */
	char logtext[MAX_MSG]; /* Log text */
    sem_t sem;
} ClientLog;

ClientLog *shm_struct;

void manejador (int sig) {
	if (sig == SIGUSR1) {
        sem_wait(&(shm_struct->sem));
		printf ("Log %ld: Pid %ld: %s\n",shm_struct->logid, (long)shm_struct->processid, shm_struct->logtext);
        sem_post(&(shm_struct->sem));
    }
}

int main(int argc, char *argv[]) {
    struct sigaction act;
    sigset_t set;
    sigset_t setsuspend;
    pid_t pid, ppid;
    char message[MAX_MSG];
    int i, fd_shm;
	int n, m;
	int ret = EXIT_FAILURE;

	if (argc < 3) {
		fprintf(stderr,"usage: %s <n_process> <n_logs> \n",argv[0]);
		return ret;
	}

	n = atoi(argv[1]);
	m = atoi(argv[2]);

    ppid = getpid();

    /* DEFINIMOS LAS MÁSCARAS Y EL MANEJADOR */

    sigemptyset(&(act.sa_mask));
    act.sa_flags = 0;
    act.sa_handler = manejador;
    if (sigaction(SIGUSR1, &act, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }

    sigfillset(&setsuspend);
    sigdelset(&setsuspend, SIGUSR1);

    /* CREAMOS LA MEMORIA */

    fd_shm = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd_shm == -1) {
        fprintf(stderr, "Error creating the shared memory segment\n");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd_shm, sizeof(ClientLog)) == -1) {
        fprintf(stderr, "Error resizing the shared memory segment\n");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    shm_struct = mmap(NULL, sizeof(*shm_struct), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
    if (shm_struct == MAP_FAILED) {
        fprintf(stderr, "Error mapping the shared memory segment\n");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    close(fd_shm);

    /* CREAMOS EL SEMAFORO */

    if (sem_init(&(shm_struct->sem), 1, 0) == -1) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    /* CREAMOS LOS HIJOS */

    for(i = 0; i < n; i++) {
        if ((pid = fork()) == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (!pid)
            break;
    }

    /* CÓDIGO DEL HIJO */

    if(pid == 0) {
        for(i = 0; i < m; i++) {
            usleep((rand()%801+100)*1000);

            sem_wait(&(shm_struct->sem));

            shm_struct->processid = getpid();
            shm_struct->logid++;
            sprintf(message, "Soy el proceso %ld a las ", (long)getpid());
            getMilClock(message+strlen(message));
            memcpy(shm_struct->logtext, message, sizeof(message));

            sem_post(&(shm_struct->sem));

            if (kill(ppid, SIGUSR1) == -1) {
                perror("kill");
                exit(EXIT_FAILURE);
            }
        }

        munmap(shm_struct, sizeof(*shm_struct));

        exit(EXIT_SUCCESS);
    }

    /* CÓDIGO DEL PADRE */

    else {
        shm_struct->logid = -1;

        sem_post(&(shm_struct->sem));

        do {
            sigsuspend(&setsuspend);
        } while(shm_struct->logid < n*m - 1);

        for(i = 0; i < n; i++) {
            wait(NULL);
        }
        sem_destroy(&(shm_struct->sem));

        munmap(shm_struct, sizeof(*shm_struct));
        shm_unlink(SHM_NAME);

        exit(EXIT_SUCCESS);
    }

	return ret;
}
