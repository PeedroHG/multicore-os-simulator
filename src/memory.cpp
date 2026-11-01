#include "memory.hpp"
#include <stdexcept>

uint16_t RAM[TAM_RAM];

int clockGlobal      = 0;
int instExecutadas   = 0;
int acessosMemoria   = 0;
int acessosDisco     = 0;


uint16_t LerMemoria(uint16_t endereco) {
    if (endereco < OS_RESERVED_MEM) {
        throw std::runtime_error("SEGFAULT: Tentativa de leitura em area restrita do SO!");
    }
    clockGlobal++;      // Acesso a memória consome 1 ciclo
    acessosMemoria++;
    return RAM[endereco];
}

void EscreverMemoria(uint16_t endereco, uint16_t dado) {
    if (endereco < OS_RESERVED_MEM) {
        throw std::runtime_error("SEGFAULT: Tentativa de escrita em area restrita do SO!");
    }
    clockGlobal++;
    acessosMemoria++;
    RAM[endereco] = dado;
}

int LerDisco(uint16_t dadoBuscado, const std::vector<uint16_t>& Disco) {
    acessosDisco++;
    for (size_t i = 0; i < Disco.size(); i++) {
        clockGlobal += 50;                  // Penalidade: 50 clocks por setor percorrido
        if (Disco[i] == dadoBuscado) return static_cast<int>(i);
    }
    return -1;
}