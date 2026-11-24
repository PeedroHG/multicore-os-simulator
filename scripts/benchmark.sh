#!/usr/bin/env bash
# ==============================================================================
# Bateria de benchmarks (E1, E2, E3, E4, E5) + baseline v1
# Runner original: Pessoa 1 | Lotes, E3/E5, baseline e integracao com os graficos: Pessoa 4
#
# O que roda (tudo reprodutivel; nada aleatorio):
#   grade   E1/E2/E5: lote x cores {1,2,4,8} x politica {fcfs,sjn,rr,prio} x modo {np,p}
#   quantum E3: lote x politica {rr,prio} x Q {2,5,10,20,50,100} x custo-troca {0,5}, modo p, 2 nucleos
#   baseline_v1: o simulador single-core original (tag v1-baseline, ou o 1o commit) nos mesmos lotes
#   E4: delega ao scripts/experimento_e4.sh (Pessoa 2) se a tabela ainda nao existir
#
# Saida: output/benchmarks/{grade,quantum}/<lote>/<config>/{resumo,processos,nucleos,timeline}.csv
#        output/benchmarks/baseline_v1.csv
#
# Variaveis opcionais:  SKIP_BASELINE=1  SKIP_E4=1  BENCH_DIR=<dir>
# ==============================================================================
set -uo pipefail

# Deve rodar na raiz do repositorio (use 'make benchmark').
[ -f Makefile ] && [ -d tests/lotes ] || { echo "Rode na raiz do repositorio: make benchmark"; exit 1; }

SIMULADOR="./simulador"
DIR="${BENCH_DIR:-output/benchmarks}"
LOTES=(cpu_bound memory_bound misto)
LOTES_E3=(cpu_bound misto)

CORES_LIST=(1 2 4 8)
POLITICAS=(fcfs sjn rr prio)
MODOS=(np p)
QUANTUM_PADRAO=10
CUSTO_PADRAO=5
MOLDURAS=16

QUANTUNS_E3=(2 5 10 20 50 100)
CUSTOS_E3=(0 5)
POLITICAS_E3=(rr prio)
CORES_E3=2

if [ ! -x "$SIMULADOR" ]; then
    echo "==> Compilando o simulador..."
    make || exit 1
fi

FALHAS=()
EXECUCOES=0

# Roda uma configuracao. $1 = diretorio de saida, $2 = n. de jobs esperado, demais = argumentos.
# Descarta a execucao se algum job nao foi carregado (ex.: parser sem .priority/.estimate).
rodar() {
    local dir="$1" esperados="$2"; shift 2
    rm -rf "$dir"; mkdir -p "$dir"
    "$SIMULADOR" "$@" --saida "$dir" > "$dir/console.log" 2>&1
    local linhas=0
    [ -f "$dir/processos.csv" ] && linhas=$(( $(wc -l < "$dir/processos.csv") - 1 ))
    if [ "$linhas" -ne "$esperados" ]; then
        FALHAS+=("$dir ($linhas/$esperados jobs carregados)")
        rm -rf "$dir"
        return 1
    fi
    EXECUCOES=$((EXECUCOES + 1))
    return 0
}

jobs_do_lote() { ls tests/lotes/"$1"/*.txt 2>/dev/null; }

echo "================================================================"
echo "    BATERIA DE BENCHMARKS (E1, E2, E3, E5) - saida em $DIR"
echo "================================================================"
mkdir -p "$DIR"

# ---------------------------------------------------------------- E1 / E2 / E5
echo ""
echo "[E1/E2/E5] grade: lote x nucleos x politica x modo"
for lote in "${LOTES[@]}"; do
    mapfile -t arqs < <(jobs_do_lote "$lote")
    [ "${#arqs[@]}" -eq 0 ] && { echo "  (lote $lote nao encontrado, pulando)"; continue; }
    for n in "${CORES_LIST[@]}"; do
        for pol in "${POLITICAS[@]}"; do
            for modo in "${MODOS[@]}"; do
                rodar "$DIR/grade/$lote/c${n}_${pol}_${modo}" "${#arqs[@]}" \
                    --cores "$n" --politica "$pol" --modo "$modo" --quantum "$QUANTUM_PADRAO" \
                    --custo-troca "$CUSTO_PADRAO" --molduras "$MOLDURAS" "${arqs[@]}"
            done
        done
    done
    echo "  lote $lote: concluido"
done

# ------------------------------------------------------------------------- E3
echo ""
echo "[E3] quantum x custo de troca (modo preemptivo, $CORES_E3 nucleos)"
for lote in "${LOTES_E3[@]}"; do
    mapfile -t arqs < <(jobs_do_lote "$lote")
    [ "${#arqs[@]}" -eq 0 ] && continue
    for pol in "${POLITICAS_E3[@]}"; do
        for q in "${QUANTUNS_E3[@]}"; do
            for custo in "${CUSTOS_E3[@]}"; do
                rodar "$DIR/quantum/$lote/${pol}_q${q}_t${custo}" "${#arqs[@]}" \
                    --cores "$CORES_E3" --politica "$pol" --modo p --quantum "$q" \
                    --custo-troca "$custo" --molduras "$MOLDURAS" "${arqs[@]}"
            done
        done
    done
    echo "  lote $lote: concluido"
done

# ----------------------------------------------------------------- baseline v1
baseline_v1() {
    local ref src
    if git rev-parse --verify -q "v1-baseline^{commit}" > /dev/null; then
        ref="v1-baseline"
    else
        ref=$(git rev-list --max-parents=0 HEAD | tail -1)
    fi
    src="$DIR/baseline_v1_src"
    if [ ! -x "$src/simulador" ]; then
        rm -rf "$src"; mkdir -p "$src"
        git archive "$ref" | tar -x -C "$src"
        make -C "$src" > "$src/make.log" 2>&1 || { echo "  [AVISO] nao foi possivel compilar o baseline ($ref)"; return 1; }
    fi
    local csv="$DIR/baseline_v1.csv"
    echo "lote,jobs,makespan,throughput_por_1000" > "$csv"
    local tmp; tmp=$(mktemp -d)
    for lote in "${LOTES[@]}"; do
        local total=0 n=0 f clock
        for f in $(jobs_do_lote "$lote"); do
            grep -v -E '^\.(priority|estimate)' "$f" > "$tmp/job.txt"   # v1 nao conhece as diretivas
            clock=$(cd "$src" && ./simulador "$tmp/job.txt" 2>&1 | sed -n 's/.*CLOCK TOTAL DESTE JOB: \([0-9]*\) ciclos.*/\1/p' | tail -1)
            [ -z "$clock" ] && { echo "  [AVISO] baseline falhou em $f"; clock=0; }
            total=$((total + clock)); n=$((n + 1))
        done
        [ "$n" -gt 0 ] && echo "$lote,$n,$total,$(awk -v n=$n -v t=$total 'BEGIN{printf "%.4f", n/t*1000}')" >> "$csv"
    done
    rm -rf "$tmp"
    echo "  baseline ($ref) salvo em $csv"
}

echo ""
echo "[BASELINE v1] simulador single-core original (jobs em sequencia)"
if [ "${SKIP_BASELINE:-0}" = "1" ]; then echo "  (pulado: SKIP_BASELINE=1)"; else baseline_v1 || true; fi

# -------------------------------------------------------------------------- E4
echo ""
echo "[E4] faltas de pagina x molduras (FIFO/LRU/OPT)"
if [ "${SKIP_E4:-0}" = "1" ]; then
    echo "  (pulado: SKIP_E4=1)"
elif [ -f output/experimento_e4/tabela_e4_singlecore.csv ]; then
    echo "  tabela ja existe (output/experimento_e4); rode 'make experimento_e4' para refazer"
else
    tr -d '\r' < scripts/experimento_e4.sh | bash > "$DIR/experimento_e4.log" 2>&1 \
        && echo "  concluido" || echo "  [AVISO] experimento_e4.sh falhou (veja $DIR/experimento_e4.log)"
fi

# ---------------------------------------------------------------------- resumo
echo ""
echo "================================================================"
echo "  $EXECUCOES execucoes concluidas em: $DIR"
if [ "${#FALHAS[@]}" -gt 0 ]; then
    echo "  [AVISO] ${#FALHAS[@]} execucao(oes) descartada(s):"
    printf '          - %s\n' "${FALHAS[@]:0:5}"
    [ "${#FALHAS[@]}" -gt 5 ] && echo "          ... e mais $(( ${#FALHAS[@]} - 5 ))"
    echo "          (lote misto exige o parser com .priority/.estimate do Igor)"
fi
echo "================================================================"
exit 0