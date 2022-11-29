/**
 * @file ejercicio_hilos.c
 * @author Rubén García de la Fuente ruben.garciadelafuente@estudiante.uam.es
 * @author Elena Cano Castillejo, elena.canoc@estudiante.uam.es
 * @group 2202
 * @date 21-02-2020
 *
 * @brief Este programa crea un número de hilos que se le pasa como argumento, cada hilo
 * esperará durante el tiempo indicado por el número aleatorio y, a continuación realizara el
 * cálculo de x^3, siendo x el núemero de hilo creado. El programa imprimirá por pantalla los
 * resultados devueltos por los hilos.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>


/**
 * @brief NewStruct
 *
 * Esta estructura contiene dos variables de tipo int, aleatnum es el número generado aleatoriamente
 * y x es el número de hilo creado.
 */
typedef struct NewStruct {
    int aleatnum;
    int x;
} NewStruct;

/**
 * @brief Esta función es la que ejecutará cada uno de los hilos tras su creación.
 * funcion espera el número de segundos que tiene la variable aleatnum de NewStruct, reserva memoria
 * dinámica para introducir el cálculo de x^3, imprime el resultado y termina.
 *
 * @param arg, puntero void al argumento de entrada, que en este caso será una esctructura NewStruct
 */
void* funcion(void *arg) {
     int* resultado;

     NewStruct* calculo=arg;
     sleep(calculo->aleatnum);
     resultado=malloc(sizeof(int));
     *resultado=(calculo->x)*(calculo->x)*(calculo->x);

     fprintf(stdout, "%d ", *resultado);

     free(resultado);

     return NULL;
 }

int main(int argc, char** argv) {
    int error;
    int i;
    int param;
    NewStruct* arg;
    pthread_t* h;

    srand(time(NULL));

    if(argv[1]==NULL) {
        fprintf(stdout, "Se esperaba al menos un argumento de entrada\n");
        exit(EXIT_FAILURE);
    }

    param=atoi(argv[1]);

    /*Creamos tantas estructuras como número indque el argumento de entrada*/
    arg=(NewStruct*)malloc(param*sizeof(NewStruct));
    if (arg==NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    /*Creamos tantas hilos como número indque el argumento de entrada*/
    h=(pthread_t*)malloc(param*sizeof(pthread_t));
    if (h==NULL) {
        perror("malloc");
        free(arg);
        exit(EXIT_FAILURE);
    }

    /*Creamos los hilos*/
    for(i=0; i<param; i++) {
        arg[i].aleatnum=rand()%11;
        arg[i].x=i+1;
        error = pthread_create(h+i, NULL, funcion, &arg[i]);
        if(error != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    /*Esperamos a los hilos*/
    for(i=0; i<param; i++) {
        error = pthread_join(*(h+i), NULL);
        if(error != 0) {
            perror("pthread_join");
            free(arg);
            exit(EXIT_FAILURE);
        }
    }

    /*Liberamos memoria*/
    free(arg);
    free(h);
    exit(EXIT_SUCCESS);
}
