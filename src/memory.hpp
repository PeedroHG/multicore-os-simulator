#ifndef MEMORY_HPP
#define MEMORY_HPP

#include "defines.hpp"
#include <vector>
#include <cstdint>

extern uint16_t RAM[TAM_RAM];

// Estatísticas globais do sistema
extern int clockGlobal;
extern int instExecutadas;
extern int acessosMemoria;
extern int acessosDisco;

// INTERFACE DE MEMÓRIA E DISCO (Módulo de I/O)

/**
 * LerMemoria - Interface mediadora de leitura da RAM
 * @param endereco Endereço físico a ser lido
 * @return Valor armazenado no endereço
 * @throws std::runtime_error se endereço estiver na área reservada do SO
 */
uint16_t LerMemoria(uint16_t endereco);

/**
 * EscreverMemoria - Interface mediadora de escrita da RAM
 * @param endereco Endereço físico a ser escrito
 * @param dado     Valor a ser armazenado
 * @throws std::runtime_error se endereço estiver na área reservada do SO
 */
void EscreverMemoria(uint16_t endereco, uint16_t dado);

/**
 * LerDisco - Interface de Disco Simulado (Penalidade de I/O)
 * @param dadoBuscado Valor a ser procurado no disco
 * @param Disco       Vetor representando o disco
 * @return Índice onde o dado foi encontrado, ou -1 se não encontrado
 */
int LerDisco(uint16_t dadoBuscado, const std::vector<uint16_t>& Disco);

#endif 