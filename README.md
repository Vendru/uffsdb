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

The project includes a Buffer Manager (BM) layer to keep database pages in memory and reduce direct disk access. It starts with the DBMS in `Fonte/uffsdb.c` and is released when the program ends.

## Implementation
The BM uses two main structures:
- `tp_buffer`, which stores the pages loaded in memory.
- `tp_metadados`, which stores the state of each frame, including `table_id`, dirty bit (`db`), and pin counter (`pc`).

When a page is requested, the BM first checks whether it is already in memory. If it is, the frame is reused and the pin counter is increased. If not, the BM looks for a free frame. When a dirty page needs to be replaced, it is written back to disk before the new page is loaded.

## LRU
The replacement policy used is **LRU (Least Recently Used)**. This helps keep recently used pages in memory, which improves performance when access patterns show temporal locality.

## Buffer Size
The number of frames in the Buffer Pool is configured in `Fonte/macros.h`.

To change the default size, edit the `BM_DEFAULT_PAGES` macro:
```c
#define BM_DEFAULT_PAGES 60
```
