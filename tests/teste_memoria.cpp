#include "../src/memory.hpp"
#include <cassert>
#include <iostream>

using namespace std;

void testeLeituraEscrita() {
    EscreverMemoria(600, 42);
    assert(LerMemoria(600) == 42);
    cout << "OK: leitura/escrita basica\n";
}

void testeIncrementoDoClock() {
    clockGlobal = 0;
    LerMemoria(600);
    EscreverMemoria(601, 1);
    assert(clockGlobal == 2); // 1 ciclo por acesso
    cout << "OK: clock incrementa corretamente\n";
}

void testeProtecaoLeitura() {
    bool lancou = false;
    try {
        LerMemoria(0);
    } catch (exception&) {
        lancou = true;
    }
    assert(lancou);
    cout << "OK: protecao de leitura na area do SO\n";
}

void testeProtecaoEscrita() {
    bool lancou = false;
    try {
        EscreverMemoria(511, 99);
    } catch (exception&) {
        lancou = true;
    }
    assert(lancou);
    cout << "OK: protecao de escrita na area do SO\n";
}

void testeEnderecoMaximo() {
    EscreverMemoria(65535, 123);
    assert(LerMemoria(65535) == 123);
    cout << "OK: endereco maximo (65535) funciona\n";
}

void testeLerDisco() {
    vector<uint16_t> disco = {10, 20, 30, 999};
    clockGlobal = 0;
    int idx = LerDisco(30, disco);
    assert(idx == 2);
    assert(clockGlobal == 150); // 50 ciclos x 3 posicoes percorridas (indices 0,1,2)
    cout << "OK: LerDisco encontra o dado e cobra o clock certo\n";
}

void testeLerDiscoNaoEncontrado() {
    vector<uint16_t> disco = {10, 20, 30};
    int idx = LerDisco(999, disco);
    assert(idx == -1);
    cout << "OK: LerDisco retorna -1 quando nao encontra\n";
}

int main() {
    testeLeituraEscrita();
    testeIncrementoDoClock();
    testeProtecaoLeitura();
    testeProtecaoEscrita();
    testeEnderecoMaximo();
    testeLerDisco();
    testeLerDiscoNaoEncontrado();
    cout << "Todos os testes de memoria passaram!\n";
    return 0;
}