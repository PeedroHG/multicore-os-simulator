#include "kernel.hpp"
#include <iostream>
#include <algorithm>

Kernel::Kernel(int nCores, uint32_t custoTroca, uint32_t limiteInst)
    : numCores(std::max(1, nCores)),
      tickGlobal(0),
      custoTrocaContexto(custoTroca),
      limiteInstrucoesPorJob(limiteInst) {
    // Inicializa o vetor de núcleos dando o ID para cada um
    cores.reserve(numCores);
    for (int i = 0; i < numCores; ++i) {
        cores.emplace_back(i);
    }
    contadoresNucleo.assign(numCores, ContadoresNucleo{});
    ultimoPidNucleo.assign(numCores, -1);
    segmentoAberto.assign(numCores, -1);
}

void Kernel::admitirProcesso(std::unique_ptr<PCB> pcb) {
    pcb->cicloChegada = tickGlobal;
    pcb->ultimoEntrouPronto = tickGlobal;
    pcb->estado = EstadoProcesso::PRONTO;

    processosProntos.push_back(pcb.get());
    todosProcessos.push_back(std::move(pcb));
}

void Kernel::conectarMMU(InterfaceMMU* m) {
    mmu = m;
    for (auto& core : cores) {
        core.conectarMMU(m);
    }
}

void Kernel::fecharSegmento(int nucleo, uint64_t tick) {
    if (segmentoAberto[nucleo] >= 0) {
        segmentos[segmentoAberto[nucleo]].fim = tick;
        segmentoAberto[nucleo] = -1;
    }
}

void Kernel::encerrarProcesso(CPU& core, PCB* pcb, EstadoProcesso estadoFinal, uint64_t cicloFim) {
    pcb->estado = estadoFinal;
    pcb->cicloFim = cicloFim;
    if (mmu) {
        mmu->liberarProcesso(*pcb); // devolve as molduras ao terminar/abortar
    }
    fecharSegmento(static_cast<int>(&core - cores.data()), cicloFim);
    core.desalocarProcesso();
}

bool Kernel::temProcessosAtivos() const {
    // Verifica se ainda há alguém na fila de prontos
    if (!processosProntos.empty()) {
        return true;
    }

    // Ou se algum núcleo ainda está ocupado trabalhando (ou em stall de uma
    // falta de página gerada pelo HALT do último processo)
    for (const auto& core : cores) {
        if (!core.estaLivre() || core.emStall()) {
            return true;
        }
    }

    return false;
}

// Executa 1 ciclo de clock global em lockstep
void Kernel::executarPasso() {
    // 2. Itera por cada núcleo de 0 até numCores - 1 de forma síncrona
    for (int i = 0; i < numCores; ++i) {
        CPU& core = cores[i];

        // Se o núcleo está em stall (espera de memória/disco), apenas decrementa o tempo
        if (core.emStall()) {
            core.decrementarStall();
            if (PCB* p = core.getProcessoAtual()) {
                p->ciclosStall++;
                contadoresNucleo[i].ciclosStall++;
                if (!core.emStall() && core.isHalted()) {
                    core.salvarContexto(*p);
                    encerrarProcesso(core, p, EstadoProcesso::FINALIZADO, tickGlobal + 1);
                }
            }
            continue;
        }

        // Se o núcleo está livre, tenta pegar o próximo processo da fila de prontos
        if (core.estaLivre() && !processosProntos.empty()) {
            PCB* proximo = processosProntos.front();
            processosProntos.erase(processosProntos.begin());

            // Se for a primeira vez que o processo roda, registra o início
            if (proximo->cicloInicio == 0 && tickGlobal > 0) {
                proximo->cicloInicio = tickGlobal;
            }

            // Métricas: despacho e troca de contexto (só conta quando o PID muda no núcleo)
            contadoresNucleo[i].despachos++;
            if (ultimoPidNucleo[i] != proximo->pid) {
                contadoresNucleo[i].trocasContexto++;
                proximo->trocasContexto++;
            }
            ultimoPidNucleo[i] = proximo->pid;

            segmentos.push_back({i, proximo->pid, tickGlobal, tickGlobal});
            segmentoAberto[i] = static_cast<int>(segmentos.size()) - 1;

            core.restaurarContexto(*proximo);
        }

        // Se o núcleo tem um processo alocado, executa 1 instrução
        if (!core.estaLivre()) {
            PCB* atual = core.getProcessoAtual();
            atual->ciclosExecucao++;
            atual->quantumUsado++;
            contadoresNucleo[i].ciclosExecucao++;

            try {
                // Proteção contra loops infinitos
                if (atual->instrucoesExecutadas >= limiteInstrucoesPorJob) {
                    encerrarProcesso(core, atual, EstadoProcesso::ABORTADO, tickGlobal + 1);
                    continue;
                }

                // Executa 1 ciclo completo de instrução no núcleo
                core.setTick(tickGlobal);
                core.step();
                atual->instrucoesExecutadas++;

                // Faltas de página geradas pela instrução (busca e/ou dados):
                // o núcleo fica em stall pelos ciclos devolvidos pela MMU.
                const uint32_t penalidade = core.getUltimaPenalidade();

                if (penalidade > 0) {
                    // Se houve falta de página (inclusive na busca do HALT),
                    // o processo permanece no núcleo mantendo suas molduras
                    // alocadas até o término dos ciclos de stall.
                    core.adicionarStall(penalidade);
                } else if (core.isHalted()) {
                    core.salvarContexto(*atual);
                    encerrarProcesso(core, atual, EstadoProcesso::FINALIZADO, tickGlobal + 1);
                }
            } catch (const std::exception& e) {
                // Erro em tempo de execução (ex: opcode inválido ou SEGFAULT da MMU)
                core.salvarContexto(*atual);
                encerrarProcesso(core, atual, EstadoProcesso::ABORTADO, tickGlobal + 1);
            }
        }
    }

    // Tempo de espera (TE_i): só quem continua na fila de prontos DEPOIS do despacho
    // deste tick esperou; quem foi despachado agora já executou neste mesmo tick.
    // (Contar antes do despacho somava +1 em todo processo: TE + exec + stall > cicloFim.)
    for (PCB* pcb : processosProntos) {
        pcb->ciclosEspera++;
    }

    tickGlobal++;
}

void Kernel::rodarSimulacao() {
    while (temProcessosAtivos()) {
        executarPasso();
    }
}