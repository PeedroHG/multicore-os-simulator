#include "reporter.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <ostream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

constexpr int LARGURA_PALAVRA_16_BITS = 4;
constexpr int CASAS_DECIMAIS = 4;

// ============================================================================
// Formatacao (legado)
// ============================================================================

void imprimirHex16(std::ostream& saida, uint16_t valor) {
    saida << "0x"
          << std::uppercase
          << std::hex
          << std::setw(LARGURA_PALAVRA_16_BITS)
          << std::setfill('0')
          << valor
          << std::dec
          << std::nouppercase
          << std::setfill(' ');
}

void imprimirRelatorio(std::ostream& saida, const JobMetrics& metricas) {
    saida << "--- ESTADO DA CPU ---\n";

    saida << "PC: ";
    imprimirHex16(saida, metricas.pcFinal);

    saida << " IR: ";
    imprimirHex16(saida, metricas.irFinal);

    saida << '\n';
    saida << "Registradores Gerais:\n";
    for (int i = 0; i < 8; ++i) {
        saida << 'R' << i << ": ";
        imprimirHex16(saida, metricas.registradores[i]);

        if (i % 4 == 3) {
            saida << '\n';
        } else {
            saida << "   ";
        }
    }

    saida << "--- ESTATISTICAS DO JOB ---\n";
    saida << "Instrucoes Executadas: " << metricas.instrucoesExecutadas << '\n';
    saida << "Acessos a Memoria (RAM): " << metricas.acessosRAM << '\n';
    saida << "Acessos ao Disco (I/O): " << metricas.acessosDisco << '\n';
    saida << "CLOCK TOTAL DESTE JOB: " << metricas.clockTotal << " ciclos\n";
}

// Formatacao (multicore) - locale "C" e casas fixas => saida deterministica

// Real com casas fixas; valor nao finito vira campo vazio.
std::string real(double v, int casas = CASAS_DECIMAIS) {
    if (!std::isfinite(v)) return "";
    std::ostringstream o;
    o.imbue(std::locale::classic());
    o << std::fixed << std::setprecision(casas) << v;
    return o.str();
}

// Escapa um campo CSV (RFC 4180).
std::string csv(const std::string& campo) {
    if (campo.find_first_of(",\"\n\r") == std::string::npos) return campo;
    std::string saida = "\"";
    for (char c : campo) {
        if (c == '"') saida += '"';
        saida += c;
    }
    saida += '"';
    return saida;
}

const char* estadoFinal(const MetricasProcesso& p) {
    return p.abortado ? "ABORTADO" : "FINALIZADO";
}

std::string montarProcessosCsv(const ResultadoExecucao& r) {
    std::ostringstream o;
    o.imbue(std::locale::classic());
    o << "pid,nome,estado,prioridade,ciclo_inicio,ciclo_fim,tempo_espera,"
         "tempo_retorno,ciclos_execucao,ciclos_stall,instrucoes,"
         "trocas_contexto,faltas_pagina,acessos_memoria,write_backs\n";
    for (const auto& p : r.processos) {
        o << p.pid << ',' << csv(p.nome) << ',' << estadoFinal(p) << ','
          << p.prioridade << ',' << p.cicloInicio << ',' << p.cicloFim << ','
          << p.tempoEspera << ',' << p.tempoRetorno << ','
          << p.ciclosExecucao << ',' << p.ciclosStall << ',' << p.instrucoes << ','
          << p.trocasContexto << ',' << p.faltasPagina << ','
          << p.acessosMemoria << ',' << p.writeBacks << '\n';
    }
    return o.str();
}

std::string montarNucleosCsv(const ResultadoExecucao& r) {
    std::ostringstream o;
    o.imbue(std::locale::classic());
    o << "nucleo,ciclos_execucao,ciclos_stall,ciclos_troca,ocupado,ocioso,"
         "eficiencia,eficiencia_util,despachos,trocas_contexto\n";
    for (const auto& k : r.nucleos) {
        o << k.id << ',' << k.ciclosExecucao << ',' << k.ciclosStall << ','
          << k.ciclosTroca << ',' << k.ocupado << ',' << k.ocioso << ','
          << real(k.eficiencia) << ',' << real(k.eficienciaUtil) << ','
          << k.despachos << ',' << k.trocasContexto << '\n';
    }
    return o.str();
}

// Uma unica linha de dados: facilita concatenar varias execucoes (pandas).
std::string montarResumoCsv(const ResultadoExecucao& r) {
    const ConfigExperimento& c = r.config;
    const ResumoSistema& s = r.resumo;
    std::ostringstream o;
    o.imbue(std::locale::classic());
    o << "politica,modo,nucleos,quantum,substituicao,molduras,penalidade_falta,"
         "custo_troca,aging,processos_concluidos,processos_abortados,makespan,"
         "tme,tmr,utilizacao_media,utilizacao_util,throughput_por_1000,"
         "speedup,eficiencia_paralela,trocas_contexto,faltas_pagina,"
         "acessos_memoria,taxa_faltas,write_backs,instrucoes,ciclos_stall,ciclos_troca\n";
    o << csv(c.politica) << ',' << csv(c.modo) << ',' << c.nucleos << ','
      << c.quantum << ',' << csv(c.substituicao) << ',' << c.molduras << ','
      << c.penalidadeFalta << ',' << c.custoTroca << ',' << c.aging << ','
      << s.concluidos << ',' << s.abortados << ',' << s.makespan << ','
      << real(s.tme) << ',' << real(s.tmr) << ','
      << real(s.utilizacaoMedia) << ',' << real(s.utilizacaoUtil) << ','
      << real(s.throughput) << ','
      << (s.speedupValido ? real(s.speedup) : std::string()) << ','
      << (s.speedupValido ? real(s.eficienciaParalela) : std::string()) << ','
      << s.trocasContexto << ',' << s.faltasPagina << ',' << s.acessosMemoria << ','
      << real(s.taxaFaltas) << ',' << s.writeBacks << ',' << s.instrucoes << ','
      << s.ciclosStall << ',' << s.ciclosTroca << '\n';
    return o.str();
}

std::string montarLog(const ResultadoExecucao& r) {
    const ConfigExperimento& c = r.config;
    const ResumoSistema& s = r.resumo;
    std::ostringstream o;
    o.imbue(std::locale::classic());

    o << "=== LOG DETALHADO DA EXECUCAO ===\n\n";

    o << "[CONFIGURACAO]\n"
      << "politica: " << c.politica << "\n"
      << "modo: " << c.modo << "\n"
      << "nucleos: " << c.nucleos << "\n"
      << "quantum: " << c.quantum << "\n"
      << "substituicao: " << c.substituicao << "\n"
      << "molduras: " << c.molduras << "\n"
      << "penalidade_falta: " << c.penalidadeFalta << "\n"
      << "custo_troca: " << c.custoTroca << "\n"
      << "aging: " << c.aging << "\n\n";

    o << "[EVENTOS]\n";
    if (r.eventos.empty()) {
        o << "(nenhum evento registrado)\n";
    } else {
        for (const auto& e : r.eventos) o << e << '\n';
    }
    o << '\n';

    o << "[PROCESSOS]\n";
    for (const auto& p : r.processos) {
        o << "PID " << p.pid << " (" << p.nome << ") " << estadoFinal(p)
          << " | prio=" << p.prioridade
          << " | inicio=" << p.cicloInicio << " fim=" << p.cicloFim
          << " | TE=" << p.tempoEspera << " TR=" << p.tempoRetorno
          << " | exec=" << p.ciclosExecucao << " stall=" << p.ciclosStall
          << " | instr=" << p.instrucoes
          << " | trocas=" << p.trocasContexto
          << " | faltas=" << p.faltasPagina << '\n';
    }
    o << '\n';

    o << "[NUCLEOS]\n";
    for (const auto& k : r.nucleos) {
        o << "Core " << k.id
          << " | exec=" << k.ciclosExecucao << " stall=" << k.ciclosStall
          << " troca=" << k.ciclosTroca << " ocioso=" << k.ocioso
          << " | eficiencia=" << real(k.eficiencia)
          << " (util=" << real(k.eficienciaUtil) << ")"
          << " | despachos=" << k.despachos
          << " trocas=" << k.trocasContexto << '\n';
    }
    o << '\n';

    o << "[RESUMO]  (N = " << s.concluidos << " concluidos, "
      << s.abortados << " abortados; n = " << r.nucleos.size() << " nucleos)\n"
      << "Makespan = max(cicloFim) = " << s.makespan << " ciclos\n"
      << "TME = media(TE_i) = " << real(s.tme) << " ciclos\n"
      << "TMR = media(TR_i), TR_i = cicloFim_i = " << real(s.tmr) << " ciclos\n"
      << "Utilizacao media = soma(ocupado_k)/(n*makespan) = " << real(s.utilizacaoMedia) << "\n"
      << "Utilizacao util  = soma(exec_k)/(n*makespan)    = " << real(s.utilizacaoUtil) << "\n"
      << "Throughput = N/makespan*1000 = " << real(s.throughput) << " processos/1000 ciclos\n";
    if (s.speedupValido) {
        o << "Speedup S(n) = makespan(1)/makespan(n) = " << s.makespanBase << "/"
          << s.makespan << " = " << real(s.speedup) << "\n"
          << "Eficiencia paralela E(n) = S(n)/n = " << real(s.eficienciaParalela) << "\n";
    } else {
        o << "Speedup / Eficiencia paralela: indisponiveis (sem baseline n=1)\n";
    }
    o << "Memoria: acessos=" << s.acessosMemoria << " faltas=" << s.faltasPagina
      << " taxa=" << real(s.taxaFaltas) << " write-backs=" << s.writeBacks << "\n"
      << "Trocas de contexto=" << s.trocasContexto
      << " | ciclos de troca=" << s.ciclosTroca
      << " | ciclos de stall=" << s.ciclosStall << "\n\n";

    o << "[VERIFICACAO DE CONSISTENCIA]\n";
    const auto erros = Metricas::verificarConsistencia(r);
    if (erros.empty()) {
        o << "OK\n";
    } else {
        for (const auto& e : erros) o << "AVISO: " << e << '\n';
    }
    return o.str();
}

// Recria o arquivo por inteiro (trunc): nunca acrescenta.
bool gravar(const fs::path& caminho, const std::string& conteudo) {
    std::ofstream arq(caminho, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!arq.is_open()) {
        std::cerr << "[REPORTER] Falha ao abrir arquivo de saida: " << caminho.string() << '\n';
        return false;
    }
    arq << conteudo;
    arq.flush();
    if (!arq) {
        std::cerr << "[REPORTER] Falha ao escrever: " << caminho.string() << '\n';
        return false;
    }
    return true;
}

} // namespace

// ============================================================================
// API legada
// ============================================================================

void Reporter::exibirTerminal(const JobMetrics& metricas) {
    imprimirRelatorio(std::cout, metricas);
}

void Reporter::salvarOutputDat(const std::string& caminhoArquivo, const JobMetrics& metricas) {
    std::ofstream arquivo(caminhoArquivo, std::ios::app);

    if (!arquivo.is_open()) {
        std::cerr << "[REPORTER] Falha ao abrir arquivo de saida: "
                  << caminhoArquivo << '\n';
        return;
    }

    imprimirRelatorio(arquivo, metricas);
    arquivo << "===================================================\n";
}

// ============================================================================
// API multicore
// ============================================================================

bool Reporter::exportar(const std::string& diretorio, const ResultadoExecucao& r) {
    std::error_code ec;
    const fs::path base(diretorio);
    fs::create_directories(base, ec);
    if (ec) {
        std::cerr << "[REPORTER] Nao foi possivel criar o diretorio '" << diretorio
                  << "': " << ec.message() << '\n';
        return false;
    }

    // Monta tudo em memoria e grava; sem '&&' curto-circuito para tentar todos.
    bool ok = true;
    ok &= gravar(base / "processos.csv", montarProcessosCsv(r));
    ok &= gravar(base / "nucleos.csv",   montarNucleosCsv(r));
    ok &= gravar(base / "resumo.csv",    montarResumoCsv(r));
    ok &= gravar(base / "execucao.log",  montarLog(r));
    return ok;
}

void Reporter::exibirResumo(const ResultadoExecucao& r, std::ostream& saida) {
    const ResumoSistema& s = r.resumo;
    saida << "--- RESUMO DA EXECUCAO (" << r.config.politica << "/" << r.config.modo
          << ", " << r.nucleos.size() << " nucleo(s)) ---\n"
          << "Concluidos: " << s.concluidos << "  Abortados: " << s.abortados << '\n'
          << "Makespan: " << s.makespan << " ciclos\n"
          << "TME: " << real(s.tme) << "  TMR: " << real(s.tmr) << '\n'
          << "Utilizacao media: " << real(s.utilizacaoMedia)
          << "  util: " << real(s.utilizacaoUtil) << '\n'
          << "Throughput: " << real(s.throughput) << " proc/1000 ciclos\n";
    if (s.speedupValido) {
        saida << "Speedup: " << real(s.speedup)
              << "  Eficiencia paralela: " << real(s.eficienciaParalela) << '\n';
    }
    for (const auto& k : r.nucleos) {
        saida << "Core " << k.id << ": eficiencia " << real(k.eficiencia)
              << " (util " << real(k.eficienciaUtil) << ")\n";
    }
}