#ifndef PCB_HPP
#define PCB_HPP

#include "cpu/registers/register_bank.hpp"
#include <cstdint>
#include <string>
#include <vector>

enum class EstadoProcesso {
    NOVO,
    PRONTO,
    EXECUTANDO,
    FINALIZADO,
    ABORTADO
};

struct EntradaPagina {
    bool valida = false;
    bool suja = false;
    bool referenciada = false;
    uint16_t moldura = 0;
};

struct PCB {
    // Informações básicas da PCB
    int pid = 0;
    std::string nomeArquivo;
    EstadoProcesso estado = EstadoProcesso::NOVO;

    // Estrutura que guarda o contexto do processador
    CPUContext contexto{};
    bool halted = false;

    // Informações do escalonador
    int prioridadeBase = 0;
    int prioridadeEfetiva = 0;
    uint32_t estimativa = 0;
    uint32_t instrucoesExecutadas = 0;
    uint32_t quantumUsado = 0;

    std::vector<EntradaPagina> tabelaPaginas;

    // Métricas para estatísticas
    uint64_t cicloChegada = 0;
    uint64_t cicloInicio = 0;
    uint64_t cicloFim = 0;
    uint64_t ultimoEntrouPronto = 0;

    uint64_t ciclosEspera = 0;
    uint64_t ciclosExecucao = 0;
    uint64_t ciclosStall = 0;
    uint32_t trocasContexto = 0;
    uint32_t faltasPagina = 0;
    int ultimoCore = -1;

    PCB() = default;

    PCB(int id, const std::string& nome, int prio = 0, uint32_t est = 0)
        : pid(id),
          nomeArquivo(nome),
          estado(EstadoProcesso::NOVO),
          prioridadeBase(prio),
          prioridadeEfetiva(prio),
          estimativa(est) {
        contexto.PC = 0;
        contexto.IR = 0;
        contexto.R.fill(0);
    }
};

#endif // PCB_HPP
