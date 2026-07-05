# uffsdb
A simple educational DBMS.

# Dependencies
1. Bison
2. Flex
3. Library readline

For Ubuntu/Debian-based systems, run:
```bash
sudo apt update
sudo apt install bison flex libreadline-dev
```

# How to Compile
Inside the `Fonte` directory, run:
```bash
make
```

# How to Execute
```bash
./uffsdb
```

# Compiler
The uffsdb commands are interpreted using `yacc` and `lex`.
In the `interface` folder, type `make` to compile both.
You can edit the following files: `parser.h`, `parser.c`, `lex.l`, and `yacc.y`.

---

# Buffer Manager

O projeto inclui uma camada de Buffer Manager (BM) para manter páginas do banco de dados na memória e reduzir o acesso direto ao disco. Ele é iniciado junto com o SGBD em `Fonte/uffsdb.c` e liberado quando o programa termina.

## Implementação
O BM usa duas estruturas principais:
- `tp_buffer`, que armazena as páginas carregadas na memória.
- `tp_metadados`, que armazena o estado de cada frame, incluindo `table_id`, bit sujo (`db`) e contador de pin (`pc`).

Quando uma página é solicitada, o BM primeiro verifica se ela já está na memória. Se estiver, o frame é reutilizado e o contador de pin é incrementado. Caso contrário, o BM procura um frame livre. Quando uma página suja precisa ser substituída, ela é gravada de volta no disco antes de a nova página ser carregada.

## Política de Substituição
A política de substituição usada pelo Buffer Manager é **aleatória** entre frames que não estão fixados (`pc == 0`). O BM primeiro tenta usar um frame livre. Se não houver nenhum disponível, ele escolhe uma vítima aleatoriamente. Quando o frame vítima está sujo (`db == 1`), seu conteúdo é gravado de volta no disco antes de a nova página ser carregada.

## Tamanho do Buffer
A quantidade de frames no Buffer Pool é configurada em `Fonte/macros.h`.

Para alterar o tamanho padrão, edite a macro `BM_DEFAULT_PAGES`:
```c
#define BM_DEFAULT_PAGES 60
```

Também é possível sobrescrever o tamanho na carga do SGBD, sem recompilar, pela variável de ambiente `UFFSDB_BM_PAGES`:
```bash
UFFSDB_BM_PAGES=100 ./uffsdb
```

## Como Testar
A pasta `Testes/` contém uma demonstração que cria um banco a partir do zero (`CREATE DATABASE`, tabelas com PK/FK, 53 inserts, consultas, violações de integridade, `UPDATE` e `DELETE`).

Para rodar com o pool padrão (60 frames):
```bash
cd Testes
./roda_apresentacao.sh
```

Para forçar a substituição aleatória de páginas (pool de apenas 2 frames para ~4 páginas de dados):
```bash
UFFSDB_BM_PAGES=2 ./roda_apresentacao.sh
```

O resultado deve ser idêntico nas duas execuções: os dois `INSERT` de integridade devem falhar (PK duplicada e FK inexistente), o `UPDATE` deve refletir na consulta seguinte e o `DELETE` deve retornar `0 Rows` na releitura. O script apaga a pasta `data/` antes de começar, garantindo a criação do banco do zero.
