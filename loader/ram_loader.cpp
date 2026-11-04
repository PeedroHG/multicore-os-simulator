#include "ram_loader.hpp"
#include "../src/defines.hpp"

// OS_RESERVED_MEM agora vem de src/defines.hpp (antes era redefinido aqui).
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

bool RAMLoader::montarImagem(const JobData& job, std::vector<uint16_t>& imagem) {
    const uint32_t tamEspacoLogico = static_cast<uint32_t>(MAX_END_LOGICO) + 1; // 4096 palavras
    imagem.assign(tamEspacoLogico, 0);

    // 1) Instruções a partir do endereço LÓGICO 0x000 (o PC inicial é 0)
    if (job.instrucoes.size() > tamEspacoLogico) {
        return false;
    }
    for (size_t i = 0; i < job.instrucoes.size(); ++i) {
        imagem[i] = job.instrucoes[i];
    }

    // 2) Variáveis nos seus endereços lógicos (já relativos ao início do processo)
    for (const auto& variavel : job.variaveis) {
        if (variavel.first > MAX_END_LOGICO) {
            return false;
        }
        imagem[variavel.first] = variavel.second;
    }

    return true;
}
