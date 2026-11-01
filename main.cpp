#include "loader/batch_manager.hpp"
#include <iostream>
#include <vector>
#include <string>

int main(int argc, char* argv[]) {
    std::vector<std::string> listaJobs;

    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            listaJobs.push_back(argv[i]);
        }
    } else {
        // Cargas padrão de teste para validação do Simulador RISC
        listaJobs = {
            "tests/job1_soma.txt",
            "tests/job2_vetor.txt",
            "tests/job3_loop.txt"
        };
    }

    BatchManager::executarLote(listaJobs);
    return 0;
}