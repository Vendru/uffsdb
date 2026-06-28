#define FBUFFER 1 // flag controlar os includes

#ifndef FMACROS // garante que macros.h não seja reincluída
   #include "macros.h"
#endif
//
#ifndef FTYPES // garante que types.h não seja reincluída
  #include "types.h"
#endif

/* ==============================================================================
                            BUFFER MANAGER (BM)

   O Buffer Manager é o ÚNICO componente que faz E/S de disco (fread/fwrite) nos
   arquivos de dados das tabelas. A aplicação sempre trabalha sobre as páginas que
   estão no Buffer Pool (vetor de molduras em memória). Em caso de falta (miss), o
   BM carrega a página do disco; quando o pool está cheio, ele escolhe uma moldura
   ALEATÓRIA (não-pinada) para substituir, gravando-a antes se estiver suja.

   Tamanho da página (page_size) e número de páginas do pool (num_frames) são
   configuráveis na carga do SGBD pelo arquivo data/uffsdb.conf.
   ============================================================================== */

/* Inicializa o Buffer Manager: lê a configuração (uffsdb.conf), aloca o Buffer
   Pool e registra o flush automático na saída. Deve ser chamada na carga do SGBD. */
void bm_init(void);

/* Encerra o Buffer Manager: grava no disco todas as páginas sujas e libera o pool.
   Idempotente (segura para ser chamada mais de uma vez). */
void bm_shutdown(void);

/* Recupera a página 'page_id' do arquivo 'filename' já carregada no Buffer Pool e
   PINADA (em uso). Carrega do disco apenas em caso de falta. Retorna NULL se a
   página não existir em disco ou se o pool estiver totalmente pinado. */
tp_buffer *bm_get_page(const char *filename, int page_id);

/* Garante no pool uma página NOVA e vazia para (filename, page_id), PINADA e já
   marcada como suja. Usada para estender o arquivo com uma nova página. */
tp_buffer *bm_new_page(const char *filename, int page_id);

/* Libera um pino da página (filename, page_id). Se 'dirty' != 0, marca a página
   como modificada (será gravada na substituição ou no flush). */
void bm_unpin(const char *filename, int page_id, int dirty);

/* Marca a página (filename, page_id) como suja sem alterar o pino. */
void bm_mark_dirty(const char *filename, int page_id);

/* Grava em disco todas as páginas sujas que estão no pool. */
void bm_flush_all(void);

/* Imprime os metadados/estatísticas do Buffer Pool (comando \b da interface). */
void bm_print_stats(void);

/* ============================== FUNÇÕES DE PÁGINA ============================== */

/*
    Esta função imprime todos os dados carregados numa determinada página do buffer
    *buffer - Estrutura para armazenar tuplas na memória
    *s - Estrutura que armazena esquema da tabela para ler os dados do buffer
    *objeto - Estrutura que armazena dados sobre a tabela que está no buffer
    *num_page - Número da página a ser impressa
*/
int printbufferpoll(tp_buffer *buffpoll, tp_table *s,struct fs_objects objeto, int num_page);
/*
    Esta função insere uma tupla em uma página do buffer em que haja espaço suficiente.
    Retorna ERRO_BUFFER_CHEIO caso não haja espeço para a tupla

    *buffer - Estrutura para armazenar tuplas na meméria
    *from   - Número da tupla a ser posta no buffer. Este número é relativo a ordem de inserção da
              tupla na tabela em disco.
    *campos - Estrutura que armazena esquema da tabela para ler os dados do buffer
    *objeto - Estrutura que armazena dados sobre a tabela que está no buffer
*/
int colocaTuplaBuffer(tp_buffer *buffer, int from, tp_table *campos, struct fs_objects objeto);

/*
    Recupera (via BM) a página de número 'id' do arquivo 'filename'. A página fica
    carregada no Buffer Pool e PINADA. Retorna NULL em caso de erro.
*/
tp_buffer *getBlock(unsigned int id, char* filename);

/*
    Retorna (via BM) uma página NOVA, vazia e pinada com o identificador 'id'.
    O nome do arquivo é resolvido pelo chamador (insert) através de getBlock/bm_new_page.
*/
tp_buffer * initBuffer(unsigned int id);

/*
    Esta função recupera uma página do buffer e retorna a mesma em uma estrutura do tipo tupla
    A estrutura column possui informações de como manipular os dados
    *campos - Estrutura que armazena esquema da tabela para ler os dados do buffer
    *objeto - Estrutura que armazena dados sobre a tabela que está no buffer
    *page - Número da página a ser recuperada (0 a PAGES)
*/
PageResult * getPage(tp_table *campos, struct fs_objects objeto, int page);
/*
    Esta função uma determinada tupla do buffer e retorna a mesma em uma estrutura do tipo column;
    A estrutura column possui informações de como manipular os dados
    *buffer - Estrutura para armazenar tuplas na meméria
    *campos - Estrutura que armazena esquema da tabela para ler os dados do buffer
    *objeto - Estrutura que armazena dados sobre a tabela que está no buffer
    *page   - Número da página a ser recuperada uma tupla (0 a PAGES)
    *nTupla - Número da tupla a ser excluida, este número é relativo a página do buffer e não a
              todos os registros carregados
*/
column * excluirTuplaBuffer(tp_buffer *buffer, tp_table *campos, struct fs_objects objeto, int page, int nTupla);
////
char *getTupla(tp_table *campos,struct fs_objects objeto, int from);

void setTupla(tp_buffer *buffer,char *tupla, int tam, int pos);
////
void cria_campo(int , int , char *, int );

/* ----------------------------------------------------------------------------------------------
    Objetivo:   Utilizada para gravar as mudanças do buffer no disco (via BM).
    Parametros: Buffer (tp_buffer) e dados da tabela (fs_objects)
    Retorno:    1 para sucesso, 0 para falha.
   ---------------------------------------------------------------------------------------------*/
int writeBufferToDisk(tp_buffer *bufferpool, struct fs_objects *objeto);

void addColumn(column **colList, column *c);
