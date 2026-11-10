#include "metrics.hpp"
#include "clock_core.hpp"

#include <algorithm>
#include <sstream>

// ============================================================================
// PARTE 1 - Coletor LEGADO (sem alteracoes)
// ============================================================================

JobMetrics MetricsTracker::metricasAtuais = {};

void MetricsTracker::iniciarJob(int id, const std::string& nome) {
    metricasAtuais = JobMetrics{};
    metricasAtuais.idJob = id;
    metricasAtuais.nomeJob = nome;
}

void MetricsTracker::registrarInstrucao(uint32_t qtd) {
    metricasAtuais.instrucoesExecutadas += qtd;
}
void MetricsTracker::registrarAcessoRAM(uint32_t qtd) {
    metricasAtuais.acessosRAM += qtd;
}
void MetricsTracker::registrarAcessoDisco(uint32_t qtd) {
    metricasAtuais.acessosDisco += qtd;
}
void MetricsTracker::capturarEstadoCPU(uint16_t pc, uint16_t ir, const uint16_t regs[8]) {
    metricasAtuais.pcFinal = pc;
    metricasAtuais.irFinal = ir;
    for (int i = 0; i < 8; ++i) {
        metricasAtuais.registradores[i] = regs[i];
    }
}
void MetricsTracker::marcarErroKernel() {
    metricasAtuais.erroKernel = true;
}
void MetricsTracker::sincronizar(uint32_t instrucoes, uint32_t ram, uint32_t disco, uint32_t clock) {
    metricasAtuais.instrucoesExecutadas = instrucoes;
    metricasAtuais.acessosRAM = ram;
    metricasAtuais.acessosDisco = disco;
    metricasAtuais.clockTotal = clock;
}
JobMetrics MetricsTracker::obterMetricas() {
    metricasAtuais.clockTotal = ClockCore::getCycles();
    return metricasAtuais;
}

// ============================================================================
// PARTE 2 - Metricas multicore
// ============================================================================

namespace {

constexpr double CICLOS_POR_MIL = 1000.0;

// Divisao segura: denominador zero devolve 0 (execucao vazia).
double razao(double numerador, double denominador) {
    return denominador > 0.0 ? numerador / denominador : 0.0;
}

} // namespace

void Metricas::consolidar(ResultadoExecucao& r) {
    // Ordem canonica => saida deterministica independente da ordem de insercao.
    std::sort(r.processos.begin(), r.processos.end(),
              [](const MetricasProcesso& a, const MetricasProcesso& b) { return a.pid < b.pid; });
    std::sort(r.nucleos.begin(), r.nucleos.end(),
              [](const MetricasNucleo& a, const MetricasNucleo& b) { return a.id < b.id; });

    ResumoSistema& s = r.resumo;

    // ---- Por processo + makespan ----------------------------------------
    uint64_t somaTE = 0, somaTR = 0, makespan = 0;
    s.concluidos = 0;
    s.abortados = 0;
    s.faltasPagina = s.acessosMemoria = s.writeBacks = 0;
    s.instrucoes = s.ciclosStall = 0;

    for (auto& p : r.processos) {
        p.tempoRetorno = p.cicloFim;            // TR_i = cicloFim_i - 0
        makespan = std::max(makespan, p.cicloFim);

        if (p.abortado) {
            ++s.abortados;                      // reportados a parte (Secao 2.4)
        } else {
            ++s.concluidos;
            somaTE += p.tempoEspera;
            somaTR += p.tempoRetorno;
        }
        s.faltasPagina   += p.faltasPagina;
        s.acessosMemoria += p.acessosMemoria;
        s.writeBacks     += p.writeBacks;
        s.instrucoes     += p.instrucoes;
    }
    s.makespan = makespan;
    s.tme = razao(static_cast<double>(somaTE), s.concluidos);
    s.tmr = razao(static_cast<double>(somaTR), s.concluidos);
    s.taxaFaltas = razao(static_cast<double>(s.faltasPagina),
                         static_cast<double>(s.acessosMemoria));

    // ---- Por nucleo ------------------------------------------------------
    uint64_t somaOcupado = 0, somaExec = 0;
    s.trocasContexto = 0;
    s.ciclosTroca = 0;
    s.ciclosStall = 0;

    for (auto& k : r.nucleos) {
        k.ocupado = k.ciclosExecucao + k.ciclosStall + k.ciclosTroca;
        k.ocioso = makespan > k.ocupado ? makespan - k.ocupado : 0;
        k.eficiencia     = razao(static_cast<double>(k.ocupado),        static_cast<double>(makespan));
        k.eficienciaUtil = razao(static_cast<double>(k.ciclosExecucao), static_cast<double>(makespan));

        somaOcupado += k.ocupado;
        somaExec    += k.ciclosExecucao;
        s.trocasContexto += k.trocasContexto;
        s.ciclosTroca    += k.ciclosTroca;
        s.ciclosStall    += k.ciclosStall;
    }

    // ---- Sistema ---------------------------------------------------------
    const double capacidade = static_cast<double>(r.nucleos.size()) * static_cast<double>(makespan);
    s.utilizacaoMedia = razao(static_cast<double>(somaOcupado), capacidade);
    s.utilizacaoUtil  = razao(static_cast<double>(somaExec),    capacidade);
    s.throughput      = razao(static_cast<double>(s.concluidos), static_cast<double>(makespan)) * CICLOS_POR_MIL;

    // Speedup so vale depois de aplicarSpeedup(); consolidar invalida o anterior.
    s.speedupValido = false;
    s.makespanBase = 0;
    s.speedup = 0.0;
    s.eficienciaParalela = 0.0;
}

void Metricas::aplicarSpeedup(ResultadoExecucao& r, uint64_t makespanBase) {
    ResumoSistema& s = r.resumo;
    if (makespanBase == 0 || s.makespan == 0 || r.nucleos.empty()) {
        s.speedupValido = false;
        return;
    }
    s.makespanBase = makespanBase;
    s.speedup = static_cast<double>(makespanBase) / static_cast<double>(s.makespan);
    s.eficienciaParalela = s.speedup / static_cast<double>(r.nucleos.size());
    s.speedupValido = true;
}

std::vector<std::string> Metricas::verificarConsistencia(const ResultadoExecucao& r) {
    std::vector<std::string> erros;

    uint64_t execProc = 0, stallProc = 0, trocasProc = 0;
    for (const auto& p : r.processos) {
        execProc   += p.ciclosExecucao;
        stallProc  += p.ciclosStall;
        trocasProc += p.trocasContexto;

        // Todo ciclo de vida (chegada=0) e dividido em espera + execucao + stall (+ troca).
        if (p.tempoEspera + p.ciclosExecucao + p.ciclosStall > p.cicloFim) {
            std::ostringstream m;
            m << "PID " << p.pid << ": TE + exec + stall ("
              << (p.tempoEspera + p.ciclosExecucao + p.ciclosStall)
              << ") excede cicloFim (" << p.cicloFim << ")";
            erros.push_back(m.str());
        }
    }

    uint64_t execNuc = 0, stallNuc = 0, trocasNuc = 0;
    for (const auto& k : r.nucleos) {
        execNuc   += k.ciclosExecucao;
        stallNuc  += k.ciclosStall;
        trocasNuc += k.trocasContexto;
        if (k.ocupado > r.resumo.makespan) {
            std::ostringstream m;
            m << "Nucleo " << k.id << ": ocupado (" << k.ocupado
              << ") excede o makespan (" << r.resumo.makespan << ")";
            erros.push_back(m.str());
        }
    }

    auto comparar = [&erros](const char* nome, uint64_t proc, uint64_t nuc) {
        if (proc != nuc) {
            std::ostringstream m;
            m << nome << ": soma por processo (" << proc
              << ") != soma por nucleo (" << nuc << ")";
            erros.push_back(m.str());
        }
    };
    comparar("Ciclos de execucao", execProc, execNuc);
    comparar("Ciclos de stall", stallProc, stallNuc);
    comparar("Trocas de contexto", trocasProc, trocasNuc);

    return erros;
}