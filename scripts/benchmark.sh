#!/usr/bin/env bash
# ==============================================================================
# Script de Automação de Benchmarks: Single-Core vs Multicore
# Responsável: Pessoa 1
# Gera as saídas e medições para a análise de Speedup e Throughput no Artigo IEEE
# ==============================================================================

set -e

SIMULADOR="./simulador"
DIR_BASE_SAIDA="output/benchmarks"

# Garante que o executável está compilado
if [ ! -f "$SIMULADOR" ]; then
    echo "==> Compilando o simulador..."
    make
fi

mkdir -p "$DIR_BASE_SAIDA"

echo "================================================================"
echo "    BATERIA DE BENCHMARKS MULTICORE (EXPERIMENTO E1 - IEEE)     "
echo "================================================================"

CORES_LIST=(1 2 4 8)
POLITICAS=("fcfs" "rr")
MODOS=("np" "p")

for cores in "${CORES_LIST[@]}"; do
    for pol in "${POLITICAS[@]}"; do
        for modo in "${MODOS[@]}"; do
            dir_exec="$DIR_BASE_SAIDA/cores_${cores}_${pol}_${modo}"
            mkdir -p "$dir_exec"
            
            echo "[BENCHMARK] Executando: Cores=$cores | Politica=$pol | Modo=$modo"
            $SIMULADOR --cores "$cores" --politica "$pol" --modo "$modo" --quantum 10 --saida "$dir_exec" > "$dir_exec/execucao.log" 2>&1
        done
    done
done

echo ""
echo "================================================================"
echo "  TODOS OS BENCHMARKS FORAM EXECUTADOS COM SUCESSO!             "
echo "  Resultados salvos em: $DIR_BASE_SAIDA                         "
echo "================================================================"

