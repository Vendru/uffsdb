# uffsdb
A simple educational DBMS

# dependencies
 1. Bison
 2. Flex
 3. Library readline

 - For Ubuntu/Debian-based systems, run:
 ```
 sudo apt install bison
 sudo apt install flex
 sudo apt-get install libreadline-dev
```

# how to compile
 uffsdb/Fonte/make

# how to execute
 `uffsdb/Fonte/./uffsdb`

# Buffer Manager (BM)
 O UFFSDB possui um Gerenciador de Buffer (BM) que é o único componente que faz
 E/S de disco (`fread`/`fwrite`) nos arquivos de dados das tabelas. A aplicação
 sempre trabalha sobre as páginas que estão no Buffer Pool (memória).

 - Inicializado na carga do SGBD (`bm_init()` em `uffsdb.c`).
 - Política de substituição: **aleatória** (não usa LRU/MRU).
 - Tamanho da página e número de páginas do pool são **configuráveis na carga**
   pelo arquivo `data/uffsdb.conf` (criado automaticamente com os defaults):
   ```
   page_size  = 1024   # bytes por página (máx. 8192)
   pool_pages = 16     # número de páginas (frames) do Buffer Pool
   ```
   Veja `Fonte/uffsdb.conf.example`. Detalhes em `Leia-me[Instrucoes].txt` (Parte 7).

 > Observação: ao alterar `page_size`, recrie os arquivos de dados (`make clean`).
 
# compiler
 uffsdb commands are interpreted using `yacc` and `lex`.
 In the `interface` folder type `make` to compile both.
 You can edit the following files: `parser.h`, `parser.c`, `lex.l`, and `yacc.y`.
