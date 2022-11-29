/**
 * @file shm_producer_file.c
 * @author Rubén García de la Fuente, ruben.garciadelafuente@estudiante.uam.es
 * @author Elena Cano Castillejo, elena.canoc@estudiante.uam.es
 * @group 2202
 * @date 13-04-2020
 *
 * @brief
 * Este programa se encarga de abrir un un fichero común que mapeará en su
 * memoria. Producirá N elementos que irá guardando en el almacén vigilando los
 * accesos al mismo a través de semáforos. Una vez produzca todos los elementos,
 * producirá el elemento final y terminará liberando todos los recursos asociados.
 */

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <semaphore.h>
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

#define FILE "almacen.txt"
#define TAM_ALMACEN 10

typedef struct {
    sem_t mutex;
    sem_t empty;
    sem_t fill;
    int stock[TAM_ALMACEN];
} Almacen;

int main(int argc, char **argv) {
    int i, file, N, R;
    Almacen *almacen_struct;

    if (argc < 3) {
        fprintf(stdout, "Se esperaban 2 argumentos de entrada\n");
        return 1;
    }

    N = atoi(argv[1]);
    R = atoi(argv[2]);

    /* CREAMOS LA MEMORIA */

    file = open(FILE, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (file == -1) {
        perror("shm_open");
        return 1;
    }

    if (ftruncate(file, sizeof(Almacen)) == -1) {
        perror("ftruncate");
        unlink(FILE);
        return 1;
    }

    almacen_struct = mmap(NULL, sizeof(*almacen_struct), PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
    if (almacen_struct == MAP_FAILED) {
        perror("mmap");
        unlink(FILE);
        return 1;
    }

    close(file);

    /* CREAMOS LOS SEMÁFOROS */

    if (sem_init(&(almacen_struct->mutex), 1, 1) == -1) {
        perror("sem_init");
        unlink(FILE);
        return 1;
    }

    if (sem_init(&(almacen_struct->empty), 1, TAM_ALMACEN) == -1) {
        perror("sem_init");
        unlink(FILE);
        return 1;
    }

    if (sem_init(&(almacen_struct->fill), 1, 0) == -1) {
        perror("sem_init");
        unlink(FILE);
        return 1;
    }

    /* PRODUCIMOS LOS N NÚMEROS */

    if (R == 0) {
        for(i = 0; i < N; i++) {
            sem_wait(&(almacen_struct->empty));
            sem_wait(&(almacen_struct->mutex));

            almacen_struct->stock[i%TAM_ALMACEN] = rand()%10;

            sem_post(&(almacen_struct->mutex));
            sem_post(&(almacen_struct->fill));
        }
    }

    else if (R == 1) {
        for(i = 0; i < N; i++) {
            sem_wait(&(almacen_struct->empty));
            sem_wait(&(almacen_struct->mutex));

            almacen_struct->stock[i%TAM_ALMACEN] = i%10;

            sem_post(&(almacen_struct->mutex));
            sem_post(&(almacen_struct->fill));
        }
    }

    sem_wait(&(almacen_struct->empty));
    sem_wait(&(almacen_struct->mutex));

    almacen_struct->stock[i%TAM_ALMACEN] = -1;

    sem_post(&(almacen_struct->mutex));
    sem_post(&(almacen_struct->fill));

    munmap(almacen_struct, sizeof(*almacen_struct));

    return 0;
}
