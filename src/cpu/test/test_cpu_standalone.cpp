#include "../cpu.hpp"
#include "../../memory.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

// ==========================================
// MOCK DA MEMÓRIA (Fingindo que somos o SO)
// ==========================================
uint16_t RAM[TAM_RAM];
int clockGlobal = 0;
int instExecutadas = 0;
int acessosMemoria = 0;
int acessosDisco = 0;

uint16_t LerMemoria(uint16_t endereco) {
    clockGlobal++;
    acessosMemoria++;
    return RAM[endereco];
}

void EscreverMemoria(uint16_t endereco, uint16_t dado) {
    clockGlobal++;
    acessosMemoria++;
    RAM[endereco] = dado;
}

int LerDisco(uint16_t dadoBuscado, const std::vector<uint16_t>& Disco) {
    return -1; // Não usado no teste da CPU
}
// ==========================================


// Função auxiliar para ler o arquivo .txt em hexadecimal
void carregarJob(const std::string& filepath, uint16_t baseMemoria) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir o arquivo: " << filepath << std::endl;
        return;
    }

    std::string line;
    uint16_t endereco = baseMemoria;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        uint16_t val;
        std::stringstream ss;
        ss << std::hex << line; // Converte a string Hexadecimal para inteiro
        ss >> val;
        RAM[endereco++] = val;
    }
}

int main() {
    std::cout << "===================================================" << std::endl;
    std::cout << "   TESTE ISOLADO DA CPU   " << std::endl;
    std::cout << "===================================================" << std::endl;
    
    // 1. Instancia a CPU
    CPU cpu;
    
    // 2. Carrega o Job diretamente na "RAM falsa" (a partir do endereço 512)
    uint16_t base = OS_RESERVED_MEM;
    std::string jobPath = "../jobs/teste_condicional_beq.txt";
    std::cout << "Carregando o arquivo " << jobPath << " na memoria falsa..." << std::endl;
    carregarJob(jobPath, base);

    // 3. Configura a CPU para iniciar na linha base
    cpu.setPC(base);

    // 4. Ciclo de Vida da Máquina
    std::cout << "Iniciando execucao da CPU..." << std::endl;
    while (!cpu.isHalted()) {
        cpu.step();
    }

    std::cout << "\nExecucao finalizada com sucesso (Instrucao HALT encontrada)!" << std::endl;
    std::cout << "===================================================\n" << std::endl;

    // 5. Verifica o Estado Final
    cpu.imprimirEstado();
    
    std::cout << "--- ESTATISTICAS DA MEMORIA FALSA ---" << std::endl;
    std::cout << "Acessos na RAM (Load/Store): " << acessosMemoria << std::endl;
    std::cout << "Instrucoes Executadas na CPU: " << instExecutadas << std::endl;
    std::cout << "Ciclos de clock globais: " << clockGlobal << " ciclos\n" << std::endl;
    
    return 0;
}
