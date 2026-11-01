#include "register_table.hpp"
#include "register_bank.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

// ----------------------------------------------------------------------------
// Sanitização e Normalização de Strings de Entrada
// ----------------------------------------------------------------------------
std::string RegisterTable::sanitizeAndUpper(const std::string& str) {
    if (str.empty()) return "";

    // 1. Trim: Localiza o primeiro e o último caractere válido
    // Ignora espaços, tabulações (\t), retornos de carro (\r), quebras de linha (\n) e vírgulas (,)
    size_t first = str.find_first_not_of(" \t\r\n,");
    if (first == std::string::npos) return ""; // String composta apenas por espaços/delimitadores
    
    size_t last = str.find_last_not_of(" \t\r\n,");
    std::string trimmed = str.substr(first, (last - first + 1));

    // 2. Conversão segura para caixa alta (maiúsculas)
    // O cast explícito para (unsigned char) é boa prática para evitar Comportamento Indefinido (UB) em std::toupper
    std::transform(trimmed.begin(), trimmed.end(), trimmed.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    return trimmed;
}

// ----------------------------------------------------------------------------
// Construtor: Inicialização dos Mapeamentos Bidirecionais
// ----------------------------------------------------------------------------
RegisterTable::RegisterTable() {
    // 1. Registradores Gerais (R0 a R7)
    for (uint8_t i = 0; i < RegisterBank::NUM_REGISTERS; ++i) {
        std::string canonical = "R" + std::to_string(i);
        
        // Mapeamento Canônico: "R0" -> 0 e 0 -> "R0"
        nameToIndex[canonical] = i;
        indexToName[i] = canonical;

        // Variações e aliases suportados para compatibilidade com código assembly:
        // Ex: "$R0" (padrão MIPS) e "$0" (número puro com cifrão)
        nameToIndex["$" + canonical] = i;
        nameToIndex["$" + std::to_string(i)] = i;
    }

    // 2. Registradores Especiais de Controle (PC e IR)
    // Program Counter (PC)
    nameToIndex["PC"] = SPECIAL_REG_PC;
    nameToIndex["$PC"] = SPECIAL_REG_PC;
    indexToName[SPECIAL_REG_PC] = "PC";

    // Instruction Register (IR)
    nameToIndex["IR"] = SPECIAL_REG_IR;
    nameToIndex["$IR"] = SPECIAL_REG_IR;
    indexToName[SPECIAL_REG_IR] = "IR";
}

// ----------------------------------------------------------------------------
// Métodos de Validação
// ----------------------------------------------------------------------------
bool RegisterTable::isValidName(const std::string& regName) const {
    std::string normalized = sanitizeAndUpper(regName);
    if (normalized.empty()) return false;
    return nameToIndex.find(normalized) != nameToIndex.end();
}

bool RegisterTable::isValidIndex(uint8_t index) const {
    return indexToName.find(index) != indexToName.end();
}

bool RegisterTable::isGeneralPurpose(uint8_t index) const {
    // Retorna true somente para os 8 registradores gerais (0 a 7)
    return index < RegisterBank::NUM_REGISTERS;
}

// ----------------------------------------------------------------------------
// Consulta Nome -> Índice
// ----------------------------------------------------------------------------
uint8_t RegisterTable::getIndex(const std::string& regName) const {
    // Normaliza a string de entrada para ignorar diferenças de caixa ou espaços
    std::string normalized = sanitizeAndUpper(regName);
    auto it = nameToIndex.find(normalized);
    if (it != nameToIndex.end()) {
        return it->second;
    }
    // Lança exceção com mensagem detalhada se o registrador for inválido
    throw std::invalid_argument("RegisterTable::getIndex - Registrador desconhecido: '" + regName + "'");
}

// ----------------------------------------------------------------------------
// Consulta Índice -> Nome Canônico
// ----------------------------------------------------------------------------
std::string RegisterTable::getName(uint8_t index) const {
    auto it = indexToName.find(index);
    if (it != indexToName.end()) {
        return it->second;
    }
    // Lança exceção de fora de limites se o índice não pertencer à tabela
    throw std::out_of_range("RegisterTable::getName - Indice invalido: " + std::to_string(index));
}

