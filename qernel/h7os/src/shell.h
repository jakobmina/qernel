/*
 * shell.h — shell de H7-OS sobre el almacén Z7
 */
#ifndef SHELL_H
#define SHELL_H

#include <stdbool.h>

extern bool shell_vgadump;               /* cmdline "vgadump": copiar la VGA al serial al salir */

void shell_seed(void);                   /* siembra los 14 n de prueba y corre la autoprueba */
void shell_exec(char *line);
void shell_run_script(const char *s);    /* comandos separados por ';' (cmdline run=...) */
void shell_loop(void) __attribute__((noreturn));
void label(const char *s);

#endif
