#include <iostream>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include "../ula/ula.hpp"
#include "../../defines.hpp"

#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "[FALHA] " << __FILE__ << ":" << __LINE__ \
                      << " Assercao falhou: " #condition << std::endl; \
            std::exit(1); \
        } \
    } while (0)

void testBasicArithmetic() {
    std::cout << "[TEST] Testando Operacoes Aritmeticas Basicas...\n";
    ULA ula;

    // 1. ADD
    ula.execute(ADD, 10, 5);
    TEST_ASSERT(ula.result == 15);
    TEST_ASSERT(!ula.overflow);

    // 2. SUB
    ula.execute(SUB, 10, 5);
    TEST_ASSERT(ula.result == 5);
    TEST_ASSERT(!ula.overflow);

    ula.execute(SUB, 5, 10);
    TEST_ASSERT(ula.result == -5);
    TEST_ASSERT(!ula.overflow);

    std::cout << "[PASS] Aritmetica basica validada!\n";
}

void testBitwiseAndLogic() {
    std::cout << "[TEST] Testando Operacoes Logicas e Bit-a-Bit...\n";
    ULA ula;

    // 1. AND
    ula.execute(AND_OP, 0b1100, 0b1010);
    TEST_ASSERT(static_cast<uint16_t>(ula.result) == 0b1000);

    // 2. OR
    ula.execute(OR_OP, 0b1100, 0b1010);
    TEST_ASSERT(static_cast<uint16_t>(ula.result) == 0b1110);

    std::cout << "[PASS] Operacoes logicas validadas!\n";
}

void testEdgeCasesAndOverflow() {
    std::cout << "[TEST] Testando Casos de Borda e Detecao de Overflow...\n";
    ULA ula;

    // 1. Overflow em ADD positivo
    ula.execute(ADD, 30000, 10000); // 40000 > 32767
    TEST_ASSERT(ula.overflow);

    // 2. Overflow em SUB negativo
    ula.execute(SUB, static_cast<uint16_t>(-30000), 10000); // -40000 < -32768
    TEST_ASSERT(ula.overflow);

    std::cout << "[PASS] Casos de borda validados!\n";
}

void testOpcodeBridge() {
    std::cout << "[TEST] Testando Ponte de Opcodes com defines.hpp...\n";
    ULA ula;

    TEST_ASSERT(ula.executeOpcode(OP_ADD, 12, 18));
    TEST_ASSERT(ula.result == 30);

    TEST_ASSERT(ula.executeOpcode(OP_SUB, 50, 20));
    TEST_ASSERT(ula.result == 30);

    TEST_ASSERT(ula.executeOpcode(OP_AND, 0xFF00, 0x0F0F));
    TEST_ASSERT(static_cast<uint16_t>(ula.result) == 0x0F00);

    TEST_ASSERT(ula.executeOpcode(OP_OR, 0xFF00, 0x00FF));
    TEST_ASSERT(static_cast<uint16_t>(ula.result) == 0xFFFF);

    // Opcodes nao computacionais devem retornar false
    TEST_ASSERT(!ula.executeOpcode(OP_BEQ, 42, 42));
    TEST_ASSERT(!ula.executeOpcode(OP_LOAD, 10, 20));
    TEST_ASSERT(!ula.executeOpcode(OP_JUMP, 0, 0));
    TEST_ASSERT(!ula.executeOpcode(OP_HALT, 0, 0));

    std::cout << "[PASS] Ponte de opcodes validada!\n";
}

int main() {
    std::cout << "==================================================\n";
    std::cout << "        SUITE DE TESTES: SUBSISTEMA ULA\n";
    std::cout << "==================================================\n\n";

    testBasicArithmetic();
    testBitwiseAndLogic();
    testEdgeCasesAndOverflow();
    testOpcodeBridge();

    std::cout << "\nTODOS OS TESTES DA ULA PASSARAM COM SUCESSO!\n";
    return 0;
}
