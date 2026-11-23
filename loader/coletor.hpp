#ifndef COLETOR_HPP
#define COLETOR_HPP

#include "metrics.hpp"
#include "../src/kernel.hpp"
#include "../src/mmu.hpp"

/**
 * Coletor — ponte entre o Kernel/MMU e as métricas formais (Seção 2.4).
 * Responsável: Otávio Hiratsuka (Pessoa 4)
 *
 * Copia os contadores brutos dos PCBs, dos núcleos e da MMU para um
 * ResultadoExecucao. Os campos derivados (TR, TME, TMR, utilização, eficiência,
 * throughput) ficam por conta de Metricas::consolidar(), chamado depois.
 */
ResultadoExecucao coletarResultado(const Kernel& kernel, const MMU& mmu,
                                   const ConfigExperimento& config);

#endif // COLETOR_HPP