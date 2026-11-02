#include "config.hpp"
#include <iostream>
#include <iomanip>

ConfiguracaoSimulador ConfiguracaoSimulador::parse(int argc, char* argv[]) {
    ConfiguracaoSimulador cfg;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--cores" && i + 1 < argc) {
            cfg.numCores = std::stoi(argv[++i]);
        } else if (arg == "--politica" && i + 1 < argc) {
            cfg.politica = argv[++i];
        } else if (arg == "--modo" && i + 1 < argc) {
            cfg.modo = argv[++i];
        } else if (arg == "--quantum" && i + 1 < argc) {
            cfg.quantum = std::stoul(argv[++i]);
        } else if (arg == "--substituicao" && i + 1 < argc) {
            cfg.substituicao = argv[++i];
        } else if (arg == "--molduras" && i + 1 < argc) {
            cfg.numMolduras = std::stoul(argv[++i]);
        } else if (arg == "--penalidade-falta" && i + 1 < argc) {
            cfg.penalidadeFalta = std::stoul(argv[++i]);
        } else if (arg == "--custo-troca" && i + 1 < argc) {
            cfg.custoTroca = std::stoul(argv[++i]);
        } else if (arg == "--aging" && i + 1 < argc) {
            cfg.intervaloAging = std::stoul(argv[++i]);
        } else if (arg == "--saida" && i + 1 < argc) {
            cfg.dirSaida = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Uso: ./simulador [opcoes] [arquivos_jobs...]\n"
                      << "Opcoes disponiveis:\n"
                      << "  --cores <N>             Numero de nucleos de CPU (padrao: 4)\n"
                      << "  --politica <alg>        Politica: fcfs, sjn, rr, prio (padrao: fcfs)\n"
                      << "  --modo <np|p>           Modo: np (nao-preemptivo), p (preemptivo) (padrao: np)\n"
                      << "  --quantum <Q>           Tamanho do quantum em ciclos (padrao: 10)\n"
                      << "  --substituicao <alg>    Substituicao: fifo, lru (padrao: fifo)\n"
                      << "  --molduras <M>          Numero de molduras da RAM para usuario (padrao: 16)\n"
                      << "  --penalidade-falta <C>  Custo da falta de pagina em ciclos (padrao: 50)\n"
                      << "  --custo-troca <C>       Custo de troca de contexto em ciclos (padrao: 5)\n"
                      << "  --aging <C>             Intervalo de envelhecimento em ciclos (padrao: 50)\n"
                      << "  --saida <dir>           Diretorio de saida de relatorios/logs\n";
            std::exit(0);
        } else if (arg.rfind("--", 0) != 0) {
            // Nao comeca com '--', logo e arquivo de job
            cfg.arquivosJobs.push_back(arg);
        }
    }

    // Se nenhum job foi passado na linha de comando, usa os padroes de teste
    if (cfg.arquivosJobs.empty()) {
        cfg.arquivosJobs = {
            "tests/job1_soma.txt",
            "tests/job2_vetor.txt",
            "tests/job3_loop.txt"
        };
    }

    return cfg;
}

void ConfiguracaoSimulador::imprimir() const {
    std::cout << "===================================================\n";
    std::cout << "  SIMULADOR RISC MULTICORE - CONFIGURACAO ATIVA    \n";
    std::cout << "===================================================\n";
    std::cout << " Nucleos de CPU:           " << numCores << "\n";
    std::cout << " Politica de Escalonamento:" << politica << "\n";
    std::cout << " Modo de Execucao:         " << (modo == "p" ? "Preemptivo" : "Nao-Preemptivo") << "\n";
    std::cout << " Quantum de Tempo:         " << quantum << " ciclos\n";
    std::cout << " Substituicao de Paginas:  " << substituicao << "\n";
    std::cout << " Molduras de Usuario (RAM):" << numMolduras << "\n";
    std::cout << " Penalidade Falta Pagina:  " << penalidadeFalta << " ciclos\n";
    std::cout << " Custo Troca de Contexto:  " << custoTroca << " ciclos\n";
    std::cout << " Intervalo de Aging:       " << intervaloAging << " ciclos\n";
    std::cout << " Total de Jobs no Lote:    " << arquivosJobs.size() << "\n";
    std::cout << "===================================================\n";
}

