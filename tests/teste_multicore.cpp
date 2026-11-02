#include <iostream>
#include <cassert>
#include <vector>
#include <memory>
#include "../src/kernel.hpp"
#include "../src/pcb.hpp"
#include "../src/memory.hpp"
#include "../src/defines.hpp"

// Teste 1: Regressao Baseline (1 Nucleo)
void testarBaseline1Core() {
    std::cout << "[TEST] 1. Teste de Regressao Baseline: 1 Nucleo (Single-Core)...\n";

    Kernel kernel(1);
    assert(kernel.getNumCores() == 1);

    // Instrucao HALT em OS_RESERVED_MEM (512)
    RAM[OS_RESERVED_MEM] = 0xF000;

    auto pcb = std::make_unique<PCB>(1, "baseline_job.txt");
    pcb->contexto.PC = OS_RESERVED_MEM;
    pcb->contexto.R[0] = 123;

    PCB* ref = pcb.get();
    kernel.admitirProcesso(std::move(pcb));

    kernel.rodarSimulacao();

    assert(ref->estado == EstadoProcesso::FINALIZADO);
    assert(ref->ultimoCore == 0);
    assert(ref->contexto.R[0] == 123);
    assert(kernel.getTickGlobal() == 1);

    std::cout << "[PASS] 1 Nucleo validado com comportamento de baseline!\n";
}

// Teste 2: Concorrencia Multicore (4 Processos em 2 e 4 Nucleos)
void testarConcorrenciaMulticore() {
    std::cout << "[TEST] 2. Teste Multicore: 4 Processos rodando concorrentemente em 4 Nucleos...\n";

    Kernel kernel(4);
    assert(kernel.getNumCores() == 4);

    RAM[OS_RESERVED_MEM] = 0xF000;

    std::vector<PCB*> refs;
    for (int i = 1; i <= 4; ++i) {
        auto pcb = std::make_unique<PCB>(i, "job" + std::to_string(i) + ".txt");
        pcb->contexto.PC = OS_RESERVED_MEM;
        refs.push_back(pcb.get());
        kernel.admitirProcesso(std::move(pcb));
    }

    assert(kernel.temProcessosAtivos());

    // No Tick 0, todos os 4 processos devem ser alocados aos 4 nucleos distintos e finalizar no mesmo tick
    kernel.executarPasso();

    for (int i = 0; i < 4; ++i) {
        assert(refs[i]->estado == EstadoProcesso::FINALIZADO);
        assert(refs[i]->ultimoCore == i); // Processo 1 no Core 0, Processo 2 no Core 1, etc.
    }

    assert(kernel.getTickGlobal() == 1);
    assert(!kernel.temProcessosAtivos());

    std::cout << "[PASS] 4 Nucleos executaram 4 processos de forma 100% paralela!\n";
}

// Teste 3: Balanceamento com mais processos que nucleos (4 Processos em 2 Nucleos)
void testarBalanceamento2Cores() {
    std::cout << "[TEST] 3. Teste Multicore: 4 Processos em 2 Nucleos (2 rodadas de execucao)...\n";

    Kernel kernel(2);
    assert(kernel.getNumCores() == 2);

    RAM[OS_RESERVED_MEM] = 0xF000;

    std::vector<PCB*> refs;
    for (int i = 1; i <= 4; ++i) {
        auto pcb = std::make_unique<PCB>(i, "job" + std::to_string(i) + ".txt");
        pcb->contexto.PC = OS_RESERVED_MEM;
        refs.push_back(pcb.get());
        kernel.admitirProcesso(std::move(pcb));
    }

    // Tick 0: Roda Processo 1 (Core 0) e Processo 2 (Core 1)
    kernel.executarPasso();
    assert(refs[0]->estado == EstadoProcesso::FINALIZADO);
    assert(refs[1]->estado == EstadoProcesso::FINALIZADO);
    assert(refs[2]->estado == EstadoProcesso::PRONTO);
    assert(refs[3]->estado == EstadoProcesso::PRONTO);
    assert(kernel.temProcessosAtivos());

    // Tick 1: Núcleos vagam e puxam Processo 3 (Core 0) e Processo 4 (Core 1)
    kernel.executarPasso();
    assert(refs[2]->estado == EstadoProcesso::FINALIZADO);
    assert(refs[3]->estado == EstadoProcesso::FINALIZADO);
    assert(!kernel.temProcessosAtivos());
    assert(kernel.getTickGlobal() == 2);

    std::cout << "[PASS] Despacho e balanceamento em 2 nucleos validados com sucesso!\n";
}

int main() {
    std::cout << "==================================================\n";
    std::cout << "  SUITE DE TESTES: VALIDACAO MULTICORE & BASELINE \n";
    std::cout << "==================================================\n";

    testarBaseline1Core();
    testarConcorrenciaMulticore();
    testarBalanceamento2Cores();

    std::cout << "\nTODOS OS TESTES DA ETAPA 5 PASSARAM COM SUCESSO!\n";
    return 0;
}

