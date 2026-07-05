#!/bin/bash
# Roda a demo do BM criando o banco do zero.
# Uso: ./roda_apresentacao.sh            (pool padrao, 60 frames)
#      UFFSDB_BM_PAGES=2 ./roda_apresentacao.sh   (pool minimo, forca substituicao)

cd "$(dirname "$0")/../Fonte" || exit 1
make -s || exit 1
rm -rf data   # banco a partir do zero

echo "== Buffer Pool: ${UFFSDB_BM_PAGES:-60 (padrao)} frames =="
./uffsdb < ../Testes/apresentacao_bm.sql
