#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "memoryContext.h"

#ifndef FMACROS // garante que macros.h não seja reincluída
   #include "macros.h"
#endif
///
#ifndef FTYPES // garante que types.h não seja reincluída
  #include "types.h"
#endif

#include "misc.h"
#include "dictionary.h"

static int isDeleted(char *linha);

/* ==============================================================================
                            BUFFER MANAGER (BM)

   Instância global do gerenciador de buffer. É o ÚNICO ponto do SGBD que executa
   E/S de disco (fread/fwrite) sobre os arquivos de dados das tabelas. A aplicação
   só enxerga as páginas que estão carregadas no Buffer Pool (vetor de molduras).
   ============================================================================== */
BufferManager bufferManager = { .frames = NULL, .initialized = 0 };

/* ----- tamanho de um bloco em disco = cabeçalho da página + bytes de dados ----- */
static long bm_block_stride(void) {
    return (long)PAGE_HEADER_DISK_SIZE + (long)bufferManager.header.page_size;
}

/* Lê os parâmetros de configuração (page_size, pool_pages) do arquivo
   data/uffsdb.conf. Linhas no formato "chave valor" ou "chave=valor"; '#' inicia
   comentário. Valores ausentes/ inválidos caem nos defaults. Se o arquivo não
   existir, ele é criado com os valores default para facilitar a edição. */
static void bm_load_config(uint32_t *page_size, uint32_t *num_frames) {
    *page_size  = BM_DEFAULT_PAGE_SIZE;
    *num_frames = BM_DEFAULT_POOL_PAGES;

    FILE *cfg = fopen(BM_CONFIG_FILE, "r");
    if (cfg == NULL) {
        cfg = fopen(BM_CONFIG_FILE, "w");
        if (cfg != NULL) {
            fprintf(cfg, "# Configuracao do Buffer Manager do UFFSDB\n");
            fprintf(cfg, "# page_size  = tamanho de cada pagina, em bytes (max %d)\n", BM_PAGE_CAPACITY);
            fprintf(cfg, "# pool_pages = numero de paginas (frames) do Buffer Pool\n");
            fprintf(cfg, "page_size  = %u\n", (unsigned)*page_size);
            fprintf(cfg, "pool_pages = %u\n", (unsigned)*num_frames);
            fclose(cfg);
        }
        return;
    }

    char linha[128], chave[64];
    long valor;
    while (fgets(linha, sizeof(linha), cfg) != NULL) {
        char *p = linha;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;

        // aceita "chave = valor" e "chave valor"
        for (char *c = p; *c; c++) if (*c == '=') *c = ' ';
        if (sscanf(p, "%63s %ld", chave, &valor) != 2) continue;

        if (objcmp(chave, "page_size") == 0)       *page_size  = (uint32_t)valor;
        else if (objcmp(chave, "pool_pages") == 0) *num_frames = (uint32_t)valor;
    }
    fclose(cfg);

    // Validação dos limites.
    if (*page_size < 64) *page_size = 64;
    if (*page_size > BM_PAGE_CAPACITY) *page_size = BM_PAGE_CAPACITY;
    if (*num_frames < BM_MIN_POOL_PAGES) *num_frames = BM_MIN_POOL_PAGES;
}

void bm_init(void) {
    if (bufferManager.initialized) return;

    uint32_t page_size, num_frames;
    bm_load_config(&page_size, &num_frames);

    BufferPoolHeader *h = &bufferManager.header;
    h->num_frames  = num_frames;
    h->page_size   = page_size;
    h->frames_used = 0;
    h->hits = h->misses = h->reads = h->writes = h->evictions = 0;

    bufferManager.frames = (BufferFrame *)calloc(num_frames, sizeof(BufferFrame));
    if (bufferManager.frames == NULL) {
        printf("ERROR: Buffer Manager: falha ao alocar o Buffer Pool.\n");
        exit(EXIT_FAILURE);
    }
    for (uint32_t i = 0; i < num_frames; i++) {
        bufferManager.frames[i].valid   = 0;
        bufferManager.frames[i].page_id = -1;
        bufferManager.frames[i].pin     = 0;
        bufferManager.frames[i].dirty   = 0;
        bufferManager.frames[i].filename[0] = '\0';
    }

    srand((unsigned)time(NULL));        // semente da substituição aleatória
    bufferManager.initialized = 1;
    atexit(bm_shutdown);                // garante o flush ao sair (\q usa exit())

    printf("Buffer Manager iniciado: %u frames de %u bytes (Buffer Pool = %u bytes).\n",
           (unsigned)num_frames, (unsigned)page_size,
           (unsigned)(num_frames * page_size));
}

void bm_shutdown(void) {
    if (!bufferManager.initialized) return;
    bm_flush_all();
    free(bufferManager.frames);
    bufferManager.frames = NULL;
    bufferManager.initialized = 0;
}

/* --------------------------- E/S de disco (somente BM) ------------------------- */

/* Grava uma página no arquivo 'filename' na posição id*stride.
   Persiste apenas o cabeçalho (id, nrec, position) + page_size bytes de dados. */
static int bm_write_page_disk(const char *filename, tp_buffer *page) {
    FILE *fd = fopen(filename, "r+b");
    if (fd == NULL) fd = fopen(filename, "w+b"); // cria se ainda não existir
    if (fd == NULL) {
        printf("ERROR: Buffer Manager: nao foi possivel abrir %s para escrita.\n", filename);
        return 0;
    }

    uint32_t hdr[3];
    hdr[0] = (uint32_t)page->id;
    hdr[1] = (uint32_t)page->nrec;
    hdr[2] = (uint32_t)page->position;

    fseek(fd, (long)page->id * bm_block_stride(), SEEK_SET);
    fwrite(hdr, sizeof(uint32_t), 3, fd);
    fwrite(page->data, sizeof(char), bufferManager.header.page_size, fd);
    fclose(fd);

    bufferManager.header.writes++;
    return 1;
}

/* Lê a página 'page_id' do arquivo 'filename' para 'out'.
   Retorna 1 em sucesso, 0 se a página não existe (EOF) e -1 se erro de arquivo. */
static int bm_read_page_disk(const char *filename, int page_id, tp_buffer *out) {
    FILE *fd = fopen(filename, "rb");
    if (fd == NULL) return -1;

    if (fseek(fd, (long)page_id * bm_block_stride(), SEEK_SET) != 0) {
        fclose(fd);
        return 0;
    }

    uint32_t hdr[3];
    if (fread(hdr, sizeof(uint32_t), 3, fd) != 3) { // página não existe
        fclose(fd);
        return 0;
    }

    memset(out, 0, sizeof(tp_buffer));
    out->id       = hdr[0];
    out->nrec     = hdr[1];
    out->position = hdr[2];
    fread(out->data, sizeof(char), bufferManager.header.page_size, fd);
    out->db = out->pc = 0;

    fclose(fd);
    bufferManager.header.reads++;
    return 1;
}

/* --------------------------- gerência de molduras ----------------------------- */

static BufferFrame *bm_find_frame(const char *filename, int page_id) {
    for (uint32_t i = 0; i < bufferManager.header.num_frames; i++) {
        BufferFrame *f = &bufferManager.frames[i];
        if (f->valid && f->page_id == page_id && objcmp(f->filename, (char *)filename) == 0)
            return f;
    }
    return NULL;
}

/* Localiza a moldura cujo conteúdo é apontado por 'p' (usado por writeBufferToDisk). */
static BufferFrame *bm_frame_of_ptr(tp_buffer *p) {
    for (uint32_t i = 0; i < bufferManager.header.num_frames; i++) {
        BufferFrame *f = &bufferManager.frames[i];
        if (f->valid && &f->page == p) return f;
    }
    return NULL;
}

/* Grava uma moldura suja no disco e limpa o dirty bit. */
static void bm_write_frame(BufferFrame *f) {
    if (f == NULL || !f->valid || !f->dirty) return;
    f->page.db = f->page.pc = 0;
    bm_write_page_disk(f->filename, &f->page);
    f->dirty = 0;
}

/* Obtém uma moldura para receber uma página nova:
   1) reutiliza uma moldura livre, se houver;
   2) senão, escolhe uma moldura ALEATÓRIA entre as NÃO pinadas (substituição
      aleatória), gravando-a antes se estiver suja.
   Retorna NULL se todas as molduras estiverem pinadas. */
static BufferFrame *bm_acquire_frame(void) {
    // 1) moldura livre
    for (uint32_t i = 0; i < bufferManager.header.num_frames; i++) {
        if (!bufferManager.frames[i].valid) {
            bufferManager.header.frames_used++;
            return &bufferManager.frames[i];
        }
    }

    // 2) substituição aleatória entre as molduras não pinadas
    uint32_t n = bufferManager.header.num_frames;
    uint32_t naoPinadas = 0;
    for (uint32_t i = 0; i < n; i++)
        if (bufferManager.frames[i].pin == 0) naoPinadas++;

    if (naoPinadas == 0) {
        printf("ERROR: Buffer Manager: todas as paginas do pool estao pinadas.\n");
        return NULL;
    }

    uint32_t alvo = (uint32_t)(rand() % (int)naoPinadas); // k-ésima não pinada
    BufferFrame *vitima = NULL;
    for (uint32_t i = 0, k = 0; i < n; i++) {
        if (bufferManager.frames[i].pin == 0) {
            if (k == alvo) { vitima = &bufferManager.frames[i]; break; }
            k++;
        }
    }

    bm_write_frame(vitima);               // grava se estiver suja
    bufferManager.header.evictions++;
    return vitima;
}

/* ------------------------------- API pública ---------------------------------- */

tp_buffer *bm_get_page(const char *filename, int page_id) {
    if (!bufferManager.initialized) bm_init();
    if (page_id < 0) return NULL;

    BufferFrame *f = bm_find_frame(filename, page_id);
    if (f != NULL) {                       // HIT
        bufferManager.header.hits++;
        f->pin++;
        return &f->page;
    }

    // MISS: tenta carregar do disco
    bufferManager.header.misses++;
    tp_buffer tmp;
    int r = bm_read_page_disk(filename, page_id, &tmp);
    if (r <= 0) return NULL;               // página inexistente ou erro

    f = bm_acquire_frame();
    if (f == NULL) return NULL;

    f->page = tmp;
    strncpy(f->filename, filename, LEN_DB_NAME_IO - 1);
    f->filename[LEN_DB_NAME_IO - 1] = '\0';
    f->page_id = page_id;
    f->valid   = 1;
    f->dirty   = 0;
    f->pin     = 1;
    return &f->page;
}

tp_buffer *bm_new_page(const char *filename, int page_id) {
    if (!bufferManager.initialized) bm_init();
    if (page_id < 0) return NULL;

    BufferFrame *f = bm_find_frame(filename, page_id);
    if (f != NULL) { f->pin++; return &f->page; }

    f = bm_acquire_frame();
    if (f == NULL) return NULL;

    memset(&f->page, 0, sizeof(tp_buffer));
    f->page.id = page_id;
    strncpy(f->filename, filename, LEN_DB_NAME_IO - 1);
    f->filename[LEN_DB_NAME_IO - 1] = '\0';
    f->page_id = page_id;
    f->valid   = 1;
    f->dirty   = 1;                        // página nova precisa ser gravada
    f->pin     = 1;
    return &f->page;
}

void bm_unpin(const char *filename, int page_id, int dirty) {
    BufferFrame *f = bm_find_frame(filename, page_id);
    if (f == NULL) return;
    if (f->pin > 0) f->pin--;
    if (dirty) { f->dirty = 1; f->page.db = 1; }
}

void bm_mark_dirty(const char *filename, int page_id) {
    BufferFrame *f = bm_find_frame(filename, page_id);
    if (f != NULL) { f->dirty = 1; f->page.db = 1; }
}

void bm_flush_all(void) {
    if (bufferManager.frames == NULL) return;
    for (uint32_t i = 0; i < bufferManager.header.num_frames; i++)
        bm_write_frame(&bufferManager.frames[i]);
}

void bm_print_stats(void) {
    BufferPoolHeader *h = &bufferManager.header;
    printf("\n===== Buffer Pool =====\n");
    printf(" Paginas (frames) : %u\n", (unsigned)h->num_frames);
    printf(" Tamanho da pagina: %u bytes\n", (unsigned)h->page_size);
    printf(" Frames ocupados  : %u\n", (unsigned)h->frames_used);
    printf(" Hits / Misses    : %llu / %llu\n",
           (unsigned long long)h->hits, (unsigned long long)h->misses);
    printf(" Leituras (fread) : %llu\n", (unsigned long long)h->reads);
    printf(" Escritas (fwrite): %llu\n", (unsigned long long)h->writes);
    printf(" Substituicoes    : %llu\n", (unsigned long long)h->evictions);
    printf("=======================\n\n");
}

/* ==============================================================================
                  FUNÇÕES DE PÁGINA (agora apoiadas no Buffer Manager)
   ============================================================================== */

//// imprime os dados no buffer (deprecated?)
int printbufferpoll(tp_buffer *buffpoll, tp_table *s,struct fs_objects objeto, int num_page){

    int aux, i, num_reg = objeto.qtdCampos;

    if(buffpoll[num_page].nrec == 0){
        return ERRO_IMPRESSAO;
    }

    i = aux = 0;
    aux = cabecalho(s, num_reg);
    while(i < buffpoll[num_page].nrec){ // Enquanto i < numero de registros * tamanho de uma instancia da tabela
        drawline(buffpoll, s, objeto, i, num_page);
        i++;
    }
    return SUCCESS;
}

/* initBuffer: legado. O fluxo de inserção usa bm_new_page diretamente (que conhece
   o arquivo). Mantida por compatibilidade da API; devolve uma página vazia. */
tp_buffer* initBuffer(unsigned int id){
    tp_buffer *buffer = uffslloc(sizeof(tp_buffer));

    if (buffer == NULL) {
        printf("ERROR: Memory allocation failed.\n\n");
        return NULL;
    }
    memset(buffer, 0, sizeof(tp_buffer));
    buffer->id = id;
    return buffer;
}

/* getBlock: recupera a página 'id' do arquivo 'filename' através do Buffer Manager.
   A página retorna carregada no Buffer Pool e PINADA (lembre de chamar bm_unpin). */
tp_buffer *getBlock(unsigned int id, char* filename){
    return bm_get_page(filename, (int)id);
}

// RETORNA PAGINA DO BUFFER (lida via Buffer Manager)
PageResult *getPage(tp_table *campos, struct fs_objects objeto, int page){

    if(page < 0) return ERRO_PAGINA_INVALIDA;

    char directory[LEN_DB_NAME_IO];
    strcpy(directory, connected.db_directory);
    strcat(directory, objeto.nArquivo);

    tp_buffer *buffer = bm_get_page(directory, page);
    if(buffer == NULL) return NULL;       // página inexistente no disco

    if (!buffer->position) {              // página vazia
        bm_unpin(directory, page, 0);
        return NULL;
    }

    tupla *tuplas = (tupla *)uffslloc(sizeof(tupla) * (buffer->nrec)); //Aloca a quantidade de tuplas necessária
    if(!tuplas){
        bm_unpin(directory, page, 0);
        return ERRO_DE_ALOCACAO;
    }

    int  indiceTupla=0, i=0;

    char* nullos =(char *)uffslloc(objeto.qtdCampos * sizeof(char));

    while(i < buffer->position){

        if(isDeleted(buffer->data + i)) {
            i+=tamTupla(campos, objeto);
            continue;
        }
        tuplas[indiceTupla].offset = i;
        tuplas[indiceTupla].ncols = objeto.qtdCampos;
        i++; //para o byte de deleted
        memcpy(nullos, buffer->data + i, objeto.qtdCampos);
        i += objeto.qtdCampos;


        tuplas[indiceTupla].column = (column *)uffslloc(sizeof(column) * objeto.qtdCampos);
        tuplas[indiceTupla].bufferPage = page;
        for (int ic = 0; ic < objeto.qtdCampos; ic++){
            column *c = &tuplas[indiceTupla].column[ic];

            c->tipoCampo = campos[ic].tipo;
            strcpy(c->nomeCampo, campos[ic].nome); //Guarda nome do campo
            if(nullos[ic]) c->valorCampo = COLUNA_NULL;
            else {
                c->valorCampo = (char *)uffslloc(sizeof(char) * campos[ic].tam + 1);
                memcpy(c->valorCampo, buffer->data + i, campos[ic].tam);
                c->valorCampo[campos[ic].tam] = '\0';
            }
            i += campos[ic].tam;
        }

        indiceTupla++;
    }
    PageResult *pg = (PageResult *)uffslloc(sizeof(PageResult));
    pg->tuplas = tuplas;
    pg->nrec = indiceTupla;

    bm_unpin(directory, page, 0);         // dados já copiados; libera a página

    return pg; //Retorna a 'page' do buffer
}

// EXCLUIR TUPLA BUFFER
column * excluirTuplaBuffer(tp_buffer *buffer, tp_table *campos, struct fs_objects objeto, int page, int nTupla){
    column *tuplas = (column *)uffslloc(sizeof(column)*objeto.qtdCampos);

    if(tuplas == NULL)
        return ERRO_DE_ALOCACAO;

    if(buffer[page].nrec == 0) //Essa página não possui registros
        return ERRO_PARAMETRO;

    int i, tamTpl = tamTupla(campos, objeto), j=0, t=0;
    i = tamTpl*nTupla; //Calcula onde começa o registro

    while(i < tamTpl*nTupla+tamTpl){
        t=0;

        tuplas[j].valorCampo = (char *)uffslloc(sizeof(char)*campos[j].tam); //Aloca a quantidade necessária para cada campo
        tuplas[j].tipoCampo = campos[j].tipo;  // Guarda o tipo do campo
        strcpylower(tuplas[j].nomeCampo, campos[j].nome);   //Guarda o nome do campo

        while(t < campos[j].tam){
            tuplas[j].valorCampo[t] = buffer[page].data[i];    //Copia os dados
            t++;
            i++;
        }
        j++;
    }
    j = i;
    i = tamTpl*nTupla;
    for(; i < buffer[page].position; i++, j++) //Desloca os bytes do buffer sobre a tupla excluida
        buffer[page].data[i] = buffer[page].data[j];

    buffer[page].position -= tamTpl;
    buffer[page].nrec--;

    return tuplas; //Retorna a tupla excluida do buffer
}
// INSERE UMA TUPLA NO BUFFER!
char *getTupla(tp_table *campos,struct fs_objects objeto, int from){ //Pega uma tupla do disco a partir do valor de from
    // + qtdCampos para os bytes de coluna null e +1 para o byte de tupla valida
    int tamTpl = tamTupla(campos, objeto);
    char *linha=(char *)uffslloc(sizeof(char)*tamTpl);

    FILE *dados;
    from = from * tamTpl;
	char directory[LEN_DB_NAME_IO];
    strcpy(directory, connected.db_directory);
    strcat(directory, objeto.nArquivo);

    dados = fopen(directory, "r");

    if (dados == NULL) {
        return ERRO_DE_LEITURA;
    }

    fseek(dados, from, SEEK_CUR);
    if(fgetc (dados) == EOF){
        fclose(dados);
        return ERRO_DE_LEITURA;
    }

    fseek(dados, -1, SEEK_CUR);
    fread(linha, sizeof(char), tamTpl, dados); //Traz a tupla inteira do arquivo

    fclose(dados);
    return linha;
}
/////
void setTupla(tp_buffer *buffer,char *tupla, int tam, int pos) { //Coloca uma tupla de tamanho "tam" no buffer e na página "pos"
  int i = buffer[pos].position;
  for (; i < buffer[pos].position + tam; i++)
    buffer[pos].data[i] = *(tupla++);
}
//// insere uma tupla no buffer
int colocaTuplaBuffer(tp_buffer *buffer, int from, tp_table *campos, struct fs_objects objeto){//Define a página que será incluida uma nova tupla
    int i, found;
    char *tupla = getTupla(campos, objeto, from);
    if(tupla == ERRO_DE_LEITURA)  return ERRO_LEITURA_DADOS;

    int tam = tamTupla(campos, objeto);
    uint32_t pageSize = bufferManager.header.page_size;

    for(i = found = 0; !found && i < PAGES; i++) {//Procura pagina com espaço para a tupla.
        if(pageSize - buffer[i].position > (uint32_t)tam) {// Se na pagina i do buffer tiver espaço para a tupla, coloca tupla.
            setTupla(buffer, tupla, tam, i);
            found = 1;
            buffer[i].position += tam; // Atualiza proxima posição vaga dentro da pagina.
            if(isDeleted(tupla)) {
                return ERRO_LEITURA_DADOS_DELETADOS;
            }
             buffer[i].nrec++;
        }
    }
    return found ? SUCCESS : ERRO_BUFFER_CHEIO;
}
////////

void cria_campo(int tam, int header, char *val, int x) {
  int i;
  char aux[30];
  if(header){
    for(i = 0; i <= 30 && val[i] != '\0'; i++) aux[i] = val[i];
    for(;i < 30;i++) aux[i] = ' ';
    aux[i] ='\0';
    printf("%s", aux);
    return;
  }
  for(i = 0; i < x; i++) printf(" ");
}

/* ----------------------------------------------------------------------------------------------
    Objetivo:   Grava as mudanças de uma página no disco através do Buffer Manager.
    Parametros: Buffer (tp_buffer) e dados da tabela (fs_objects).
    Retorno:    1 para sucesso, 0 para falha.
   ---------------------------------------------------------------------------------------------*/
int writeBufferToDisk(tp_buffer *buffer, struct fs_objects *objeto) {
    if(buffer==NULL){
        printf("ERROR: empty buffer\n");
        return 0;
    }

    BufferFrame *f = bm_frame_of_ptr(buffer);
    if (f != NULL) {                 // página do pool: grava via BM
        f->dirty = 1;
        bm_write_frame(f);
        return 1;
    }

    // Página fora do pool (caso legado): grava diretamente pelo BM.
    char directory[LEN_DB_NAME_IO];
    strcpy(directory, connected.db_directory);
    strcat(directory, objeto->nArquivo);
    buffer->db = 0;
    buffer->pc = 0;
    return bm_write_page_disk(directory, buffer);
}

static int isDeleted(char *linha){
    return linha[0]; //byte se foi deletado
}

void addColumn(column **colList, column *c){
    c->next = NULL;
    if(*colList == NULL) {
        *colList = c;
        return;
    }
    column *t = *colList;
    while(t->next != NULL) t = t->next;

    t->next = c;
}
