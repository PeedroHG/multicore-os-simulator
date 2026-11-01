#ifndef REPORTER_HPP
#define REPORTER_HPP

#include "clock_types.hpp"
#include <string>

/**
 * Reporter - Formatador e gerador de relatórios (Terminal e output.dat)
 * Integrante responsável: Integrante 4
 */
class Reporter {
public:
    // Exibe no terminal a saída formatada de acordo com o padrão exigido no PDF
    static void exibirTerminal(const JobMetrics& metricas);

    // Grava de forma persistente no arquivo output.dat
    static void salvarOutputDat(const std::string& caminhoArquivo, const JobMetrics& metricas);
};

#endif // REPORTER_HPP
