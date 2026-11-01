#include "so.hpp"
#include <iostream>
#include <iomanip>
#include <exception>


void SistemaOperacional::carregarJob(const std::vector<uint16_t>& jobInstrucoes,
                                     const std::vector<uint16_t>& dadosIniciais) {
    uint16_t baseMemoria = OS_RESERVED_MEM;

    // Carrega as instruções do job na RAM (a partir do endereço base)
    for (size_t i = 0; i < jobInstrucoes.size(); i++) {
        RAM[baseMemoria + i] = jobInstrucoes[i];
    }

    // Simula a carga de variáveis nas posições 10, 11 e 12 (relativas ao Job)
    // A = dadosIniciais[0], B = dadosIniciais[1], C = 0 (inicialmente)
    RAM[baseMemoria + 10] = dadosIniciais[0];
    RAM[baseMemoria + 11] = dadosIniciais[1];
    RAM[baseMemoria + 12] = 0;   // Variável C (inicializada com 0)

    cpu.setPC(baseMemoria);
}

void SistemaOperacional::executarJob(int idJob,
                                     const std::string& nomeJob,
                                     const std::vector<uint16_t>& inst,
                                     const std::vector<uint16_t>& dados) {
    std::cout << "\n===================================================" << std::endl;
    std::cout << "[SIMULADOR RISC 16-BITS] - EXECUCAO EM LOTE" << std::endl;
    std::cout << "===================================================" << std::endl;
    std::cout << "Carregando Job " << idJob << ": " << nomeJob << "..." << std::endl;

    // Limpa estatísticas e contexto da CPU
    clockGlobal     = 0;
    instExecutadas  = 0;
    acessosMemoria  = 0;
    acessosDisco    = 0;
    cpu.reset();

    // Loader: copia o job para a RAM
    carregarJob(inst, dados);

    // Laço de execução bare-metal (sequencial)
    try {
        while (!cpu.isHalted()) {
            cpu.step();
        }
        std::cout << "Execucao finalizada (HALT encontrado)." << std::endl;
    } catch (const std::exception& e) {
        std::cout << "\n[ERRO CRITICO] " << e.what() << std::endl;
        std::cout << "O Job " << idJob << " foi abortado pelo Sistema Operacional." << std::endl;
    }

    // Exibe o estado final da CPU
    cpu.imprimirEstado();

    // Exibe as estatísticas do job
    std::cout << "--- ESTATISTICAS DO JOB ---" << std::endl;
    std::cout << "Instrucoes Executadas: " << instExecutadas << std::endl;
    std::cout << "Acessos a Memoria (RAM): " << acessosMemoria << std::endl;
    std::cout << "Acessos ao Disco (I/O): " << acessosDisco << std::endl;
    std::cout << "CLOCK TOTAL DESTE JOB: " << clockGlobal << " ciclos" << std::endl;
    std::cout << "===================================================\n" << std::endl;
}