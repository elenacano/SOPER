/**
 * @file mq_injector.c
 * @author Rubén García de la Fuente, ruben.garciadelafuente@estudiante.uam.es
 * @author Elena Cano Castillejo, elena.canoc@estudiante.uam.es
 * @group 2202
 * @date 13-04-2020
 *
 * @brief
 * Este programa se encarga de abrir un fichero y una cola de mensajes
 * cuyos nombres se le pasan como argumento. Irá leyendo el fichero,
 * almacenádolo en el buffer y solicitando enviar dicho buffer mediante a
 * cola de mensajes. Cuando haya acabao de leer todo el fichero cerrará y
 * liberara correctamente todos los recursos.
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
#include <unistd.h>

#define TAM_MSG 2048

int main(int argc, char **argv) {
    struct mq_attr attributes = {
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_msgsize = TAM_MSG,
        .mq_curmsgs = 0
    };
    int file;
    ssize_t ret;
    mqd_t queue;
    char *buffer;

    buffer = malloc(TAM_MSG);
    if (buffer == NULL) {
        perror("malloc");
        return 1;
    }

    if (argc < 3) {
        fprintf(stdout, "Se esperaban 2 argumentos de entrada\n");
        free(buffer);
        return 1;
    }

    /* ABRIMOS EL FICHERO */

    file = open(argv[1], O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (file == -1) {
        perror("open");
        free(buffer);
        return 1;
    }

    /* CREAMOS LA COLA DE MENSAJES */

    queue = mq_open(argv[2], O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR, &attributes);
    if (queue == (mqd_t)-1) {
        perror("mq_open");
        free(buffer);
        close(file);
        return 1;
    }

    /* LEEMOS DEL FICHERO Y ENVIAMOS LOS MENAJES */

    do {
        ret = read(file, buffer, TAM_MSG);
        if (ret == -1) {
            perror("read");
            free(buffer);
            close(file);
            mq_close(queue);
            mq_unlink(argv[2]);
            return 1;
        }

        if (mq_send(queue, buffer, ret, 1) == -1) {
            perror("mq_send");
            free(buffer);
            close(file);
            mq_close(queue);
            mq_unlink(argv[2]);
            return 1;
        }
    } while(ret != 0);

    /* ENVIAMOS EL MENSAJE DE FINALIZACIÓN Y TERMINAMOS */

    if (mq_send(queue, "\r", sizeof("\r"), 1) == -1) {
        perror("mq_send");
        free(buffer);
        close(file);
        mq_close(queue);
        mq_unlink(argv[2]);
        return 1;
    }

    free(buffer);
    close(file);
    mq_close(queue);
    mq_unlink(argv[2]);

    return 0;
}
