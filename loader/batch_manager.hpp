#ifndef BATCH_MANAGER_HPP
#define BATCH_MANAGER_HPP

#include <vector>
#include <string>

/**
 * BatchManager - Gerenciador do ciclo de vida em lote, transição de jobs e reset de contexto
 * Integrante responsável: Integrante 7
 */
class BatchManager {
public:
    // Executa uma lista sequencial de arquivos de jobs em lote
    static void executarLote(const std::vector<std::string>& listaArquivosJobs);
};

#endif // BATCH_MANAGER_HPP
