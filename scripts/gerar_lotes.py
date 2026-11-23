#!/usr/bin/env python3
"""
Gerador dos lotes experimentais (tests/lotes/) - Responsavel: Otavio (Pessoa 4)

Gera tres lotes de 10 jobs em Assembly RISC 16 bits:
    cpu_bound/     laços longos e calculo intensivo (centenas a milhares de instrucoes)
    memory_bound/  variaveis e codigo espalhados por varias paginas de 16 palavras
    misto/         duracoes e acessos variados + diretivas .priority / .estimate

Como o PDF pede reprodutibilidade, os jobs NAO sao escritos a mao: este script
monta cada programa, valida as restricoes da ISA (Secao 7 do plano) e executa
o job num simulador da ISA para obter o numero EXATO de instrucoes executadas
(usado em .estimate e em tests/lotes/perfil_esperado.csv).

Uso:  python3 scripts/gerar_lotes.py            (regrava tests/lotes/)

Restricoes da ISA respeitadas:
  * LOAD/STORE/BEQ tem 8 bits de endereco -> dados em 0x00..0xFF.
  * BEQ Rd,Rf1,alvo: o alvo e o byte baixo da instrucao e o nibble alto dele
    e o proprio Rf1 -> comparando com R0 so se salta para 0x00..0x0F.
    Por isso todo job comeca com um "trampolim": 0x00 JUMP inicio; 0x01 e 0x02
    sao o epilogo (STORE do resultado + HALT), alcancavel por BEQ R?,R0.
  * JUMP tem 12 bits (codigo pode passar de 0xFF).
  * O loader sobrescreve codigo com dados se os enderecos colidirem: o gerador
    confere que nenhum dado cai dentro do codigo.
"""
import os
import sys

RAIZ = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
DESTINO = os.path.join(RAIZ, "tests", "lotes")

TAM_PAGINA = 16
OP = {"LOAD": 0x1, "ADD": 0x2, "STORE": 0x3, "SUB": 0x4, "AND": 0x5,
      "OR": 0x6, "BEQ": 0x7, "JUMP": 0x8, "HALT": 0xF}


# ---------------------------------------------------------------------------
# Montador
# ---------------------------------------------------------------------------
class Programa:
    def __init__(self):
        self.itens = []          # ("label", nome) | (mnemonico, args...)

    def label(self, nome):
        self.itens.append(("label", nome))

    def i(self, *instr):
        self.itens.append(instr)
        return self

    def montar(self):
        rotulos, pos = {}, 0
        for it in self.itens:
            if it[0] == "label":
                rotulos[it[1]] = pos
            else:
                pos += 1
        palavras, textos = [], []
        for it in self.itens:
            if it[0] == "label":
                continue
            m = it[0]
            if m in ("ADD", "SUB", "AND", "OR"):
                _, rd, a, b = it
                w = (OP[m] << 12) | (rd << 8) | (a << 4) | b
                t = f"{m:<5} R{rd}, R{a}, R{b}"
            elif m in ("LOAD", "STORE"):
                _, rd, end = it
                assert 0 <= end <= 0xFF, f"{m}: endereco de dados {end:#x} excede 8 bits"
                w = (OP[m] << 12) | (rd << 8) | end
                t = f"{m:<5} R{rd}, [{end:#04x}]"
            elif m == "BEQ":
                _, rd, rf1, alvo = it
                alvo = rotulos[alvo] if isinstance(alvo, str) else alvo
                assert alvo >> 4 == rf1, (
                    f"BEQ R{rd},R{rf1}: alvo {alvo:#x} fora da pagina {rf1:#x}0..{rf1:#x}F "
                    f"(o nibble alto do alvo e o proprio Rf1)")
                w = (OP[m] << 12) | (rd << 8) | alvo
                t = f"BEQ   R{rd}, R{rf1} -> {alvo:#05x}"
            elif m == "JUMP":
                _, alvo = it
                alvo = rotulos[alvo] if isinstance(alvo, str) else alvo
                assert 0 <= alvo <= 0xFFF
                w = (OP[m] << 12) | alvo
                t = f"JUMP  {alvo:#05x}"
            elif m == "HALT":
                w, t = 0xF000, "HALT"
            else:
                raise ValueError(m)
            palavras.append(w)
            textos.append(t)
        return palavras, textos


# ---------------------------------------------------------------------------
# Simulador da ISA (conta instrucoes executadas, como o Kernel: HALT incluso)
# ---------------------------------------------------------------------------
def simular(palavras, dados, limite=2_000_000):
    mem = [0] * 4096
    for k, w in enumerate(palavras):
        mem[k] = w
    for end, val in dados.items():
        mem[end] = val
    r, pc, n = [0] * 16, 0, 0
    while n < limite:
        ir = mem[pc]
        pc += 1
        n += 1
        op, rd, rf1, rf2, end8 = ir >> 12, (ir >> 8) & 15, (ir >> 4) & 15, ir & 15, ir & 0xFF
        if op == 0x1:   r[rd] = mem[end8]
        elif op == 0x3: mem[end8] = r[rd]
        elif op == 0x2: r[rd] = (r[rf1] + r[rf2]) & 0xFFFF
        elif op == 0x4: r[rd] = (r[rf1] - r[rf2]) & 0xFFFF
        elif op == 0x5: r[rd] = r[rf1] & r[rf2]
        elif op == 0x6: r[rd] = r[rf1] | r[rf2]
        elif op == 0x7:
            if r[rd] == r[rf1]:
                pc = end8
        elif op == 0x8: pc = ir & 0xFFF
        elif op == 0xF: return n, mem
        else: raise RuntimeError(f"opcode invalido {op:#x}")
    raise RuntimeError("job nao terminou (laco infinito?)")


# ---------------------------------------------------------------------------
# Programas
# ---------------------------------------------------------------------------
ZERO, UM, N_ITER, SEMENTE, RES = 0xF0, 0xF1, 0xF2, 0xF3, 0xF4   # pagina 15

# Corpos de laco CPU-bound (so ALU; R3..R7 acumuladores, R2 contador, R1 = 1)
CORPOS_CPU = {
    "soma":   [("ADD", 3, 3, 2), ("ADD", 4, 4, 3), ("AND", 5, 4, 3)],
    "mistura": [("ADD", 3, 3, 2), ("OR", 4, 3, 2), ("AND", 5, 4, 3), ("SUB", 6, 5, 1),
                ("ADD", 3, 3, 6)],
    "cadeia": [("ADD", 3, 3, 2), ("ADD", 4, 3, 1), ("ADD", 5, 4, 3), ("SUB", 6, 5, 4),
               ("OR", 7, 6, 3), ("AND", 4, 7, 5), ("ADD", 3, 3, 4)],
    "longo":  [("ADD", 3, 3, 2), ("SUB", 4, 3, 1), ("AND", 5, 4, 3), ("OR", 6, 5, 2),
               ("ADD", 7, 6, 4), ("SUB", 3, 7, 5), ("ADD", 4, 4, 3), ("OR", 5, 4, 1),
               ("AND", 6, 5, 7), ("ADD", 3, 3, 6)],
}


def prog_cpu(corpo, n_iter, semente=1):
    p = Programa()
    p.i("JUMP", "inicio")                      # 0x00 trampolim
    p.i("STORE", 3, RES)                       # 0x01 epilogo
    p.i("HALT")                                # 0x02
    p.label("inicio")
    p.i("LOAD", 0, ZERO).i("LOAD", 1, UM).i("LOAD", 2, N_ITER).i("LOAD", 3, SEMENTE)
    p.label("laco")
    p.i("BEQ", 2, 0, 0x01)                     # contador == 0 -> epilogo
    for op in CORPOS_CPU[corpo]:
        p.i(*op)
    p.i("SUB", 2, 2, 1)
    p.i("JUMP", "laco")
    dados = {ZERO: 0, UM: 1, N_ITER: n_iter, SEMENTE: semente, RES: 0}
    return p, dados


def prog_mem(acessos, n_iter):
    """acessos: lista de (endereco, 'r'|'rw') percorrida a cada iteracao."""
    p = Programa()
    p.i("JUMP", "inicio")
    p.i("STORE", 3, RES)
    p.i("HALT")
    p.label("inicio")
    p.i("LOAD", 0, ZERO).i("LOAD", 1, UM).i("LOAD", 2, N_ITER).i("LOAD", 3, SEMENTE)
    p.label("laco")
    p.i("BEQ", 2, 0, 0x01)
    for end, modo in acessos:
        p.i("LOAD", 4, end)
        p.i("ADD", 3, 3, 4)
        if modo == "rw":
            p.i("STORE", 3, end)
    p.i("SUB", 2, 2, 1)
    p.i("JUMP", "laco")
    dados = {ZERO: 0, UM: 1, N_ITER: n_iter, SEMENTE: 3, RES: 0}
    for end, _ in acessos:
        dados.setdefault(end, (end * 7 + 1) & 0xFFFF)   # valor deterministico
    return p, dados


def prog_curto(a, b):
    """Linha reta, sem laco (estilo job1_soma): so 5-6 instrucoes."""
    p = Programa()
    p.i("LOAD", 0, 0x0A).i("LOAD", 1, 0x0B).i("ADD", 2, 0, 1)
    p.i("SUB", 3, 2, 1).i("STORE", 2, 0x0C).i("HALT")
    return p, {0x0A: a, 0x0B: b, 0x0C: 0}


def end_pag(pagina, desloc=0):
    return pagina * TAM_PAGINA + desloc


# ---------------------------------------------------------------------------
# Definicao dos lotes
# ---------------------------------------------------------------------------
def lote_cpu():
    # (corpo, iteracoes)  -> de ~250 a ~3800 instrucoes executadas
    cfg = [("soma", 40), ("mistura", 34), ("cadeia", 60), ("soma", 160), ("longo", 40),
           ("mistura", 125), ("cadeia", 150), ("longo", 120), ("soma", 600), ("longo", 300)]
    jobs = []
    for k, (corpo, n) in enumerate(cfg, 1):
        p, d = prog_cpu(corpo, n, semente=k)
        jobs.append((f"cpu_{k:02d}_{corpo}", p, d, None, None,
                     f"CPU-bound: laco '{corpo}' com {n} iteracoes (so ALU, ~2 paginas de codigo)"))
    return jobs


def lote_mem():
    X = end_pag
    cfg = [
        # (descricao, acessos por iteracao, iteracoes)
        ("ciclo de 3 paginas, so leitura",
         [(X(3), "r"), (X(7), "r"), (X(11), "r")], 40),
        ("ciclo de 4 paginas, leitura/escrita",
         [(X(3), "rw"), (X(6), "rw"), (X(9), "rw"), (X(12), "rw")], 30),
        ("ciclo de 5 paginas (ruim para LRU com poucas molduras)",
         [(X(3), "r"), (X(5), "r"), (X(7), "r"), (X(9), "r"), (X(11), "r")], 40),
        ("ciclo de 6 paginas, metade com escrita",
         [(X(3), "rw"), (X(4), "r"), (X(7), "rw"), (X(8), "r"), (X(11), "rw"), (X(12), "r")], 30),
        ("localidade: 1 pagina quente (3x) + 1 fria por iteracao",
         [(X(4, 0), "rw"), (X(4, 1), "rw"), (X(4, 2), "rw"), (X(10), "r")], 60),
        ("ciclo de 8 paginas, leitura",
         [(X(3 + i), "r") for i in range(8)], 20),
        ("2 paginas quentes alternadas, escrita pesada",
         [(X(5), "rw"), (X(9), "rw"), (X(5, 4), "rw"), (X(9, 4), "rw")], 70),
        ("ciclo de 7 paginas espalhadas (0x30..0xE0), leitura/escrita",
         [(X(p), "rw") for p in (3, 5, 7, 9, 11, 13, 14)], 25),
        ("ciclo longo de 4 paginas, 90 iteracoes",
         [(X(3), "r"), (X(8), "rw"), (X(10), "r"), (X(13), "rw")], 90),
        ("varredura de 10 paginas, leitura (grande volume de faltas)",
         [(X(3 + i), "r") for i in range(10)], 24),
    ]
    jobs = []
    for k, (desc, acessos, n) in enumerate(cfg, 1):
        p, d = prog_mem(acessos, n)
        pags = sorted({a >> 4 for a, _ in acessos})
        jobs.append((f"mem_{k:02d}", p, d, None, None,
                     f"Memory-bound: {desc}; {n} iteracoes; paginas de dados {pags}"))
    return jobs


def lote_misto():
    X = end_pag
    # (nome, programa, prioridade, descricao). .estimate = instrucoes executadas exatas.
    # Prioridade: MAIOR numero = MAIOR prioridade. Os jobs longos recebem prioridade
    # alta e os curtos baixa de proposito: assim Prioridade difere de SJN e o aging
    # importa (jobs curtos de baixa prioridade correm risco de inanicao).
    esp = []
    esp.append(("curto_a", prog_curto(7, 5), 2, "linha reta, 6 instrucoes"))
    esp.append(("cpu_longo", prog_cpu("longo", 200, 1), 8, "CPU-bound longo (~2400 instr.)"))
    esp.append(("mem_ciclo", prog_mem([(X(3), "rw"), (X(6), "r"), (X(9), "rw"), (X(12), "r")], 35), 5,
                "memory-bound, ciclo de 4 paginas"))
    esp.append(("curto_b", prog_curto(100, 23), 1, "linha reta, 6 instrucoes"))
    esp.append(("cpu_medio", prog_cpu("mistura", 80, 4), 5, "CPU-bound medio (~750 instr.)"))
    esp.append(("mem_quente", prog_mem([(X(4, 0), "rw"), (X(4, 1), "rw"), (X(10), "r")], 80), 3,
                "memory-bound com pagina quente"))
    esp.append(("cpu_curto", prog_cpu("soma", 12, 6), 9, "CPU-bound curto, prioridade maxima"))
    esp.append(("mem_varredura", prog_mem([(X(3 + i), "r") for i in range(6)], 22), 4,
                "memory-bound, varredura de 6 paginas"))
    esp.append(("cpu_cadeia", prog_cpu("cadeia", 110, 8), 7, "CPU-bound medio-longo (~1000 instr.)"))
    esp.append(("mem_misto", prog_mem([(X(3), "rw"), (X(5), "r"), (X(7), "rw"), (X(9), "r"),
                                       (X(11), "rw")], 45), 2, "memory-bound, 5 paginas"))
    jobs = []
    for k, (nome, (p, d), prio, desc) in enumerate(esp, 1):
        jobs.append((f"misto_{k:02d}_{nome}", p, d, prio, True, f"Misto: {desc}"))
    return jobs


# ---------------------------------------------------------------------------
# Escrita
# ---------------------------------------------------------------------------
def escrever_job(pasta, nome, prog, dados, prioridade, com_estimate, descricao):
    palavras, textos = prog.montar()
    n, _ = simular(palavras, dados)

    # Nenhum dado pode cair dentro do codigo (o loader sobrescreveria as instrucoes).
    for end in dados:
        assert end >= len(palavras), f"{nome}: dado em {end:#x} colide com o codigo (0..{len(palavras)-1:#x})"

    linhas = [f"// {nome}.txt - {descricao}",
              f"// Instrucoes estaticas: {len(palavras)} | executadas: {n} (HALT incluso)",
              "// Gerado por scripts/gerar_lotes.py - nao editar a mao."]
    if prioridade is not None:
        linhas.append(f".priority {prioridade}")
    if com_estimate:
        linhas.append(f".estimate {n}")
    linhas.append("")
    for k, (w, t) in enumerate(zip(palavras, textos)):
        linhas.append(f"{w:04X} // {k:#05x}: {t}")
    linhas += ["", ".data"]
    for end in sorted(dados):
        linhas.append(f"{end:02X} {dados[end] & 0xFFFF:04X}")
    caminho = os.path.join(pasta, nome + ".txt")
    with open(caminho, "w", newline="\n") as f:
        f.write("\n".join(linhas) + "\n")

    paginas_cod = (len(palavras) + TAM_PAGINA - 1) // TAM_PAGINA
    paginas_dados = sorted({e >> 4 for e in dados})
    return dict(arquivo=os.path.relpath(caminho, RAIZ).replace(os.sep, "/"),
                estaticas=len(palavras), executadas=n, prioridade=prioridade,
                paginas_codigo=paginas_cod, paginas_dados=len(paginas_dados))


def main():
    lotes = {"cpu_bound": lote_cpu(), "memory_bound": lote_mem(), "misto": lote_misto()}
    resumo = []
    for pasta_nome, jobs in lotes.items():
        pasta = os.path.join(DESTINO, pasta_nome)
        os.makedirs(pasta, exist_ok=True)
        for antigo in os.listdir(pasta):               # regrava do zero
            if antigo.endswith(".txt"):
                os.remove(os.path.join(pasta, antigo))
        for nome, prog, dados, prio, est, desc in jobs:
            info = escrever_job(pasta, nome, prog, dados, prio, est, desc)
            info["lote"] = pasta_nome
            resumo.append(info)

    with open(os.path.join(DESTINO, "perfil_esperado.csv"), "w", newline="\n") as f:
        f.write("lote,arquivo,instrucoes_estaticas,instrucoes_executadas,prioridade,"
                "paginas_codigo,paginas_dados\n")
        for r in resumo:
            f.write(f"{r['lote']},{r['arquivo']},{r['estaticas']},{r['executadas']},"
                    f"{'' if r['prioridade'] is None else r['prioridade']},"
                    f"{r['paginas_codigo']},{r['paginas_dados']}\n")

    for pasta_nome in lotes:
        rs = [r for r in resumo if r["lote"] == pasta_nome]
        ex = [r["executadas"] for r in rs]
        print(f"{pasta_nome:13s} {len(rs)} jobs | instrucoes executadas: min {min(ex)}, "
              f"max {max(ex)}, total {sum(ex)}")


if __name__ == "__main__":
    sys.exit(main())