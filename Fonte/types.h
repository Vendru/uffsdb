#pragma once

#include "Utility.h"
#include "int.h"
#include <stdint.h>
#include "macros.h"

#define FTYPES 1 // flag para identificar se types.h já foi incluída

struct fs_objects { // Estrutura usada para carregar fs_objects.dat
    char nome[TAMANHO_NOME_TABELA];      // Nome da tabela.
    int cod;                             // Código da tabela.
    char nArquivo[TAMANHO_NOME_ARQUIVO]; // Nome do arquivo onde estão armazenados os dados da tabela.
    int qtdCampos;                       // Quantidade de campos da tabela.
    int qtdIndice;						 // Quantidade de índices da tabela.
    int16_t lastBuffer;
};

typedef struct tp_table{ // Estrutura usada para carregar fs_schema.dat
    int id;                       // Código da tabela.                   2bytes
    char nome[TAMANHO_NOME_CAMPO];  // Nome do Campo.                    40bytes
    char tipo;                      // Tipo do Campo.                     1bytes
    int tam;                        // Tamanho do Campo.                  4bytes
    int chave;                      // Tipo da chave                      4bytes
    char tabelaApt[TAMANHO_NOME_TABELA]; //Nome da Tabela Apontada        20bytes
    char attApt[TAMANHO_NOME_CAMPO];    //Nome do Atributo Apontado       40bytes
    struct tp_table *next;          // Encadeamento para o próximo campo.
}tp_table;

typedef struct column{ // Estrutura utilizada para inserir em uma tabela, excluir uma tupla e retornar valores de uma página.
    char tipoCampo;                     // Tipo do Campo.
    char nomeCampo[TAMANHO_NOME_CAMPO]; //Nome do Campo.
    char *valorCampo;                   // Valor do Campo.
    struct column *next;                // Encadeamento para o próximo campo.
}column;

typedef struct tupla {
    unsigned int offset;
    uint ncols; // Número de colunas na tupla.
    uint bufferPage; // Página do buffer onde a tupla está armazenada.
    column *column;
}tupla;

typedef struct {
    tupla *tuplas;
    int nrec;
} PageResult;

typedef struct table{ // Estrutura utilizada para criar uma tabela.
    char nome[TAMANHO_NOME_TABELA]; // Nome da tabela.
    tp_table *esquema;              // Esquema de campos da tabela.
}table;

/* tp_buffer: conteúdo de uma PÁGINA do Buffer Pool.
   Os três primeiros campos (id, nrec, position) formam o CABEÇALHO DA PÁGINA e são
   persistidos em disco junto com os bytes de data[]. Os campos db/pc são apenas de
   memória (não vão para o disco). Toda E/S de disco desta página é feita pelo BM. */
typedef struct tp_buffer{ // Estrutura utilizada para armazenar uma página do buffer.
    unsigned int id;         // Número da página no arquivo da tabela [0,1,2,...[ (header)
    unsigned int nrec;       // Número de registros armazenados na página.        (header)
    uint32_t position;       // Offset livre na página (bytes já ocupados).       (header)
    unsigned char db;        // Dirty bit (apenas memória).
    unsigned char pc;        // Pin counter (apenas memória).
    char data[BM_PAGE_CAPACITY]; // Dados da página (usa-se até page_size bytes).
}tp_buffer;

/* PAGE_HEADER_DISK_SIZE: bytes do cabeçalho da página gravados em disco
   (id + nrec + position). O bloco em disco = cabeçalho + page_size bytes de dados. */
#define PAGE_HEADER_DISK_SIZE (3 * sizeof(uint32_t))

/* BufferFrame: MOLDURA do Buffer Pool. Guarda a página carregada mais os
   metadados de gerência mantidos em memória pelo Buffer Manager. */
typedef struct BufferFrame {
    tp_buffer     page;                       // Página carregada nesta moldura.
    char          filename[LEN_DB_NAME_IO];   // Arquivo de origem ("" = moldura livre).
    int           page_id;                    // Página do arquivo (-1 = moldura livre).
    unsigned char valid;                      // 1 se a moldura está ocupada.
    unsigned char dirty;                      // 1 se a página foi modificada (precisa gravar).
    int           pin;                        // Contador de pinos (página em uso).
} BufferFrame;

/* BufferPoolHeader: CABEÇALHO/metadados do Buffer Pool. */
typedef struct BufferPoolHeader {
    uint32_t num_frames;   // Número de páginas (frames) do pool   [configurável].
    uint32_t page_size;    // Tamanho de cada página em bytes      [configurável].
    uint32_t frames_used;  // Quantidade de molduras ocupadas no momento.
    uint64_t hits;         // Acessos atendidos pelo pool (sem ir ao disco).
    uint64_t misses;       // Acessos que exigiram leitura de disco.
    uint64_t reads;        // Operações de fread realizadas.
    uint64_t writes;       // Operações de fwrite realizadas.
    uint64_t evictions;    // Substituições (páginas removidas do pool).
} BufferPoolHeader;

/* BufferManager: o Gerenciador de Buffer (instância global única). */
typedef struct BufferManager {
    BufferPoolHeader header;       // Metadados do Buffer Pool.
    BufferFrame     *frames;       // Vetor de molduras (o Buffer Pool em si).
    unsigned char    initialized;  // 1 após bm_init().
} BufferManager;

typedef struct rc_insert {
    char    *objName;           // Nome do objeto (tabela, banco de dados, etc...)
    char   **columnName;        // Colunas da tabela
    char   **values;            // Valores da inserção ou tamanho das strings na criação
    int      N;                 // Número de colunas de valores
    char    *type;              // Tipo do dado da inserção ou criação de tabela
    int     *attribute;         // Utilizado na criação (NPK, PK,FK)
    char   **fkTable;           // Recebe o nome da tabela FK
    char   **fkColumn;          // Recebe o nome da coluna FK
}rc_insert;

typedef struct inf_where{
  int id;
  void *token;
}inf_where;

typedef struct  inf_query {
  char *tabela;     // Nome da tabela
  int tamTokens;    // Número de tokens na cláusula WHERE
  Lista *tok;       // Lista de condições WHERE
  Lista *proj;      // Colunas recuperadas (NULL para DELETE)
  Lista *values;    // Lista de valores para UPDATE
  char queryType;   // 'S' for SELECT, 'D' for DELETE, 'U' for UPDATE
} inf_query;

typedef struct rc_parser {
    int         mode;           // Modo de operação (definido em /interface/parser.h)
    int         parentesis;     // Contador de parenteses abertos
    int         step;           // Passo atual (token)
    int         noerror;        // Nenhum erro encontrado na identificação dos tokens
    int         col_count;      // Contador de colunas
    int         val_count;      // Contador de valores
    int         consoleFlag;   // Auxiliar para não imprimir duas vezes nome=#
}rc_parser;

typedef struct data_base{
	char 		valid;
	char 		db_name[LEN_DB_NAME_IO];
	char 		db_directory[LEN_DB_NAME_IO];
}data_base;

typedef struct db_connected {
	char db_directory[LEN_DB_NAME_IO];
    char *db_name;
    int conn_active;
}db_connected;

// Sessão para fs do sistema 

typedef struct fs_constraint {
    uint tableId; // ID da tabela
    uint idFKtable; //  ID da Tabela referenciada
    char constraintName[TAMANHO_NOME_CONSTRAINT]; // Nome da restrição
    char columnName[TAMANHO_NOME_CAMPO]; // Nome da coluna
    char columnApt[TAMANHO_NOME_CAMPO]; // Nome da coluna referenciada
    byte constraintType; // Tipo de restrição (PK, FK, NPK)
    byte deltype; // Tipo de deleção (CASCADE, SET NULL, RESTRICT)
} fs_constraint;

typedef enum {
   TEMPORARY,
   PERMANENT
} MemoryContextType;

typedef struct MemoryContext {
   MemoryContextType type;
   uint used;
   struct MemoryContext *next;
    char memoryPool[MEMORY_CONTEXT_SIZE];
} MemoryContext;

typedef struct MemoryContextRoot {
   struct MemoryContext *temporary, *permanent;
} MemoryContextRoot;


typedef struct {
    size_t size;
    char data[FLEXIBLE_ARRAY];
} uffs_mem_header;

// Union's utilizados na conversão de variáveis do tipo inteiro e double.

union c_double{

    double dnum;
    char double_cnum[sizeof(double)];
};

union c_int{

    int  num;
    char cnum[sizeof(int)];
};

/************************************************************************************************
**************************************  VARIAVEIS GLOBAIS  **************************************/

extern db_connected connected;
extern BufferManager bufferManager; // Gerenciador de Buffer global (ver buffer.c).

/************************************************************************************************
 ************************************************************************************************/
