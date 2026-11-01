# Relatório dos testes de memória:

Existem dois arquivos, cada um com um objetivo diferente:
 
- `teste_memoria.cpp` → testa o módulo de **Memória isoladamente**
- `teste_cpu_memoria.cpp` → testa a **integração entre CPU e Memória**

## 1. `test_memoria.cpp` — testa a Memória isoladamente
 
Inclui só `memory.hpp`/`memory.cpp`. Ele não usa a `CPU` nem o `SO`, provando que as funções de interface de memória
(`LerMemoria`, `EscreverMemoria`, `LerDisco`) funcionam corretamente por si só.

| Teste | O que verifica |
|---|---|
| `testeLeituraEscrita` | Escreve um valor num endereço e lê de volta. Confirma o caminho mais básico: `EscreverMemoria` → `LerMemoria`. |
| `testeIncrementoDoClock` | Confirma que cada acesso à RAM (leitura ou escrita) soma exatamente 1 ciclo ao `clockGlobal`, conforme a especificação. |
| `testeProtecaoLeitura` | Tenta **ler** o endereço `0` (dentro da área do SO) e confirma que uma exceção é lançada. |
| `testeProtecaoEscrita` | Tenta **escrever** no endereço `511` (última posição da área do SO) e confirma que uma exceção é lançada. |
| `testeEnderecoMaximo` | Escreve e lê no endereço `65535` (o maior endereço possível numa RAM de 16 bits). Este teste foi o que revelou o bug do `TAM_RAM`. |
| `testeLerDisco` | Simula uma busca no disco virtual e confirma o índice retornado e a penalidade de clock (50 ciclos por posição percorrida). |
| `testeLerDiscoNaoEncontrado` | Confirma que `LerDisco` retorna `-1` quando o dado buscado não existe no disco. |

Os testes `testProtecaoLeitura` e `testProtecaoEscrita` atacam a regra
central da segurança do simulador: **nenhum acesso a endereços abaixo de
`OS_RESERVED_MEM` (512) pode ser bem-sucedido**, nem para leitura nem para
escrita. Se algum dia essa checagem for removida ou quebrada por engano em
`memory.cpp`, esses dois testes falham imediatamente.

## 2. `test_cpu_memoria.cpp` — testa CPU e Memória juntas
 
Inclui `cpu.hpp` **e** `memory.hpp`. Um objeto `CPU` executa instruções
sozinha, chamando `cpu.step()` em loop — testando o ciclo real de **busca →
decodificação → execução**, não as funções de memória isoladas.
 
Cada teste monta um job diretamente na RAM (a partir do endereço
`OS_RESERVED_MEM`, igual o `SO` faria) e deixa a CPU rodar até encontrar
`HALT`.
 
| Teste | O que verifica |
|---|---|
| `testeCpuLeEEscreveNaRamCorretamente` | Monta o programa `C = A + B` (LOAD, LOAD, ADD, STORE, HALT) e confirma que, depois da CPU rodar sozinha, o resultado (`12`) está gravado no endereço correto da RAM. Prova que **LOAD lê certo e STORE grava certo através da CPU**. |
| `testeCpuRespeitaEnderecoRelativoAoJob` | Repete o mesmo teste com outros valores (`100 + 23 = 123`) para garantir que o resultado anterior não foi coincidência. |
| `testeCpuNaoEscreveForaDoEsperado` | Planta um valor "sentinela" (`0xDEAD`) num endereço vizinho ao destino do `STORE` e confirma que ele continua intacto depois da execução — ou seja, o `STORE` não escreveu em lugar nenhum além do endereço de destino esperado. |
| `testeOpcodeInvalidoLancaExcecao` | Coloca um opcode inválido (`0x9`) na RAM e confirma que `cpu.step()` lança uma exceção. Isso permite ao `so.cpp` capturar o erro e abortar o job com segurança, em vez de travar o simulador inteiro. |

Isso prova que a ligação real (CPU chamando a interface de memória por dentro,
durante a execução de uma instrução) está funcionando, e não apenas que as
funções de memória funcionam quando chamadas isoladamente.

## Bug encontrado e corrigido por esses testes
 
Ao escrever `testEnderecoMaximo`, foi observado que `defines.hpp` declarava:
 
```cpp
const uint16_t TAM_RAM = 65536;
```
 
Como `uint16_t` vai só até `65535`, esse valor estourava silenciosamente
para `0` (o compilador até avisa com `-Wall`: *"changes value from '65536'
to '0'"*). Isso fazia o array `RAM[TAM_RAM]` em `memory.cpp` ser, na
prática, um array de tamanho zero e todo acesso à RAM era, tecnicamente,
acesso fora dos limites
 
**Correção aplicada**: o tipo da constante foi trocado para `uint32_t`,
resolvendo o overflow sem alterar nenhum outro comportamento do sistema.

## Como compilar e rodar
 
```bash
# Teste de memória isolada
g++ -std=c++11 -Wall -Wextra tests/test_memoria.cpp src/memory.cpp -o tests/test_memoria
./tests/test_memoria
 
# Teste de integração CPU + memória
g++ -std=c++11 -Wall -Wextra tests/test_cpu_memoria.cpp src/cpu.cpp src/memory.cpp -o tests/test_cpu_memoria
./tests/test_cpu_memoria
```
 
Se tudo estiver certo, a saída de cada um termina com:
 
```
Todos os testes de memoria passaram!
```
ou
```
Todos os testes de integracao CPU-memoria passaram!
```
 
Se algum `assert` falhar, o programa encerra imediatamente na linha do
teste que quebrou (isso facilita achar o problema: basta olhar qual foi o
último "OK: ..." impresso antes do encerramento).