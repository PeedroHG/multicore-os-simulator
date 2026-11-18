#!/usr/bin/env python3
"""
===============================================================================
Algoritmo Ótimo de Belady (OPT Offline) e Detecção da Anomalia de Belady
Responsável: João Paulo (Pessoa 2)
===============================================================================

Lê arquivos de rastro `referencias_memoria.csv` (gerados pela MMU do simulador
com colunas `tick,core,pid,pagina,tipo,falta`) e simula offline as políticas:
  - FIFO (First-In, First-Out)
  - LRU  (Least Recently Used - algoritmo de pilha)
  - OPT  (Algoritmo Ótimo de Belady - substitui a página cujo próximo uso
          está mais distante no futuro; limite inferior teórico de faltas)

Também possui modo `--belady-classico` para demonstrar a Anomalia de Belady na
cadeia canônica de Belady, Nelson & Shedler (1969): 1,2,3,4,1,2,5,1,2,3,4,5.
"""

import argparse
import csv
import os
import sys
from collections import deque
from typing import Dict, List, Optional, Tuple

ChavePagina = Tuple[int, int]  # (pid, pagina)


def carregar_rastro(caminho_csv: str) -> List[ChavePagina]:
    """Lê referencias_memoria.csv e retorna a sequência de pares (pid, pagina)."""
    sequencia: List[ChavePagina] = []
    with open(caminho_csv, mode="r", encoding="utf-8") as f:
        leitor = csv.DictReader(f)
        for linha in leitor:
            pid = int(linha["pid"])
            pagina = int(linha["pagina"])
            sequencia.append((pid, pagina))
    return sequencia


def simular_fifo(sequencia: List[ChavePagina], num_molduras: int) -> int:
    """Simula substituição global FIFO sobre a sequência de referências."""
    residentes = set()
    fila = deque()
    faltas = 0

    for item in sequencia:
        if item in residentes:
            continue
        faltas += 1
        if len(residentes) >= num_molduras:
            vitima = fila.popleft()
            residentes.remove(vitima)
        residentes.add(item)
        fila.append(item)

    return faltas


def simular_lru(sequencia: List[ChavePagina], num_molduras: int) -> int:
    """Simula substituição global LRU exata sobre a sequência de referências."""
    ultimo_uso: Dict[ChavePagina, int] = {}
    faltas = 0

    for instante, item in enumerate(sequencia):
        if item in ultimo_uso:
            ultimo_uso[item] = instante
            continue
        faltas += 1
        if len(ultimo_uso) >= num_molduras:
            vitima = min(ultimo_uso, key=ultimo_uso.get)
            del ultimo_uso[vitima]
        ultimo_uso[item] = instante

    return faltas


def simular_opt(sequencia: List[ChavePagina], num_molduras: int) -> int:
    """
    Simula o algoritmo ótimo offline de Belady (MIN / OPT).
    Pré-computa o índice da próxima ocorrência de cada referência em O(N)
    para que cada escolha de vítima leve O(M), totalizando O(N * M).
    """
    n = len(sequencia)
    proximo_uso = [n] * n
    ultima_aparicao: Dict[ChavePagina, int] = {}

    for i in range(n - 1, -1, -1):
        item = sequencia[i]
        if item in ultima_aparicao:
            proximo_uso[i] = ultima_aparicao[item]
        ultima_aparicao[item] = i

    residentes: Dict[ChavePagina, int] = {}  # item -> próximo índice onde será usado
    faltas = 0

    for i, item in enumerate(sequencia):
        if item in residentes:
            residentes[item] = proximo_uso[i]
            continue
        faltas += 1
        if len(residentes) >= num_molduras:
            # Escolhe a página residente com maior distância até o próximo uso
            # (desempate determinístico pela menor chave (pid, pagina))
            vitima = max(residentes.keys(), key=lambda k: (residentes[k], k))
            del residentes[vitima]
        residentes[item] = proximo_uso[i]

    return faltas


def detectar_anomalias(molduras: List[int], faltas_por_m: Dict[int, int]) -> List[Tuple[int, int, int, int]]:
    """
    Retorna tuplas (m1, m2, faltas_m1, faltas_m2) onde m1 < m2 porém
    faltas(m2) > faltas(m1) — caracterizando a Anomalia de Belady.
    """
    anomalias = []
    ordenadas = sorted(molduras)
    for i in range(len(ordenadas)):
        for j in range(i + 1, len(ordenadas)):
            m1, m2 = ordenadas[i], ordenadas[j]
            if faltas_por_m[m2] > faltas_por_m[m1]:
                anomalias.append((m1, m2, faltas_por_m[m1], faltas_por_m[m2]))
    return anomalias


def gerar_grafico_e4(
    molduras: List[int],
    fifo: Dict[int, int],
    lru: Dict[int, int],
    opt: Dict[int, int],
    caminho_saida_base: str,
    titulo: str = "Experimento E4: Faltas de Página × Número de Molduras",
) -> None:
    """Gera o gráfico comparativo FIFO vs LRU vs OPT em PDF e PNG para o artigo IEEE."""
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("[AVISO] matplotlib não disponível; pulando geração de gráfico.", file=sys.stderr)
        return

    xs = sorted(molduras)
    y_fifo = [fifo[m] for m in xs]
    y_lru = [lru[m] for m in xs]
    y_opt = [opt[m] for m in xs]

    plt.rcParams.update({
        "font.size": 10,
        "axes.labelsize": 10,
        "axes.titlesize": 11,
        "legend.fontsize": 9,
        "figure.figsize": (6.0, 3.8),
    })

    fig, ax = plt.subplots()
    ax.plot(xs, y_fifo, marker="o", linewidth=1.8, linestyle="-", color="#d95f02", label="FIFO (Global)")
    ax.plot(xs, y_lru, marker="s", linewidth=1.8, linestyle="--", color="#1b9e77", label="LRU Exato (Pilha)")
    ax.plot(xs, y_opt, marker="^", linewidth=1.8, linestyle="-.", color="#7570b3", label="OPT de Belady (Offline)")

    # Destaca anomalias se existirem no FIFO
    anomalias = detectar_anomalias(xs, fifo)
    for m1, m2, f1, f2 in anomalias:
        ax.annotate(
            f"Anomalia ({m1}→{m2}: {f1}→{f2})",
            xy=(m2, f2),
            xytext=(m2, f2 + max(y_fifo) * 0.06),
            arrowprops=dict(arrowstyle="->", color="#d95f02", lw=1.0),
            fontsize=8,
            color="#d95f02",
            ha="center",
        )

    ax.set_title(titulo)
    ax.set_xlabel("Número de Molduras de Usuário (M)")
    ax.set_ylabel("Total de Faltas de Página (Page Faults)")
    ax.set_xticks(xs)
    ax.grid(True, linestyle=":", alpha=0.6)
    ax.legend(loc="best", frameon=True)
    fig.tight_layout()

    os.makedirs(os.path.dirname(os.path.abspath(caminho_saida_base)), exist_ok=True)
    fig.savefig(caminho_saida_base + ".pdf", bbox_inches="tight")
    fig.savefig(caminho_saida_base + ".png", dpi=300, bbox_inches="tight")
    plt.close(fig)


def executar_analise_rastro(
    caminho_rastro: str,
    molduras: List[int],
    saida_csv: Optional[str],
    saida_grafico: Optional[str],
    faltas_reais_fifo: Optional[Dict[int, int]] = None,
    faltas_reais_lru: Optional[Dict[int, int]] = None,
) -> int:
    sequencia = carregar_rastro(caminho_rastro)
    total_refs = len(sequencia)
    if total_refs == 0:
        print(f"[ERRO] Rastro vazio em: {caminho_rastro}", file=sys.stderr)
        return 1

    fifo_res: Dict[int, int] = {}
    lru_res: Dict[int, int] = {}
    opt_res: Dict[int, int] = {}

    for m in molduras:
        fifo_res[m] = (
            faltas_reais_fifo[m]
            if (faltas_reais_fifo and m in faltas_reais_fifo)
            else simular_fifo(sequencia, m)
        )
        lru_res[m] = (
            faltas_reais_lru[m]
            if (faltas_reais_lru and m in faltas_reais_lru)
            else simular_lru(sequencia, m)
        )
        opt_res[m] = simular_opt(sequencia, m)

    print("==========================================================================")
    print(f" ANÁLISE DE SUBSTITUIÇÃO DE PÁGINAS (Total de referências: {total_refs})")
    print("==========================================================================")
    print(f"{'Molduras':>8} | {'FIFO':>8} {'Taxa FIFO':>10} | {'LRU':>8} {'Taxa LRU':>10} | {'OPT':>8} {'Taxa OPT':>10}")
    print("-" * 74)
    for m in molduras:
        tf = fifo_res[m] / total_refs
        tl = lru_res[m] / total_refs
        to = opt_res[m] / total_refs
        print(f"{m:>8} | {fifo_res[m]:>8} {tf:>9.4f} | {lru_res[m]:>8} {tl:>9.4f} | {opt_res[m]:>8} {to:>9.4f}")
    print("==========================================================================")

    anomalias_fifo = detectar_anomalias(molduras, fifo_res)
    anomalias_lru = detectar_anomalias(molduras, lru_res)
    if anomalias_fifo:
        print("[BELADY] Anomalia de Belady detectada no FIFO:")
        for m1, m2, f1, f2 in anomalias_fifo:
            print(f"  -> De M={m1} ({f1} faltas) para M={m2} ({f2} faltas): +{f2 - f1} falta(s)")
    else:
        print("[BELADY] Nenhuma Anomalia de Belady observada no FIFO para os tamanhos testados.")
    print(f"[PILHA]  Anomalias no LRU (esperado 0 pela propriedade de inclusão de Mattson): {len(anomalias_lru)}")

    if saida_csv:
        os.makedirs(os.path.dirname(os.path.abspath(saida_csv)), exist_ok=True)
        with open(saida_csv, mode="w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow([
                "molduras", "referencias",
                "faltas_fifo", "taxa_fifo",
                "faltas_lru", "taxa_lru",
                "faltas_opt", "taxa_opt",
                "gap_fifo_opt", "gap_lru_opt",
            ])
            for m in molduras:
                w.writerow([
                    m, total_refs,
                    fifo_res[m], f"{fifo_res[m] / total_refs:.6f}",
                    lru_res[m], f"{lru_res[m] / total_refs:.6f}",
                    opt_res[m], f"{opt_res[m] / total_refs:.6f}",
                    fifo_res[m] - opt_res[m],
                    lru_res[m] - opt_res[m],
                ])
        print(f"[CSV] Tabela consolidada salva em: {saida_csv}")

    if saida_grafico:
        gerar_grafico_e4(molduras, fifo_res, lru_res, opt_res, saida_grafico)
        print(f"[GRAFICO] Figuras salvas em: {saida_grafico}.pdf e {saida_grafico}.png")

    return 0


def demonstrar_belady_classico(saida_csv: Optional[str], saida_grafico: Optional[str]) -> int:
    """Executa a sequência canônica de Belady et al. (1969) variando M de 1 a 5."""
    seq = [(1, p) for p in [1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5]]
    molduras = [1, 2, 3, 4, 5]
    fifo_res = {m: simular_fifo(seq, m) for m in molduras}
    lru_res = {m: simular_lru(seq, m) for m in molduras}
    opt_res = {m: simular_opt(seq, m) for m in molduras}

    print("\n==========================================================================")
    print(" DEMONSTRAÇÃO CLÁSSICA DA ANOMALIA DE BELADY (Cadeia: 1,2,3,4,1,2,5,1,2,3,4,5)")
    print("==========================================================================")
    for m in molduras:
        print(f"  M={m} molduras -> FIFO={fifo_res[m]:2d} faltas | LRU={lru_res[m]:2d} faltas | OPT={opt_res[m]:2d} faltas")
    anomalias = detectar_anomalias(molduras, fifo_res)
    for m1, m2, f1, f2 in anomalias:
        print(f"  [!] ANOMALIA CONFIRMADA NO FIFO: M={m1} ({f1} faltas) < M={m2} ({f2} faltas)")

    if saida_csv:
        os.makedirs(os.path.dirname(os.path.abspath(saida_csv)), exist_ok=True)
        with open(saida_csv, mode="w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow(["molduras", "referencias", "faltas_fifo", "faltas_lru", "faltas_opt"])
            for m in molduras:
                w.writerow([m, len(seq), fifo_res[m], lru_res[m], opt_res[m]])

    if saida_grafico:
        gerar_grafico_e4(
            molduras, fifo_res, lru_res, opt_res, saida_grafico,
            titulo="Anomalia de Belady no FIFO (Sequência Canônica de 12 Referências)",
        )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Calcula o OPT de Belady offline e compara FIFO vs LRU vs OPT."
    )
    parser.add_argument(
        "--rastro",
        type=str,
        default="output/referencias_memoria.csv",
        help="Caminho para referencias_memoria.csv gerado pelo simulador.",
    )
    parser.add_argument(
        "--diretorio-e4",
        type=str,
        default=None,
        help="Diretório base com subpastas fifo_m* e lru_m* geradas por experimento_e4.sh.",
    )
    parser.add_argument(
        "--molduras",
        type=int,
        nargs="+",
        default=[4, 6, 8, 12, 16, 24, 32],
        help="Lista de quantidades de molduras de usuário a avaliar.",
    )
    parser.add_argument(
        "--saida-csv",
        type=str,
        default=None,
        help="Caminho de saída para o CSV consolidado (FIFO, LRU, OPT).",
    )
    parser.add_argument(
        "--saida-grafico",
        type=str,
        default=None,
        help="Prefixo de saída (sem extensão) para salvar o gráfico em .pdf e .png.",
    )
    parser.add_argument(
        "--belady-classico",
        action="store_true",
        help="Executa também a demonstração canônica da Anomalia de Belady (M=1..5).",
    )
    args = parser.parse_args()

    faltas_fifo_reais: Dict[int, int] = {}
    faltas_lru_reais: Dict[int, int] = {}

    if args.diretorio_e4:
        for m in args.molduras:
            for pol, destino in [("fifo", faltas_fifo_reais), ("lru", faltas_lru_reais)]:
                cam_mem = os.path.join(args.diretorio_e4, f"{pol}_m{m}", "memoria.csv")
                if os.path.isfile(cam_mem):
                    with open(cam_mem, mode="r", encoding="utf-8") as f:
                        for row in csv.DictReader(f):
                            if row["pid"] == "global":
                                destino[m] = int(row["faltas"])

    rc = executar_analise_rastro(
        args.rastro,
        args.molduras,
        args.saida_csv,
        args.saida_grafico,
        faltas_fifo_reais if faltas_fifo_reais else None,
        faltas_lru_reais if faltas_lru_reais else None,
    )

    if args.belady_classico:
        prefixo_belady = (args.saida_grafico + "_belady") if args.saida_grafico else None
        csv_belady = (
            os.path.join(os.path.dirname(args.saida_csv), "anomalia_belady.csv")
            if args.saida_csv
            else None
        )
        demonstrar_belady_classico(csv_belady, prefixo_belady)

    return rc


if __name__ == "__main__":
    sys.exit(main())
