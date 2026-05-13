#define _USE_MATH_DEFINES  // Obligatorio para activar constantes como M_PI en ciertos compiladores
#include <stdio.h>
#include <math.h>          // Contiene cos(), sin(), tan(), etc.
#include <stdlib.h> // Requerido para rand() y srand()
#include <time.h>   // Requerido para time()

// Definición manual de Phi (proporción áurea) ya que no viene por defecto
#define PHI 1.618033988749895

// Función que recibe un entero y devuelve el valor de paridad
int operador(int n);

int main(){
    srand(time(NULL)); // inicializa la semilla para la generación de números pseudoaleatorios
    int nList[] = {1,2,3,4,5,6,7}; // lista de números
    int n = nList[rand(7)%size(nList)]; // selecciona un número aleatorio de la lista
    const float PI = 3.1415; //constante pi
    double resultado = n*PI; //retorna el valor de n multiplicado por PI
    double paridad_cos = cos(resultado);
    double cuasiperiodo=paridad_cos * PHI; 
    printf("[DEBUG] Se eligió el número n = %d del arreglo\n", n);
    printf("[DEBUG] El resultado es n*PI = %f\n", resultado);
}