#set page(
  paper: "us-letter",
  margin: (x: 1.8cm, top: 2.2cm, bottom: 2.5cm),
)

#set text(
  size: 10pt,
  lang: "pt",
)

#set par(justify: true, leading: 0.55em)

#align(center)[
  #text(size: 18pt, weight: "bold")[Simulador de Arquitetura Multicore com Gerenciamento de Memória Paginada e Escalonamento Dinâmico de Tarefas]
  
  #v(1em)
  
  #grid(
    columns: (1fr, 1fr),
    gutter: 2em,
    [
      #text(weight: "bold")[Pedro Henrique Galvão, João Paulo]\
      #text(style: "italic")[Departamento de Computação (DECOM-DV)]\
      #text(style: "italic")[Centro Federal de Educação Tecnológica de Minas Gerais (CEFET-MG)]\
      Divinópolis, Brasil\
      `devpedrohg@gmail.com`
    ],
    [
      #text(weight: "bold")[Igor Peixoto, Otávio Hiratsuka]\
      #text(style: "italic")[Departamento de Computação (DECOM-DV)]\
      #text(style: "italic")[Centro Federal de Educação Tecnológica de Minas Gerais (CEFET-MG)]\
      Divinópolis, Brasil
    ]
  )
  #v(1.5em)
]

#columns(2)[
#strong[Resumo] --- #emph[[A SER ESCRITO POR OTÁVIO HIRATSUKA] Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum. Repositório: https://github.com/PeedroHG/multicore-os-simulator.]

#v(0.5em)
#strong[Palavras-chave] --- #emph[Sistemas Operacionais, Multicore, Troca de Contexto, PCB, Lockstep, Amdahl.]

= I. Introdução
_[Seção sob responsabilidade de Otávio Hiratsuka]_

Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur.

Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum. Sed ut perspiciatis unde omnis iste natus error sit voluptatem accusantium doloremque laudantium, totam rem aperiam.

== A. Repositório e Instruções de Compilação
O projeto encontra-se versionado e documentado publicamente no GitHub:
#align(center)[`https://github.com/PeedroHG/multicore-os-simulator`]

O fluxo de automação de compilação baseia-se na ferramenta GNU Make e no compilador GCC com suporte ao padrão C++17. A reprodução experimental dos resultados pode ser realizada através dos comandos:
```bash
# 1. Compilação do executável principal
make

# 2. Execução parametrizada do simulador
./simulador --cores 4 --politica rr --modo p

# 3. Execução da suíte completa de testes
make test

# 4. Execução da bateria de benchmarks
make benchmark
```

= II. Arquitetura Multicore e Modelo de Execução
O sistema foi estruturado com um Kernel centralizado, responsável pelo despacho e controle de tempo de $n$ núcleos de processamento idênticos ($"CPU"_0, "CPU"_1, dots, "CPU"_(n-1)$). Todos os núcleos compartilham a mesma memória principal, cujo espaço físico é dividido entre a área reservada do sistema operacional (endereços `0x0000` a `0x01FF`) e a área dos programas de usuário.

== A. Modelo de Execução Temporal em Lockstep
Para afastar condições de corrida não-determinísticas inerentes à manipulação de threads assíncronas do sistema hospedeiro e assegurar reprodutibilidade experimental exata, a simulação adota o modelo de _passo sincronizado_ (_lockstep_).

O Kernel gerencia o relógio central do sistema através do contador `tickGlobal`. Em cada pulso de clock ($t arrow.r t + 1$), o Kernel percorre os núcleos de $0$ a $n-1$:
+ Incrementa o tempo de espera ($"ciclosEspera"$) de todos os processos que permanecem na fila de aptos (`PRONTO`);
+ Para núcleos em estolagem de memória, consome 1 ciclo de latência de E/S sem despachar instrução;
+ Para núcleos desocupados, aciona o despachante para alocar o próximo processo da fila;
+ Para núcleos com processo ativo, dispara o ciclo elementar de instrução (`CPU::step()`), atualizando registradores e o contador de quantum.

== B. Bloco de Controle de Processo (PCB)
A abstração de processo é consolidada na estrutura `PCB` (_Process Control Block_), que atua como o repositório de persistência do estado lógico e arquitetural de cada tarefa admitida no lote inicial (em $t = 0$). O PCB encapsula:
- *Identificação e Ciclo de Vida:* Identificador numérico exclusivo (`PID`), nome do arquivo fonte e estado formal (`NOVO`, `PRONTO`, `EXECUTANDO`, `FINALIZADO` ou `ABORTADO`);
- *Contexto de Hardware:* Snapshot estruturado (`CPUContext`) composto pelos registradores de uso geral $R_0$ a $R_7$, Program Counter ($P C$) lógico, Instruction Register ($I R$) e sinalizadores de término da Unidade de Controle (`halted`);
- *Descritores de Escalonamento:* Prioridade estática base, prioridade dinâmica efetiva (para mecanismos de _aging_), estimativa estática de rajada e quantum consumido;
- *Telemetria e Métricas:* _Timestamps_ dos ciclos de admissão, início e encerramento, além de contadores para tempo de espera na fila, tempo de execução na CPU, tempo parado em stall e quantidade de trocas de contexto.

== C. Mecanismo e Penalidade de Troca de Contexto
A troca de contexto é implementada desacopladamente na classe `CPU` através dos métodos primitivos `salvarContexto(PCB&)` e `restaurarContexto(PCB&)`.

Quando um processo esgota sua fatia temporal (quantum) ou sofre preempção por prioridade, o núcleo extrai os valores dos registradores físicos e os salva no PCB. Em seguida, a CPU carrega os registradores do novo processo que vai assumir o núcleo.

Para refletir o custo operacional real de salvamento de registradores e sobrecarga do despachante, o simulador aplica uma penalidade de tempo configurável:
$ C_("troca") = 5 " ciclos de clock" $
Essa penalidade é estritamente contabilizada apenas quando há alternância efetiva de processos ($"PID"_("anterior") eq.not "PID"_("novo")$) no mesmo núcleo.

== D. Tratamento de Faltas de Página via Estolagem (Stall)
Em conformidade com a especificação formal do trabalho que proíbe o chaveamento de tarefas no cenário não-preemptivo, as faltas de bloco/página (_page faults_) geradas pelo subsistema de memória virtual não transitam o processo para um estado tradicional `BLOQUEADO`. Em substituição, a CPU ativa um mecanismo de estolagem do núcleo (_pipeline stall_) por:
$ C_("falta") = 50 " ciclos de clock" $
somados a 50 ciclos adicionais caso a moldura vítima substituída exija escrita retroativa (_write-back_) de dados alterados para o disco virtual. Durante os ciclos de estolagem, o núcleo permanece paralisado simulando o barramento de transferência de memória secundária, garantindo fidelidade temporal ao cenário não-preemptivo.

== E. Fluxograma Geral da Arquitetura
O fluxo operacional do Kernel e a interação síncrona entre núcleos, despachante e memória estão esquematizados na Figura 1.

#figure(
  image("fluxograma.svg", width: 95%),
  caption: [Fluxograma do ciclo de execução multicore em passo sincronizado (Lockstep) gerenciado pelo Kernel.],
) <fig-fluxograma>

= III. Gerenciamento de Memória Virtual
Embora o enunciado empregue a expressão "memória segmentada", os mecanismos prescritos --- divisão do espaço em blocos de tamanho fixo combinados a deslocamento (_offset_) e substituição por FIFO ou LRU --- caracterizam formalmente a *paginação sob demanda* no modelo clássico de Tanenbaum [3]. Para viabilizar a multiprogramação sem colisão de endereços entre núcleos, a Unidade de Controle (UC) foi desacoplada da memória física: o Contador de Programa ($P C$) de todo processo inicia em `0x000`, a soma estática de `OS_RESERVED_MEM` ($512$) foi eliminada das instruções `LOAD`, `STORE`, `BEQ` e `JUMP`, e todos os acessos --- inclusive a busca de instruções (_fetch_) --- são interceptados pela Unidade de Gerenciamento de Memória (MMU).

== A. Fatiamento de Bits e Espaço de Endereçamento
Na arquitetura RISC de 16 bits adotada, as instruções do tipo I (`LOAD`, `STORE` e `BEQ`) dispõem de 8 bits de endereço imediato (`0x00`--`0xFF`), ao passo que desvios incondicionais (`JUMP`) alcançam 12 bits (`0x000`--`0xFFF`). Caso fossem empregadas páginas grandes (e.g., 64 ou 256 palavras), programas inteiros caberiam em uma única página, tornando indistinguíveis as políticas de substituição. Adotou-se, portanto, o tamanho de página $P = 2^4 = 16$ palavras, estabelecendo o fatiamento da Tabela I:

#figure(
  table(
    columns: (auto, auto, auto, auto),
    align: (left, left, left, left),
    stroke: 0.5pt,
    [*Endereço*], [*Largura*], [*Fatiamento de Bits*], [*Capacidade*],
    [Lógico], [12 bits], [`[11:4]` pág. (8b) | `[3:0]` offset (4b)], [256 págs. $times$ 16 pal.],
    [Físico], [16 bits], [`[15:4]` mold. (12b) | `[3:0]` offset (4b)], [4096 molduras (64K)],
  ),
  caption: [Fatiamento de Endereços Lógicos e Físicos na MMU.],
)

Endereços lógicos com bits `[15:12]` $eq.not 0$ ultrapassam o limite de 4096 palavras (`0x0FFF`) e disparam falha de segmentação (`SEGFAULT`), abortando o processo infrator sem interromper o lote. Dado um endereço lógico $e$, a página $p$, o deslocamento $d$ e o endereço físico $e_f$ associado à moldura física $m$ são obtidos em tempo constante por operações bit a bit:
$ p = e >> 4, quad d = e and "0x000F", quad e_f = (m << 4) or d $

As primeiras $"MOLDURAS_SO" = 512 / 16 = 32$ molduras físicas (endereços `0x0000` a `0x01FF`) permanecem estritamente reservadas ao Sistema Operacional. Os processos de usuário compartilham um conjunto configurável de $M in [4, 32]$ molduras físicas ($m = 32 + i$, com $0 <= i < M$), impondo escassez controlada de RAM para exercitar a paginação.

== B. Tabela de Páginas, Core Map e Paginação sob Demanda
O isolamento entre espaços virtuais é assegurado por três estruturas complementares:
+ *Tabela de Páginas por Processo (`PCB::tabelaPaginas`):* Vetor de 256 descritores `EntradaPagina` embutido no próprio PCB (atuando de forma análoga ao registrador base _PTBR_). Cada entrada mantém os bits de presença (`valida`), modificação (`suja` / _dirty bit_), referência (`referenciada`) e o índice da `moldura` física correspondente;
+ *Tabela de Molduras Global (_Core Map_):* Vetor de $M$ descritores que rastreia o estado de ocupação de cada quadro de usuário (`livre`, `pid` proprietário, número da `pagina`, ordem de carga `carregadaEm` e carimbo temporal `ultimoAcesso`);
+ *Disco Virtual por Processo:* Na admissão do lote ($t = 0$), o `RAMLoader` constrói a imagem lógica de 4096 palavras de cada tarefa diretamente no disco virtual simulado, sem tocar na RAM física, e inicializa todas as 256 páginas do PCB com `valida = false`.

Quando um núcleo requisita uma palavra cuja página encontra-se inválida, ocorre uma *falta de página* (_page fault_). A MMU aloca a moldura livre de menor índice ou, na ausência de quadros vagos, aciona o algoritmo de substituição global [3], podendo selecionar vítimas pertencentes a qualquer processo ativo. Se a página vítima possuir `suja = true` (modificada por instrução `STORE`), seus 16 endereços são copiados de volta para a imagem no Disco Virtual (_write-back_) antes da carga da nova página, adicionando $+50$ ciclos à penalidade base de $50$ ciclos da falta. Ao término ou aborto de um processo, todas as suas molduras são liberadas no _Core Map_.

== C. Políticas de Substituição Global: FIFO, LRU e OPT
Foram implementadas duas políticas online na MMU e um oráculo offline de avaliação:
- *FIFO (_First-In, First-Out_):* Mantém uma fila circular de índices de molduras na ordem estrita de carregamento (`contadorCargas`). Substitui a página residente há mais tempo, independentemente de sua frequência de uso, estando sujeito à Anomalia de Belady [6];
- *LRU Exato (_Least Recently Used_):* A cada acesso (busca, leitura ou escrita), a MMU incrementa um *contador monotônico global de acessos* (`contadorAcessos`) e registra-o em `ultimoAcesso` da moldura. O uso de um contador de eventos em vez do `tickGlobal` elimina empates quando múltiplos núcleos acessam a memória no mesmo pulso de clock. Como algoritmo de pilha, o LRU satisfaz a propriedade de inclusão de Mattson et al. [7] e é imune à Anomalia de Belady;
- *OPT de Belady (Offline):* A MMU exporta o rastro completo de referências $(t, "core", "PID", p, "tipo")$ em CSV, processado pelo script `scripts/opt_belady.py` para substituir a página cujo próximo uso está mais distante no futuro [5], estabelecendo o limite inferior teórico de faltas.

O fluxo completo de tradução de endereços, detecção de faltas e substituição com escrita retroativa está ilustrado na Figura 2.

#figure(
  image("fluxograma_mmu.svg", width: 95%),
  caption: [Fluxograma de tradução de endereços, tratamento de falta de página e substituição na MMU.],
) <fig-fluxograma-mmu>

= IV. Políticas de Escalonamento de Tarefas
_[Seção sob responsabilidade de Igor Peixoto: FCFS, SJN, RR, Prioridade + Aging e Pseudocódigos Formais]_

= V. Metodologia Experimental e Cargas de Teste
_[Seção sob responsabilidade de Otávio Hiratsuka: Lotes CPU-bound, Memory-bound e Misto]_

= VI. Resultados e Discussão

== A. Análise de Speedup e Escalabilidade Multicore (Experimento E1)
A quantificação do ganho advindo da paralelização física do hardware toma como referência a formulação clássica da Lei de Amdahl. O Speedup $S(n)$ sobre $n$ núcleos de processamento é mensurado comparando o makespan (tempo total de conclusão do lote) obtido na configuração uniprocessada $T(1)$ contra a execução paralela em $n$ núcleos $T(n)$:
$ S(n) = (T(1)) / (T(n)) $

A eficiência paralela normalizada $E(n)$, que traduz a fração de hardware efetivamente convertida em aceleração líquida, é definida por:
$ E(n) = (S(n)) / n times 100% $

_[Os gráficos comparativos e a discussão aprofundada dos resultados serão inseridos após a execução completa dos benchmarks com os lotes finais de teste.]_

== B. Desempenho da Paginação: FIFO vs. LRU vs. OPT e Anomalia de Belady (Experimento E4)
Para avaliar o subsistema de memória virtual sob diferentes graus de pressão de RAM, o Experimento E4 submeteu um lote de 6 processos (totalizando $1.150$ referências à memória e conjunto de trabalho agregado de 25 páginas lógicas distintas, combinando varreduras cíclicas e laços com localidade temporal [8]) à variação da quantidade de molduras de usuário $M in {4, 6, 8, 12, 16, 24, 32}$, contrastando as políticas FIFO, LRU e o limite ótimo offline (OPT de Belady). A Tabela II sintetiza as faltas de página obtidas nos cenários uniprocessado ($n = 1$, sem entrelaçamento entre processos) e multiprocessado ($n = 4$, com competição simultânea pelas molduras globais).

#figure(
  table(
    columns: (auto, auto, auto, auto, auto, auto, auto),
    align: (right, right, right, right, right, right, right),
    stroke: 0.5pt,
    [*M*], [*FIFO (1c)*], [*LRU (1c)*], [*OPT (1c)*], [*FIFO (4c)*], [*LRU (4c)*], [*OPT (4c)*],
    [4],  [308 (26,8%)], [303 (26,3%)], [185 (16,1%)], [553 (48,1%)], [507 (44,1%)], [357 (31,0%)],
    [6],  [308 (26,8%)], [226 (19,7%)], [89 (7,7%)],   [455 (39,6%)], [361 (31,4%)], [258 (22,4%)],
    [8],  [25 (2,2%)],   [25 (2,2%)],   [25 (2,2%)],   [364 (31,7%)], [307 (26,7%)], [200 (17,4%)],
    [12], [25 (2,2%)],   [25 (2,2%)],   [25 (2,2%)],   [294 (25,6%)], [258 (22,4%)], [128 (11,1%)],
    [16], [25 (2,2%)],   [25 (2,2%)],   [25 (2,2%)],   [157 (13,7%)], [185 (16,1%)], [70 (6,1%)],
    [24], [25 (2,2%)],   [25 (2,2%)],   [25 (2,2%)],   [28 (2,4%)],   [26 (2,3%)],   [25 (2,2%)],
    [32], [25 (2,2%)],   [25 (2,2%)],   [25 (2,2%)],   [25 (2,2%)],   [25 (2,2%)],   [25 (2,2%)],
  ),
  caption: [Faltas de Página e Taxa de Faltas em Função do Número de Molduras ($1.150$ Referências).],
)

A análise dos dados experimentais e da curva apresentada na Figura 3 evidencia quatro fenômenos centrais da teoria de memória virtual:
+ *Superioridade do LRU sob Localidade Temporal:* Em $n = 1$ com $M = 6$ molduras, o FIFO permanece estagnado em $308$ faltas ($26,8%$), pois expulsa ciclicamente a página de código do laço interno quente pelo simples fato de ela ter sido carregada no início do processo. O LRU preserva as páginas recentemente referenciadas e reduz as faltas para $226$ ($-26,6%$). Em $n = 4$ com $M = 6$, o LRU evita $94$ faltas em relação ao FIFO ($361$ contra $455$, redução de $20,7%$);
+ *Pressão de Memória Multicore e Thrashing:* Enquanto no modo sequencial ($n = 1$) $M = 8$ molduras bastam para acomodar o _working set_ [8] individual de cada tarefa e atingir o piso de $25$ faltas compulsórias (_cold-start misses_, $2,17%$), na execução paralela em $n = 4$ núcleos quatro processos disputam simultaneamente o _Core Map_ global. Com $M = 4$ (apenas 1 moldura por núcleo ativo), a taxa de faltas no FIFO atinge $48,1%$ ($553$ faltas e $82$ _write-backs_), caracterizando _thrashing_; o piso de faltas compulsórias só é alcançado quando $M >= 24$ comporta a soma dos conjuntos de trabalho concorrentes;
+ *Padrões Cíclicos e Distância para o Ótimo (OPT):* Em $n = 4$ e $M = 16$, observa-se o caso clássico descrito por Silberschatz [2] em que uma varredura cíclica ligeiramente maior que o número de molduras disponíveis penaliza o LRU ($185$ faltas) frente ao FIFO ($157$ faltas), ao passo que o algoritmo clarividente OPT de Belady reduz as faltas para apenas $70$ ($6,1%$), comprovando seu papel como limite inferior inatingível por políticas puramente causais;
+ *Verificação da Anomalia de Belady:* Submetendo a cadeia canônica de Belady et al. [6] ($angle.l 1,2,3,4,1,2,5,1,2,3,4,5 angle.r$) à MMU implementada e ao verificador `opt_belady.py`, constatou-se que a ampliação de $M = 3$ para $M = 4$ molduras *eleva* as faltas do FIFO de $9$ para $10$, ao passo que no LRU as faltas caem monotonicamente de $10$ para $8$ (e no OPT de $7$ para $6$), confirmando empiricamente a propriedade de inclusão dos algoritmos de pilha [7].

#figure(
  image("fig_e4_faltas.png", width: 98%),
  caption: [Curva de faltas de página em função do número de molduras de usuário ($M$) para FIFO, LRU e OPT de Belady (Experimento E4).],
) <fig-e4-faltas>

= VII. Conclusão
_[Seção sob responsabilidade de Otávio Hiratsuka]_

Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur.

= Referências
+ J. L. Hennessy e D. A. Patterson, _Computer Architecture: A Quantitative Approach_, 6ª ed., Morgan Kaufmann, 2017.
+ A. Silberschatz, P. B. Galvin e G. Gagne, _Operating System Concepts_, 10ª ed., Wiley, 2018.
+ A. S. Tanenbaum e H. Bos, _Modern Operating Systems_, 4ª ed., Pearson, 2015.
+ G. M. Amdahl, "Validity of the single processor approach to achieving large scale computing capabilities", _AFIPS Conf. Proc._, 1967.
+ L. A. Belady, "A study of replacement algorithms for a virtual-storage computer", _IBM Systems Journal_, 1966.
+ L. A. Belady, R. A. Nelson e G. S. Shedler, "An anomaly in space-time characteristics of certain programs running in a paging machine", _Communications of the ACM_, 12(6), 1969.
+ R. L. Mattson, J. Gecsei, D. R. Slutz e I. L. Traiger, "Evaluation techniques for storage hierarchies", _IBM Systems Journal_, 9(2), 1970.
+ P. J. Denning, "The working set model for program behavior", _Communications of the ACM_, 11(5), 1968.
]
