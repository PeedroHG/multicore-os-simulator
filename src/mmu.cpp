#include "mmu.hpp"
#include "memory.hpp"
#include "../loader/ram_loader.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

std::string hex3(uint16_t v) {
    std::ostringstream os;
    os << "0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << v;
    return os.str();
}

// Proteção comum às duas MMUs: o espaço lógico tem 12 bits.
void validarEnderecoLogico(const PCB& pcb, uint16_t endLogico) {
    if (endLogico > MAX_END_LOGICO) {
        throw std::runtime_error("SEGFAULT: endereco logico " + hex3(endLogico) +
                                 " fora do espaco de 12 bits (pid " +
                                 std::to_string(pcb.pid) + ")");
    }
}

} // namespace

// ============================================================================
// MMU Provisória (stub da Fase 1)
// ============================================================================

uint32_t MMUProvisoria::traduzir(int pid, uint16_t endLogico) {
    if (endLogico >= TAM_PARTICAO) {
        throw std::runtime_error("SEGFAULT: MMU provisoria so mapeia 256 palavras por processo (" +
                                 hex3(endLogico) + ")");
    }
    uint32_t fisico = OS_RESERVED_MEM + static_cast<uint32_t>(pid) * TAM_PARTICAO + endLogico;
    if (fisico >= TAM_RAM) {
        throw std::runtime_error("SEGFAULT: MMU provisoria sem espaco para o pid " +
                                 std::to_string(pid));
    }
    return fisico;
}

void MMUProvisoria::carregarProcesso(PCB& pcb, const JobData& job) {
    std::vector<uint16_t> imagem;
    if (!RAMLoader::montarImagem(job, imagem)) {
        throw std::runtime_error("MMU provisoria: job nao cabe no espaco logico (pid " +
                                 std::to_string(pcb.pid) + ")");
    }
    for (uint32_t i = 0; i < TAM_PARTICAO; ++i) {
        EscreverFisico(traduzir(pcb.pid, static_cast<uint16_t>(i)), imagem[i]);
    }
    pcb.tabelaPaginas.clear(); // tradução fixa: não há tabela de páginas
}

void MMUProvisoria::liberarProcesso(PCB&) {}

ResultadoAcesso MMUProvisoria::ler(PCB& pcb, uint16_t endLogico, int, uint64_t, TipoAcesso) {
    validarEnderecoLogico(pcb, endLogico);
    ResultadoAcesso r;
    r.valor = LerFisico(traduzir(pcb.pid, endLogico));
    return r;
}

ResultadoAcesso MMUProvisoria::escrever(PCB& pcb, uint16_t endLogico, uint16_t dado, int, uint64_t) {
    validarEnderecoLogico(pcb, endLogico);
    EscreverFisico(traduzir(pcb.pid, endLogico), dado);
    ResultadoAcesso r;
    r.valor = dado;
    return r;
}

uint16_t MMUProvisoria::lerImagem(int pid, uint16_t endLogico) const {
    return LerFisico(traduzir(pid, endLogico));
}

// ============================================================================
// MMU real — paginação sob demanda, substituição global FIFO / LRU
// ============================================================================

MMU::MMU(PoliticaSubstituicao pol, uint32_t numMoldurasUsuario, uint32_t penalidade)
    : politica(pol), penalidadeFalta(penalidade) {
    const uint32_t maxMolduras = NUM_MOLDURAS_FISICAS - MOLDURAS_SO; // 4064
    if (numMoldurasUsuario == 0 || numMoldurasUsuario > maxMolduras) {
        throw std::invalid_argument("MMU: numero de molduras de usuario deve estar em [1, " +
                                    std::to_string(maxMolduras) + "]");
    }
    tabelaMolduras.assign(numMoldurasUsuario, Moldura{});
}

PoliticaSubstituicao MMU::politicaDeTexto(const std::string& nome) {
    if (nome == "fifo" || nome == "FIFO") return PoliticaSubstituicao::FIFO;
    if (nome == "lru" || nome == "LRU") return PoliticaSubstituicao::LRU;
    throw std::invalid_argument("Politica de substituicao desconhecida: " + nome +
                                " (use fifo ou lru)");
}

const char* MMU::nomePolitica(PoliticaSubstituicao p) {
    return p == PoliticaSubstituicao::FIFO ? "FIFO" : "LRU";
}

void MMU::carregarProcesso(PCB& pcb, const JobData& job) {
    std::vector<uint16_t> imagem;
    if (!RAMLoader::montarImagem(job, imagem)) {
        throw std::runtime_error("MMU: job nao cabe no espaco logico de 12 bits (pid " +
                                 std::to_string(pcb.pid) + ")");
    }
    if (processos.count(pcb.pid)) {
        throw std::runtime_error("MMU: pid " + std::to_string(pcb.pid) + " ja carregado");
    }

    discoVirtual[pcb.pid] = std::move(imagem);
    processos[pcb.pid] = &pcb;
    estatPorProcesso[pcb.pid] = EstatisticasMemoria{};

    // Paginação sob demanda: todas as páginas começam inválidas.
    pcb.tabelaPaginas.assign(NUM_PAGINAS_LOGICAS, EntradaPagina{});
    pcb.faltasPagina = 0;
}

void MMU::copiarMolduraParaDisco(uint32_t indice) {
    const Moldura& m = tabelaMolduras[indice];
    std::vector<uint16_t>& img = discoVirtual.at(m.pid);
    const uint32_t molduraFisica = MOLDURAS_SO + indice;
    for (uint16_t off = 0; off < TAM_PAGINA; ++off) {
        img[static_cast<uint32_t>(m.pagina) * TAM_PAGINA + off] =
            LerFisico(montarEnderecoFisico(molduraFisica, off));
    }
}

void MMU::removerDaFilaFIFO(uint32_t indice) {
    auto it = std::find(filaFIFO.begin(), filaFIFO.end(), indice);
    if (it != filaFIFO.end()) filaFIFO.erase(it);
}

void MMU::liberarProcesso(PCB& pcb) {
    for (uint32_t i = 0; i < tabelaMolduras.size(); ++i) {
        Moldura& m = tabelaMolduras[i];
        if (m.livre || m.pid != pcb.pid) continue;

        // Páginas sujas são descarregadas no disco (sem custo: o processo já
        // terminou) para que a imagem final continue consultável no relatório.
        if (pcb.tabelaPaginas[m.pagina].suja) {
            copiarMolduraParaDisco(i);
        }
        pcb.tabelaPaginas[m.pagina] = EntradaPagina{};
        m = Moldura{};
        removerDaFilaFIFO(i);
    }
    processos.erase(pcb.pid);
}

uint32_t MMU::moldurasLivres() const {
    return static_cast<uint32_t>(std::count_if(tabelaMolduras.begin(), tabelaMolduras.end(),
                                               [](const Moldura& m) { return m.livre; }));
}

EstatisticasMemoria MMU::getEstatisticas(int pid) const {
    auto it = estatPorProcesso.find(pid);
    return it == estatPorProcesso.end() ? EstatisticasMemoria{} : it->second;
}

uint32_t MMU::escolherVitima() const {
    if (politica == PoliticaSubstituicao::FIFO) {
        // Moldura carregada há mais tempo = início da fila
        return filaFIFO.front();
    }
    // LRU exato: menor carimbo do contador monotônico de acessos
    uint32_t vitima = 0;
    for (uint32_t i = 1; i < tabelaMolduras.size(); ++i) {
        if (tabelaMolduras[i].ultimoAcesso < tabelaMolduras[vitima].ultimoAcesso) {
            vitima = i;
        }
    }
    return vitima;
}

uint32_t MMU::tratarFalta(PCB& pcb, uint16_t pagina, ResultadoAcesso& res) {
    res.falta = true;
    res.ciclosPenalidade += penalidadeFalta; // leitura da página no disco

    // 1. Procura moldura livre (menor índice => determinístico)
    uint32_t indice = static_cast<uint32_t>(tabelaMolduras.size());
    for (uint32_t i = 0; i < tabelaMolduras.size(); ++i) {
        if (tabelaMolduras[i].livre) { indice = i; break; }
    }

    // 2. Sem moldura livre: substituição global (FIFO ou LRU)
    if (indice == tabelaMolduras.size()) {
        indice = escolherVitima();
        Moldura& vit = tabelaMolduras[indice];
        PCB* dono = processos.at(vit.pid);
        EntradaPagina& entVit = dono->tabelaPaginas[vit.pagina];

        if (entVit.suja) {
            // Write-back: a página modificada volta ao Disco Virtual
            copiarMolduraParaDisco(indice);
            res.ciclosPenalidade += penalidadeFalta;
            estatGlobal.writebacks++;
            estatPorProcesso[pcb.pid].writebacks++;
        }
        entVit = EntradaPagina{}; // página da vítima fica inválida
        removerDaFilaFIFO(indice);
    }

    // 3. Carrega a página do Disco Virtual para a moldura física
    const uint32_t molduraFisica = MOLDURAS_SO + indice;
    const std::vector<uint16_t>& img = discoVirtual.at(pcb.pid);
    for (uint16_t off = 0; off < TAM_PAGINA; ++off) {
        EscreverFisico(montarEnderecoFisico(molduraFisica, off),
                       img[static_cast<uint32_t>(pagina) * TAM_PAGINA + off]);
    }

    // 4. Atualiza a tabela de molduras e a tabela de páginas do processo
    Moldura& m = tabelaMolduras[indice];
    m.livre = false;
    m.pid = pcb.pid;
    m.pagina = pagina;
    m.carregadaEm = ++contadorCargas;
    filaFIFO.push_back(indice);

    EntradaPagina& ent = pcb.tabelaPaginas[pagina];
    ent.valida = true;
    ent.suja = false;
    ent.referenciada = false;
    ent.moldura = static_cast<uint16_t>(molduraFisica);

    pcb.faltasPagina++;
    estatGlobal.faltas++;
    estatPorProcesso[pcb.pid].faltas++;

    return indice;
}

uint32_t MMU::traduzir(PCB& pcb, uint16_t endLogico, bool escrita, int coreId,
                       uint64_t tick, TipoAcesso tipo, ResultadoAcesso& res) {
    validarEnderecoLogico(pcb, endLogico);
    if (!processos.count(pcb.pid)) {
        throw std::runtime_error("MMU: acesso de processo nao carregado (pid " +
                                 std::to_string(pcb.pid) + ")");
    }

    const uint16_t pagina = extrairPagina(endLogico);
    const uint16_t offset = extrairOffset(endLogico);

    estatGlobal.acessos++;
    EstatisticasMemoria& ep = estatPorProcesso[pcb.pid];
    ep.acessos++;

    EntradaPagina& ent = pcb.tabelaPaginas[pagina];
    uint32_t indice;
    if (ent.valida) {
        // Acerto (hit)
        estatGlobal.acertos++;
        ep.acertos++;
        indice = static_cast<uint32_t>(ent.moldura) - MOLDURAS_SO;
    } else {
        // Falta de página (page fault)
        indice = tratarFalta(pcb, pagina, res);
    }

    ent.referenciada = true;
    if (escrita) ent.suja = true;
    tabelaMolduras[indice].ultimoAcesso = ++contadorAcessos;

    if (registrarRastro) {
        rastro.push_back({tick, coreId, pcb.pid, pagina, static_cast<char>(tipo), res.falta});
    }

    return montarEnderecoFisico(ent.moldura, offset);
}

ResultadoAcesso MMU::ler(PCB& pcb, uint16_t endLogico, int coreId, uint64_t tick, TipoAcesso tipo) {
    ResultadoAcesso res;
    const uint32_t fisico = traduzir(pcb, endLogico, false, coreId, tick, tipo, res);
    res.valor = LerFisico(fisico);
    return res;
}

ResultadoAcesso MMU::escrever(PCB& pcb, uint16_t endLogico, uint16_t dado, int coreId, uint64_t tick) {
    ResultadoAcesso res;
    const uint32_t fisico = traduzir(pcb, endLogico, true, coreId, tick, TipoAcesso::ESCRITA, res);
    EscreverFisico(fisico, dado);
    res.valor = dado;
    return res;
}

uint16_t MMU::lerImagem(int pid, uint16_t endLogico) const {
    if (endLogico > MAX_END_LOGICO) {
        throw std::out_of_range("MMU::lerImagem - endereco logico fora do espaco de 12 bits");
    }
    auto itP = processos.find(pid);
    if (itP != processos.end()) {
        const EntradaPagina& ent = itP->second->tabelaPaginas[extrairPagina(endLogico)];
        if (ent.valida) {
            return LerFisico(montarEnderecoFisico(ent.moldura, extrairOffset(endLogico)));
        }
    }
    return discoVirtual.at(pid)[endLogico];
}

bool MMU::exportarRastroCSV(const std::string& caminho) const {
    std::ofstream out(caminho, std::ios::trunc);
    if (!out) return false;
    out << "tick,core,pid,pagina,tipo,falta\n";
    for (const auto& r : rastro) {
        out << r.tick << ',' << r.core << ',' << r.pid << ',' << r.pagina << ','
            << r.tipo << ',' << (r.falta ? 1 : 0) << '\n';
    }
    return true;
}

bool MMU::exportarEstatisticasCSV(const std::string& caminho) const {
    std::ofstream out(caminho, std::ios::trunc);
    if (!out) return false;
    out << "pid,politica,molduras,acessos,acertos,faltas,writebacks,taxa_faltas\n";
    auto linha = [&](const std::string& pid, const EstatisticasMemoria& e) {
        out << pid << ',' << nomePolitica(politica) << ',' << tabelaMolduras.size() << ','
            << e.acessos << ',' << e.acertos << ',' << e.faltas << ',' << e.writebacks << ','
            << std::fixed << std::setprecision(6) << e.taxaFaltas() << '\n';
    };
    for (const auto& [pid, e] : estatPorProcesso) linha(std::to_string(pid), e);
    linha("global", estatGlobal);
    return true;
}
