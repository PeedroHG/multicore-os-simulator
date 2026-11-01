#include "../control/control_unit.hpp"
#include "../registers/register_bank.hpp"
#include "../ula/ula.hpp"
#include "../../defines.hpp"
#include "../../memory.hpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>


#define TEST_ASSERT(condition)                                                 \
  do {                                                                         \
    if (!(condition)) {                                                        \
      std::cerr << "[FALHA] " << __FILE__ << ":" << __LINE__                   \
                << " Assercao falhou: " #condition << std::endl;               \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)

// Reinicia as estatísticas globais e a memória usada pelos testes para que um
// teste nunca interfira no resultado do outro
static void resetAmbiente(RegisterBank &regBank, ControlUnit &uc) {
  clockGlobal = 0;
  instExecutadas = 0;
  acessosMemoria = 0;
  acessosDisco = 0;
  regBank.reset();
  regBank.setPC(OS_RESERVED_MEM);
  uc.reset();
}

// Teste 1: Decodificação pura (mascaramento de bits), isolada do resto da CPU
void testDecode() {
  std::cout << "[TEST] Testando Decodificacao (mascaramento de bits)...\n";
  ControlUnit uc;

  // Exemplo do PDF do trabalho: 0x124F -> LOAD R2, [0x4F]
  InstrucaoDecodificada instr = uc.decode(0x124F);
  TEST_ASSERT(instr.opcode == 0x1);
  TEST_ASSERT(instr.regDest == 0x2);
  TEST_ASSERT(instr.endereco == 0x4F);

  // ADD R2, R0, R1 (0x2201)
  InstrucaoDecodificada add = uc.decode(0x2201);
  TEST_ASSERT(add.opcode == OP_ADD);
  TEST_ASSERT(add.regDest == 2);
  TEST_ASSERT(add.regF1 == 0);
  TEST_ASSERT(add.regF2 == 1);

  // JUMP para endereco absoluto 0x0A0 (0x80A0 -> opcode 8, resto 0x0A0)
  InstrucaoDecodificada jmp = uc.decode(0x80A0);
  TEST_ASSERT(jmp.opcode == OP_JUMP);
  TEST_ASSERT(jmp.enderecoJump == 0x0A0);

  std::cout << "[PASS] Decodificacao validada!\n";
}

// Teste 2: Fetch isolado (busca + atualizacao de PC/IR)
void testFetch() {
  std::cout << "[TEST] Testando Busca (Fetch) e atualizacao de PC/IR...\n";
  ControlUnit uc;
  RegisterBank regBank;
  resetAmbiente(regBank, uc);

  EscreverMemoria(OS_RESERVED_MEM, 0xF000); // HALT

  uint16_t pcAntes = regBank.getPC();
  uint16_t IR = uc.fetch(regBank);

  TEST_ASSERT(IR == 0xF000);
  TEST_ASSERT(regBank.getIR() == 0xF000);
  TEST_ASSERT(regBank.getPC() == pcAntes + 1);

  std::cout << "[PASS] Busca validada!\n";
}

// Teste 3: Ciclo completo C = A + B (exemplo central do PDF)
void testCicloCompletoSoma() {
  std::cout << "[TEST] Testando Ciclo Completo (Fetch-Decode-Execute): C = A + "
               "B...\n";
  ControlUnit uc;
  RegisterBank regBank;
  ULA ula;
  resetAmbiente(regBank, uc);

  // Programa: LOAD R0,[10]; LOAD R1,[11]; ADD R2,R0,R1; STORE R2,[12]; HALT
  uint16_t base = OS_RESERVED_MEM;
  EscreverMemoria(base + 0, 0x100A);
  EscreverMemoria(base + 1, 0x110B);
  EscreverMemoria(base + 2, 0x2201);
  EscreverMemoria(base + 3, 0x320C);
  EscreverMemoria(base + 4, 0xF000);

  EscreverMemoria(base + 10, 5); // A = 5
  EscreverMemoria(base + 11, 7); // B = 7
  EscreverMemoria(base + 12, 0); // C

  TEST_ASSERT(!uc.isHalted());

  uc.step(regBank, ula); // LOAD R0, [10]
  TEST_ASSERT(regBank.read(0) == 5);

  uc.step(regBank, ula); // LOAD R1, [11]
  TEST_ASSERT(regBank.read(1) == 7);

  uc.step(regBank, ula); // ADD R2, R0, R1
  TEST_ASSERT(regBank.read(2) == 12);
  TEST_ASSERT(!ula.overflow);

  uc.step(regBank, ula); // STORE R2, [12]
  TEST_ASSERT(LerMemoria(base + 12) == 12);

  uc.step(regBank, ula); // HALT
  TEST_ASSERT(uc.isHalted());

  // Depois do HALT, novos steps nao devem ter efeito algum
  uint16_t pcFinal = regBank.getPC();
  uc.step(regBank, ula);
  TEST_ASSERT(regBank.getPC() == pcFinal);

  std::cout << "[PASS] Ciclo completo de soma validado!\n";
}

// Teste 4: Controle de fluxo - BEQ (desvio tomado e nao tomado) e JUMP
void testControleDeFluxo() {
  std::cout << "[TEST] Testando Controle de Fluxo (BEQ / JUMP)...\n";
  ControlUnit uc;
  RegisterBank regBank;
  ULA ula;
  resetAmbiente(regBank, uc);

  uint16_t base = OS_RESERVED_MEM;

  // BEQ R0, R1, [20] -> opcode 7, regDest=0, regF1=1, endereco=0x14(20)
  // Como R0 == R1 (ambos 0), o desvio deve ser tomado: PC = base + 20
  EscreverMemoria(base + 0, 0x7014);
  uc.step(regBank, ula);
  TEST_ASSERT(regBank.getPC() == base + 20);

  // JUMP incondicional para o endereco absoluto 0x1FF
  EscreverMemoria(base + 20, 0x81FF);
  uc.step(regBank, ula);
  TEST_ASSERT(regBank.getPC() == base + 0x1FF);

  std::cout << "[PASS] Controle de fluxo validado!\n";
}

// Teste 5: Opcode invalido deve lancar excecao (protecao da UC)
void testOpcodeInvalido() {
  std::cout << "[TEST] Testando deteccao de Opcode Invalido...\n";
  ControlUnit uc;
  RegisterBank regBank;
  ULA ula;
  resetAmbiente(regBank, uc);

  // 0x9000 -> opcode 0x9, nao mapeado em nenhuma categoria
  EscreverMemoria(OS_RESERVED_MEM, 0x9000);

  bool lancou = false;
  try {
    uc.step(regBank, ula);
  } catch (const std::runtime_error &) {
    lancou = true;
  }
  TEST_ASSERT(lancou);

  std::cout << "[PASS] Deteccao de opcode invalido validada!\n";
}

int main() {
  std::cout << "==================================================\n";
  std::cout << "     TESTES DA UNIDADE DE CONTROLE (UC)\n";
  std::cout << "==================================================\n\n";

  testDecode();
  testFetch();
  testCicloCompletoSoma();
  testControleDeFluxo();
  testOpcodeInvalido();

  std::cout
      << "\nTODOS OS TESTES DA UNIDADE DE CONTROLE PASSARAM COM SUCESSO!\n";
  return 0;
}