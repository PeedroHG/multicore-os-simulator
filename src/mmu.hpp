#ifndef MMU_HPP
#define MMU_HPP

/**
 * ============================================================================
 * MMU — Gerenciamento de Memória Paginada (Modelo Tanenbaum)
 * Responsável: João Paulo (Pessoa 2)
 * ============================================================================
 *
 * Fatiamento de bits (ver defines.hpp):
 *   - Lógico (12 bits):  [11:4] página (8 bits)  | [3:0] offset (4 bits)
 *   - Físico (16 bits):  [15:4] moldura (12 bits) | [3:0] offset (4 bits)
 *   - Molduras 0..31 pertencem ao SO (OS_RESERVED_MEM = 512 palavras).
 *
 * Estruturas:
 *   - Tabela de páginas POR PROCESSO, armazenada no PCB (pcb.tabelaPaginas),
 *     com os bits valida / suja / referenciada e o número da moldura.
 *   - Tabela de molduras GLOBAL (core map): livre, pid, página, carregadaEm
 *     (FIFO) e ultimoAcesso (LRU, contador monotônico de acessos).
 *   - Disco Virtual: uma imagem de 4096 palavras por PID (espaço lógico
 *     inteiro do processo), de onde as páginas são trazidas sob demanda.
 *
 * Paginação sob demanda + substituição GLOBAL (Tanenbaum 3.5.1):
 *   - Toda página começa inválida; o primeiro acesso gera falta de página.
 *   - Se não houver moldura livre, a vítima é escolhida entre TODAS as
 *     molduras de usuário (de qualquer processo) por FIFO ou LRU exato.
 *   - Vítima suja => write-back no Disco Virtual (custo extra de E/S).
 *
 * Modelo de tempo: a MMU não avança relógio nenhum. Cada acesso devolve em
 * ResultadoAcesso::ciclosPenalidade quantos ciclos o núcleo deve ficar em
 * stall (PENALIDADE_FALTA, + PENALIDADE_FALTA se houve write-back).
 */

#include "defines.hpp"
#include "pcb.hpp"
#include "../loader/clock_types.hpp"

#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

enum class PoliticaSubstituicao { FIFO, LRU };

/** Tipo do acesso, gravado no rastro de referências. */
enum class TipoAcesso : char {
    BUSCA   = 'X', // fetch de instrução
    LEITURA = 'R', // LOAD
    ESCRITA = 'W'  // STORE
};

/** Resultado de um acesso à memória através da MMU. */
struct ResultadoAcesso {
    uint16_t valor = 0;            ///< Valor lido (ou escrito)
    uint32_t ciclosPenalidade = 0; ///< Ciclos de stall a aplicar no núcleo
    bool falta = false;            ///< true se houve falta de página
};

/** Contadores de memória (globais ou por processo). */
struct EstatisticasMemoria {
    uint64_t acessos = 0;
    uint64_t acertos = 0;
    uint64_t faltas = 0;
    uint64_t writebacks = 0;

    double taxaFaltas() const {
        return acessos ? static_cast<double>(faltas) / static_cast<double>(acessos) : 0.0;
    }
};

/** Entrada da tabela de molduras (core map). */
struct Moldura {
    bool livre = true;
    int pid = -1;
    uint16_t pagina = 0;
    uint64_t carregadaEm = 0;  ///< ordem de carga (FIFO)
    uint64_t ultimoAcesso = 0; ///< carimbo do contador monotônico (LRU)
};

/** Uma referência à memória, para o rastro CSV (OPT de Belady offline). */
struct ReferenciaMemoria {
    uint64_t tick;
    int core;
    int pid;
    uint16_t pagina;
    char tipo;
    bool falta;
};

/**
 * Interface comum às MMUs (provisória e real). A ControlUnit e o Kernel só
 * conhecem esta interface, o que permite trocar a implementação sem tocar
 * no restante do simulador.
 */
class InterfaceMMU {
public:
    virtual ~InterfaceMMU() = default;

    /** Monta a imagem do job no Disco Virtual e inicializa a tabela de páginas. */
    virtual void carregarProcesso(PCB& pcb, const JobData& job) = 0;

    /** Devolve as molduras do processo ao terminar/abortar. */
    virtual void liberarProcesso(PCB& pcb) = 0;

    virtual ResultadoAcesso ler(PCB& pcb, uint16_t endLogico, int coreId, uint64_t tick,
                                TipoAcesso tipo = TipoAcesso::LEITURA) = 0;

    virtual ResultadoAcesso escrever(PCB& pcb, uint16_t endLogico, uint16_t dado,
                                     int coreId, uint64_t tick) = 0;

    /**
     * Lê o conteúdo lógico de um processo SEM efeitos colaterais (sem falta,
     * sem métricas). Usado para relatórios e testes de regressão.
     */
    virtual uint16_t lerImagem(int pid, uint16_t endLogico) const = 0;
};

/**
 * MMU Provisória (stub da Fase 1): tradução fixa, sem faltas de página.
 *     físico = 512 + pid * 256 + lógico
 * Limitação: cada processo só dispõe de 256 palavras lógicas.
 */
class MMUProvisoria : public InterfaceMMU {
public:
    static constexpr uint32_t TAM_PARTICAO = 256;

    void carregarProcesso(PCB& pcb, const JobData& job) override;
    void liberarProcesso(PCB& pcb) override;
    ResultadoAcesso ler(PCB& pcb, uint16_t endLogico, int coreId, uint64_t tick,
                        TipoAcesso tipo = TipoAcesso::LEITURA) override;
    ResultadoAcesso escrever(PCB& pcb, uint16_t endLogico, uint16_t dado,
                             int coreId, uint64_t tick) override;
    uint16_t lerImagem(int pid, uint16_t endLogico) const override;

    static uint32_t traduzir(int pid, uint16_t endLogico);
};

/**
 * MMU real: paginação sob demanda com substituição global FIFO ou LRU.
 */
class MMU : public InterfaceMMU {
public:
    MMU(PoliticaSubstituicao politica, uint32_t numMoldurasUsuario, uint32_t penalidadeFalta);

    void carregarProcesso(PCB& pcb, const JobData& job) override;
    void liberarProcesso(PCB& pcb) override;
    ResultadoAcesso ler(PCB& pcb, uint16_t endLogico, int coreId, uint64_t tick,
                        TipoAcesso tipo = TipoAcesso::LEITURA) override;
    ResultadoAcesso escrever(PCB& pcb, uint16_t endLogico, uint16_t dado,
                             int coreId, uint64_t tick) override;
    uint16_t lerImagem(int pid, uint16_t endLogico) const override;

    // --- Consultas (métricas, testes e relatórios) ---
    PoliticaSubstituicao getPolitica() const { return politica; }
    uint32_t getNumMolduras() const { return static_cast<uint32_t>(tabelaMolduras.size()); }
    uint32_t getPenalidadeFalta() const { return penalidadeFalta; }
    const std::vector<Moldura>& getTabelaMolduras() const { return tabelaMolduras; }
    const EstatisticasMemoria& getEstatisticas() const { return estatGlobal; }
    EstatisticasMemoria getEstatisticas(int pid) const;
    const std::vector<ReferenciaMemoria>& getRastro() const { return rastro; }
    uint32_t moldurasLivres() const;

    /** Liga/desliga a gravação do rastro de referências (ligada por padrão). */
    void setRegistrarRastro(bool ligado) { registrarRastro = ligado; }

    /** Grava o rastro (tick,core,pid,pagina,tipo,falta) em CSV. */
    bool exportarRastroCSV(const std::string& caminho) const;

    /** Grava as métricas de memória por processo + linha global em CSV. */
    bool exportarEstatisticasCSV(const std::string& caminho) const;

    static PoliticaSubstituicao politicaDeTexto(const std::string& nome);
    static const char* nomePolitica(PoliticaSubstituicao p);

private:
    PoliticaSubstituicao politica;
    uint32_t penalidadeFalta;

    std::vector<Moldura> tabelaMolduras;           ///< índice i -> moldura física MOLDURAS_SO + i
    std::deque<uint32_t> filaFIFO;                 ///< índices das molduras, em ordem de carga
    std::map<int, std::vector<uint16_t>> discoVirtual; ///< pid -> imagem lógica (4096 palavras)
    std::map<int, PCB*> processos;                 ///< pid -> PCB (para invalidar páginas vítimas)
    std::map<int, EstatisticasMemoria> estatPorProcesso;
    EstatisticasMemoria estatGlobal;

    uint64_t contadorAcessos = 0; ///< relógio lógico monotônico do LRU
    uint64_t contadorCargas = 0;  ///< ordem de carga do FIFO

    bool registrarRastro = true;
    std::vector<ReferenciaMemoria> rastro;

    /** Traduz lógico -> físico, tratando falta de página se necessário. */
    uint32_t traduzir(PCB& pcb, uint16_t endLogico, bool escrita, int coreId,
                      uint64_t tick, TipoAcesso tipo, ResultadoAcesso& res);

    /** Trata a falta: escolhe moldura (livre ou vítima), write-back e carga. */
    uint32_t tratarFalta(PCB& pcb, uint16_t pagina, ResultadoAcesso& res);

    uint32_t escolherVitima() const;
    void removerDaFilaFIFO(uint32_t indice);
    void copiarMolduraParaDisco(uint32_t indice);
};

#endif // MMU_HPP
