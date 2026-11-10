# Checklist de Desenvolvimento por Integrante (Sincronizado)

Este documento reflete fielmente as decisões de projeto e contratos globais fechados no [PLANO_DE_DESENVOLVIMENTO.md](PLANO_DE_DESENVOLVIMENTO.md), garantindo 100% de conformidade com os critérios de avaliação da especificação oficial ([pratica.pdf](pratica.pdf)).

---

## 👤 Pedro Henrique Galvão (Arquitetura Multicore, Kernel Lockstep, PCB, Troca de Contexto e CLI)

### Fase 1: Interfaces e Estruturas de Dados (até 08/11)
- [x] Criar o arquivo `src/pcb.hpp` reaproveitando `CPUContext` de `src/cpu/registers/register_bank.hpp`:
  - [x] Campos de identificação: `int pid`, `std::string nomeArquivo`.
  - [x] Estados do processo: `enum class EstadoProcesso { NOVO, PRONTO, EXECUTANDO, FINALIZADO, ABORTADO }` (sem BLOQUEADO, falta de página gera *stall*).
  - [x] Contexto de hardware: `CPUContext contexto` e `bool halted` (para salvar o `haltStatus` da UC).
  - [x] Parâmetros de escalonamento: `int prioridadeBase`, `int prioridadeEfetiva`, `uint32_t estimativa`, `uint32_t instrucoesExecutadas`, `uint32_t quantumUsado`.
  - [x] Tabela de páginas por processo: `std::vector<EntradaPagina> tabelaPaginas`.
  - [x] Contadores e métricas de tempo: `cicloInicio`, `cicloFim`, `ultimoEntrouPronto`, `ciclosEspera`, `ciclosExecucao`, `ciclosStall`, `trocasContexto`, `faltasPagina`, `ultimoCore`.
- [x] Definir a interface de salvamento/restauração de contexto em `src/cpu/cpu.hpp` (`salvarContexto(PCB&)` e `restaurarContexto(const PCB&)`).
- [x] Definir a interface do `Kernel` em `src/kernel.hpp` contendo o laço central de simulação em lockstep com `uint64_t tick` único.
- [x] Criar a tag Git do baseline para comparações experimentais futuras: `git tag v1-baseline 3351311`.

### Fase 2: Implementação do Core Multicore & Laço de Tempo (até 22/11)
- [x] Adaptar a classe `CPU` (`src/cpu/cpu.hpp` e `src/cpu/cpu.cpp`):
  - [x] Adicionar identificador do núcleo (`int coreId`), ponteiro para processo atual (`PCB* processoAtual`) e contador de instrução multictick (`uint32_t ciclosRestantes`).
  - [x] Implementar `salvarContexto(PCB&)` e `restaurarContexto(const PCB&)` integrados ao `RegisterBank` e `ControlUnit`.
- [ ] Atualizar `ControlUnit` (`src/cpu/control/control_unit.cpp`):
  - [ ] Remover a adição estática de `+ OS_RESERVED_MEM` em `LOAD`, `STORE`, `BEQ` e `JUMP`.
  - [ ] Redirecionar todas as leituras e escritas (inclusive o `fetch` de instruções) para a MMU, passando `endLogico`, `pid` e `coreId`.
  - [ ] Inicializar o PC lógico sempre em `0x000`.
- [x] Implementar o `Kernel` (`src/kernel.cpp`):
  - [x] Instanciar `std::vector<CPU> cores(n)`.
  - [x] Laço determinístico de clock global: a cada tick `t`, iterar núcleos de `0` até `n-1`.
  - [x] Tratar penalidades de *stall* (falta de página) e custos de troca de contexto (`CUSTO_TROCA`, padrão 5 ciclos, apenas se o PID mudar).
  - [x] Atualizar contadores de estado de cada PCB (`ciclosEspera` para quem está na fila de prontos; `ciclosExecucao`/`ciclosStall` para quem está nos núcleos).
  - [x] Integrar proteção contra laços infinitos (`LIMITE_INSTRUCOES_POR_JOB`).
- [x] Adaptar `loader/batch_manager.cpp` e `main.cpp`:
  - [x] Implementar parsing dos argumentos de linha de comando (`--cores`, `--politica`, `--modo`, `--quantum`, `--substituicao`, `--molduras`, `--penalidade-falta`, `--custo-troca`, `--aging`, `--saida`).
  - [x] Substituir o uso da classe legada/morta `SistemaOperacional` pelo novo `Kernel`.

### Fase 3: Integração e Validação (até 29/11)
- [x] Criar teste de regressão e validação multicore (`tests/test_multicore.cpp`):
  - [x] Validar que com `--cores 1 --politica fcfs --modo np` e memória folgada, os resultados de registradores e memória são idênticos ao baseline (`v1-baseline`).
  - [x] Validar execução paralela com 2 e 4 núcleos sem colisão de contexto.
- [x] Criar o script mestre de execução da matriz de testes `scripts/benchmark.sh`.

### Fase 4: Artigo IEEE & Análise de Resultados (até 05/12)
- [x] Redigir a seção de **Arquitetura Multicore e Modelo de Execução Lockstep**.
- [x] Descrever a estrutura do **PCB** e a semântica da **Troca de Contexto**.
- [x] Elaborar o **Fluxograma Geral do Sistema** (Ciclo de Vida de Processos e Laço do Kernel).
- [ ] Analisar e discutir os dados do **Experimento E1** (Speedup $S(n)$, Eficiência Paralela $E(n)$ e Throughput global variando $n \in \{1, 2, 4, 8\}$). *(Aguardando execução dos benchmarks finais com os lotes da equipe)*

---

## 👤 João Paulo (Gerenciamento de Memória - Paginação, Modelo Tanenbaum)

### Fase 1: Interfaces, Fatiamento de Bits e MMU Provisória (até 08/11)
- [ ] Definir constantes e máscaras de paginação em `src/defines.hpp`:
  - [ ] `BITS_OFFSET = 4`, `TAM_PAGINA = 16` palavras.
  - [ ] `MASK_OFFSET = 0x000F`.
  - [ ] `MAX_END_LOGICO = 0x0FFF` (12 bits; bits `[15:12]` devem ser 0).
  - [ ] `MOLDURAS_SO = 32` (512 palavras reservadas ao SO / 16).
  - [ ] Macros/funções inline `extrairPagina(e)` e `extrairOffset(e)`.
- [ ] Criar `src/mmu.hpp` com a interface definitiva:
  - [ ] `enum class PoliticaSubstituicao { FIFO, LRU }`.
  - [ ] `struct ResultadoAcesso { uint16_t valor; uint32_t ciclosPenalidade; bool falta; }`.
  - [ ] Assinaturas de `ler()`, `escrever()`, `carregarProcesso()` e `liberarProcesso()`.
- [ ] Implementar uma **MMU Provisória (Stub)**:
  - [ ] Tradução direta sem faltas (`fisico = 512 + pid * 256 + logico`) para destravar o desenvolvimento das Pessoas 1 e 3 já na Fase 2.

### Fase 2: Implementação da MMU Real, FIFO e LRU (até 22/11)
- [ ] Implementar a tabela de molduras de página (*Core Map*) em `src/mmu.cpp`:
  - [ ] Estrutura para cada moldura: `livre`, `pid`, `pagina`, `carregadaEm` (FIFO), `ultimoAcesso` (LRU via contador monotônico global de acessos).
  - [ ] Suporte a `NUM_MOLDURAS_USUARIO` configurável (escassez forçada: 4 a 32 molduras).
- [ ] Implementar as estruturas do Disco Virtual para paginação sob demanda:
  - [ ] Alocar imagem de memória por processo no disco simulado.
  - [ ] Inicializar páginas lógicas dos processos como inválidas (`valida = false`).
- [ ] Implementar algoritmo de substituição global **FIFO**:
  - [ ] Fila circular de molduras alocadas; seleção da moldura carregada há mais tempo.
- [ ] Implementar algoritmo de substituição global **LRU**:
  - [ ] Atualização de contador monotônico a cada acesso; seleção da moldura com menor carimbo de acesso.
- [ ] Implementar o tratamento de Falta de Página (*Page Fault*):
  - [ ] Detecção de página inválida.
  - [ ] *Write-back* no Disco Virtual se a página vítima estiver suja (`suja == true`).
  - [ ] Cópia da página do Disco Virtual para a moldura física na RAM e atualização da tabela de páginas do PCB.
  - [ ] Cálculo da penalidade em ciclos (`PENALIDADE_FALTA = 50` + 50 adicionais se houver *write-back*).
- [ ] Gerar arquivo de rastro de referências de memória `referencias_memoria.csv` com `(tick, core, pid, pagina, tipo_acesso)`.
- [ ] Atualizar `loader/ram_loader.cpp` para carregar dados/instruções para a imagem em Disco Virtual em vez de escrever direto na RAM física.

### Fase 3: Integração, Validação e OPT de Belady (até 29/11)
- [ ] Integrar a MMU real no Kernel (substituindo a MMU provisória).
- [ ] Criar testes unitários em `tests/test_mmu.cpp`:
  - [ ] Teste de isolamento de memória entre processos com mesmo endereço virtual.
  - [ ] Teste determinístico de faltas de página e substituição com FIFO.
  - [ ] Teste determinístico de substituição com LRU.
  - [ ] Teste de gravação de *dirty bit* e *write-back* em instruções `STORE`.
- [ ] Criar script `scripts/opt_belady.py` para calcular o algoritmo ótimo offline (OPT de Belady) a partir do CSV de referências e detectar a Anomalia de Belady no FIFO.

### Fase 4: Artigo IEEE & Análise de Resultados (até 05/12)
- [ ] Redigir a seção de **Gerenciamento de Memória (Modelo Tanenbaum)**:
  - [ ] Justificar o modelo de paginação (bloco fixo + deslocamento), fatiamento 8+4 lógico e 12+4 físico.
  - [ ] Detalhar tabela de páginas por processo, tabela de molduras (*core map*) e paginação sob demanda.
- [ ] Desenhar o **Fluxograma da MMU** (Tradução de Endereços, Hit, Falta de Página e Substituição).
- [ ] Analisar e discutir os dados do **Experimento E4** (Taxa de faltas × número de molduras, comparação FIFO vs LRU vs OPT, discussão sobre Anomalia de Belady).

---

## 👤 Igor Peixoto (Políticas de Escalonamento e Preempção)

### Fase 1: Interfaces do Escalonador & Parser de Diretivas (até 08/11)
- [ ] Criar `src/scheduler.hpp` com design desacoplado (política pura, sem laço próprio):
  - [ ] Enums: `enum class Politica { FCFS, SJN, RR, PRIORIDADE }` e `enum class Modo { NAO_PREEMPTIVO, PREEMPTIVO }`.
  - [ ] Assinaturas de métodos: `admitir(PCB*, tick)`, `proximo(tick)`, `devolver(PCB*, tick)`, `devePreemptar(PCB&)`, `envelhecer(tick)`, `vazio()`.
- [ ] Atualizar `loader/parser.cpp` e `loader/clock_types.hpp` (`JobData`):
  - [ ] Adicionar suporte às diretivas opcionais `.priority N` e `.estimate N` sem corromper a decodificação hexadecimal de instruções.
- [ ] Implementar a ferramenta de perfilamento (`--perfil`):
  - [ ] Executa cada job isoladamente (1 core, memória infinita) e relata a contagem real de instruções executadas para usar como `.estimate`.

### Fase 2: Implementação dos 4 Algoritmos e Semântica de Modos (até 22/11)
- [ ] Implementar `src/scheduler.cpp` segundo a matriz semântica oficial:
  - [ ] **FCFS:** Fila por ordem de chegada/ID do processo. No modo preemptivo com quantum, volta ao fim da fila (equivale ao RR).
  - [ ] **SJN / SRTF:** Menor tempo estimado de execução. No modo preemptivo, opera como SRTF (reavaliação do tempo restante a cada fim de quantum).
  - [ ] **Round Robin (RR):** Fila circular com quantum $Q$ de ciclos úteis de execução. No modo não-preemptivo ($Q=\infty$), equivale ao FCFS.
  - [ ] **Escalonamento por Prioridade + Aging:** Fila ordenada por prioridade decrescente (maior valor = maior prioridade). Implementar técnica de envelhecimento (*aging*): a cada intervalo `--aging` ciclos esperando na fila, incrementa a prioridade efetiva em +1 para prevenir inanição (*starvation*). Desempate por menor PID.
- [ ] Regra estrita de admissão:
  - [ ] Admissão em lote fechado: todos os jobs do lote são admitidos em $t=0$; proibida inserção posterior durante a simulação.
- [ ] Implementar verificação de preempção:
  - [ ] Disparada na fronteira entre instruções quando `quantumUsado >= quantum` (ou quando a prioridade efetiva de um processo na fila superar a do processo em execução no modo com preempção).

### Fase 3: Integração e Validação (até 29/11)
- [ ] Integrar o despachante com o `Kernel` da **Pessoa 1**:
  - [ ] Núcleos livres solicitam o próximo processo via `scheduler.proximo()`.
  - [ ] Preempções devolvem o processo à fila de prontos via `scheduler.devolver()`.
- [ ] Criar suíte de testes em `tests/test_scheduler.cpp`:
  - [ ] Ordem de conclusão em FCFS vs SJN.
  - [ ] Validação do Oráculo SPT (SJN minimizando o Tempo Médio de Retorno no lote estático).
  - [ ] Preempção periódica do RR e alternância justa de processos.
  - [ ] Eficácia do mecanismo de *aging* na prevenção de *starvation*.
  - [ ] Teste formal das equivalências teóricas: FCFS-P $\equiv$ RR-P e RR-NP $\equiv$ FCFS-NP.

### Fase 4: Artigo IEEE & Análise de Resultados (até 05/12)
- [ ] Redigir a seção de **Metodologia de Escalonamento de Tarefas**.
- [ ] Documentar a tabela de **Semântica Política × Modo** (justificando o comportamento em lote estático e o *aging*).
- [ ] Escrever os **Pseudocódigos Formais dos 4 Escalonadores** (requisito obrigatório da especificação).
- [ ] Analisar e discutir os dados dos experimentos:
  - [ ] **Experimento E2:** Comparativo das 4 políticas nos modos Não-Preemptivo e Preemptivo (TME e TMR).
  - [ ] **Experimento E3:** Impacto do tamanho do quantum $Q \in \{2, 5, 10, 20, 50, 100\}$ e a sobrecarga gerada pelas trocas de contexto.

---

## 👤 Otávio Hiratsuka (Métricas, Bateria de Testes, Gráficos & Integração do Artigo)

### Fase 1: Padronização das Métricas e Estrutura de Exportação (até 08/11)
- [x] Atualizar `loader/metrics.hpp` e `loader/metrics.cpp` com as fórmulas formais:
  - [x] Métricas por processo: Tempo de Espera ($TE_i$), Tempo de Retorno/Turnaround ($TR_i = \text{cicloFim}_i$), trocas de contexto e faltas de página.
  - [x] Métricas do sistema: Tempo Médio de Espera (TME), Tempo Médio de Retorno (TMR), Makespan global.
  - [x] Métricas de CPU: Utilização média global e Utilização útil (descontando *stall* e troca de contexto).
  - [x] Métricas por núcleo: Eficiência de cada núcleo $k$ ($\text{ocupado}_k / \text{makespan}$).
  - [x] Throughput global: $N / \text{makespan}$ (processos por 1.000 ciclos).
  - [x] Speedup $S(n) = \text{makespan}(1) / \text{makespan}(n)$ e Eficiência Paralela $E(n) = S(n) / n$.
- [x] Atualizar `loader/reporter.cpp`:
  - [ ] Exportação determinística de arquivos em diretório `--saida`: `processos.csv`, `nucleos.csv`, `resumo.csv` e log detalhado da execução (sem truncamento ou anexos acidentais).

### Fase 2: Construção dos Lotes Experimentais Realistas (até 22/11)
- [ ] Criar a pasta `tests/lotes/` com programas em Assembly RISC 16-bits respeitando as restrições da ISA (endereçamento imediato de 8 bits e trampolins para saltos com `BEQ`):
  - [ ] **Lote CPU-Bound:** Jobs com laços longos e cálculo intensivo (centenas/milhares de instruções).
  - [ ] **Lote Memory-Bound:** Jobs com variáveis e acessos espalhados por diferentes páginas (`0x00`, `0x40`, `0x80`, `0xC0`) para forçar concorrência na MMU.
  - [ ] **Lote Misto:** Conjunto diversificado de jobs variando duração, acessos à memória e diretivas `.priority` e `.estimate`.

### Fase 3: Automação de Gráficos e Execução dos Benchmarks (até 29/11)
- [ ] Implementar o script em Python `scripts/gerar_graficos.py` (usando `matplotlib` e `seaborn`):
  - [ ] **Gráfico E1:** Curvas de Speedup $S(n)$ e Throughput vs número de núcleos ($n \in \{1, 2, 4, 8\}$) comparados contra a linha ideal linear e contra o baseline `v1`.
  - [ ] **Gráfico E2:** Gráficos de barras de TME e TMR agrupados por política e modo de execução.
  - [ ] **Gráfico E3:** Curva de Turnaround (TMR) e percentual de sobrecarga de troca de contexto em função do Quantum ($Q$).
  - [ ] **Gráfico E4:** Curva de Page Faults em função do número de molduras (FIFO vs LRU vs OPT).
  - [ ] **Gráfico E5:** Distribuição de carga e eficiência por núcleo (diagrama de barras e linha do tempo de ocupação).
- [ ] Configurar no `Makefile` o alvo `make benchmark` conectando o `scripts/benchmark.sh` ao gerador de gráficos.

### Fase 4: Consolidação do Artigo IEEE e Revisão Final (até 05/12)
- [ ] Configurar o projeto no Overleaf com o **template oficial da IEEE Conference** (`IEEEtran`).
- [ ] Redigir a **Introdução**:
  - [ ] Contextualização e objetivos da transição single-core $\rightarrow$ multicore.
  - [ ] Link do repositório público no GitHub e **instruções exatas de compilação e execução** (`make`, `make test`, `make benchmark`).
- [ ] Redigir o **Resumo / Abstract** e a **Conclusão**.
- [ ] Montar e formatar a seção de **Resultados e Discussão**:
  - [ ] Diagramar e inserir todas as figuras e gráficos em alta resolução gerados pelos scripts.
  - [ ] Coordenar e revisar os textos analíticos de cada integrante (P1 no E1, P2 no E4, P3 no E2/E3, P4 no E5 e síntese geral).
- [ ] Realizar a revisão cruzada do texto, padronização de referências bibliográficas (`.bib`) e conformidade com o limite de páginas e regras da IEEE.

---

## 📅 Marcos de Entrega e Sincronização

```text
[M1: 08/11] ─── Compilação única de cabeçalhos e defines (make passa; MMU stub pronta)
      │
[M2: 15/11] ─── Esqueleto Multicore funcional (Kernel + CPU com contexto + MMU provisória)
      │
[M3: 22/11] ─── Módulos finalizados (make test_mmu test_scheduler test_multicore)
      │
[M4: 29/11] ─── Simulações completas rodadas e gráficos gerados (make benchmark)
      │
[M5: 05/12] ─── Artigo IEEE finalizado e revisado; repositório público pronto
      │
[06/12/2026] ── Submissão final no AVA (Folga de segurança)
```
