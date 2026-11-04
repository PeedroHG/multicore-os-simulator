#include "src/config.hpp"
#include "src/kernel.hpp"
#include "src/pcb.hpp"
#include "src/mmu.hpp"
#include "loader/parser.hpp"
#include "src/memory.hpp"
#include "src/defines.hpp"
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>

static const char* nomeEstado(EstadoProcesso e) {
    switch (e) {
        case EstadoProcesso::NOVO:       return "NOVO";
        case EstadoProcesso::PRONTO:     return "PRONTO";
        case EstadoProcesso::EXECUTANDO: return "EXECUTANDO";
        case EstadoProcesso::FINALIZADO: return "FINALIZADO";
        case EstadoProcesso::ABORTADO:   return "ABORTADO";
    }
    return "?";
}

int main(int argc, char* argv[]) {
    // 1. Faz o parse dos argumentos do terminal
    ConfiguracaoSimulador cfg = ConfiguracaoSimulador::parse(argc, argv);
    cfg.imprimir();

    // 2. Inicializa o gerenciador de memória paginada (MMU real, Pessoa 2)
    std::unique_ptr<MMU> mmu;
    try {
        mmu = std::make_unique<MMU>(MMU::politicaDeTexto(cfg.substituicao),
                                    cfg.numMolduras, cfg.penalidadeFalta);
    } catch (const std::exception& e) {
        std::cerr << "[ERRO] " << e.what() << "\n";
        return 1;
    }

    // 3. Inicializa o Kernel com o numero de nucleos configurado e liga a MMU
    Kernel kernel(cfg.numCores, cfg.custoTroca);
    kernel.conectarMMU(mmu.get());

    // 4. Carga do Lote fechado no tick t=0 (regra estrita do PDF): a imagem de
    //    cada job vai para o Disco Virtual; as paginas entram sob demanda.
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
        pcb->contexto.PC = 0x000; // PC logico: todo processo comeca em 0
        try {
            mmu->carregarProcesso(*pcb, job);
        } catch (const std::exception& e) {
            std::cerr << "[AVISO] " << e.what() << "\n";
            continue;
        }

        std::cout << "  -> Processo " << pcb->pid << " admitido (" << pcb->nomeArquivo << ")\n";
        kernel.admitirProcesso(std::move(pcb));
    }

    // 5. Executa a simulacao multicore em lockstep
    std::cout << "\n[KERNEL] Iniciando execucao em lockstep nos nucleos...\n";
    kernel.rodarSimulacao();

    std::cout << "\n===================================================\n";
    std::cout << "[SIMULADOR] Lote concluido com sucesso!\n";
    std::cout << "Tempo total de simulacao (Makespan): " << kernel.getTickGlobal() << " ciclos\n";
    std::cout << "===================================================\n";

    // 6. Resumo do gerenciamento de memoria
    const EstatisticasMemoria& eg = mmu->getEstatisticas();
    std::cout << "\n[MEMORIA] Substituicao " << MMU::nomePolitica(mmu->getPolitica())
              << " | " << mmu->getNumMolduras() << " molduras de usuario\n";
    std::cout << std::left << std::setw(5) << "PID" << std::setw(12) << "Estado"
              << std::setw(9) << "Acessos" << std::setw(8) << "Faltas"
              << std::setw(11) << "WriteBack" << "TaxaFaltas\n";
    for (const auto& p : kernel.getProcessos()) {
        EstatisticasMemoria e = mmu->getEstatisticas(p->pid);
        std::cout << std::left << std::setw(5) << p->pid << std::setw(12) << nomeEstado(p->estado)
                  << std::setw(9) << e.acessos << std::setw(8) << e.faltas
                  << std::setw(11) << e.writebacks << std::fixed << std::setprecision(4)
                  << e.taxaFaltas() << "\n";
    }
    std::cout << "Global: " << eg.acessos << " acessos, " << eg.acertos << " acertos, "
              << eg.faltas << " faltas, " << eg.writebacks << " write-backs (taxa "
              << std::fixed << std::setprecision(4) << eg.taxaFaltas() << ")\n";

    // 7. Exporta o rastro de referencias (OPT de Belady offline) e as metricas
    std::error_code ec;
    std::filesystem::create_directories(cfg.dirSaida, ec);
    const std::string rastro = cfg.dirSaida + "/referencias_memoria.csv";
    const std::string memCsv = cfg.dirSaida + "/memoria.csv";
    if (mmu->exportarRastroCSV(rastro) && mmu->exportarEstatisticasCSV(memCsv)) {
        std::cout << "[MEMORIA] Rastro salvo em " << rastro << " | metricas em " << memCsv << "\n";
    } else {
        std::cerr << "[AVISO] Nao foi possivel gravar os CSVs de memoria em " << cfg.dirSaida << "\n";
    }

    return 0;
}
