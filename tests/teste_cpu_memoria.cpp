#include "../src/cpu/cpu.hpp"
#include "../src/memory.hpp"
#include <cassert>
#include <iostream>

using namespace std;

// Monta o exemplo da sessão 4 do PDF - CICLO DE EXECUÇÃO EM LOTE
// restringe o teste apenas a integração CPU <-> Memoria.
void carregarJobSoma(uint16_t base, uint16_t a, uint16_t b) {
    EscreverMemoria(base + 0, 0x100A); // LOAD  R0, [10]
    EscreverMemoria(base + 1, 0x110B); // LOAD  R1, [11]
    EscreverMemoria(base + 2, 0x2201); // ADD   R2, R0, R1
    EscreverMemoria(base + 3, 0x320C); // STORE R2, [12]
    EscreverMemoria(base + 4, 0xF000); // HALT

    EscreverMemoria(base + 10, a); // variavel A
    EscreverMemoria(base + 11, b); // variavel B
    EscreverMemoria(base + 12, 0); // variavel C (inicia em zero)
}

void testeCpuLeEEscreveNaRamCorretamente() {
    uint16_t base = OS_RESERVED_MEM;
    carregarJobSoma(base, 5, 7);

    CPU cpu;
    cpu.setPC(base);

    int passos = 0;
    while (!cpu.isHalted()) {
        cpu.step();
        passos++;
        assert(passos < 100 && "CPU nao parou - possivel loop infinito");
    }

    // CPU leu A e B, somou e escreveu C corretamente, e guardou no endereço correto.
    assert(LerMemoria(base + 12) == 12);
    cout << "OK: CPU calcula e grava o resultado corretamente na RAM (5+7=12)\n";
}

void testeCpuRespeitaEnderecoRelativoAoJob() {

    // Testa outra base pra ver se a CPU respeita o endereço relativo ao job
    // e nao vaza pra fora do endereço especificado.

    uint16_t base = OS_RESERVED_MEM;
    carregarJobSoma(base, 100, 23);

    CPU cpu;
    cpu.setPC(base);
    while (!cpu.isHalted()) cpu.step();

    assert(LerMemoria(base + 12) == 123);
    cout << "OK: enderecamento relativo ao job funciona com outros valores de A e B\n";
}

void testeCpuNaoEscreveForaDoEsperado() {
    // Garante que o STORE so mexeu no endereco de destino
    uint16_t base = OS_RESERVED_MEM;
    EscreverMemoria(base + 13, 0xDEAD); // sentinela antes de rodar o job
    carregarJobSoma(base, 1, 1);

    CPU cpu;
    cpu.setPC(base);
    while (!cpu.isHalted()) cpu.step();

    assert(LerMemoria(base + 12) == 2);
    assert(LerMemoria(base + 13) == 0xDEAD); // sentinela intacta
    cout << "OK: STORE nao escreve fora do endereco de destino\n";
}

void testeOpcodeInvalidoLancaExcecao() {
    uint16_t base = OS_RESERVED_MEM;
    EscreverMemoria(base + 0, 0x9000); // 0x9
    EscreverMemoria(base + 1, 0xF000); // HALT

    CPU cpu;
    cpu.setPC(base);

    bool lancou = false;
    try {
        cpu.step();
    } catch (exception&) {
        lancou = true;
    }
    assert(lancou && "Opcode invalido deveria lancar excecao (para o SO poder abortar o job)");
    cout << "OK: opcode invalido lanca excecao corretamente\n";
}

int main() {
    testeCpuLeEEscreveNaRamCorretamente();
    testeCpuRespeitaEnderecoRelativoAoJob();
    testeCpuNaoEscreveForaDoEsperado();
    testeOpcodeInvalidoLancaExcecao();
    cout << "Todos os testes de integracao CPU-memoria passaram!\n";
    return 0;
}