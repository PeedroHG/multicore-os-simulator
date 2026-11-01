#include "ram_loader.hpp"

// Endereço base da RAM física após a área reservada ao Sistema Operacional.
// (Mesmo valor citado no comentário do ram_loader.hpp: OS_RESERVED_MEM = 512)
static const uint16_t OS_RESERVED_MEM = 512;
static const uint32_t DEFAULT_RAM_SIZE = 65536;

bool RAMLoader::carregarNaRAM(const JobData& job, uint16_t* ram, uint32_t tamRam) {
    if (ram == nullptr) {
        return false;
    }

    // Proteção defensiva: caso tamRam venha zerado por overflow de 16-bits (ex: TAM_RAM = 65536),
    // assume o padrão de 64K palavras da especificação
    if (tamRam == 0) {
        tamRam = DEFAULT_RAM_SIZE;
    }

    // 1) Copiar as instruções para a RAM, a partir do endereço base (512)
    uint32_t enderecoInstrucao = OS_RESERVED_MEM;

    for (uint16_t instrucao : job.instrucoes) {
        if (enderecoInstrucao >= tamRam) {
            // Job não cabe na RAM disponível
            return false;
        }
        ram[enderecoInstrucao] = instrucao;
        enderecoInstrucao++;
    }

    // 2) Posicionar as variáveis nos endereços corretos.
    // O endereço de cada variável é relativo à base (512), conforme
    // documentado no campo `variaveis` de JobData.
    for (const auto& variavel : job.variaveis) {
        uint16_t enderecoRelativo = variavel.first;
        uint16_t valor = variavel.second;

        uint32_t enderecoAbsoluto = static_cast<uint32_t>(OS_RESERVED_MEM) + enderecoRelativo;

        if (enderecoAbsoluto >= tamRam) {
            // Endereço da variável estoura o limite da RAM
            return false;
        }

        ram[enderecoAbsoluto] = valor;
    }

    return true;
}
