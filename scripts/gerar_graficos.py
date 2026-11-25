#!/usr/bin/env python3
"""
Gerador dos graficos E1-E5 do artigo - Responsavel: Otavio (Pessoa 4)

Le os resultados de `scripts/benchmark.sh` (output/benchmarks/) e do E4 da
Pessoa 2 (output/experimento_e4/) e gera figuras em PNG (300 dpi) e PDF
(vetorial), no estilo de coluna dupla da IEEE, em output/graficos/.

    E1  Speedup S(n) e throughput x n, contra a linha ideal e o baseline v1
    E2  TME e TMR por politica e modo (barras agrupadas), n in {1, 4}
    E3  TMR e % de sobrecarga de troca de contexto x quantum Q
    E4  Faltas de pagina x molduras (FIFO, LRU, OPT)
    E5  Eficiencia por nucleo (barras) e linha do tempo de ocupacao (Gantt)

Tambem grava tabelas CSV de apoio (output/graficos/tabelas/) e, para o artigo,
copia as figuras em PDF para artigo/figuras/ e gera tabelas LaTeX (E1, E5 e
descricao dos lotes) em artigo/tabelas/. O artigo as inclui com \\input, entao
os numeros nunca ficam desatualizados em relacao aos benchmarks.

Uso:  python3 scripts/gerar_graficos.py [--benchmarks DIR] [--e4 DIR] [--saida DIR]
                                         [--formatos png pdf] [--artigo DIR | --sem-artigo]
Dependencias: matplotlib, seaborn, pandas, numpy.
"""
import argparse
import re
import shutil
import sys
from pathlib import Path

try:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt  # noqa: E402
    import numpy as np  # noqa: E402
    import pandas as pd  # noqa: E402
    import seaborn as sns  # noqa: E402
    from matplotlib.lines import Line2D  # noqa: E402
    from matplotlib.patches import Patch  # noqa: E402
except ImportError as erro:
    sys.exit(
        f"[ERRO] Dependencia Python ausente: {erro.name}\n"
        "Instale com UMA das opcoes (os resultados em output/benchmarks ja estao salvos;\n"
        "depois rode 'make graficos'):\n"
        "  sudo apt install python3-matplotlib python3-seaborn python3-pandas python3-numpy\n"
        "  ou:  python3 -m venv .venv && . .venv/bin/activate && pip install -r scripts/requirements.txt"
    )

RAIZ = Path(__file__).resolve().parent.parent

POLITICAS = ["fcfs", "sjn", "rr", "prio"]
MODOS = ["np", "p"]
NOME_LOTE = {"cpu_bound": "CPU-bound", "memory_bound": "Memory-bound", "misto": "Misto"}
ORDEM_LOTES = ["cpu_bound", "memory_bound", "misto"]
COR_POL = dict(zip(POLITICAS, sns.color_palette("colorblind")[:4]))
ESTILO_MODO = {"np": "-", "p": "--"}
MARCA_MODO = {"np": "o", "p": "s"}
LARGURA = 7.16  # polegadas: largura de duas colunas IEEE

AVISOS = []
GERADOS = []


def aviso(msg):
    AVISOS.append(msg)
    print(f"[AVISO] {msg}", file=sys.stderr)


def rotulo(pol, modo):
    return f"{pol.upper()}-{modo.upper()}"


def configurar_estilo():
    sns.set_theme(style="whitegrid", context="paper")
    plt.rcParams.update({
        "font.size": 8, "axes.titlesize": 8.5, "axes.labelsize": 8,
        "xtick.labelsize": 7, "ytick.labelsize": 7, "legend.fontsize": 7,
        "figure.dpi": 100, "savefig.dpi": 300, "savefig.bbox": "tight",
        "pdf.fonttype": 42, "ps.fonttype": 42, "axes.titlepad": 4,
        "grid.linewidth": 0.5, "lines.linewidth": 1.3, "lines.markersize": 4,
    })


def salvar(fig, nome, saida, formatos):
    saida.mkdir(parents=True, exist_ok=True)
    for ext in formatos:
        caminho = saida / f"{nome}.{ext}"
        fig.savefig(caminho)
    plt.close(fig)
    GERADOS.append(nome)
    print(f"[GRAFICO] {nome}.{'/'.join(formatos)}")


# ---------------------------------------------------------------------------
# Carga de dados
# ---------------------------------------------------------------------------
RE_GRADE = re.compile(r"c(?P<n>\d+)_(?P<pol>[a-z]+)_(?P<modo>np|p)$")
RE_QUANTUM = re.compile(r"(?P<pol>[a-z]+)_q(?P<q>\d+)_t(?P<custo>\d+)$")


def carregar_familia(base, familia, regex):
    """Um DataFrame com 1 linha por execucao (resumo.csv) + colunas lote/cfg."""
    linhas = []
    for resumo in sorted((base / familia).glob("*/*/resumo.csv")):
        m = regex.match(resumo.parent.name)
        if not m:
            continue
        df = pd.read_csv(resumo)
        if df.empty:
            continue
        linha = df.iloc[0].to_dict()
        linha.update({k: (int(v) if v.isdigit() else v) for k, v in m.groupdict().items()})
        linha["lote"] = resumo.parent.parent.name
        linha["dir"] = resumo.parent
        linhas.append(linha)
    return pd.DataFrame(linhas)


def lotes_presentes(df):
    return [l for l in ORDEM_LOTES if l in set(df["lote"])] if not df.empty else []


def legenda_figura(fig, handles, ncol, y=-0.02):
    fig.legend(handles=handles, loc="lower center", ncol=ncol, frameon=False,
               bbox_to_anchor=(0.5, y))


def painel_vazio_aviso(df, nome):
    if df.empty:
        aviso(f"{nome}: sem dados (rode 'make benchmark' antes); figura nao gerada.")
        return True
    return False


# ---------------------------------------------------------------------------
# E1 - Speedup e throughput
# ---------------------------------------------------------------------------
def grafico_e1(grade, baseline, saida, formatos, tabelas):
    if painel_vazio_aviso(grade, "E1"):
        return
    lotes = lotes_presentes(grade)
    # Speedup S(n) = makespan(1) / makespan(n), por (lote, politica, modo)
    base1 = (grade[grade["n"] == 1].set_index(["lote", "pol", "modo"])["makespan"])
    if base1.empty:
        aviso("E1: faltam execucoes com 1 nucleo; speedup nao calculado.")
        return
    g = grade.copy()
    g["speedup"] = [base1.get((r.lote, r.pol, r.modo), np.nan) / r.makespan for r in g.itertuples()]
    g["eficiencia_paralela"] = g["speedup"] / g["n"]
    tabelas["e1_speedup_throughput.csv"] = (
        g[["lote", "pol", "modo", "n", "makespan", "speedup", "eficiencia_paralela",
           "throughput_por_1000"]].sort_values(["lote", "pol", "modo", "n"]))

    ns = sorted(g["n"].unique())
    fig, eixos = plt.subplots(2, len(lotes), figsize=(LARGURA, 4.2), squeeze=False)
    for j, lote in enumerate(lotes):
        sub = g[g["lote"] == lote]
        ax_s, ax_t = eixos[0][j], eixos[1][j]
        for pol in POLITICAS:
            for modo in MODOS:
                s = sub[(sub["pol"] == pol) & (sub["modo"] == modo)].sort_values("n")
                if s.empty:
                    continue
                kw = dict(color=COR_POL[pol], linestyle=ESTILO_MODO[modo], marker=MARCA_MODO[modo])
                ax_s.plot(s["n"], s["speedup"], **kw)
                ax_t.plot(s["n"], s["throughput_por_1000"], **kw)
        ax_s.plot(ns, ns, color="gray", linestyle=":", linewidth=1.0)          # ideal linear
        if baseline is not None and lote in set(baseline["lote"]):
            b = baseline[baseline["lote"] == lote].iloc[0]
            ax_t.axhline(b["throughput_por_1000"], color="black", linestyle="-.", linewidth=1.0)
        ax_s.set_title(NOME_LOTE.get(lote, lote))
        ax_s.set_xscale("log", base=2)
        ax_t.set_xscale("log", base=2)
        for ax in (ax_s, ax_t):
            ax.set_xticks(ns)
            ax.set_xticklabels(ns)
            ax.minorticks_off()
        ax_t.set_xlabel("núcleos (n)")
        if j == 0:
            ax_s.set_ylabel("speedup $S(n)$")
            ax_t.set_ylabel("throughput (proc./1000 ciclos)")
    handles = [Line2D([0], [0], color=COR_POL[p], lw=2, label=p.upper()) for p in POLITICAS]
    handles += [Line2D([0], [0], color="k", linestyle=ESTILO_MODO[m], marker=MARCA_MODO[m],
                       label="não-preemptivo" if m == "np" else "preemptivo") for m in MODOS]
    handles.append(Line2D([0], [0], color="gray", linestyle=":", label="speedup ideal"))
    if baseline is not None:
        handles.append(Line2D([0], [0], color="black", linestyle="-.", label="baseline v1 (1 núcleo)"))
    legenda_figura(fig, handles, ncol=min(len(handles), 5), y=-0.07)
    # (sem titulo interno: a legenda fica no artigo)
    fig.tight_layout(rect=(0, 0.05, 1, 0.98))
    salvar(fig, "fig_e1_speedup_throughput", saida, formatos)


# ---------------------------------------------------------------------------
# E2 - TME / TMR por politica e modo
# ---------------------------------------------------------------------------
def grafico_e2(grade, saida, formatos, tabelas):
    if painel_vazio_aviso(grade, "E2"):
        return
    lotes = lotes_presentes(grade)
    ns = [n for n in (1, 4) if n in set(grade["n"])]
    if not ns:
        aviso("E2: faltam execucoes com n = 1 ou 4.")
        return
    tabelas["e2_tme_tmr.csv"] = (grade[grade["n"].isin(ns)]
                                 [["lote", "n", "pol", "modo", "tme", "tmr", "makespan"]]
                                 .sort_values(["lote", "n", "pol", "modo"]))
    degenerados = []
    for metrica, nome_y in (("tme", "TME (ciclos)"), ("tmr", "TMR (ciclos)")):
        fig, eixos = plt.subplots(len(ns), len(lotes), figsize=(LARGURA, 1.9 * len(ns) + 0.6),
                                  squeeze=False)
        for i, n in enumerate(ns):
            for j, lote in enumerate(lotes):
                ax = eixos[i][j]
                sub = grade[(grade["lote"] == lote) & (grade["n"] == n)]
                if sub.empty:
                    ax.set_visible(False)
                    continue
                if sub[metrica].round(4).nunique() == 1:
                    degenerados.append(f"{NOME_LOTE.get(lote, lote)}/n={n}")
                sns.barplot(data=sub, x="pol", y=metrica, hue="modo", order=POLITICAS,
                            hue_order=MODOS, palette=["#4c72b0", "#dd8452"], ax=ax,
                            errorbar=None, legend=False, linewidth=0.4, edgecolor="black")
                ax.set_title(f"{NOME_LOTE.get(lote, lote)} — n = {n}")
                ax.set_xlabel("")
                ax.set_xticks(range(len(POLITICAS)))
                ax.set_xticklabels([p.upper() for p in POLITICAS])
                ax.set_ylabel(nome_y if j == 0 else "")
        legenda_figura(fig, [Patch(color="#4c72b0", label="não-preemptivo"),
                             Patch(color="#dd8452", label="preemptivo")], ncol=2, y=-0.03)
        # (sem titulo interno: a legenda fica no artigo)
        fig.tight_layout(rect=(0, 0.04, 1, 0.98))
        salvar(fig, f"fig_e2_{metrica}", saida, formatos)
    if degenerados:
        aviso("E2: barras IDENTICAS entre politicas/modos em "
              + ", ".join(sorted(set(degenerados)))
              + ". O escalonador provavelmente ainda nao esta integrado ao Kernel.")


# ---------------------------------------------------------------------------
# E3 - Quantum
# ---------------------------------------------------------------------------
def grafico_e3(quantum, saida, formatos, tabelas):
    if painel_vazio_aviso(quantum, "E3"):
        return
    q = quantum.copy()
    ocupado = q["instrucoes"] + q["ciclos_stall"] + q["ciclos_troca"]
    q["sobrecarga_pct"] = 100.0 * q["ciclos_troca"] / ocupado.replace(0, np.nan)
    tabelas["e3_quantum.csv"] = (q[["lote", "pol", "q", "custo", "makespan", "tmr", "tme",
                                    "trocas_contexto", "ciclos_troca", "sobrecarga_pct"]]
                                 .sort_values(["lote", "pol", "custo", "q"]))
    lotes = lotes_presentes(q)
    pols = [p for p in ("rr", "prio") if p in set(q["pol"])]
    qs = sorted(q["q"].unique())
    estilo_custo = {0: ":", 5: "-"}
    marca_custo = {0: "^", 5: "o"}

    fig, eixos = plt.subplots(2, len(lotes), figsize=(LARGURA, 4.0), squeeze=False)
    for j, lote in enumerate(lotes):
        for pol in pols:
            for custo in sorted(q["custo"].unique()):
                s = q[(q["lote"] == lote) & (q["pol"] == pol) & (q["custo"] == custo)].sort_values("q")
                if s.empty:
                    continue
                kw = dict(color=COR_POL[pol], linestyle=estilo_custo.get(custo, "-"),
                          marker=marca_custo.get(custo, "o"))
                eixos[0][j].plot(s["q"], s["tmr"], **kw)
                eixos[1][j].plot(s["q"], s["sobrecarga_pct"], **kw)
        eixos[0][j].set_title(NOME_LOTE.get(lote, lote))
        for ax in eixos[:, j]:
            ax.set_xscale("log")
            ax.set_xticks(qs)
            ax.set_xticklabels(qs)
            ax.minorticks_off()
        eixos[1][j].set_xlabel("quantum $Q$ (ciclos)")
        if j == 0:
            eixos[0][0].set_ylabel("TMR (ciclos)")
            eixos[1][0].set_ylabel("sobrecarga de troca (%)")
    handles = [Line2D([0], [0], color=COR_POL[p], lw=2, label=p.upper()) for p in pols]
    handles += [Line2D([0], [0], color="k", linestyle=estilo_custo.get(c, "-"),
                       marker=marca_custo.get(c, "o"), label=f"custo de troca = {c}")
                for c in sorted(q["custo"].unique())]
    legenda_figura(fig, handles, ncol=len(handles), y=-0.04)
    # (sem titulo interno: a legenda fica no artigo)
    fig.tight_layout(rect=(0, 0.04, 1, 0.98))
    salvar(fig, "fig_e3_quantum", saida, formatos)

    if (q["ciclos_troca"] == 0).all():
        aviso("E3: ciclos_troca = 0 em todas as execucoes; o Kernel ainda nao cobra o custo "
              "de troca de contexto (a sobrecarga sai 0%).")
    elif q.groupby(["lote", "pol", "custo"])["tmr"].nunique().max() == 1:
        aviso("E3: o TMR nao varia com o quantum; a preempcao ainda nao esta integrada ao Kernel.")


# ---------------------------------------------------------------------------
# E4 - Faltas x molduras (tabelas da Pessoa 2)
# ---------------------------------------------------------------------------
def grafico_e4(dir_e4, saida, formatos):
    arquivos = [("tabela_e4_singlecore.csv", "1 núcleo"), ("tabela_e4_multicore4.csv", "4 núcleos")]
    dados = [(rot, pd.read_csv(dir_e4 / nome)) for nome, rot in arquivos if (dir_e4 / nome).exists()]
    if not dados:
        aviso(f"E4: tabelas nao encontradas em {dir_e4} (rode 'make experimento_e4').")
        return
    fig, eixos = plt.subplots(1, len(dados), figsize=(LARGURA, 2.6), squeeze=False)
    estilos = {"fifo": ("FIFO", "#c44e52", "o"), "lru": ("LRU", "#4c72b0", "s"),
               "opt": ("OPT (ótimo)", "#55a868", "^")}
    for ax, (rot, df) in zip(eixos[0], dados):
        df = df.sort_values("molduras")
        for chave, (nome, cor, marca) in estilos.items():
            ax.plot(df["molduras"], df[f"faltas_{chave}"], color=cor, marker=marca, label=nome)
        ax.set_xscale("log", base=2)
        ax.set_xticks(df["molduras"])
        ax.set_xticklabels(df["molduras"])
        ax.minorticks_off()
        ax.set_title(rot)
        ax.set_xlabel("molduras")
    eixos[0][0].set_ylabel("faltas de página")
    h, l = eixos[0][0].get_legend_handles_labels()
    legenda_figura(fig, h, ncol=3, y=-0.06)
    # (sem titulo interno: a legenda fica no artigo)
    fig.tight_layout(rect=(0, 0.06, 1, 0.97))
    salvar(fig, "fig_e4_faltas", saida, formatos)


# ---------------------------------------------------------------------------
# E5 - Distribuicao de carga por nucleo
# ---------------------------------------------------------------------------
def carregar_nucleos(grade, n=4):
    partes = []
    for r in grade[grade["n"] == n].itertuples():
        arq = Path(r.dir) / "nucleos.csv"
        if arq.exists():
            df = pd.read_csv(arq)
            df["lote"], df["pol"], df["modo"] = r.lote, r.pol, r.modo
            partes.append(df)
    return pd.concat(partes, ignore_index=True) if partes else pd.DataFrame()


def grafico_e5_barras(grade, saida, formatos, tabelas, n=4):
    nuc = carregar_nucleos(grade, n)
    if painel_vazio_aviso(nuc, f"E5 (n={n})"):
        return
    lotes = lotes_presentes(nuc)
    nuc["config"] = [rotulo(p, m) for p, m in zip(nuc["pol"], nuc["modo"])]
    ordem = [rotulo(p, m) for p in POLITICAS for m in MODOS]
    # Balanceamento: dispersao da eficiencia entre os nucleos de cada execucao
    bal = (nuc.groupby(["lote", "config"])["eficiencia"]
           .agg(media="mean", desvio="std", minimo="min", maximo="max").reset_index())
    bal["desvio"] = bal["desvio"].fillna(0.0)
    tabelas["e5_balanceamento.csv"] = bal

    fig, eixos = plt.subplots(2, len(lotes), figsize=(LARGURA, 4.2), squeeze=False)
    paleta = sns.color_palette("tab10", len(ordem))
    for j, lote in enumerate(lotes):
        sub = nuc[nuc["lote"] == lote]
        for i, (col, nome_y) in enumerate((("eficiencia", "eficiência do núcleo"),
                                           ("eficiencia_util", "eficiência útil do núcleo"))):
            ax = eixos[i][j]
            sns.barplot(data=sub, x="nucleo", y=col, hue="config", hue_order=ordem,
                        palette=paleta, ax=ax, errorbar=None, legend=False,
                        linewidth=0.3, edgecolor="black")
            ax.set_xlabel("núcleo" if i == 1 else "")
            ax.set_ylabel(nome_y if j == 0 else "")
            ax.set_ylim(0, 1.05 if col == "eficiencia" else None)
            if i == 0:
                ax.set_title(NOME_LOTE.get(lote, lote))
    legenda_figura(fig, [Patch(color=c, label=o) for c, o in zip(paleta, ordem)], ncol=8, y=-0.04)
    # (sem titulo interno: a legenda fica no artigo)
    fig.tight_layout(rect=(0, 0.04, 1, 0.98))
    salvar(fig, "fig_e5_eficiencia_nucleos", saida, formatos)


def grafico_e5_timeline(grade, saida, formatos, n=4):
    escolhidas = [("fcfs", "np"), ("sjn", "np"), ("rr", "p"), ("prio", "p")]
    for lote in lotes_presentes(grade):
        sub = grade[(grade["lote"] == lote) & (grade["n"] == n)]
        painel = []
        for pol, modo in escolhidas:
            r = sub[(sub["pol"] == pol) & (sub["modo"] == modo)]
            if r.empty or not (Path(r.iloc[0]["dir"]) / "timeline.csv").exists():
                continue
            painel.append((pol, modo, pd.read_csv(Path(r.iloc[0]["dir"]) / "timeline.csv"),
                           int(r.iloc[0]["makespan"])))
        if not painel:
            continue
        fim = max(p[3] for p in painel)
        pids = sorted({int(x) for p in painel for x in p[2]["pid"]})
        cores = dict(zip(pids, sns.color_palette("tab20", max(len(pids), 1))))
        fig, eixos = plt.subplots(len(painel), 1, figsize=(LARGURA, 1.35 * len(painel) + 0.8),
                                  sharex=True, squeeze=False)
        for ax, (pol, modo, tl, mk) in zip(eixos[:, 0], painel):
            for seg in tl.itertuples():
                ax.broken_barh([(seg.inicio, seg.fim - seg.inicio)], (seg.nucleo - 0.4, 0.8),
                               facecolors=cores[int(seg.pid)], edgecolors="black", linewidth=0.3)
                if (seg.fim - seg.inicio) > fim * 0.035:
                    ax.text((seg.inicio + seg.fim) / 2, seg.nucleo, str(seg.pid), ha="center",
                            va="center", fontsize=6)
            ax.set_ylim(n - 0.5, -0.5)
            ax.set_yticks(range(n))
            ax.set_yticklabels([f"núcleo {k}" for k in range(n)])
            ax.set_ylabel(rotulo(pol, modo), rotation=0, ha="right", va="center", labelpad=26,
                          fontweight="bold")
            ax.grid(axis="y", visible=False)
        eixos[-1][0].set_xlim(0, fim)
        eixos[-1][0].set_xlabel("ciclos")
        # (sem titulo interno: a legenda fica no artigo)
        fig.tight_layout()
        salvar(fig, f"fig_e5_timeline_{lote}", saida, formatos)


# ---------------------------------------------------------------------------
# Exportacao para o artigo (LaTeX)
# ---------------------------------------------------------------------------
REF_POL, REF_MODO = "fcfs", "np"   # configuracao de referencia das tabelas do artigo


def num(x, casas=1):
    """Numero com virgula decimal (portugues)."""
    return f"{x:.{casas}f}".replace(".", ",")


def milhar(x):
    """Inteiro com ponto como separador de milhar (ex.: 15.482)."""
    return f"{int(round(x)):,}".replace(",", ".")


def tex_e1(grade, baseline):
    """Tabela E1: T(n), S(n), E(n), ganho sobre o baseline v1, throughput e taxa de faltas."""
    ref = grade[(grade["pol"] == REF_POL) & (grade["modo"] == REF_MODO)]
    if ref.empty or (ref["n"] == 1).sum() == 0:
        return None
    linhas = [r"\begin{tabular}{@{}rrrrrrr@{}}", r"\toprule",
              r"$n$ & $T(n)$ & $S(n)$ & $E(n)$ & $T_{v1}/T(n)$ & Thr. & Faltas \\", r"\midrule"]
    primeiro = True
    for lote in lotes_presentes(ref):
        sub = ref[ref["lote"] == lote].sort_values("n")
        t1 = float(sub[sub["n"] == 1].iloc[0]["makespan"])
        b = None
        if baseline is not None and lote in set(baseline["lote"]):
            b = baseline[baseline["lote"] == lote].iloc[0]
        if not primeiro:
            linhas.append(r"\midrule")
        primeiro = False
        linhas.append(rf"\multicolumn{{7}}{{@{{}}l}}{{\textit{{{NOME_LOTE.get(lote, lote)}}}}} \\")
        if b is not None:
            linhas.append(rf"v1 & {milhar(b['makespan'])} & --- & --- & --- & "
                          rf"{num(b['throughput_por_1000'], 2)} & --- \\")
        for r in sub.itertuples():
            ganho = (f"{num(float(b['makespan']) / r.makespan, 2)}" if b is not None else "---")
            linhas.append(rf"{r.n} & {milhar(r.makespan)} & {num(t1 / r.makespan, 2)} & "
                          rf"{num(100 * t1 / r.makespan / r.n, 1)}\% & {ganho} & "
                          rf"{num(r.throughput_por_1000, 2)} & {num(100 * r.taxa_faltas, 1)}\% \\")
    linhas += [r"\bottomrule", r"\end{tabular}"]
    return "\n".join(linhas) + "\n"


def tex_e5(grade, n=4):
    """Tabela E5: eficiencia (ocupado/makespan) e eficiencia util por nucleo, config. de referencia."""
    nuc = carregar_nucleos(grade[(grade["pol"] == REF_POL) & (grade["modo"] == REF_MODO)], n)
    if nuc.empty:
        return None
    cab = " & ".join(f"N{k}" for k in range(n))
    linhas = [rf"\begin{{tabular}}{{@{{}}l{'r' * n}rr@{{}}}}", r"\toprule",
              rf"& {cab} & M\'edia & $\sigma$ (p.p.) \\", r"\midrule"]
    primeiro = True
    for lote in lotes_presentes(nuc):
        sub = nuc[nuc["lote"] == lote].sort_values("nucleo")
        if not primeiro:
            linhas.append(r"\midrule")
        primeiro = False
        linhas.append(rf"\multicolumn{{{n + 3}}}{{@{{}}l}}{{\textit{{{NOME_LOTE.get(lote, lote)}}}}} \\")
        for col, nome in (("eficiencia", "Efici\\^encia"), ("eficiencia_util", "Efic. \\'util")):
            v = list(sub[col])
            celulas = " & ".join(f"{num(100 * x, 1)}\\%" for x in v)
            sigma = float(np.std(v, ddof=1)) if len(v) > 1 else 0.0
            linhas.append(rf"{nome} & {celulas} & {num(100 * np.mean(v), 1)}\% & {num(100 * sigma, 1)} \\")
    linhas += [r"\bottomrule", r"\end{tabular}"]
    return "\n".join(linhas) + "\n"


def tex_lotes():
    """Tabela descritiva dos lotes, a partir de tests/lotes/perfil_esperado.csv."""
    arq = RAIZ / "tests" / "lotes" / "perfil_esperado.csv"
    if not arq.exists():
        return None
    p = pd.read_csv(arq)
    linhas = [r"\begin{tabular}{@{}lrrrrr@{}}", r"\toprule",
              r"Lote & Jobs & Mín. & Máx. & Total & Págs. dados \\", r"\midrule"]
    for lote in ORDEM_LOTES:
        sub = p[p["lote"] == lote]
        if sub.empty:
            continue
        ex = sub["instrucoes_executadas"]
        pg = sub["paginas_dados"]
        faixa = f"{pg.min()}" if pg.min() == pg.max() else f"{pg.min()}--{pg.max()}"
        linhas.append(rf"{NOME_LOTE[lote]} & {len(sub)} & {milhar(ex.min())} & {milhar(ex.max())} & "
                      rf"{milhar(ex.sum())} & {faixa} \\")
    linhas += [r"\bottomrule", r"\end{tabular}"]
    return "\n".join(linhas) + "\n"


def exportar_para_artigo(saida, artigo, grade, baseline):
    """Copia as figuras (PDF) e grava as tabelas LaTeX dentro de artigo/."""
    figuras = artigo / "figuras"
    tabelas = artigo / "tabelas"
    figuras.mkdir(parents=True, exist_ok=True)
    tabelas.mkdir(parents=True, exist_ok=True)
    copiadas = 0
    for pdf in sorted(saida.glob("fig_e*.pdf")):
        if pdf.name.startswith("fig_e4"):
            continue            # a figura do E4 e da Pessoa 2 e fica em artigo/
        shutil.copy2(pdf, figuras / pdf.name)
        copiadas += 1
    escritas = []
    for nome, conteudo in (("tab_e1.tex", tex_e1(grade, baseline) if not grade.empty else None),
                           ("tab_e5.tex", tex_e5(grade) if not grade.empty else None),
                           ("tab_lotes.tex", tex_lotes())):
        if conteudo is None:
            aviso(f"artigo: tabela {nome} nao gerada (dados ausentes).")
            continue
        (tabelas / nome).write_text(conteudo, encoding="utf-8")
        escritas.append(nome)
    print(f"[ARTIGO] {copiadas} figura(s) em {figuras}; tabelas {', '.join(escritas)} em {tabelas}")


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--benchmarks", default=str(RAIZ / "output" / "benchmarks"))
    ap.add_argument("--e4", default=str(RAIZ / "output" / "experimento_e4"))
    ap.add_argument("--saida", default=str(RAIZ / "output" / "graficos"))
    ap.add_argument("--formatos", nargs="+", default=["png", "pdf"])
    ap.add_argument("--artigo", default=str(RAIZ / "artigo"),
                    help="pasta do artigo que recebe figuras/ e tabelas/ (padrao: artigo)")
    ap.add_argument("--sem-artigo", action="store_true", help="nao atualiza a pasta do artigo")
    args = ap.parse_args()

    base, dir_e4, saida = Path(args.benchmarks), Path(args.e4), Path(args.saida)
    configurar_estilo()
    tabelas = {}

    grade = carregar_familia(base, "grade", RE_GRADE)
    quantum = carregar_familia(base, "quantum", RE_QUANTUM)
    arq_base = base / "baseline_v1.csv"
    baseline = pd.read_csv(arq_base) if arq_base.exists() else None
    if baseline is None:
        aviso("baseline v1 nao encontrado; E1 sai sem a linha de referencia do v1.")

    grafico_e1(grade, baseline, saida, args.formatos, tabelas)
    grafico_e2(grade, saida, args.formatos, tabelas)
    grafico_e3(quantum, saida, args.formatos, tabelas)
    grafico_e4(dir_e4, saida, args.formatos)
    if not grade.empty:
        grafico_e5_barras(grade, saida, args.formatos, tabelas)
        grafico_e5_timeline(grade, saida, args.formatos)

    if not args.sem_artigo and "pdf" in args.formatos:
        exportar_para_artigo(saida, Path(args.artigo), grade, baseline)

    pasta_tab = saida / "tabelas"
    pasta_tab.mkdir(parents=True, exist_ok=True)
    for nome, df in tabelas.items():
        df.to_csv(pasta_tab / nome, index=False, float_format="%.4f")

    print(f"\n{len(GERADOS)} figura(s) em {saida}; {len(tabelas)} tabela(s) em {pasta_tab}")
    if AVISOS:
        print(f"{len(AVISOS)} aviso(s) acima: leia antes de usar as figuras no artigo.")
    return 0 if GERADOS else 1


if __name__ == "__main__":
    sys.exit(main())