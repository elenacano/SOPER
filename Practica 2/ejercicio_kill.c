/**
 * @file ejercicio_kill.c
 * @author Rubén García de la Fuente ruben.garciadelafuente@estudiante.uam.es
 * @author Elena Cano Castillejo, elena.canoc@estudiante.uam.es
 * @group 2202
 * @date 26-03-2020
 *
 * @brief Este programa envía la señal que se pasa como primer argumento al
 * proceso que se pasa como segundo argumento.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

int main(int argc, char** argv) {
    pid_t pid;
    int sig;

    if (argc < 3) {
        fprintf(stdout, "Se esperaban 2 argumentos de entrada\n");
        return 1;
    }

    sig = atoi(argv[1]+1);
    pid = atoi(argv[2]);

    if (kill(pid, sig) == -1) {
        perror("kill");
        return 1;
    }

    return 0;
}
