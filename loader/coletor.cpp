#include "coletor.hpp"

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

    Metricas::consolidar(r);
    return r;
}