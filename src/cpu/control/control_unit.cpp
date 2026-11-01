#include "control_unit.hpp"
#include "../../memory.hpp"
#include <stdexcept>
#include <string>

// ----------------------------------------------------------------------------
// Construtor e Controle Básico
// ----------------------------------------------------------------------------
ControlUnit::ControlUnit() : haltStatus(false) {}

void ControlUnit::reset() { haltStatus = false; }

bool ControlUnit::isHalted() const { return haltStatus; }

// ----------------------------------------------------------------------------
// Etapa 1: BUSCA (Fetch)
// ----------------------------------------------------------------------------
uint16_t ControlUnit::fetch(RegisterBank &regBank) {
  // Lê a instrução de 16 bits apontada pelo PC, através da Interface de
  // Memória (nunca acessando o vetor RAM diretamente).
  uint16_t pc = regBank.getPC();
  uint16_t IR = LerMemoria(pc);

  regBank.setIR(IR);
  regBank.incrementPC(); // Avança o PC para a próxima palavra

  return IR;
}

// ----------------------------------------------------------------------------
// Etapa 2: DECODIFICAÇÃO (Decode)
// ----------------------------------------------------------------------------
InstrucaoDecodificada ControlUnit::decode(uint16_t IR) const {
  InstrucaoDecodificada instr;

  instr.opcode = (IR >> 12) & 0x000F; // Bits [15:12] -> Código da Operação
  instr.regDest =
      (IR >> 8) & 0x000F; // Bits [11:8]  -> Registrador de Destino (ou Fonte 1)
  instr.regF1 =
      (IR >> 4) & 0x000F;       // Bits [7:4]   -> Primeiro Registrador Fonte
  instr.regF2 = IR & 0x000F;    // Bits [3:0]   -> Segundo Registrador Fonte
  instr.endereco = IR & 0x00FF; // Bits [7:0]   -> Endereço imediato (Tipo I)
  instr.enderecoJump = IR & 0x0FFF; // Bits [11:0]  -> Endereço absoluto (JUMP)

  return instr;
}

// ----------------------------------------------------------------------------
// Despacho para a ULA (operações puramente computacionais)
// ----------------------------------------------------------------------------
void ControlUnit::dispararULA(RegisterBank &regBank, ULA &ula, uint16_t opcode,
                              uint16_t regDest, uint16_t regF1,
                              uint16_t regF2) {
  // 1. A UC lê os operandos do RegisterBank...
  uint16_t val1 = regBank.read(regF1);
  uint16_t val2 = regBank.read(regF2);

  // 2. ...e comanda a ULA para realizar o cálculo (a UC não calcula nada).
  if (!ula.executeOpcode(opcode, val1, val2)) {
    throw std::runtime_error(
        "ControlUnit::dispararULA - Opcode invalido ou nao computacional: " +
        std::to_string(opcode));
  }

  // 3. A UC grava o resultado devolvido pela ULA de volta no destino.
  regBank.write(regDest, static_cast<uint16_t>(ula.result));
}

// ----------------------------------------------------------------------------
// Etapa 3: EXECUÇÃO (Execute)
// ----------------------------------------------------------------------------
void ControlUnit::execute(const InstrucaoDecodificada &instr,
                          RegisterBank &regBank, ULA &ula) {
  switch (instr.opcode) {

  // --- Transferência de Memória (Paradigma Load/Store) ---
  case OP_LOAD:
    // RAM -> Registrador. Endereço do job é relativo; soma-se a base
    // reservada ao SO para obter o endereço físico real.
    regBank.write(instr.regDest, LerMemoria(instr.endereco + OS_RESERVED_MEM));
    break;

  case OP_STORE:
    // Registrador -> RAM.
    EscreverMemoria(instr.endereco + OS_RESERVED_MEM,
                    regBank.read(instr.regDest));
    break;

  // --- Aritmética e Lógica (delegadas integralmente à ULA) ---
  case OP_ADD:
  case OP_SUB:
  case OP_AND:
  case OP_OR:
    dispararULA(regBank, ula, instr.opcode, instr.regDest, instr.regF1,
                instr.regF2);
    break;

  // --- Controle de Fluxo ---
  case OP_BEQ:
    // A UC pede à ULA para comparar (via subtração); a UC decide o desvio.
    // Obs: BEQ não é uma operação da ULA (enum operation só tem
    // ADD/SUB/AND_OP/OR_OP) — o desvio é lido a partir do resultado
    // de uma subtração: se (regDest - regF1) == 0, os valores são iguais.
    ula.execute(SUB, regBank.read(instr.regDest), regBank.read(instr.regF1));
    if (ula.result == 0) {
      regBank.setPC(instr.endereco + OS_RESERVED_MEM);
    }
    break;

  case OP_JUMP:
    // Salto incondicional: a UC altera o PC diretamente (não é
    // responsabilidade da ULA, conforme diretrizes do projeto).
    regBank.setPC(instr.enderecoJump + OS_RESERVED_MEM);
    break;

  // --- Sistema ---
  case OP_HALT:
    // Fim do job atual: sinaliza para o Sistema Operacional (Loader)
    // encerrar o laço de execução e carregar o próximo job do lote.
    haltStatus = true;
    break;

  default:
    throw std::runtime_error(
        "ControlUnit::execute - Opcode Invalido detectado: 0x" +
        std::to_string(instr.opcode));
  }
}

// ----------------------------------------------------------------------------
// Ciclo Completo de Instrução (Fetch -> Decode -> Execute)
// ----------------------------------------------------------------------------
void ControlUnit::step(RegisterBank &regBank, ULA &ula) {
  if (haltStatus)
    return;

  uint16_t IR = fetch(regBank);
  InstrucaoDecodificada instr = decode(IR);
  execute(instr, regBank, ula);

  instExecutadas++;
}