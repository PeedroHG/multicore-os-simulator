#!/usr/bin/env bash
# ==============================================================================
# Script de Execução do Experimento E4 (Gerenciamento de Memória — Paginação)
# Responsável: João Paulo (Pessoa 2)
# Avalia a taxa de faltas de página e write-backs variando M ∈ {4,6,8,12,16,24,32}
# para FIFO, LRU e OPT de Belady offline, gerando tabelas CSV e gráficos IEEE.
# ==============================================================================

set -e

SIMULADOR="./simulador"
DIR_E4="output/experimento_e4"
JOBS_E4=(
    "tests/memoria/mem_localidade.txt"
    "tests/memoria/mem_ciclico_a.txt"
    "tests/memoria/mem_ciclico_b.txt"
    "tests/job1_soma.txt"
    "tests/job2_vetor.txt"
    "tests/job3_loop.txt"
)

if [ ! -f "$SIMULADOR" ]; then
    echo "==> Compilando o simulador..."
    make
fi

mkdir -p "$DIR_E4"

echo "================================================================"
echo "    EXPERIMENTO E4: SUBSTITUIÇÃO DE PÁGINAS (FIFO vs LRU vs OPT)"
echo "================================================================"

MOLDURAS_LIST=(4 6 8 12 16 24 32)
SUBSTS=("fifo" "lru")

for m in "${MOLDURAS_LIST[@]}"; do
    for sub in "${SUBSTS[@]}"; do
        dir_run="$DIR_E4/${sub}_m${m}"
        mkdir -p "$dir_run"
        echo "[E4] Executando: Cores=1 | Substituicao=${sub} | Molduras=${m}"
        $SIMULADOR --cores 1 --politica fcfs --modo np --substituicao "$sub" --molduras "$m" --saida "$dir_run" "${JOBS_E4[@]}" > "$dir_run/execucao.log" 2>&1
    done
done

# Executa também cenário multicore (4 núcleos) para medir concorrência na MMU
for m in "${MOLDURAS_LIST[@]}"; do
    for sub in "${SUBSTS[@]}"; do
        dir_run="$DIR_E4/multicore4_${sub}_m${m}"
        mkdir -p "$dir_run"
        $SIMULADOR --cores 4 --politica fcfs --modo np --substituicao "$sub" --molduras "$m" --saida "$dir_run" "${JOBS_E4[@]}" > "$dir_run/execucao.log" 2>&1
    done
done

echo ""
echo "==> Calculando limite inferior ótimo offline (OPT de Belady) e gerando figuras..."
python3 scripts/opt_belady.py \
    --rastro "$DIR_E4/lru_m32/referencias_memoria.csv" \
    --diretorio-e4 "$DIR_E4" \
    --molduras 4 6 8 12 16 24 32 \
    --saida-csv "$DIR_E4/tabela_e4_singlecore.csv" \
    --saida-grafico "artigo/fig_e4_faltas" \
    --belady-classico

python3 scripts/opt_belady.py \
    --rastro "$DIR_E4/multicore4_lru_m32/referencias_memoria.csv" \
    --molduras 4 6 8 12 16 24 32 \
    --saida-csv "$DIR_E4/tabela_e4_multicore4.csv" \
    --saida-grafico "$DIR_E4/fig_e4_multicore4"

echo "================================================================"
echo "  EXPERIMENTO E4 CONCLUÍDO COM SUCESSO!                         "
echo "  Tabelas em: $DIR_E4/tabela_e4_*.csv                           "
echo "  Gráficos em: artigo/fig_e4_faltas.{pdf,png}                   "
echo "================================================================"
