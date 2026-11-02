#include "cpu.hpp"
#include <iostream>

CPU::CPU() : coreId(0), processoAtual(nullptr), ciclosStall(0) {
    reset();
}

CPU::CPU(int id) : coreId(id), processoAtual(nullptr), ciclosStall(0) {
    reset();
}

void CPU::reset() {
    regBank.reset();
    regBank.setPC(OS_RESERVED_MEM); // Mantém compatibilidade com testes legados enquanto a MMU da Fase 2 não entra
    controlUnit.reset();
    processoAtual = nullptr;
    ciclosStall = 0;
}

void CPU::setPC(uint16_t startAddress) {
    regBank.setPC(startAddress);
}

bool CPU::isHalted() const {
    return controlUnit.isHalted();
}

void CPU::desalocarProcesso() {
    processoAtual = nullptr;
    ciclosStall = 0;
    regBank.reset();
    controlUnit.reset();
}

// Salva o estado dos registradores e da UC no PCB
void CPU::salvarContexto(PCB& pcb) {
    pcb.contexto = regBank.getContext();
    pcb.halted = controlUnit.isHalted();
    pcb.ultimoCore = coreId;
}

// Restaura o estado dos registradores e da UC a partir do PCB
void CPU::restaurarContexto(PCB& pcb) {
    processoAtual = &pcb;
    regBank.setContext(pcb.contexto);
    controlUnit.setHalted(pcb.halted);
    pcb.ultimoCore = coreId;
    pcb.estado = EstadoProcesso::EXECUTANDO;
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

void CPU::step() {
    controlUnit.step(regBank, ula);
}

void CPU::imprimirEstado(std::ostream& out) const {
    regBank.imprimirEstado(out);
}
