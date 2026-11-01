#include "reporter.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <ostream>

namespace {

constexpr int LARGURA_PALAVRA_16_BITS = 4;

void imprimirHex16(std::ostream& saida, uint16_t valor) {
    saida << "0x"
          << std::uppercase
          << std::hex
          << std::setw(LARGURA_PALAVRA_16_BITS)
          << std::setfill('0')
          << valor
          << std::dec
          << std::nouppercase
          << std::setfill(' ');
}

void imprimirRelatorio(std::ostream& saida, const JobMetrics& metricas) {
    saida << "--- ESTADO DA CPU ---\n";

    saida << "PC: ";
    imprimirHex16(saida, metricas.pcFinal);
    
    saida << " IR: ";
    imprimirHex16(saida, metricas.irFinal);
    
    saida << '\n';
    saida << "Registradores Gerais:\n";
    for (int i = 0; i < 8; ++i) {
        saida << 'R' << i << ": ";
        imprimirHex16(saida, metricas.registradores[i]);

        if (i % 4 == 3) {
            saida << '\n';
        } else {
            saida << "   ";
        }
    }

    saida << "--- ESTATISTICAS DO JOB ---\n";
    saida << "Instrucoes Executadas: " << metricas.instrucoesExecutadas << '\n';
    saida << "Acessos a Memoria (RAM): " << metricas.acessosRAM << '\n';
    saida << "Acessos ao Disco (I/O): " << metricas.acessosDisco << '\n';
    saida << "CLOCK TOTAL DESTE JOB: " << metricas.clockTotal << " ciclos\n";
}

} // namespace

void Reporter::exibirTerminal(const JobMetrics& metricas) {
    imprimirRelatorio(std::cout, metricas);
}

void Reporter::salvarOutputDat(const std::string& caminhoArquivo, const JobMetrics& metricas) {
    std::ofstream arquivo(caminhoArquivo, std::ios::app);

    if (!arquivo.is_open()) {
        std::cerr << "[REPORTER] Falha ao abrir arquivo de saida: "
                  << caminhoArquivo << '\n';
        return;
    }

    imprimirRelatorio(arquivo, metricas);
    arquivo << "===================================================\n";
}
