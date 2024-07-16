#ifndef SIMULATED_FILE_SYSTEM_H
#define SIMULATED_FILE_SYSTEM_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

// Function declarations
int do_root(char *name, char *size);
int do_print(char *name, char *size);
int do_chdir(char *name, char *size);
int do_mkdir(char *name, char *size);
int do_rmdir(char *name, char *size);
int do_mvdir(char *name, char *size);
int do_mkfil(char *name, char *size);
int do_rmfil(char *name, char *size);
int do_mvfil(char *name, char *size);
int do_szfil(char *name, char *size);
int do_exit(char *name, char *size);

// Add any other function declarations you want to expose to the GUI

#endif // SIMULATED_FILE_SYSTEM_H