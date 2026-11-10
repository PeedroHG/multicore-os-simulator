#ifndef REPORTER_HPP
#define REPORTER_HPP

#include "clock_types.hpp"
#include "metrics.hpp"   // ResultadoExecucao (API multicore)
#include <iostream>
#include <ostream>
#include <string>

class Reporter {
public:
    static void exibirTerminal(const JobMetrics& metricas);

    static void salvarOutputDat(const std::string& caminhoArquivo, const JobMetrics& metricas);
    static bool exportar(const std::string& diretorio, const ResultadoExecucao& resultado);

    // Resumo curto das metricas globais e por nucleo no terminal.
    static void exibirResumo(const ResultadoExecucao& resultado, std::ostream& saida = std::cout);
};

#endif 