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

// ============================================================================
// Paginação (Modelo Tanenbaum) — Responsável: João Paulo (Pessoa 2)
// ----------------------------------------------------------------------------
// Endereço LÓGICO (12 bits úteis; bits [15:12] devem ser 0, senão SEGFAULT):
//     [11:4] número da página (8 bits)  |  [3:0] deslocamento (4 bits)
// Endereço FÍSICO (16 bits, RAM de 64K palavras):
//     [15:4] número da moldura (12 bits) |  [3:0] deslocamento (4 bits)
// ============================================================================
const uint16_t BITS_OFFSET     = 4;
const uint16_t TAM_PAGINA      = 1 << BITS_OFFSET;              // 16 palavras
const uint16_t MASK_OFFSET     = TAM_PAGINA - 1;                // 0x000F
const uint16_t MAX_END_LOGICO  = 0x0FFF;                        // 12 bits
const uint16_t NUM_PAGINAS_LOGICAS  = (MAX_END_LOGICO + 1) / TAM_PAGINA;  // 256 páginas/processo
const uint32_t NUM_MOLDURAS_FISICAS = TAM_RAM / TAM_PAGINA;               // 4096 molduras
const uint16_t MOLDURAS_SO     = OS_RESERVED_MEM / TAM_PAGINA;  // 32 molduras reservadas ao SO

inline uint16_t extrairPagina(uint16_t e) { return static_cast<uint16_t>(e >> BITS_OFFSET); }
inline uint16_t extrairOffset(uint16_t e) { return static_cast<uint16_t>(e & MASK_OFFSET); }
inline uint32_t montarEnderecoFisico(uint32_t moldura, uint16_t offset) {
    return (moldura << BITS_OFFSET) | (offset & MASK_OFFSET);
}

#endif // DEFINES_HPP