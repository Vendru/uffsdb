#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

// Variável global do Buffer Manager
tp_bm *bm = NULL;

// Contador global para a política de substituição LRU
long lru_counter = 0;

void initBufferManager() { // inicializa o bm
    if (bm == NULL) {
        // Usa o contexto PERMANENT para o BM não ser deletado ao fim da query
        bm = (tp_bm *)uffsllocType(sizeof(tp_bm), PERMANENT);
        
        if (bm == NULL) {
            printf("ERROR: Falha ao alocar o Buffer Manager!\n");
            return;
        }

        for (int i = 0; i < BM_PAGES; i++) {
            bm->md[i].table_id = -1; // -1 indica que o frame está vazio
            bm->md[i].pc = 0;
            bm->md[i].db = 0;
            bm->md[i].last_used_timestamp = 0;
        }
    }
}

void unpinBuffer(tp_buffer *buffer, int table_id) { // reduzir pc
    if (bm == NULL || buffer == NULL) return;
    for (int i = 0; i < BM_PAGES; i++) {
        if (bm->pages[i].id == buffer->id && bm->md[i].table_id == table_id) {
            if (bm->md[i].pc > 0){
                bm->md[i].pc--;
            }
            return;
        }
    }
}

int encontra_espaco_livre() { // encontra espaço livre no bm
    if (bm == NULL) return -1;
    for (int i = 0; i < BM_PAGES; i++) {
        if (bm->md[i].pc == 0 && bm->md[i].table_id == -1) return i;
    }
    return -1; 
}

int encontra_lru() { // encontra o que foi usado a mais tempo
    int index = -1;
    long menor_tempo = -1;
    for (int i = 0; i < BM_PAGES; i++) {
        if (bm->md[i].pc == 0) {
            if (index == -1 || bm->md[i].last_used_timestamp < menor_tempo) {
                menor_tempo = bm->md[i].last_used_timestamp;
                index = i;
            }
        }
    }
    return index;
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

    lru_counter++; // O tempo "passou" para o algoritmo LRU

    for (int i = 0; i < BM_PAGES; i++) {     // 1. BUSCA NA MEMÓRIA
        if (bm->pages[i].id == id && bm->md[i].table_id == table_id) { // se está na memória
            bm->md[i].pc++; // Incrementa quem está usando (Pin Count)
            bm->md[i].last_used_timestamp = lru_counter; // Marca uso recente
            return &(bm->pages[i]); // retorna a pagina
        }
    }

    int index = encontra_espaco_livre();     // 2. NÃO ESTÁ NA MEMÓRIA
    if (index == -1) {         // Buffer lotado, roda o LRU
        index = encontra_lru();
        
        if (index == -1) {
            printf("ERROR: Buffer cheio e todas as páginas estão em uso (Deadlock)!\n");
            return NULL; 
        }

        // Se a página antiga estiver suja (db = 1), deve ir para o disco antes de morrer
        if (bm->md[index].db == 1) {
            struct fs_objects obj_antigo = leObjetoById(bm->md[index].table_id);
            char filepath[LEN_DB_NAME_IO];
            strcpy(filepath, connected.db_directory);
            strcat(filepath, obj_antigo.nArquivo);
            
            FILE *fd_old = fopen(filepath, "r+b"); 
            if (fd_old) {
                long int pos_old = (long int)bm->pages[index].id * sizeof(tp_buffer);
                fseek(fd_old, pos_old, SEEK_SET);
                fwrite(&(bm->pages[index]), sizeof(tp_buffer), 1, fd_old);
                fclose(fd_old);
            }
            bm->md[index].db = 0; // Agora está limpa
        }
    }

    FILE *fd = fopen(filename, "r+b"); // 3. Lê do disco para a posição encontrada no Buffer Pool
    if (!fd) {
        printf("ERROR: failed to open %s\n", filename);
        return NULL;
    }

    long int pos = (long int)id * sizeof(tp_buffer);
    fseek(fd, pos, SEEK_SET);
    fread(&(bm->pages[index]), sizeof(tp_buffer), 1, fd); 
    fclose(fd);

    // 4. ATUALIZA OS METADADOS DA NOVA PÁGINA
    bm->md[index].table_id = table_id;
    bm->md[index].db = 0; 
    bm->md[index].pc = 1; // Quem pediu acabou de dar pin
    bm->md[index].last_used_timestamp = lru_counter;

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

    tupla *tuplas = (tupla *)uffslloc(sizeof(tupla) * (buffer->nrec)); //Aloca a quantidade de tuplas necessária

    if(!tuplas)
        return ERRO_DE_ALOCACAO;

    int  indiceTupla=0, i=0;

    if (!buffer->position)
        return NULL;

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
    for (int i = 0; i < BM_PAGES; i++) {
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
