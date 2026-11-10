// Teste de metricas e exportacao (valores calculados a mao).
// Cenario: 2 nucleos, 3 processos, makespan 100, baseline n=1 com makespan 180.
#include "loader/reporter.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

static int falhas = 0;
#define CHECK(cond) do { if (!(cond)) { ++falhas; \
    std::cerr << "FALHA linha " << __LINE__ << ": " #cond "\n"; } } while (0)
static bool perto(double a, double b) { return std::fabs(a - b) < 1e-9; }

static std::string ler(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream o; o << f.rdbuf(); return o.str();
}

static ResultadoExecucao cenario() {
    ResultadoExecucao r;
    r.config.politica = "fcfs"; r.config.modo = "np"; r.config.nucleos = 2;
    r.config.molduras = 16; r.config.penalidadeFalta = 50;

    MetricasProcesso p3; p3.pid = 3; p3.nome = "job,3"; // virgula: testa escape CSV
    p3.cicloFim = 80; p3.tempoEspera = 50; p3.ciclosExecucao = 25; p3.ciclosStall = 5;
    p3.trocasContexto = 1; p3.faltasPagina = 1; p3.acessosMemoria = 10;
    MetricasProcesso p1; p1.pid = 1; p1.nome = "job1";
    p1.cicloFim = 100; p1.ciclosExecucao = 90; p1.ciclosStall = 10;
    p1.trocasContexto = 1; p1.faltasPagina = 2; p1.acessosMemoria = 40;
    MetricasProcesso p2; p2.pid = 2; p2.nome = "job2";
    p2.cicloFim = 50; p2.ciclosExecucao = 50; p2.trocasContexto = 1;
    p2.acessosMemoria = 20;
    r.processos = {p3, p1, p2}; // fora de ordem de proposito

    MetricasNucleo k1; k1.id = 1; k1.ciclosExecucao = 75; k1.ciclosStall = 5;
    k1.despachos = 2; k1.trocasContexto = 2;
    MetricasNucleo k0; k0.id = 0; k0.ciclosExecucao = 90; k0.ciclosStall = 10;
    k0.despachos = 1; k0.trocasContexto = 1;
    r.nucleos = {k1, k0};
    return r;
}

int main() {
    ResultadoExecucao r = cenario();
    Metricas::consolidar(r);
    Metricas::aplicarSpeedup(r, 180);
    const ResumoSistema& s = r.resumo;

    CHECK(r.processos[0].pid == 1 && r.nucleos[0].id == 0);   // ordenacao canonica
    CHECK(s.concluidos == 3 && s.abortados == 0);
    CHECK(s.makespan == 100);
    CHECK(perto(s.tme, 50.0 / 3.0));
    CHECK(perto(s.tmr, 230.0 / 3.0));
    CHECK(perto(r.nucleos[0].eficiencia, 1.0));
    CHECK(perto(r.nucleos[1].eficiencia, 0.8));
    CHECK(perto(r.nucleos[1].eficienciaUtil, 0.75));
    CHECK(perto(s.utilizacaoMedia, 0.9));
    CHECK(perto(s.utilizacaoUtil, 0.825));
    CHECK(perto(s.throughput, 30.0));
    CHECK(s.speedupValido && perto(s.speedup, 1.8) && perto(s.eficienciaParalela, 0.9));
    CHECK(s.faltasPagina == 3 && s.acessosMemoria == 70);
    CHECK(perto(s.taxaFaltas, 3.0 / 70.0));
    CHECK(Metricas::verificarConsistencia(r).empty());

    // Inconsistencia deve ser detectada.
    ResultadoExecucao ruim = r;
    ruim.nucleos[0].ciclosExecucao += 1;
    Metricas::consolidar(ruim);
    CHECK(!Metricas::verificarConsistencia(ruim).empty());

    // Execucao vazia nao pode dividir por zero.
    ResultadoExecucao vazio;
    Metricas::consolidar(vazio);
    CHECK(vazio.resumo.makespan == 0 && perto(vazio.resumo.throughput, 0.0));
    Metricas::aplicarSpeedup(vazio, 10);
    CHECK(!vazio.resumo.speedupValido);

    // Exportacao: deterministica e sem acrescimo.
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "teste_metricas_saida";
    std::filesystem::remove_all(dir);
    CHECK(Reporter::exportar(dir.string(), r));
    const std::string a1 = ler(dir / "resumo.csv"), a2 = ler(dir / "processos.csv"),
                      a3 = ler(dir / "nucleos.csv"), a4 = ler(dir / "execucao.log");
    CHECK(Reporter::exportar(dir.string(), r));   // 2a vez: nada acumula
    CHECK(a1 == ler(dir / "resumo.csv") && a2 == ler(dir / "processos.csv"));
    CHECK(a3 == ler(dir / "nucleos.csv") && a4 == ler(dir / "execucao.log"));
    CHECK(a2.find("\"job,3\"") != std::string::npos);
    CHECK(a1.find(",1.8000,0.9000,") != std::string::npos);
    CHECK(a4.find("OK") != std::string::npos);

    if (falhas == 0) std::cout << "teste_metricas: TODOS OS TESTES PASSARAM\n";
    return falhas == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}