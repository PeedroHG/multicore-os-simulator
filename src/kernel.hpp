#ifndef KERNEL_HPP
#define KERNEL_HPP

#include "cpu/cpu.hpp"
#include "pcb.hpp"
#include "mmu.hpp"
#include <vector>
#include <cstdint>
#include <memory>
#include <functional>

/** Contadores brutos por núcleo (alimentam nucleos.csv via Metricas::consolidar). */
struct ContadoresNucleo {
    uint64_t ciclosExecucao = 0; ///< ciclos com instrução em execução
    uint64_t ciclosStall    = 0; ///< ciclos parado por falta de página
    uint64_t ciclosTroca    = 0; ///< ciclos gastos em troca de contexto
    uint32_t despachos      = 0; ///< processos despachados neste núcleo
    uint32_t trocasContexto = 0; ///< despachos em que o PID mudou no núcleo
};

/** Intervalo em que um processo ocupou um núcleo (despacho até término/preempção). */
struct SegmentoNucleo {
    int      nucleo;
    int      pid;
    uint64_t inicio;
    uint64_t fim;
};

class Kernel {
private:
    std::vector<CPU> cores;
    int numCores;
    uint64_t tickGlobal;
    uint32_t custoTrocaContexto;
    uint32_t limiteInstrucoesPorJob;

    // Métricas por núcleo (Pessoa 4) e último PID despachado em cada núcleo
    std::vector<ContadoresNucleo> contadoresNucleo;
    std::vector<int> ultimoPidNucleo;

    // Linha do tempo de ocupação (E5). segmentoAberto[k] = índice em `segmentos` ou -1.
    std::vector<SegmentoNucleo> segmentos;
    std::vector<int> segmentoAberto;

    /** Fecha o segmento aberto do núcleo. Chamar também ao PREEMPTAR (devolver à fila). */
    void fecharSegmento(int nucleo, uint64_t tick);

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
    const std::vector<ContadoresNucleo>& getContadoresNucleo() const { return contadoresNucleo; }
    const std::vector<SegmentoNucleo>& getSegmentos() const { return segmentos; }
    std::vector<CPU>& getCores() { return cores; }

    // Gerenciamento de processos
    void admitirProcesso(std::unique_ptr<PCB> pcb);
    bool temProcessosAtivos() const;

    // Execução da simulação
    void executarPasso(); // Executa 1 tick global em lockstep
    void rodarSimulacao(); // Executa até que todos os processos terminem
};

#endif // KERNEL_HPP