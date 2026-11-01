#include "cpu.hpp"
#include <iostream>

// ----------------------------------------------------------------------------
// Construtor e Controle Básico da CPU
// ----------------------------------------------------------------------------
CPU::CPU() {
    reset();
}

void CPU::reset() {
    regBank.reset();
    regBank.setPC(OS_RESERVED_MEM); // Inicia apontando para o início da área do job na RAM
    controlUnit.reset();            // Limpa a flag de HALT da UC
}

void CPU::setPC(uint16_t startAddress) {
    regBank.setPC(startAddress);
}

bool CPU::isHalted() const {
    return controlUnit.isHalted();
}

RegisterBank& CPU::getRegisterBank() {
    return regBank;
}

const RegisterBank& CPU::getRegisterBank() const {
    return regBank;
}

ULA& CPU::getULA() {
    return ula;
}

const ULA& CPU::getULA() const {
    return ula;
}

ControlUnit& CPU::getControlUnit() {
    return controlUnit;
}

const ControlUnit& CPU::getControlUnit() const {
    return controlUnit;
}

// ----------------------------------------------------------------------------
// Ciclo de Instrução (Busca -> Decodificação -> Execução)
// ----------------------------------------------------------------------------
// A CPU não implementa mais o ciclo de instrução por conta própria: ela
// apenas repassa suas duas dependências internas (regBank e ula) para a
// Unidade de Controle, que é quem efetivamente busca a instrução na RAM
// (via LerMemoria), atualiza PC/IR, decodifica os campos por mascaramento
// de bits e executa a operação (aritmética/lógica via ULA, LOAD/STORE via
// interface de memória, ou desvio de fluxo/HALT diretamente sobre o PC).
void CPU::step() {
    controlUnit.step(regBank, ula);
}

// ----------------------------------------------------------------------------
// Impressão do Estado da CPU
// ----------------------------------------------------------------------------
void CPU::imprimirEstado(std::ostream& out) const {
    regBank.imprimirEstado(out);
}