#ifndef SO_HPP
#define SO_HPP

#include "cpu/cpu.hpp"
#include <vector>
#include <string>
#include <cstdint>

class SistemaOperacional {
private:
    CPU cpu;

    /**
     * carregarJob - Loader: copia instruções e dados do disco para a RAM
     * @param jobInstrucoes Vetor de instruções (código do programa)
     * @param dadosIniciais Vetor com os valores iniciais das variáveis
     */
    void carregarJob(const std::vector<uint16_t>& jobInstrucoes,
                     const std::vector<uint16_t>& dadosIniciais);

public:
    /**
     * executarJob - Executa um job em modo batch (bare-metal)
     * @param idJob   Identificador do job
     * @param nomeJob Nome descritivo do job
     * @param inst    Vetor de instruções (código)
     * @param dados   Vetor com dados iniciais (variáveis)
     */
    void executarJob(int idJob,
                     const std::string& nomeJob,
                     const std::vector<uint16_t>& inst,
                     const std::vector<uint16_t>& dados);
};

#endif 