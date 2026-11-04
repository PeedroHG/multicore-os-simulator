// ============================================================================
// Testes da MMU — Paginação sob demanda, FIFO, LRU, dirty bit e write-back
// Responsável: João Paulo (Pessoa 2)
// ============================================================================
#include "../src/defines.hpp"
#include "../src/kernel.hpp"
#include "../src/memory.hpp"
#include "../src/mmu.hpp"
#include "../src/pcb.hpp"
#include "../loader/parser.hpp"
#include "../loader/ram_loader.hpp"

#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

static int falhas = 0;

#define CHECK(cond)                                                             \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::cerr << "  [FALHOU] " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
            ++falhas;                                                           \
        }                                                                       \
    } while (0)

// Job vazio (só HALT) com variáveis opcionais
static JobData jobCom(std::vector<std::pair<uint16_t, uint16_t>> vars = {}) {
    JobData j;
    j.id = 0;
    j.instrucoes = {0xF000};
    j.variaveis = std::move(vars);
    return j;
}

// Endereço lógico da página p (offset 0)
static uint16_t end(uint16_t pagina, uint16_t off = 0) {
    return static_cast<uint16_t>((pagina << BITS_OFFSET) | off);
}

// Executa a cadeia de referências (páginas) de um único processo e devolve o
// número de faltas.
static uint64_t rodarCadeia(PoliticaSubstituicao pol, uint32_t molduras,
                            const std::vector<uint16_t>& paginas) {
    MMU mmu(pol, molduras, 50);
    PCB p(1, "cadeia");
    mmu.carregarProcesso(p, jobCom());
    uint64_t t = 0;
    for (uint16_t pg : paginas) mmu.ler(p, end(pg), 0, t++);
    return mmu.getEstatisticas().faltas;
}

// ---------------------------------------------------------------------------
void testarFatiamento() {
    std::cout << "[TEST] Fatiamento de bits (8+4 logico, 12+4 fisico)\n";
    CHECK(TAM_PAGINA == 16);
    CHECK(MASK_OFFSET == 0x000F);
    CHECK(MAX_END_LOGICO == 0x0FFF);
    CHECK(MOLDURAS_SO == 32);
    CHECK(NUM_PAGINAS_LOGICAS == 256);
    CHECK(extrairPagina(0x0ABC) == 0xAB);
    CHECK(extrairOffset(0x0ABC) == 0xC);
    CHECK(montarEnderecoFisico(32, 0x5) == 0x0205); // 1a moldura de usuario = 512
}

void testarMMUProvisoria() {
    std::cout << "[TEST] MMU provisoria (fisico = 512 + pid*256 + logico)\n";
    CHECK(MMUProvisoria::traduzir(0, 0x00) == 512);
    CHECK(MMUProvisoria::traduzir(1, 0x10) == 512 + 256 + 16);

    MMUProvisoria stub;
    PCB a(1, "a"), b(2, "b");
    stub.carregarProcesso(a, jobCom({{0x20, 111}}));
    stub.carregarProcesso(b, jobCom({{0x20, 222}}));
    CHECK(stub.ler(a, 0x20, 0, 0).valor == 111);
    CHECK(stub.ler(b, 0x20, 0, 0).valor == 222);
    ResultadoAcesso r = stub.escrever(a, 0x21, 7, 0, 0);
    CHECK(!r.falta && r.ciclosPenalidade == 0);
    CHECK(stub.lerImagem(1, 0x21) == 7);
}

void testarIsolamento() {
    std::cout << "[TEST] Isolamento entre processos com o mesmo endereco virtual\n";
    MMU mmu(PoliticaSubstituicao::FIFO, 4, 50);
    PCB a(1, "a"), b(2, "b");
    mmu.carregarProcesso(a, jobCom({{0x20, 111}}));
    mmu.carregarProcesso(b, jobCom({{0x20, 222}}));

    CHECK(mmu.ler(a, 0x20, 0, 0).valor == 111);
    CHECK(mmu.ler(b, 0x20, 1, 0).valor == 222);
    mmu.escrever(a, 0x30, 0xAAAA, 0, 1);
    mmu.escrever(b, 0x30, 0xBBBB, 1, 1);
    CHECK(mmu.ler(a, 0x30, 0, 2).valor == 0xAAAA);
    CHECK(mmu.ler(b, 0x30, 1, 2).valor == 0xBBBB);

    // Mesma página lógica (2) em molduras físicas diferentes, fora da área do SO
    CHECK(a.tabelaPaginas[2].valida && b.tabelaPaginas[2].valida);
    CHECK(a.tabelaPaginas[2].moldura != b.tabelaPaginas[2].moldura);
    CHECK(a.tabelaPaginas[2].moldura >= MOLDURAS_SO);
    CHECK(b.tabelaPaginas[2].moldura >= MOLDURAS_SO);
}

void testarFaltasEAcertos() {
    std::cout << "[TEST] Paginacao sob demanda: falta no 1o acesso, acerto depois\n";
    MMU mmu(PoliticaSubstituicao::FIFO, 4, 50);
    PCB p(1, "p");
    mmu.carregarProcesso(p, jobCom({{0x05, 42}}));
    for (const auto& e : p.tabelaPaginas) CHECK(!e.valida);

    ResultadoAcesso r1 = mmu.ler(p, 0x05, 0, 0);
    CHECK(r1.falta && r1.ciclosPenalidade == 50 && r1.valor == 42);
    ResultadoAcesso r2 = mmu.ler(p, 0x0F, 0, 1); // mesma página 0
    CHECK(!r2.falta && r2.ciclosPenalidade == 0);
    CHECK(mmu.getEstatisticas().acessos == 2);
    CHECK(mmu.getEstatisticas().faltas == 1);
    CHECK(mmu.getEstatisticas().acertos == 1);
    CHECK(p.faltasPagina == 1);
    CHECK(p.tabelaPaginas[0].referenciada && !p.tabelaPaginas[0].suja);
}

void testarFIFO() {
    std::cout << "[TEST] FIFO deterministico + Anomalia de Belady (1,2,3,4,1,2,5,1,2,3,4,5)\n";
    const std::vector<uint16_t> belady = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
    CHECK(rodarCadeia(PoliticaSubstituicao::FIFO, 3, belady) == 9);
    CHECK(rodarCadeia(PoliticaSubstituicao::FIFO, 4, belady) == 10); // mais molduras, MAIS faltas

    // Vítima FIFO = a mais antiga, mesmo que tenha sido usada recentemente
    MMU mmu(PoliticaSubstituicao::FIFO, 3, 50);
    PCB p(1, "p");
    mmu.carregarProcesso(p, jobCom());
    for (uint16_t pg : {1, 2, 3, 1, 4}) mmu.ler(p, end(pg), 0, 0);
    CHECK(!p.tabelaPaginas[1].valida); // 1 saiu (mais antiga)
    CHECK(p.tabelaPaginas[2].valida && p.tabelaPaginas[3].valida && p.tabelaPaginas[4].valida);
}

void testarLRU() {
    std::cout << "[TEST] LRU deterministico (algoritmo de pilha: sem Anomalia de Belady)\n";
    const std::vector<uint16_t> belady = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
    CHECK(rodarCadeia(PoliticaSubstituicao::LRU, 3, belady) == 10);
    CHECK(rodarCadeia(PoliticaSubstituicao::LRU, 4, belady) == 8);

    // Vítima LRU = menos recentemente usada
    MMU mmu(PoliticaSubstituicao::LRU, 3, 50);
    PCB p(1, "p");
    mmu.carregarProcesso(p, jobCom());
    for (uint16_t pg : {1, 2, 3, 1, 4}) mmu.ler(p, end(pg), 0, 0);
    CHECK(!p.tabelaPaginas[2].valida); // 2 saiu (LRU)
    CHECK(p.tabelaPaginas[1].valida && p.tabelaPaginas[3].valida && p.tabelaPaginas[4].valida);

    // Dois núcleos no mesmo tick não empatam: o contador é monotônico
    MMU m2(PoliticaSubstituicao::LRU, 2, 50);
    PCB a(1, "a"), b(2, "b");
    m2.carregarProcesso(a, jobCom());
    m2.carregarProcesso(b, jobCom());
    m2.ler(a, end(0), 0, 7);
    m2.ler(b, end(0), 1, 7); // mesmo tick, núcleo 1 depois do núcleo 0
    m2.ler(a, end(1), 0, 8); // vítima deve ser a página de 'a' (acessada antes)
    CHECK(!a.tabelaPaginas[0].valida);
    CHECK(b.tabelaPaginas[0].valida);
}

void testarSubstituicaoGlobal() {
    std::cout << "[TEST] Substituicao global: vitima pode ser de outro processo\n";
    MMU mmu(PoliticaSubstituicao::FIFO, 2, 50);
    PCB a(1, "a"), b(2, "b");
    mmu.carregarProcesso(a, jobCom());
    mmu.carregarProcesso(b, jobCom());
    mmu.ler(a, end(0), 0, 0);
    mmu.ler(a, end(1), 0, 1);
    mmu.ler(b, end(0), 1, 2); // sem moldura livre: tira a página 0 de 'a'
    CHECK(!a.tabelaPaginas[0].valida);
    CHECK(a.tabelaPaginas[1].valida && b.tabelaPaginas[0].valida);
}

void testarWriteBack() {
    std::cout << "[TEST] Dirty bit e write-back em STORE\n";
    MMU mmu(PoliticaSubstituicao::FIFO, 1, 50);
    PCB p(1, "p");
    mmu.carregarProcesso(p, jobCom());

    ResultadoAcesso w = mmu.escrever(p, 0x05, 0xBEEF, 0, 0);
    CHECK(w.falta && w.ciclosPenalidade == 50);
    CHECK(p.tabelaPaginas[0].suja);

    // Página 1 expulsa a página 0 suja: falta (50) + write-back (50)
    ResultadoAcesso r = mmu.ler(p, end(1), 0, 1);
    CHECK(r.falta && r.ciclosPenalidade == 100);
    CHECK(mmu.getEstatisticas().writebacks == 1);
    CHECK(!p.tabelaPaginas[0].valida);
    CHECK(mmu.lerImagem(1, 0x05) == 0xBEEF); // dado sobreviveu no Disco Virtual

    // Página 1 está limpa: expulsá-la custa só a falta
    ResultadoAcesso r2 = mmu.ler(p, 0x05, 0, 2);
    CHECK(r2.falta && r2.ciclosPenalidade == 50 && r2.valor == 0xBEEF);
    CHECK(mmu.getEstatisticas().writebacks == 1);
}

void testarSegfaultELiberacao() {
    std::cout << "[TEST] SEGFAULT (> 0x0FFF) e liberacao de molduras\n";
    MMU mmu(PoliticaSubstituicao::LRU, 4, 50);
    PCB p(1, "p");
    mmu.carregarProcesso(p, jobCom());
    bool lancou = false;
    try { mmu.ler(p, 0x1000, 0, 0); } catch (const std::runtime_error&) { lancou = true; }
    CHECK(lancou);

    mmu.escrever(p, end(3, 1), 99, 0, 0);
    mmu.ler(p, end(4), 0, 1);
    CHECK(mmu.moldurasLivres() == 2);
    mmu.liberarProcesso(p);
    CHECK(mmu.moldurasLivres() == 4);
    CHECK(mmu.lerImagem(1, end(3, 1)) == 99); // página suja descarregada no disco

    bool invalido = false;
    try { MMU m0(PoliticaSubstituicao::FIFO, 0, 50); } catch (const std::invalid_argument&) { invalido = true; }
    CHECK(invalido);
}

void testarRastro() {
    std::cout << "[TEST] Rastro de referencias (tick, core, pid, pagina, tipo)\n";
    MMU mmu(PoliticaSubstituicao::FIFO, 4, 50);
    PCB p(3, "p");
    mmu.carregarProcesso(p, jobCom());
    mmu.ler(p, 0x12, 2, 10, TipoAcesso::BUSCA);
    mmu.escrever(p, 0x12, 1, 2, 11);
    const auto& r = mmu.getRastro();
    CHECK(r.size() == 2);
    CHECK(r[0].tick == 10 && r[0].core == 2 && r[0].pid == 3 && r[0].pagina == 1 &&
          r[0].tipo == 'X' && r[0].falta);
    CHECK(r[1].tipo == 'W' && !r[1].falta);
}

// ---------------------------------------------------------------------------
// Regressão: jobs reais rodando no Kernel multicore com MMU (memória escassa)
// devem terminar com registradores e memória idênticos à execução legada
// single-core (baseline: RAM física a partir de 512, sem paginação).
// ---------------------------------------------------------------------------
struct Snapshot {
    CPUContext ctx;
    std::vector<uint16_t> mem; // 256 primeiras palavras lógicas (área de dados)
};

static Snapshot rodarLegado(const JobData& job) {
    std::memset(RAM + OS_RESERVED_MEM, 0, sizeof(uint16_t) * 4096);
    RAMLoader::carregarNaRAM(job, RAM, TAM_RAM);
    CPU cpu;
    cpu.reset(); // PC = 512 (legado)
    for (int i = 0; i < 100000 && !cpu.isHalted(); ++i) cpu.step();
    Snapshot s;
    s.ctx = cpu.getRegisterBank().getContext();
    s.mem.assign(RAM + OS_RESERVED_MEM, RAM + OS_RESERVED_MEM + 256);
    return s;
}

void testarRegressaoKernel(int nCores, PoliticaSubstituicao pol, uint32_t molduras) {
    std::cout << "[TEST] Regressao vs baseline: " << nCores << " nucleo(s), "
              << MMU::nomePolitica(pol) << ", " << molduras << " moldura(s)\n";
    const std::vector<std::string> arquivos = {"tests/job1_soma.txt", "tests/job2_vetor.txt",
                                               "tests/job3_loop.txt", "tests/memoria/mem_ciclico_a.txt",
                                               "tests/memoria/mem_localidade.txt"};
    std::vector<JobData> jobs;
    std::vector<Snapshot> esperado;
    for (const auto& a : arquivos) {
        JobData j;
        if (!JobParser::carregarArquivo(a, j)) {
            std::cerr << "  [FALHOU] nao abriu " << a << " (rode a partir da raiz do repo)\n";
            ++falhas;
            return;
        }
        jobs.push_back(j);
        esperado.push_back(rodarLegado(j));
    }

    MMU mmu(pol, molduras, 50);
    Kernel kernel(nCores);
    kernel.conectarMMU(&mmu);
    std::vector<PCB*> pcbs;
    for (size_t i = 0; i < jobs.size(); ++i) {
        auto pcb = std::make_unique<PCB>(static_cast<int>(i) + 1, arquivos[i]);
        pcb->contexto.PC = 0;
        mmu.carregarProcesso(*pcb, jobs[i]);
        pcbs.push_back(pcb.get());
        kernel.admitirProcesso(std::move(pcb));
    }
    kernel.rodarSimulacao();

    for (size_t i = 0; i < pcbs.size(); ++i) {
        CHECK(pcbs[i]->estado == EstadoProcesso::FINALIZADO);
        CHECK(pcbs[i]->contexto.R == esperado[i].ctx.R);
        CHECK(pcbs[i]->faltasPagina > 0);
        for (uint16_t a = 0; a < 256; ++a) {
            if (mmu.lerImagem(pcbs[i]->pid, a) != esperado[i].mem[a]) {
                std::cerr << "  [FALHOU] pid " << pcbs[i]->pid << " memoria[0x" << std::hex << a
                          << std::dec << "] difere do baseline\n";
                ++falhas;
                break;
            }
        }
        // stall contabilizado = faltas x penalidade (+ write-backs)
        CHECK(pcbs[i]->ciclosStall >= static_cast<uint64_t>(pcbs[i]->faltasPagina) * 50);
    }
    CHECK(mmu.moldurasLivres() == molduras); // tudo liberado ao final
    CHECK(kernel.getTickGlobal() >= pcbs.back()->cicloFim);
}

int main() {
    std::cout << "==================================================\n";
    std::cout << "  SUITE DE TESTES: MMU (PAGINACAO FIFO / LRU)     \n";
    std::cout << "==================================================\n";

    testarFatiamento();
    testarMMUProvisoria();
    testarIsolamento();
    testarFaltasEAcertos();
    testarFIFO();
    testarLRU();
    testarSubstituicaoGlobal();
    testarWriteBack();
    testarSegfaultELiberacao();
    testarRastro();
    testarRegressaoKernel(1, PoliticaSubstituicao::FIFO, 32);
    testarRegressaoKernel(4, PoliticaSubstituicao::FIFO, 4);
    testarRegressaoKernel(4, PoliticaSubstituicao::LRU, 4);
    testarRegressaoKernel(2, PoliticaSubstituicao::LRU, 1);

    if (falhas) {
        std::cout << "\n" << falhas << " VERIFICACAO(OES) FALHARAM NA SUITE DA MMU!\n";
        return 1;
    }
    std::cout << "\nTODOS OS TESTES DA MMU PASSARAM COM SUCESSO!\n";
    return 0;
}
