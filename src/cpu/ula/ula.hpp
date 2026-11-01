#ifndef ULA_HPP
#define ULA_HPP

#include <cstdint>
#include "../../defines.hpp"

/**
 * ============================================================================
 * @brief Enumeração de Operações da ULA
 * ============================================================================
 * Define os identificadores das operações aritméticas da ULA.
 */
enum operation {
    ADD,    ///< Soma (A + B)
    SUB,    ///< Subtração (A - B)
    AND_OP, ///< Conjunção lógica bit a bit (A & B)
    OR_OP,  ///< Disjunção lógica bit a bit (A | B)
};

/**
 * ============================================================================
 * @brief Unidade Lógica e Aritmética (ULA / ALU) da CPU RISC 16-bits
 * ============================================================================
 * Componente puramente calculista da CPU.
 * 
 * Regras e Diretrizes Arquiteturais:
 * - A ULA não possui acesso direto à memória RAM (respeita isolamento de barramento).
 * - A ULA não manipula diretamente o Banco de Registradores; ela recebe operandos
 *   fornecidos pela CPU e devolve o resultado para ser gravado.
 * - Operações aritméticas interpretam valores como inteiros com sinal de 16 bits (int16_t).
 * - Detecção de overflow é realizada utilizando cálculos intermediários em 32 bits (int32_t).
 */
class ULA {
public:
    // --- Entradas ---
    uint16_t A = 0;             ///< Primeiro operando (16 bits)
    uint16_t B = 0;             ///< Segundo operando (16 bits)
    operation op = ADD;         ///< Operação a ser executada

    // --- Saídas ---
    int16_t result = 0;         ///< Resultado do cálculo (16 bits com sinal)
    bool overflow = false;      ///< Flag indicando estouro aritmético ou divisão por zero

    // --- Métodos de Execução ---

    /**
     * @brief Executa o cálculo com base nos valores atuais de A, B e op.
     */
    void calculate();

    /**
     * @brief Configura os operandos e dispara o cálculo na ULA.
     * @param ULA_operacao Operação a ser executada (enum operation).
     * @param a Primeiro operando de 16 bits.
     * @param b Segundo operando de 16 bits.
     * @param shamt Quantidade de deslocamento (shift amount, reservado para expansões).
     */
    void execute(operation ULA_operacao, uint16_t a, uint16_t b, uint16_t shamt = 0);

    /**
     * @brief Executa a operação correspondente ao opcode oficial do defines.hpp.
     * Facilita a integração direta com a Unidade de Controle / CPU.
     * @param opcode Código de operação (OP_ADD, OP_SUB, OP_AND, OP_OR).
     * @param a Primeiro operando de 16 bits.
     * @param b Segundo operando de 16 bits.
     * @return true se o opcode for suportado pela ULA, false caso contrário.
     */
    bool executeOpcode(uint16_t opcode, uint16_t a, uint16_t b);
};

#endif // ULA_HPP
