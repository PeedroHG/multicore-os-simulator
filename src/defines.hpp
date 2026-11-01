#ifndef DEFINES_HPP
#define DEFINES_HPP

#include <cstdint>

// Processos 
enum Opcodes {
    OP_LOAD  = 0x1, // Traz dado da Memória -> Registrador
    OP_ADD   = 0x2, // Soma
    OP_STORE = 0x3, // Salva Registrador -> Memória
    OP_SUB   = 0x4, // Subtração
    OP_AND   = 0x5, // Lógica AND
    OP_OR    = 0x6, // Lógica OR
    OP_BEQ   = 0x7, // Branch if Equal
    OP_JUMP  = 0x8, // Salto incondicional
    OP_HALT  = 0xF  // Fim do job
};

const uint32_t TAM_RAM          = 65536;
const uint16_t OS_RESERVED_MEM  = 512; // Primeiros 512 endereços são do SO

#endif // DEFINES_HPP