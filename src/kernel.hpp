#ifndef KERNEL_HPP
#define KERNEL_HPP

#include "cpu/cpu.hpp"
#include "pcb.hpp"
#include "mmu.hpp"
#include <vector>
#include <cstdint>
#include <memory>
#include <functional>

class Kernel {
private:
    std::vector<CPU> cores;
    int numCores;
    uint64_t tickGlobal;
    uint32_t custoTrocaContexto;
    uint32_t limiteInstrucoesPorJob;

    // Fila de processos gerenciada pelo Kernel
    std::vector<PCB*> processosProntos;
    std::vector<std::unique_ptr<PCB>> todosProcessos;

    // Gerenciador de memória (opcional; nullptr = modo legado sem MMU)
    InterfaceMMU* mmu = nullptr;

    // Encerra o processo do núcleo (FINALIZADO/ABORTADO) e libera suas molduras
    void encerrarProcesso(CPU& core, PCB* pcb, EstadoProcesso estadoFinal, uint64_t cicloFim);

public:
    explicit Kernel(int nCores = 4, uint32_t custoTroca = 5, uint32_t limiteInst = 1000000);

    /** Conecta a MMU a todos os núcleos (endereços passam a ser lógicos). */
    void conectarMMU(InterfaceMMU* m);
    const std::vector<std::unique_ptr<PCB>>& getProcessos() const { return todosProcessos; }

    // Getters de estado
    int getNumCores() const { return numCores; }
    uint64_t getTickGlobal() const { return tickGlobal; }
    const std::vector<CPU>& getCores() const { return cores; }
    std::vector<CPU>& getCores() { return cores; }

    // Gerenciamento de processos
    void admitirProcesso(std::unique_ptr<PCB> pcb);
    bool temProcessosAtivos() const;

    // Execução da simulação
    void executarPasso(); // Executa 1 tick global em lockstep
    void rodarSimulacao(); // Executa até que todos os processos terminem
};

#endif // KERNEL_HPP

