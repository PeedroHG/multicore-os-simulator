#ifndef RAM_LOADER_HPP
#define RAM_LOADER_HPP

#include "clock_types.hpp"
#include <cstdint>
#include <vector>

/**
 * RAMLoader - Mapeamento e alocação de instruções e dados.
 *
 * No modelo multicore com paginação (Pessoa 2), os jobs NÃO são mais gravados
 * direto na RAM física: o loader monta a IMAGEM LÓGICA do processo (endereços
 * a partir de 0x000), que fica no Disco Virtual da MMU e é trazida para a RAM
 * página a página, sob demanda.
 */
class RAMLoader {
public:
    // [LEGADO single-core] Carrega o JobData na RAM física a partir de OS_RESERVED_MEM (512)
    static bool carregarNaRAM(const JobData& job, uint16_t* ram, uint32_t tamRam = 65536);

    /**
     * Monta a imagem lógica do processo (MAX_END_LOGICO + 1 = 4096 palavras):
     * instruções a partir do endereço lógico 0x000 e variáveis nos seus
     * endereços relativos. Retorna false se algo não couber no espaço lógico.
     */
    static bool montarImagem(const JobData& job, std::vector<uint16_t>& imagem);
};

#endif // RAM_LOADER_HPP
