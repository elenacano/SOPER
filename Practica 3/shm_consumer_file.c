/**
 * @file shm_consumer_file.c
 * @author Rubén García de la Fuente, ruben.garciadelafuente@estudiante.uam.es
 * @author Elena Cano Castillejo, elena.canoc@estudiante.uam.es
 * @group 2202
 * @date 13-04-2020
 *
 * @brief
 * Este programa se encarga de abrir un un fichero común que mapeará en su
 * memoria.Consumirá N elementos del almacén que irá guardando en un array, esto
 * lo realizará hasta que consuma el elemento final controlando el acceso al
 * almacén a través de semáforos. Una vez termine imprimirá los elementos leídos
 * y el número de los mismos. A continuación terminará liberando todos los
 * recursos asociados.
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
    Almacen* almacen_struct;
    int num[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int num_leido, file;
    int i = 0;

    /* ABRIMOS LA MEMORIA */

    file = open(FILE, O_RDWR , 0);
    if (file == -1) {
        perror("shm_open");
        return 1;
    }

    almacen_struct = mmap(NULL, sizeof(*almacen_struct), PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
    if (almacen_struct == MAP_FAILED) {
        perror("mmap");
        unlink(FILE);
        return 1;
    }

    close(file);

    /* CONSUMIMOS LOS N NÚMEROS */

    while(1) {
        sem_wait(&(almacen_struct->fill));
        sem_wait(&(almacen_struct->mutex));

        num_leido = almacen_struct->stock[i%TAM_ALMACEN];
        if (num_leido == -1)
            break;
        num[num_leido]++;

        sem_post(&(almacen_struct->mutex));
        sem_post(&(almacen_struct->empty));

        i++;
    }

    for (i = 0; i < 10; i++) {
        fprintf(stdout, "%d : %d\n", i, num[i]);
    }

    /* LIBERAMOS LOS RECURSOS Y FINALIZAMOS */

    sem_destroy(&(almacen_struct->mutex));
    sem_destroy(&(almacen_struct->fill));
    sem_destroy(&(almacen_struct->empty));
    munmap(almacen_struct, sizeof(*almacen_struct));

    unlink(FILE);

    return 0;
}
