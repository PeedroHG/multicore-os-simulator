#ifndef METRICS_HPP
#define METRICS_HPP

#include "clock_types.hpp"
#include <cstdint>
#include <string>
#include <vector>

// ============================================================================
// PARTE 1 - Coletor LEGADO (baseline single-core). Mantido sem alteracoes para
// nao quebrar o BatchManager atual; o caminho multicore usa a PARTE 2.
// ============================================================================
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

// ============================================================================
// PARTE 2 - Metricas multicore (Secao 2.4 do PLANO_DE_DESENVOLVIMENTO.md)
//
// Divisao de trabalho:
//   * O Kernel PREENCHE os contadores brutos (campos marcados [KERNEL]/[MMU]).
//   * Metricas::consolidar() CALCULA todos os campos derivados ([CALCULADO]).
// Convencoes: chegada = 0 para todos os processos; "ciclo" = tick global;
// cicloFim = tick em que o processo termina (ou aborta).
// ============================================================================

/// Parametros do experimento (vao para resumo.csv e para o log).
struct ConfigExperimento {
    std::string politica      = "fcfs";   // fcfs | sjn | rr | prio
    std::string modo          = "np";     // np | p
    std::string substituicao  = "fifo";   // fifo | lru
    uint32_t nucleos          = 1;
    uint32_t quantum          = 0;        // 0 = sem quantum (nao preemptivo)
    uint32_t molduras         = 0;
    uint32_t penalidadeFalta  = 0;
    uint32_t custoTroca       = 0;
    uint32_t aging            = 0;
};

/// Metricas de UM processo (uma linha de processos.csv).
struct MetricasProcesso {
    int         pid        = 0;     // [KERNEL]
    std::string nome;               // [KERNEL]
    bool        abortado   = false; // [KERNEL] SEGFAULT, opcode invalido, limite de instrucoes
    int         prioridade = 0;     // [KERNEL] prioridade base

    uint64_t cicloInicio   = 0;     // [KERNEL] tick do primeiro despacho
    uint64_t cicloFim      = 0;     // [KERNEL] tick de termino
    uint64_t tempoEspera   = 0;     // [KERNEL] TE_i: ciclos em PRONTO (sem stall/troca)
    uint64_t ciclosExecucao= 0;     // [KERNEL] ciclos de execucao efetiva
    uint64_t ciclosStall   = 0;     // [KERNEL] ciclos parado por falta de pagina
    uint64_t instrucoes    = 0;     // [KERNEL] instrucoes executadas
    uint32_t trocasContexto= 0;     // [KERNEL] so conta quando o PID muda no nucleo
    uint32_t faltasPagina  = 0;     // [MMU]
    uint64_t acessosMemoria= 0;     // [MMU]
    uint64_t writeBacks    = 0;     // [MMU]

    uint64_t tempoRetorno  = 0;     // [CALCULADO] TR_i = cicloFim_i - chegada(=0)
};

/// Metricas de UM nucleo (uma linha de nucleos.csv).
struct MetricasNucleo {
    int      id             = 0;    // [KERNEL]
    uint64_t ciclosExecucao = 0;    // [KERNEL] ciclos com instrucao em execucao
    uint64_t ciclosStall    = 0;    // [KERNEL] ciclos parado por falta de pagina
    uint64_t ciclosTroca    = 0;    // [KERNEL] ciclos gastos em troca de contexto
    uint32_t despachos      = 0;    // [KERNEL] processos despachados neste nucleo
    uint32_t trocasContexto = 0;    // [KERNEL] despachos que mudaram o PID

    uint64_t ocupado        = 0;    // [CALCULADO] exec + stall + troca
    uint64_t ocioso         = 0;    // [CALCULADO] makespan - ocupado
    double   eficiencia     = 0.0;  // [CALCULADO] ocupado_k / makespan
    double   eficienciaUtil = 0.0;  // [CALCULADO] exec_k / makespan
};

/// Metricas globais do sistema (uma linha de resumo.csv). Tudo [CALCULADO].
struct ResumoSistema {
    uint32_t concluidos = 0;        // N (processos FINALIZADOS)
    uint32_t abortados  = 0;
    uint64_t makespan   = 0;        // max(cicloFim) sobre todos os processos

    double tme = 0.0;               // Tempo Medio de Espera   = (1/N) * soma TE_i
    double tmr = 0.0;               // Tempo Medio de Retorno  = (1/N) * soma TR_i
    double utilizacaoMedia = 0.0;   // soma ocupado_k / (n * makespan)
    double utilizacaoUtil  = 0.0;   // soma exec_k    / (n * makespan)
    double throughput      = 0.0;   // N / makespan * 1000 (processos por 1.000 ciclos)

    bool     speedupValido      = false; // false se nao houver baseline n=1
    uint64_t makespanBase       = 0;     // makespan(1)
    double   speedup            = 0.0;   // S(n) = makespan(1) / makespan(n)
    double   eficienciaParalela = 0.0;   // E(n) = S(n) / n

    uint64_t trocasContexto = 0;
    uint64_t faltasPagina   = 0;
    uint64_t acessosMemoria = 0;
    double   taxaFaltas     = 0.0;   // faltas / acessos
    uint64_t writeBacks     = 0;
    uint64_t instrucoes     = 0;
    uint64_t ciclosStall    = 0;
    uint64_t ciclosTroca    = 0;
};

/// Tudo o que uma execucao produz; entrada do Reporter::exportar().
struct ResultadoExecucao {
    ConfigExperimento             config;
    std::vector<MetricasProcesso> processos;
    std::vector<MetricasNucleo>   nucleos;
    ResumoSistema                 resumo;
    std::vector<std::string>      eventos;  // linhas ja formatadas pelo Kernel (opcional)
};

class Metricas {
public:
    /// Ordena por PID/id e calcula TR, makespan, TME, TMR, utilizacao,
    /// eficiencia por nucleo, throughput e totais de memoria.
    static void consolidar(ResultadoExecucao& r);

    /// S(n) = makespan(1)/makespan(n); E(n) = S(n)/n. Chamar APOS consolidar().
    static void aplicarSpeedup(ResultadoExecucao& r, uint64_t makespanBase);

    /// Confere invariantes entre contadores por processo e por nucleo.
    /// Retorna uma mensagem por violacao (vazio = tudo consistente).
    static std::vector<std::string> verificarConsistencia(const ResultadoExecucao& r);
};

#endif // METRICS_HPP