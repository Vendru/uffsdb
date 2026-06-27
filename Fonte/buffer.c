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

// ALTERAÇÃO **

// Variável global do Buffer Manager (vive por toda a execução do SGBD).
tp_bm *bm = NULL;

/*
    Inicializa o Buffer Manager: aloca o cabeçalho, o Buffer Pool (vetor de frames)
    e o vetor de metadados. numPages define quantos frames o pool terá; valores <= 0
    caem no padrão BM_DEFAULT_PAGES.

    O pool é alocado com calloc (heap) e não pelos MemoryContext porque seu tamanho
    (num_pages * sizeof(tp_buffer)) excede MEMORY_CONTEXT_SIZE. Assim ele persiste
    entre as consultas e só é liberado em shutdownBufferManager().
*/
void initBufferManager(int numPages) {
    if (bm != NULL) return; // já inicializado

    if (numPages <= 0) numPages = BM_DEFAULT_PAGES;

    bm = (tp_bm *)calloc(1, sizeof(tp_bm));
    if (bm == NULL) {
        printf("ERROR: Falha ao alocar o Buffer Manager!\n");
        return;
    }

    bm->num_pages = numPages;
    bm->pages = (tp_buffer *)calloc(numPages, sizeof(tp_buffer));
    bm->md    = (tp_metadados *)calloc(numPages, sizeof(tp_metadados));

    if (bm->pages == NULL || bm->md == NULL) {
        printf("ERROR: Falha ao alocar o Buffer Pool!\n");
        free(bm->pages);
        free(bm->md);
        free(bm);
        bm = NULL;
        return;
    }

    for (int i = 0; i < bm->num_pages; i++) {
        bm->md[i].table_id = -1; // -1 indica que o frame está vazio
        bm->md[i].pc = 0;
        bm->md[i].db = 0;
    }

    srand((unsigned int)time(NULL)); // semente para a substituição aleatória
}

// Libera toda a memória do Buffer Manager (chamado ao encerrar o SGBD).
void shutdownBufferManager() {
    if (bm == NULL) return;
    free(bm->pages);
    free(bm->md);
    free(bm);
    bm = NULL;
}

void unpinBuffer(tp_buffer *buffer, int table_id) { // reduz o pin counter do frame
    if (bm == NULL || buffer == NULL) return;
    for (int i = 0; i < bm->num_pages; i++) {
        if (bm->pages[i].id == buffer->id && bm->md[i].table_id == table_id) {
            if (bm->md[i].pc > 0){
                bm->md[i].pc--;
            }
            return;
        }
    }
}

void markDirtyBuffer(tp_buffer *buffer, int table_id) { // marca o frame como modificado (dirty)
    if (bm == NULL || buffer == NULL) return;
    for (int i = 0; i < bm->num_pages; i++) {
        if (bm->pages[i].id == buffer->id && bm->md[i].table_id == table_id) {
            bm->md[i].db = 1;
            return;
        }
    }
}

// Escreve um frame sujo de volta ao disco (write-back) usando o table_id dos metadados.
static void flushFrame(int index) {
    struct fs_objects obj = leObjetoById(bm->md[index].table_id);

    // Tabela não encontrada: leObjetoById devolve um fs_objects não-inicializado.
    // Aborta sem tocar no disco nem limpar o dirty bit, evitando usar obj.nArquivo
    // com lixo (caminho inválido / estouro de buffer no strcat).
    if (obj.cod != bm->md[index].table_id) return;

    char filepath[LEN_DB_NAME_IO];
    strcpy(filepath, connected.db_directory);
    strcat(filepath, obj.nArquivo);

    FILE *fd = fopen(filepath, "r+b");
    if (!fd) return; // não abriu: mantém o frame sujo, não perde dado silenciosamente

    long int pos = (long int)bm->pages[index].id * sizeof(tp_buffer);
    fseek(fd, pos, SEEK_SET);
    fwrite(&(bm->pages[index]), sizeof(tp_buffer), 1, fd);
    fclose(fd);

    bm->md[index].db = 0; // só agora a página está realmente limpa
}

// Procura um frame vazio (ainda não usado) no Buffer Pool.
static int encontra_espaco_livre() {
    if (bm == NULL) return -1;
    for (int i = 0; i < bm->num_pages; i++) {
        if (bm->md[i].table_id == -1) return i;
    }
    return -1;
}

// Política de substituição ALEATÓRIA: sorteia um frame não-fixado (pc == 0) para ser a vítima.
static int encontra_vitima_aleatoria() {
    int livres = 0;
    for (int i = 0; i < bm->num_pages; i++) {
        if (bm->md[i].pc == 0) livres++;
    }

    if (livres == 0) return -1; // todos os frames estão fixados (pin)

    int escolhido = rand() % livres; // sorteia a n-ésima posição livre
    for (int i = 0; i < bm->num_pages; i++) {
        if (bm->md[i].pc == 0) {
            if (escolhido == 0) return i;
            escolhido--;
        }
    }
    return -1; // não deve acontecer
}

// ALTERAÇÃO **

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

tp_buffer* initBuffer(unsigned int id){
    tp_buffer *buffer = uffslloc(sizeof(tp_buffer));

    if (buffer == NULL) {
        printf("ERROR: Memory allocation failed.\n\n");
        return NULL;
    }

    buffer->id = id;
    return buffer;
}

// ALTERAÇÃO **

tp_buffer *getBlock(unsigned int id, char* filename, int table_id){

    if (bm == NULL) {
        printf("ERROR: Buffer Manager não foi inicializado!\n");
        return NULL;
    }

    // 1. A página já está no Buffer Pool?
    for (int i = 0; i < bm->num_pages; i++) {
        if (bm->md[i].table_id == table_id && bm->pages[i].id == id) {
            bm->md[i].pc++; // dá pin em quem está usando
            return &(bm->pages[i]);
        }
    }

    // 2. Não está: procura um frame vazio; se não houver, sorteia uma vítima (substituição aleatória).
    int index = encontra_espaco_livre();
    if (index == -1) {
        index = encontra_vitima_aleatoria();
        if (index == -1) {
            printf("ERROR: Buffer pool cheio e todos os frames estão fixados (pin)!\n");
            return NULL;
        }
        // Write-back: se a vítima estiver suja, grava no disco antes de substituir.
        if (bm->md[index].db == 1) flushFrame(index);
    }

    // 3. Lê o bloco do disco para o frame escolhido (somente o BM faz I/O).
    FILE *fd = fopen(filename, "r+b");
    if (!fd) {
        printf("ERROR: failed to open %s\n", filename);
        return NULL;
    }

    long int pos = (long int)id * sizeof(tp_buffer);
    fseek(fd, pos, SEEK_SET);
    size_t lidos = fread(&(bm->pages[index]), sizeof(tp_buffer), 1, fd);
    fclose(fd);

    if (lidos != 1) {
        // Bloco ainda não existe no disco: inicializa o frame em vez de manter lixo da vítima.
        memset(&(bm->pages[index]), 0, sizeof(tp_buffer));
    }
    bm->pages[index].id = id; // garante coerência do índice usado na busca

    // 4. Atualiza os metadados do novo frame.
    bm->md[index].table_id = table_id;
    bm->md[index].db = 0;
    bm->md[index].pc = 1; // quem pediu acabou de dar pin

    return &(bm->pages[index]);
}

// ALTERAÇÃO **

// RETORNA PAGINA DO BUFFER
PageResult *getPage(tp_table *campos, struct fs_objects objeto, int page){

    if(page >= PAGES || page < 0) return ERRO_PAGINA_INVALIDA;

    
    char directory[LEN_DB_NAME_IO];
    strcpy(directory, connected.db_directory);
    strcat(directory, objeto.nArquivo);

    // ALTERAÇÃO **
    // tp_buffer *buffer = getBlock((unsigned int) page, directory);
    tp_buffer *buffer = getBlock((unsigned int) page, directory, objeto.cod);
    // ALTERAÇÃO **

    if (buffer == NULL)
        return ERRO_PARAMETRO;

    tupla *tuplas = (tupla *)uffslloc(sizeof(tupla) * (buffer->nrec)); //Aloca a quantidade de tuplas necessária

    if(!tuplas) {
        unpinBuffer(buffer, objeto.cod); // libera a página antes de sair
        return ERRO_DE_ALOCACAO;
    }

    int  indiceTupla=0, i=0;

    if (!buffer->position) {
        unpinBuffer(buffer, objeto.cod); // libera a página antes de sair
        return NULL;
    }

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
    // ALTERAÇÃO **
    unpinBuffer(buffer, objeto.cod); // liberar a pagina
    // ALTERAÇÃO **
    PageResult *pg = (PageResult *)uffslloc(sizeof(PageResult));
    pg->tuplas = tuplas;
    pg->nrec = indiceTupla;

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

    for(i = found = 0; !found && i < PAGES; i++) {//Procura pagina com espaço para a tupla.
        if(SIZE - buffer[i].position > tam) {// Se na pagina i do buffer tiver espaço para a tupla, coloca tupla.
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
    Objetivo:   Utilizada para gravar as mudanças do buffer no disco.
    Parametros: Buffer (tp_buffer), dados da tabela (fs_objects), número de blocos e offset do bloco.
    Retorno:    1 para sucesso, 0 para falha.
   ---------------------------------------------------------------------------------------------*/

int writeBufferToDisk(tp_buffer *buffer, struct fs_objects *objeto) {
    // ALTERAÇÃO **
    if(buffer==NULL || bm == NULL){
        printf("ERROR: buffer vazio ou BM não inicializado\n");
        return 0;
    }
    // ALTERAÇÃO **

    char directory[LEN_DB_NAME_IO];
    strcpy(directory, connected.db_directory);
    strcat(directory, objeto->nArquivo);

    // 1. Abre o arquivo e faz a escrita física no disco
    FILE *dados = fopen(directory, "r+b");
    if (!dados) {
        printf("ERROR: Unable to open file for writing.\n");
        return 0;
    }
    
    fseek(dados, buffer->id * sizeof(tp_buffer), SEEK_SET);
    fwrite(buffer, sizeof(tp_buffer), 1, dados);
    fclose(dados);

    // ALTERAÇÃO **
    // 2. Procura a página no Buffer Pool e avisa que ela agora está limpa
    for (int i = 0; i < bm->num_pages; i++) {
        if (bm->pages[i].id == buffer->id && bm->md[i].table_id == objeto->cod) {
            bm->md[i].db = 0; // A página foi salva, então o Dirty Bit volta a ser 0
            break;
        }
    }
    // ALTERAÇÃO **

    return 1;
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
