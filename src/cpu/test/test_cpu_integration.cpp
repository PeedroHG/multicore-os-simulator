#include <iostream>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include "../cpu.hpp"
#include "../../memory.hpp"
#include "../../defines.hpp"

#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "[FALHA] " << __FILE__ << ":" << __LINE__ \
                      << " Assercao falhou: " #condition << std::endl; \
            std::exit(1); \
        } \
    } while (0)

void testCPUWithRealULA() {
    std::cout << "[TEST] Testando Integracao CPU + RegisterBank + ULA Real...\n";

    // 1. Inicializa CPU e estatísticas
    clockGlobal = 0;
    instExecutadas = 0;
    acessosMemoria = 0;
    acessosDisco = 0;
    CPU cpu;
    cpu.reset();


    // 2. Prepara dados na RAM (relativos ao Job, apos OS_RESERVED_MEM = 512)
    // Endereço relativo 10: 15
    // Endereço relativo 11: 25
    EscreverMemoria(OS_RESERVED_MEM + 10, 15);
    EscreverMemoria(OS_RESERVED_MEM + 11, 25);

    // 3. Carrega programa de teste na memoria a partir de OS_RESERVED_MEM
    // 0: LOAD R0, [10]        (0x100A) -> R0 = 15
    // 1: LOAD R1, [11]        (0x110B) -> R1 = 25
    // 2: ADD  R2, R0, R1      (0x2201) -> R2 = 15 + 25 = 40 (via ULA)
    // 3: SUB  R3, R2, R0      (0x4320) -> R3 = 40 - 15 = 25 (via ULA)
    // 4: STORE R2, [12]       (0x320C) -> RAM[12] = 40
    // 5: HALT                 (0xF000)
    std::vector<uint16_t> programa = {
        0x100A, // LOAD R0, [10]
        0x110B, // LOAD R1, [11]
        0x2201, // ADD  R2, R0, R1
        0x4320, // SUB  R3, R2, R0
        0x320C, // STORE R2, [12]
        0xF000  // HALT
    };

    for (size_t i = 0; i < programa.size(); ++i) {
        EscreverMemoria(OS_RESERVED_MEM + i, programa[i]);
    }

    // 4. Executa ciclo a ciclo
    TEST_ASSERT(!cpu.isHalted());

    // Ciclo 1: LOAD R0, [10]
    cpu.step();
    TEST_ASSERT(cpu.getRegisterBank().read(0) == 15);
    TEST_ASSERT(cpu.getRegisterBank().getPC() == OS_RESERVED_MEM + 1);

    // Ciclo 2: LOAD R1, [11]
    cpu.step();
    TEST_ASSERT(cpu.getRegisterBank().read(1) == 25);
    TEST_ASSERT(cpu.getRegisterBank().getPC() == OS_RESERVED_MEM + 2);

    // Ciclo 3: ADD R2, R0, R1 (Calculo real da ULA)
    cpu.step();
    TEST_ASSERT(cpu.getRegisterBank().read(2) == 40);
    TEST_ASSERT(!cpu.getULA().overflow);

    // Ciclo 4: SUB R3, R2, R0 (Calculo real da ULA)
    cpu.step();
    TEST_ASSERT(cpu.getRegisterBank().read(3) == 25);
    TEST_ASSERT(!cpu.getULA().overflow);

    // Ciclo 5: STORE R2, [12]
    cpu.step();
    TEST_ASSERT(LerMemoria(OS_RESERVED_MEM + 12) == 40);

    // Ciclo 6: HALT
    cpu.step();
    TEST_ASSERT(cpu.isHalted());

    std::cout << "[PASS] Integracao CPU + RegisterBank + ULA passou com sucesso!\n";
}

int main() {
    std::cout << "==================================================\n";
    std::cout << "     SUITE DE INTEGRACAO: CPU + BANCO + ULA\n";
    std::cout << "==================================================\n\n";

    testCPUWithRealULA();

    std::cout << "\nTODOS OS TESTES DE INTEGRACAO PASSARAM COM EXCELENCIA!\n";
    return 0;
}
