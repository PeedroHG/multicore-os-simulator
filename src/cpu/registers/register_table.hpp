#ifndef REGISTER_TABLE_HPP
#define REGISTER_TABLE_HPP

#include <string>
#include <unordered_map>
#include <cstdint>

/**
 * ============================================================================
 * @brief Tabela de Símbolos para Mapeamento Bidirecional de Registradores
 * ============================================================================
 * Esta classe atua como o tradutor / analisador léxico dos nomes de registradores.
 * 
 * Finalidades principais:
 * 1. Parser/Montador (String -> Código Numérico):
 *    - Converte representações textuais em código binário/índice aceito pela CPU.
 *    - Suporta aliases comuns de montadores Assembly, como:
 *      "R0", "r0", "$r0", "$0", bem como "PC", "$PC", "IR", "$IR".
 * 
 * 2. Desmontador/Depurador (Código Numérico -> String Canônica):
 *    - Converte índices numéricos (0 a 7, 100, 101) de volta para seus nomes
 *      canônicos legíveis ("R0", "R7", "PC", "IR").
 * 
 * 3. Validação e Higienização:
 *    - Trata espaços extras, tabulações e vírgulas residuais comuns em linhas assembly.
 */
class RegisterTable {
public:
    /**
     * @brief Códigos numéricos reservados para registradores especiais de controle.
     * Utiliza valores fora do intervalo dos registradores gerais (0..7) para evitar colisões.
     */
    enum SpecialRegs : uint8_t {
        SPECIAL_REG_PC = 100, ///< Identificador numérico interno para o PC
        SPECIAL_REG_IR = 101  ///< Identificador numérico interno para o IR
    };

private:
    std::unordered_map<std::string, uint8_t> nameToIndex; ///< Mapeamento Nome -> Índice
    std::unordered_map<uint8_t, std::string> indexToName; ///< Mapeamento Reverso Índice -> Nome Canônico

    /**
     * @brief Higieniza e normaliza uma string de registrador.
     * Remove espaços em branco, quebras de linha e vírgulas nas extremidades,
     * convertendo todos os caracteres para letras maiúsculas.
     * @param str String bruta a ser normalizada (ex: "  r0, ").
     * @return String normalizada em maiúsculas (ex: "R0").
     */
    static std::string sanitizeAndUpper(const std::string& str);

public:
    /**
     * @brief Construtor da RegisterTable.
     * Popula as tabelas hash de mapeamento bidirecional com os registradores
     * gerais (R0..R7) e especiais (PC, IR), incluindo suas variações de sintaxe.
     */
    RegisterTable();

    /**
     * @brief Obtém o índice/código numérico do registrador a partir do nome textual.
     * @param regName Nome textual do registrador (ex: "R0", "r0", "$r0", "PC", "IR").
     * @return Código numérico correspondente (0 a 7 para gerais; 100 para PC; 101 para IR).
     * @throws std::invalid_argument Se o nome do registrador não for reconhecido.
     */
    uint8_t getIndex(const std::string& regName) const;

    /**
     * @brief Obtém o nome canônico do registrador a partir de seu código numérico.
     * @param index Código numérico do registrador (0..7, 100, 101).
     * @return Nome canônico formatado (ex: "R0", "R7", "PC", "IR").
     * @throws std::out_of_range Se o código numérico for inválido.
     */
    std::string getName(uint8_t index) const;

    /**
     * @brief Verifica se uma string corresponde a um nome de registrador válido.
     * @param regName Nome textual a ser testado.
     * @return true se o registrador existir na tabela; false caso contrário.
     */
    bool isValidName(const std::string& regName) const;

    /**
     * @brief Verifica se um código numérico corresponde a um registrador válido (geral ou especial).
     * @param index Código numérico a ser testado.
     * @return true se o código for válido (0..7, 100, 101); false caso contrário.
     */
    bool isValidIndex(uint8_t index) const;

    /**
     * @brief Verifica se o índice corresponde estritamente a um registrador de uso geral (R0..R7).
     * @param index Código numérico a ser testado.
     * @return true se 0 <= index < 8; false caso contrário.
     */
    bool isGeneralPurpose(uint8_t index) const;
};

#endif // REGISTER_TABLE_HPP

