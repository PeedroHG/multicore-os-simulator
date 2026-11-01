#ifndef CLOCK_CORE_HPP
#define CLOCK_CORE_HPP

#include <cstdint>

/**
 * ClockCore - Responsável pelo controle temporal da máquina e custos de hardware
 * Integrante responsável: Integrante 2
 */
class ClockCore {
private:
    static uint32_t ciclosJob;
    static uint32_t ciclosTotalGlobal;

public:
    static void reset();
    static void tickRAM();                    // Consome 1 ciclo por acesso à RAM
    static void tickDisco(int setores);       // Consome 50 ciclos por setor varrido
    static void tickBusca();                  // Consome 1 ciclo por busca de instrução
    static void adicionarCiclos(uint32_t ciclos); // Adiciona ciclos arbitrários (latências extras)
    static uint32_t getCycles();              // Ciclos totais do Job atual
    static uint32_t getGlobalCycles();        // Ciclos acumulados de todos os Jobs
};

#endif // CLOCK_CORE_HPP
