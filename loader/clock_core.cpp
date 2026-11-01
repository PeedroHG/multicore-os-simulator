#include "clock_core.hpp"

uint32_t ClockCore::ciclosJob = 0;
uint32_t ClockCore::ciclosTotalGlobal = 0;

void ClockCore::reset() {
    ciclosJob = 0;
}

void ClockCore::tickRAM() {
    ciclosJob += 1;
    ciclosTotalGlobal += 1;
}

void ClockCore::tickDisco(int setores) {
    if (setores > 0) {
        uint32_t ciclos = setores * 50;
        ciclosJob += ciclos;
        ciclosTotalGlobal += ciclos;
    }
}

void ClockCore::tickBusca() {
    ciclosJob += 1;
    ciclosTotalGlobal += 1;
}

void ClockCore::adicionarCiclos(uint32_t ciclos) {
    ciclosJob += ciclos;
    ciclosTotalGlobal += ciclos;
}

uint32_t ClockCore::getCycles() {
    return ciclosJob;
}

uint32_t ClockCore::getGlobalCycles() {
    return ciclosTotalGlobal;
}
