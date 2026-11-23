#!/usr/bin/env python3
"""
Valida tests/lotes/ contra o simulador real (Pessoa 4).

Roda cada lote (1 nucleo, memoria folgada) e confere, para cada job, se o numero
de instrucoes executadas e o esperado (tests/lotes/perfil_esperado.csv, calculado
por scripts/gerar_lotes.py) e se o job terminou sem abortar.

Uso: python3 scripts/validar_lotes.py [--sem-diretivas]
  --sem-diretivas  remove .priority/.estimate antes de rodar (para validar os
                   lotes enquanto o parser nao suporta as diretivas).
"""
import csv, glob, os, subprocess, sys, tempfile

RAIZ = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SIM = os.path.join(RAIZ, "simulador")


def main():
    sem_dir = "--sem-diretivas" in sys.argv
    if not os.path.exists(SIM):
        print("simulador nao encontrado: rode 'make' antes.")
        return 2
    esperado = {r["arquivo"]: int(r["instrucoes_executadas"])
                for r in csv.DictReader(open(os.path.join(RAIZ, "tests/lotes/perfil_esperado.csv")))}
    falhas = 0
    with tempfile.TemporaryDirectory() as tmp:
        for lote in ("cpu_bound", "memory_bound", "misto"):
            arqs = sorted(glob.glob(os.path.join(RAIZ, "tests/lotes", lote, "*.txt")))
            if sem_dir:
                novos = []
                for a in arqs:
                    dst = os.path.join(tmp, lote + "_" + os.path.basename(a))
                    with open(a) as f, open(dst, "w") as g:
                        g.writelines(l for l in f if not l.startswith((".priority", ".estimate")))
                    novos.append(dst)
                arqs = novos
            saida = os.path.join(tmp, "saida_" + lote)
            subprocess.run([SIM, "--cores", "1", "--molduras", "4000", "--saida", saida] + arqs,
                           capture_output=True, cwd=RAIZ)
            try:
                linhas = list(csv.DictReader(open(os.path.join(saida, "processos.csv"))))
            except FileNotFoundError:
                print(f"[FALHA] {lote}: o simulador nao gerou processos.csv (parser rejeitou algum job?)")
                falhas += 1
                continue
            if len(linhas) != len(arqs):
                print(f"[FALHA] {lote}: {len(linhas)} de {len(arqs)} jobs foram carregados")
                falhas += 1
            for r in linhas:
                nome = os.path.basename(r["nome"]).replace(lote + "_", "", 1)
                chave = f"tests/lotes/{lote}/{nome}"
                ok = r["estado"] == "FINALIZADO" and int(r["instrucoes"]) == esperado.get(chave, -1)
                if not ok:
                    falhas += 1
                    print(f"[FALHA] {chave}: esperado {esperado.get(chave)}, obtido {r['instrucoes']} ({r['estado']})")
            print(f"[OK]    {lote}: {len(linhas)} jobs conferidos")
    print("LOTES VALIDOS" if falhas == 0 else f"{falhas} PROBLEMA(S) NOS LOTES")
    return 1 if falhas else 0


if __name__ == "__main__":
    sys.exit(main())