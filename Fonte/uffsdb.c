#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifndef FMACROS
   #include "macros.h"
#endif
#ifndef FTYPES
   #include "types.h"
#endif
#ifndef FMISC
   #include "misc.h"
#endif
#ifndef FDICTIONARY
   #include "dictionary.h"
#endif
#ifndef FSQLCOMMANDS
   #include "sqlcommands.h"
#endif
#ifndef FDATABASE
   #include "database.h"
#endif
// ALTERAÇÃO **
#ifndef FBUFFER
   #include "buffer.h"
#endif
// ALTERAÇÃO **
#include "interface/y.tab.h"


db_connected connected;

int main(){
    dbInit(NULL);
    // ALTERAÇÃO **
    // Número de frames do Buffer Pool configurável na carga do SGBD via UFFSDB_BM_PAGES.
    int bmPages = BM_DEFAULT_PAGES;
    char *envPages = getenv("UFFSDB_BM_PAGES");
    if (envPages != NULL) {
        int v = atoi(envPages);
        if (v > 0) bmPages = v;
    }
    initBufferManager(bmPages); // inicia o BM
    // ALTERAÇÃO **

    printf("uffsdb (16.2).\nType \"help\" for help or \"implement\" for seeing what is or not is implemented in this project.\n\n");

    DEBUG_PRINT("UFFS DB Debugging mode.");

    interface();

    // ALTERAÇÃO **
    shutdownBufferManager(); // libera o BM ao encerrar
    // ALTERAÇÃO **
    return 0;
}
