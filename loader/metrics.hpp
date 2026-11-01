#ifndef METRICS_HPP
#define METRICS_HPP

#include "clock_types.hpp"
#include <string>
#include <cstdint>

/**
 * MetricsTracker - Coleta métricas de telemetria da CPU, Memória e Disco por Job
 * Integrante responsável: Integrante 3
 */
class MetricsTracker {
private:
    static JobMetrics metricasAtuais;

public:
    static void iniciarJob(int id, const std::string& nome);
    static void registrarInstrucao(uint32_t qtd = 1);
    static void registrarAcessoRAM(uint32_t qtd = 1);
    static void registrarAcessoDisco(uint32_t qtd = 1);
    static void capturarEstadoCPU(uint16_t pc, uint16_t ir, const uint16_t regs[8]);
    static void marcarErroKernel();
    static void sincronizar(uint32_t instrucoes, uint32_t ram, uint32_t disco, uint32_t clock);
    static JobMetrics obterMetricas();
};

#endif // METRICS_HPP
