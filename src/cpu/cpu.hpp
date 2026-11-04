#ifndef CPU_HPP
#define CPU_HPP

#include "registers/register_bank.hpp"
#include "ula/ula.hpp"
#include "control/control_unit.hpp"
#include "../memory.hpp"
#include "../defines.hpp"
#include "../pcb.hpp"
#include <cstdint>
#include <iostream>
#include <ostream>

class CPU {
private:
    RegisterBank regBank;
    ULA ula;
    ControlUnit controlUnit;

    // Informações de controle de núcleo
    int coreId = 0;
    PCB* processoAtual = nullptr;
    uint32_t ciclosStall = 0;

    // Barramento de memória (MMU compartilhada entre os núcleos — Pessoa 2)
    InterfaceMMU* mmu = nullptr;
    uint64_t tickAtual = 0;
    uint32_t ultimaPenalidade = 0;

public:
    CPU();
    explicit CPU(int id);

    // MMU: com MMU conectada, PC e endereços do processo são lógicos (base 0)
    void conectarMMU(InterfaceMMU* m) { mmu = m; }
    void setTick(uint64_t t) { tickAtual = t; }
    /** Ciclos de stall (faltas de página) gerados pela última instrução. */
    uint32_t getUltimaPenalidade() const { return ultimaPenalidade; }

    void reset();
    void setPC(uint16_t startAddress);
    bool isHalted() const;

    // Identificação do núcleo
    void setCoreId(int id) { coreId = id; }
    int getCoreId() const { return coreId; }

    // Gerenciamento de processo no núcleo
    bool estaLivre() const { return processoAtual == nullptr; }
    PCB* getProcessoAtual() { return processoAtual; }
    const PCB* getProcessoAtual() const { return processoAtual; }
    void desalocarProcesso();

    // Controle de stall (espera de falta de página / E/S)
    bool emStall() const { return ciclosStall > 0; }
    void adicionarStall(uint32_t ciclos) { ciclosStall += ciclos; }
    void decrementarStall() { if (ciclosStall > 0) --ciclosStall; }
    uint32_t getCiclosStall() const { return ciclosStall; }

    // Troca de contexto
    void salvarContexto(PCB& pcb);
    void restaurarContexto(PCB& pcb);

    // Ciclo de instrução
    void step();

    void imprimirEstado(std::ostream& out = std::cout) const;

    RegisterBank& getRegisterBank();
    const RegisterBank& getRegisterBank() const;

    ULA& getULA();
    const ULA& getULA() const;

    ControlUnit& getControlUnit();
    const ControlUnit& getControlUnit() const;
};

#endif // CPU_HPP
