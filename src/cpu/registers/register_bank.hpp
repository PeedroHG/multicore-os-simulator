#ifndef REGISTER_BANK_HPP
#define REGISTER_BANK_HPP

#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <string>
#include <iostream>
#include <ostream>

/**
 * ============================================================================
 * @brief Estrutura de Contexto da CPU (CPUContext)
 * ============================================================================
 * Representa o estado completo dos registradores de hardware da CPU em um
 * determinado instante de tempo.
 * 
 * Papel em Sistemas Operacionais:
 * - Essencial para o Process Control Block (PCB) e multiprogramação.
 * - Utilizado durante a "Troca de Contexto" (Context Switch) ao alternar entre
 *   processos: o SO salva os registradores do processo atual nesta estrutura e
 *   restaura os registradores do próximo processo escalonado.
 */
struct CPUContext {
    std::array<uint16_t, 8> R; ///< Registradores de uso geral R0 a R7 (16 bits cada)
    uint16_t PC;               ///< Program Counter (Contador de Programa)
    uint16_t IR;               ///< Instruction Register (Registrador de Instrução)
};

/**
 * ============================================================================
 * @brief Banco de Registradores da CPU RISC (16 bits)
 * ============================================================================
 * O Banco de Registradores (Register Bank / Register File) é o conjunto de
 * memórias de altíssima velocidade localizadas internamente no núcleo do processador.
 * 
 * Componentes arquiteturais gerenciados:
 * 1. 8 Registradores de Uso Geral (R0 a R7):
 *    - Cada registrador armazena 16 bits (uint16_t).
 *    - Utilizados para guardar operandos intermediários de operações da ULA
 *      e dados carregados/salvos da memória RAM (LOAD/STORE).
 *    - Obs: Na arquitetura adotada, R0 é de leitura e escrita normal.
 * 
 * 2. Registradores Especiais de Controle:
 *    - PC (Program Counter): Armazena o endereço da próxima instrução a ser buscada.
 *    - IR (Instruction Register): Armazena a instrução de 16 bits atualmente decodificada.
 */
class RegisterBank {
public:
    /// Quantidade fixa de registradores de uso geral na arquitetura (R0 a R7)
    static constexpr size_t NUM_REGISTERS = 8;

private:
    // --- Memória Interna de Registradores ---
    std::array<uint16_t, NUM_REGISTERS> R; ///< Vetor estático de 8 registradores gerais (R0..R7)
    uint16_t PC;                           ///< Program Counter (Endereço da próxima instrução)
    uint16_t IR;                           ///< Instruction Register (Instrução atual)

public:
    /**
     * @brief Construtor padrão.
     * Inicializa todos os registradores gerais (R0..R7), PC e IR com o valor 0x0000.
     */
    RegisterBank();

    // ========================================================================
    // --- Acesso aos Registradores de Uso Geral (R0..R7) ---
    // ========================================================================

    /**
     * @brief Lê o valor armazenado em um registrador de uso geral.
     * @param index Índice do registrador (0 a 7, correspondendo a R0 a R7).
     * @return Valor de 16 bits armazenado no registrador.
     * @throws std::out_of_range Se o índice for inválido (index >= 8).
     */
    uint16_t read(size_t index) const;

    /**
     * @brief Escreve um valor de 16 bits em um registrador de uso geral.
     * @param index Índice do registrador (0 a 7, correspondendo a R0 a R7).
     * @param value Valor de 16 bits a ser gravado.
     * @throws std::out_of_range Se o índice for inválido (index >= 8).
     */
    void write(size_t index, uint16_t value);

    // ========================================================================
    // --- Controle de Registradores Especiais (PC e IR) ---
    // ========================================================================

    /**
     * @brief Obtém o valor atual do Program Counter (PC).
     * @return Endereço de memória de 16 bits apontado pelo PC.
     */
    uint16_t getPC() const noexcept;

    /**
     * @brief Define um novo valor para o Program Counter (PC).
     * Usado em desvios de fluxo (JUMP, BEQ) ou inicialização de processos.
     * @param address Novo endereço de memória (16 bits).
     */
    void setPC(uint16_t address) noexcept;

    /**
     * @brief Incrementa o Program Counter em uma unidade (PC = PC + 1).
     * Executado durante a etapa de busca (Fetch) do ciclo de instrução.
     */
    void incrementPC() noexcept;

    /**
     * @brief Obtém o valor atual do Instruction Register (IR).
     * @return Palavra de instrução de 16 bits atualmente carregada.
     */
    uint16_t getIR() const noexcept;

    /**
     * @brief Armazena uma instrução no Instruction Register (IR).
     * Chamado após a busca da memória durante o ciclo de instrução.
     * @param instruction Instrução binária de 16 bits.
     */
    void setIR(uint16_t instruction) noexcept;

    // ========================================================================
    // --- Métodos de Controle, Troca de Contexto e Suporte ao SO (PCB) ---
    // ========================================================================

    /**
     * @brief Reinicia todos os registradores (R0..R7 = 0, PC = 0, IR = 0).
     * Utilizado para inicialização da CPU ou limpeza entre jobs em lote.
     */
    void reset() noexcept;

    /**
     * @brief Captura o estado completo de hardware da CPU em uma estrutura CPUContext.
     * Fundamental para o SO salvar o contexto de um processo no seu PCB.
     * @return Objeto CPUContext com cópia dos valores de R0..R7, PC e IR.
     */
    CPUContext getContext() const noexcept;

    /**
     * @brief Restaura o estado da CPU a partir de um objeto CPUContext prévio.
     * Fundamental para o SO restaurar o contexto de um processo a partir do PCB.
     * @param ctx Objeto contendo os dados salvos de R0..R7, PC e IR.
     */
    void setContext(const CPUContext& ctx) noexcept;

    /**
     * @brief Retorna uma cópia dos 8 registradores gerais em um std::vector.
     * Método de conveniência e compatibilidade com interfaces dinâmicas.
     * @return Vetor de tamanho 8 com os valores de R0 a R7.
     */
    std::vector<uint16_t> getRegistersSnapshot() const;

    /**
     * @brief Carrega os registradores de uso geral a partir de um snapshot em vetor.
     * @param snapshot Vetor contendo exatamente 8 valores de 16 bits.
     * @throws std::invalid_argument Se o vetor não contiver exatamente 8 elementos.
     */
    void loadRegistersSnapshot(const std::vector<uint16_t>& snapshot);

    // ========================================================================
    // --- Saída e Diagnóstico Formato Padrão ---
    // ========================================================================

    /**
     * @brief Imprime o estado formatado dos registradores da CPU em hexadecimal.
     * 
     * Formata no padrão exigido pela especificação:
     * - Valores hexadecimais com 4 dígitos maiúsculos (ex: 0x000F).
     * - Suporta qualquer stream de saída (std::cout para console, std::ofstream para output.dat).
     * - Restaura o estado original de formatação do stream ao finalizar.
     * 
     * @param out Fluxo de saída de destino (padrão: std::cout).
     */
    void imprimirEstado(std::ostream& out = std::cout) const;
};

#endif // REGISTER_BANK_HPP

