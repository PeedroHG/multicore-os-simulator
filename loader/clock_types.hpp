#ifndef CLOCK_TYPES_HPP
#define CLOCK_TYPES_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <utility>

/**
 * JobData - Estrutura que armazena os dados brutos de um Job lido do arquivo .txt
 */
struct JobData {
    int id;
    std::string nomeArquivo;
    std::vector<uint16_t> instrucoes;
    std::vector<std::pair<uint16_t, uint16_t>> variaveis; // {endereco_relativo, valor}
};

/**
 * JobMetrics - Estrutura consolidada com todas as métricas de execução de um Job
 */
struct JobMetrics {
    int idJob;
    std::string nomeJob;
    uint32_t instrucoesExecutadas;
    uint32_t acessosRAM;
    uint32_t acessosDisco;
    uint32_t clockTotal;
    uint16_t pcFinal;
    uint16_t irFinal;
    uint16_t registradores[8]; // Estado de R0 até R7
    bool erroKernel;
};

#endif // CLOCK_TYPES_HPP
