/**
 * @file ejercicio_shell.c
 * @author Rubén García de la Fuente ruben.garciadelafuente@estudiante.uam.es
 * @author Elena Cano Castillejo, elena.canoc@estudiante.uam.es
 * @group 2202
 * @date 21-02-2020
 *
 * @brief Este programa implementa una shell sencilla en la que cada comando se le pasará por
 * cada linea, la shell termina cuando se introduce EOF por pantalla.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wordexp.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char** argv) {
    int status=0;
    char* command=NULL;
    int tam;
    size_t size=0;
    wordexp_t p;

    /*En este bucle leemos los comandos*/
    while((tam=getline(&command, &size, stdin))!=EOF) {
        command[tam-1]='\0';
        if(fork()) {
            /*Comprobamos el estatus de retorno del proceso hijo*/
            wait(&status);
            if (WIFEXITED(status)) {
                fprintf(stderr, "\nExited with value %d\n\n", WEXITSTATUS(status));
            }
            if (WIFSIGNALED(status)) {
                fprintf(stderr, "\nTerminated by signal %d\n\n", WTERMSIG(status));
            }
        }
        else {
            fprintf(stdout, "\nExecuting new process...\n\n");
            /*Dividimos los argumentos del comando*/
            wordexp(command, &p, 0);
            /*Ejecutamos nuevo programa*/
            execvp(p.we_wordv[0], p.we_wordv);
            wordfree(&p);
            exit(EXIT_FAILURE);
        }
    }
    free(command);
    exit(EXIT_SUCCESS);
}
