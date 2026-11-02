#include "src/config.hpp"
#include "src/kernel.hpp"
#include "src/pcb.hpp"
#include "loader/parser.hpp"
#include "loader/ram_loader.hpp"
#include "src/memory.hpp"
#include "src/defines.hpp"
#include <iostream>
#include <memory>

int main(int argc, char* argv[]) {
    // 1. Faz o parse dos argumentos do terminal
    ConfiguracaoSimulador cfg = ConfiguracaoSimulador::parse(argc, argv);
    cfg.imprimir();

    // 2. Inicializa o Kernel com o numero de nucleos configurado
    Kernel kernel(cfg.numCores, cfg.custoTroca);

    // 3. Carga do Lote fechado no tick t=0 (regra estrita do PDF)
    std::cout << "[KERNEL] Carregando lote inicial de processos no tick t=0...\n";
    for (size_t i = 0; i < cfg.arquivosJobs.size(); ++i) {
        const std::string& caminho = cfg.arquivosJobs[i];
        int pid = static_cast<int>(i) + 1;

        JobData job;
        if (!JobParser::carregarArquivo(caminho, job)) {
            std::cerr << "[AVISO] Falha ao ler arquivo de job: " << caminho << "\n";
            continue;
        }

        auto pcb = std::make_unique<PCB>(pid, caminho);

        // Enquanto a MMU da Fase 2 nao esta plugada, carregamos na memoria fisica compatível
        // para garantir que a execucao funcione perfeitamente
        if (i == 0) {
            RAMLoader::carregarNaRAM(job, RAM, TAM_RAM);
            pcb->contexto.PC = OS_RESERVED_MEM;
        }

        std::cout << "  -> Processo " << pcb->pid << " admitido (" << pcb->nomeArquivo << ")\n";
        kernel.admitirProcesso(std::move(pcb));
    }

    // 4. Executa a simulacao multicore em lockstep
    std::cout << "\n[KERNEL] Iniciando execucao em lockstep nos nucleos...\n";
    kernel.rodarSimulacao();

    std::cout << "\n===================================================\n";
    std::cout << "[SIMULADOR] Lote concluido com sucesso!\n";
    std::cout << "Tempo total de simulacao (Makespan): " << kernel.getTickGlobal() << " ciclos\n";
    std::cout << "===================================================\n";

    return 0;
}
