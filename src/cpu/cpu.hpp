#ifndef CPU_HPP
#define CPU_HPP

#include "registers/register_bank.hpp"
#include "ula/ula.hpp"
#include "control/control_unit.hpp"
#include "../memory.hpp"
#include "../defines.hpp"
#include <cstdint>
#include <iostream>
#include <ostream>

/**
 * ============================================================================
 * @brief Unidade Central de Processamento (CPU)
 * ============================================================================
 * Atua como o núcleo integrador da arquitetura computacional simulada.
 * 
 * Responsabilidades:
 * 1. Integração Modular: possui o Banco de Registradores (RegisterBank),
 *    a ULA e a Unidade de Controle (ControlUnit), e os conecta entre si.
 * 2. Delegação do Ciclo de Instrução: a CPU não busca, decodifica nem executa
 *    instruções por conta própria — isso é responsabilidade exclusiva da UC.
 *    A CPU apenas aciona controlUnit.step(regBank, ula) a cada pulso de clock.
 * 3. Barramento de Memória: a leitura/escrita em RAM (LOAD/STORE) é feita
 *    pela UC através da interface mediadora LerMemoria/EscreverMemoria.
 */
class CPU {
private:
    RegisterBank regBank;     ///< Banco de Registradores modularizado (R0..R7, PC, IR)
    ULA ula;                  ///< Unidade Lógica e Aritmética modularizada
    ControlUnit controlUnit;  ///< Unidade de Controle: orquestra Fetch -> Decode -> Execute

public:
    CPU();

    /** Reinicia a CPU (zera registradores, aponta PC para a área inicial do job e limpa o HALT da UC) */
    void reset();

    /** Define o valor do Program Counter */
    void setPC(uint16_t startAddress);

    /** Retorna true se a UC já processou um HALT (job atual finalizado) */
    bool isHalted() const;

    /**
     * @brief step - Executa um ciclo completo de instrução.
     * A CPU delega inteiramente o ciclo (Busca, Decodificação, Execução) à
     * Unidade de Controle, fornecendo a ela o RegisterBank e a ULA.
     * @throws std::runtime_error se a UC encontrar um opcode desconhecido.
     */
    void step();

    /** Exibe o estado atual da CPU (PC, IR e registradores) em qualquer stream */
    void imprimirEstado(std::ostream& out = std::cout) const;

    /** Acesso ao Banco de Registradores */
    RegisterBank& getRegisterBank();
    const RegisterBank& getRegisterBank() const;

    /** Acesso à ULA */
    ULA& getULA();
    const ULA& getULA() const;

    /** Acesso à Unidade de Controle */
    ControlUnit& getControlUnit();
    const ControlUnit& getControlUnit() const;
};

#endif // CPU_HPP