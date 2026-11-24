#include "coletor.hpp"

#include <algorithm>
#include <string>

ResultadoExecucao coletarResultado(const Kernel& kernel, const MMU& mmu,
                                   const ConfigExperimento& config) {
    ResultadoExecucao r;
    r.config = config;

    for (const auto& pcb : kernel.getProcessos()) {
        const EstatisticasMemoria mem = mmu.getEstatisticas(pcb->pid);

        MetricasProcesso p;
        p.pid            = pcb->pid;
        p.nome           = pcb->nomeArquivo;
        p.abortado       = (pcb->estado == EstadoProcesso::ABORTADO);
        p.prioridade     = pcb->prioridadeBase;
        p.cicloInicio    = pcb->cicloInicio;
        p.cicloFim       = pcb->cicloFim;
        p.tempoEspera    = pcb->ciclosEspera;
        p.ciclosExecucao = pcb->ciclosExecucao;
        p.ciclosStall    = pcb->ciclosStall;
        p.instrucoes     = pcb->instrucoesExecutadas;
        p.trocasContexto = pcb->trocasContexto;
        p.faltasPagina   = static_cast<uint32_t>(mem.faltas);
        p.acessosMemoria = mem.acessos;
        p.writeBacks     = mem.writebacks;
        r.processos.push_back(p);
    }

    const auto& contadores = kernel.getContadoresNucleo();
    for (std::size_t k = 0; k < contadores.size(); ++k) {
        MetricasNucleo n;
        n.id             = static_cast<int>(k);
        n.ciclosExecucao = contadores[k].ciclosExecucao;
        n.ciclosStall    = contadores[k].ciclosStall;
        n.ciclosTroca    = contadores[k].ciclosTroca;
        n.despachos      = contadores[k].despachos;
        n.trocasContexto = contadores[k].trocasContexto;
        r.nucleos.push_back(n);
    }

    // Linha do tempo de ocupacao + eventos legiveis para o execucao.log
    for (const auto& seg : kernel.getSegmentos()) {
        r.timeline.push_back({seg.nucleo, seg.pid, seg.inicio, seg.fim});
    }
    struct Evento { uint64_t t; int ordem; std::string linha; };
    std::vector<Evento> evs;
    for (const auto& seg : r.timeline) {
        evs.push_back({seg.inicio, 1, "[t=" + std::to_string(seg.inicio) + "] core " +
                       std::to_string(seg.nucleo) + " despacha PID " + std::to_string(seg.pid)});
        evs.push_back({seg.fim, 0, "[t=" + std::to_string(seg.fim) + "] core " +
                       std::to_string(seg.nucleo) + " libera PID " + std::to_string(seg.pid)});
    }
    std::stable_sort(evs.begin(), evs.end(), [](const Evento& a, const Evento& b) {
        return a.t != b.t ? a.t < b.t : a.ordem < b.ordem;
    });
    for (const auto& e : evs) r.eventos.push_back(e.linha);

    Metricas::consolidar(r);
    return r;
}