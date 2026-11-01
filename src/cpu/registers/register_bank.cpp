#include "register_bank.hpp"
#include <iomanip>
#include <stdexcept>

// ----------------------------------------------------------------------------
// Construtor
// ----------------------------------------------------------------------------
RegisterBank::RegisterBank() {
    // Inicializa todos os registradores com zero na criação do objeto
    reset();
}

// ----------------------------------------------------------------------------
// Reinicialização de Hardware
// ----------------------------------------------------------------------------
void RegisterBank::reset() noexcept {
    // std::array::fill preenche todos os 8 registradores (R0 a R7) com 0x0000
    R.fill(0);
    PC = 0; // Aponta PC para o endereço inicial (0x0000)
    IR = 0; // Limpa o registrador de instrução
}

// ----------------------------------------------------------------------------
// Leitura de Registrador Geral (R0..R7)
// ----------------------------------------------------------------------------
uint16_t RegisterBank::read(size_t index) const {
    // Bounds Checking: Protege o simulador contra acesso fora dos limites (0 a 7)
    if (index >= NUM_REGISTERS) {
        throw std::out_of_range("RegisterBank::read - Indice invalido (" + 
                                std::to_string(index) + "). Permitido: 0 a " + 
                                std::to_string(NUM_REGISTERS - 1) + ".");
    }
    return R[index];
}

// ----------------------------------------------------------------------------
// Escrita em Registrador Geral (R0..R7)
// ----------------------------------------------------------------------------
void RegisterBank::write(size_t index, uint16_t value) {
    // Bounds Checking: Garante que apenas registradores válidos sejam alterados
    if (index >= NUM_REGISTERS) {
        throw std::out_of_range("RegisterBank::write - Indice invalido (" + 
                                std::to_string(index) + "). Permitido: 0 a " + 
                                std::to_string(NUM_REGISTERS - 1) + ".");
    }
    R[index] = value;
}

// ----------------------------------------------------------------------------
// Getters e Setters de Registradores Especiais (PC e IR)
// ----------------------------------------------------------------------------
uint16_t RegisterBank::getPC() const noexcept {
    return PC;
}

void RegisterBank::setPC(uint16_t address) noexcept {
    PC = address;
}

void RegisterBank::incrementPC() noexcept {
    // Avança para a próxima palavra de memória (instrução seguinte)
    PC++;
}

uint16_t RegisterBank::getIR() const noexcept {
    return IR;
}

void RegisterBank::setIR(uint16_t instruction) noexcept {
    IR = instruction;
}

// ----------------------------------------------------------------------------
// Troca de Contexto e Suporte ao PCB do Sistema Operacional
// ----------------------------------------------------------------------------
CPUContext RegisterBank::getContext() const noexcept {
    // Empacota o estado completo de hardware atual em uma struct CPUContext
    CPUContext ctx;
    ctx.R = R;   // Cópia direta do array de 8 registradores
    ctx.PC = PC; // Endereço de retomada do processo
    ctx.IR = IR; // Instrução em execução
    return ctx;
}

void RegisterBank::setContext(const CPUContext& ctx) noexcept {
    // Restaura o estado de hardware da CPU com os dados salvos do PCB
    R = ctx.R;
    PC = ctx.PC;
    IR = ctx.IR;
}

// ----------------------------------------------------------------------------
// Snapshots em Vetor (Compatibilidade e Interoperabilidade)
// ----------------------------------------------------------------------------
std::vector<uint16_t> RegisterBank::getRegistersSnapshot() const {
    // Converte o array interno R para um std::vector dinâmico
    return std::vector<uint16_t>(R.begin(), R.end());
}

void RegisterBank::loadRegistersSnapshot(const std::vector<uint16_t>& snapshot) {
    // Valida se o vetor recebido contém exatamente os 8 registradores da arquitetura
    if (snapshot.size() != NUM_REGISTERS) {
        throw std::invalid_argument("RegisterBank::loadRegistersSnapshot - Snapshot deve conter exatamente " + 
                                    std::to_string(NUM_REGISTERS) + " registradores.");
    }
    // Copia os dados do snapshot para o array de registradores
    for (size_t i = 0; i < NUM_REGISTERS; ++i) {
        R[i] = snapshot[i];
    }
}

// ----------------------------------------------------------------------------
// Impressão e Formatação de Diagnóstico (output.dat / Console)
// ----------------------------------------------------------------------------
void RegisterBank::imprimirEstado(std::ostream& out) const {
    // Salva o estado atual dos formatadores do stream (hex, dec, flags)
    // para evitar efeitos colaterais em outras partes do programa
    std::ios state(nullptr);
    state.copyfmt(out);

    // Cabeçalho com registradores de controle (PC e IR) em hexadecimal de 4 dígitos
    out << "--- ESTADO DA CPU ---" << std::endl;
    out << "PC: 0x" << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << PC
        << "   IR: 0x" << std::setw(4) << IR << std::nouppercase << std::dec << std::endl;
    
    // Registradores gerais R0 a R7 dispostos em duas linhas de 4 colunas
    out << "Registradores Gerais:" << std::endl;
    for (size_t i = 0; i < NUM_REGISTERS; i += 4) {
        out << "R" << i << ": 0x" << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << R[i] << "  "
            << "R" << i+1 << ": 0x" << std::setw(4) << R[i+1] << "  "
            << "R" << i+2 << ": 0x" << std::setw(4) << R[i+2] << "  "
            << "R" << i+3 << ": 0x" << std::setw(4) << R[i+3] << std::nouppercase << std::dec << std::endl;
    }

    // Restaura o formato original da stream
    out.copyfmt(state);
}

