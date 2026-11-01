#ifndef PARSER_HPP
#define PARSER_HPP

#include "clock_types.hpp"
#include <string>
#include <vector>

/**
 * JobParser - Leitura de arquivos de jobs em lote (.txt) e simulação de latência de Disco
 * Integrante responsável: Integrante 5
 */
class JobParser {
public:
    // Carrega um arquivo de texto (.txt), ignora comentários e espaços, e extrai instruções/dados
    static bool carregarArquivo(const std::string& caminho, JobData& outJob);

    // Simula a leitura sequencial em disco com penalidade de 50 clocks por setor percorrido
    static int lerDisco(uint16_t dadoBuscado, const std::vector<uint16_t>& disco);
};

#endif // PARSER_HPP
