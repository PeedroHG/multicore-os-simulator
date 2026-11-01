#include "metrics.hpp"
#include "clock_core.hpp"

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