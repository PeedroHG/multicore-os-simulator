#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstdlib>
#include "../registers/register_bank.hpp"
#include "../registers/register_table.hpp"

// Macro de teste resiliente e independente de NDEBUG
#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "[FALHA] " << __FILE__ << ":" << __LINE__ \
                      << " Assercao falhou: " #condition << std::endl; \
            std::exit(1); \
        } \
    } while (0)

#define TEST_THROWS(expression, exceptionType) \
    do { \
        bool caught = false; \
        try { \
            expression; \
        } catch (const exceptionType&) { \
            caught = true; \
        } catch (...) { \
            caught = false; \
        } \
        if (!caught) { \
            std::cerr << "[FALHA] " << __FILE__ << ":" << __LINE__ \
                      << " Esperava excecao " #exceptionType " em: " #expression << std::endl; \
            std::exit(1); \
        } \
    } while (0)

void testRegisterBank() {
    std::cout << "[TEST] Executando testes rigorosos do RegisterBank...\n";
    RegisterBank bank;

    // 1. Estado inicial zerado
    for (size_t i = 0; i < RegisterBank::NUM_REGISTERS; ++i) {
        TEST_ASSERT(bank.read(i) == 0);
    }
    TEST_ASSERT(bank.getPC() == 0);
    TEST_ASSERT(bank.getIR() == 0);

    // 2. Escrita e leitura nos registradores (incluindo R0 e valores de borda)
    bank.write(0, 0x0005);
    TEST_ASSERT(bank.read(0) == 0x0005);

    bank.write(7, 0xFFFF);
    TEST_ASSERT(bank.read(7) == 0xFFFF);

    bank.write(3, 0x1234);
    TEST_ASSERT(bank.read(3) == 0x1234);

    // 3. Controle do PC e IR
    bank.setPC(0x0100);
    bank.incrementPC();
    TEST_ASSERT(bank.getPC() == 0x0101);

    bank.setIR(0x2201);
    TEST_ASSERT(bank.getIR() == 0x2201);

    // 4. Teste de Contexto Completo para o PCB (R0..R7, PC, IR)
    CPUContext ctx = bank.getContext();
    TEST_ASSERT(ctx.R[0] == 0x0005);
    TEST_ASSERT(ctx.R[3] == 0x1234);
    TEST_ASSERT(ctx.R[7] == 0xFFFF);
    TEST_ASSERT(ctx.PC == 0x0101);
    TEST_ASSERT(ctx.IR == 0x2201);

    // Modifica o banco e depois restaura via context
    bank.reset();
    TEST_ASSERT(bank.read(0) == 0);
    TEST_ASSERT(bank.getPC() == 0);

    bank.setContext(ctx);
    TEST_ASSERT(bank.read(0) == 0x0005);
    TEST_ASSERT(bank.getPC() == 0x0101);
    TEST_ASSERT(bank.getIR() == 0x2201);

    // 5. Teste de Excecoes e Bounds Checking
    TEST_THROWS(bank.read(8), std::out_of_range);
    TEST_THROWS(bank.read(255), std::out_of_range);
    TEST_THROWS(bank.read(256), std::out_of_range);
    TEST_THROWS(bank.write(8, 0x1111), std::out_of_range);
    TEST_THROWS(bank.write(1000, 0x1111), std::out_of_range);
    TEST_THROWS(bank.loadRegistersSnapshot(std::vector<uint16_t>{1, 2, 3}), std::invalid_argument);

    // 6. Teste de Saída Formatada em Stream (output.dat / stringstream)
    std::ostringstream oss;
    bank.imprimirEstado(oss);
    std::string out = oss.str();
    TEST_ASSERT(out.find("PC: 0x0101") != std::string::npos);
    TEST_ASSERT(out.find("IR: 0x2201") != std::string::npos);
    TEST_ASSERT(out.find("R0: 0x0005") != std::string::npos);
    TEST_ASSERT(out.find("R7: 0xFFFF") != std::string::npos);

    std::cout << "[PASS] RegisterBank validado com sucesso!\n\n";
}

void testRegisterTable() {
    std::cout << "[TEST] Executando testes rigorosos do RegisterTable...\n";
    RegisterTable table;

    // 1. Mapeamento Canonico e Aliases
    TEST_ASSERT(table.getIndex("R0") == 0);
    TEST_ASSERT(table.getIndex("r0") == 0);
    TEST_ASSERT(table.getIndex("$R0") == 0);
    TEST_ASSERT(table.getIndex("$0") == 0);
    TEST_ASSERT(table.getIndex("  r7  ") == 7);
    TEST_ASSERT(table.getIndex("R7,") == 7);

    // 2. Registradores Especiais
    TEST_ASSERT(table.getIndex("PC") == RegisterTable::SPECIAL_REG_PC);
    TEST_ASSERT(table.getIndex("pc") == RegisterTable::SPECIAL_REG_PC);
    TEST_ASSERT(table.getIndex("IR") == RegisterTable::SPECIAL_REG_IR);

    // 3. Mapeamento Reverso (Index -> Name)
    TEST_ASSERT(table.getName(0) == "R0");
    TEST_ASSERT(table.getName(7) == "R7");
    TEST_ASSERT(table.getName(RegisterTable::SPECIAL_REG_PC) == "PC");
    TEST_ASSERT(table.getName(RegisterTable::SPECIAL_REG_IR) == "IR");

    // 4. Validacao de Predicados
    TEST_ASSERT(table.isValidName("R0"));
    TEST_ASSERT(table.isValidName("$r5"));
    TEST_ASSERT(!table.isValidName("R8"));
    TEST_ASSERT(!table.isValidName(""));
    TEST_ASSERT(!table.isValidName("   "));

    TEST_ASSERT(table.isValidIndex(0));
    TEST_ASSERT(table.isValidIndex(7));
    TEST_ASSERT(table.isValidIndex(RegisterTable::SPECIAL_REG_PC));
    TEST_ASSERT(!table.isValidIndex(8));
    TEST_ASSERT(!table.isValidIndex(99));

    TEST_ASSERT(table.isGeneralPurpose(0));
    TEST_ASSERT(table.isGeneralPurpose(7));
    TEST_ASSERT(!table.isGeneralPurpose(RegisterTable::SPECIAL_REG_PC));

    // 5. Excecoes
    TEST_THROWS(table.getIndex("INVALID_REG"), std::invalid_argument);
    TEST_THROWS(table.getIndex(""), std::invalid_argument);
    TEST_THROWS(table.getName(8), std::out_of_range);
    TEST_THROWS(table.getName(255), std::out_of_range);

    std::cout << "[PASS] RegisterTable validado com sucesso!\n\n";
}

int main() {
    std::cout << "==================================================\n";
    std::cout << "  SUITE RIGOROSA: AUDITORIA DO MODULO REGISTRADORES\n";
    std::cout << "==================================================\n\n";

    testRegisterBank();
    testRegisterTable();

    std::cout << "TODOS OS TESTES DE AUDITORIA PASSARAM COM EXCELENCIA!\n";
    return 0;
}
