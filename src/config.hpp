#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <cstdint>

struct ConfiguracaoSimulador {
    int numCores = 4;
    std::string politica = "fcfs";        // fcfs, sjn, rr, prio
    std::string modo = "np";              // np (nao preemptivo), p (preemptivo)
    uint32_t quantum = 10;
    std::string substituicao = "fifo";    // fifo, lru
    uint32_t numMolduras = 16;
    uint32_t penalidadeFalta = 50;
    uint32_t custoTroca = 5;
    uint32_t intervaloAging = 50;
    std::string dirSaida = "output";
    std::vector<std::string> arquivosJobs;

    static ConfiguracaoSimulador parse(int argc, char* argv[]);
    void imprimir() const;
};

#endif // CONFIG_HPP

