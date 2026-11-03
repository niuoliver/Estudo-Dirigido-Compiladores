/*
Analisador Semantico - Linguagem Mini-Lisp
Alunos:
    Oliver Neumann Brosselin

Percorre a AST (ast.hpp) construida pelo sintatico, monta a TABELA DE SIMBOLOS e
verifica escopo, tipos e numero de argumentos. Nao para no primeiro erro: acumula todos
(com linha e coluna) e usa o tipo DESCONHECIDO como "coringa" para nao gerar erros em cascata.

TIPOS
    INT, FLOAT   numeros.     BOOL  resultado de comparacao.
    VOID         expressao sem valor (print, while, if sem else, lista vazia).
    DESCONHECIDO tipo de uma expressao ja com erro, ou retorno de funcao ainda em analise (recursao).

REGRAS (resumo)
  Escopo     global -> funcao (defun) -> let. Resolucao lexica: procura do escopo mais interno ao global.
  set        (set id expr): se 'id' nao e visivel em nenhum escopo, declara na hora no escopo atual
             com o tipo de expr. Depois disso o tipo e fixo: INT -> FLOAT e aceito, FLOAT -> INT nao.
  Variavel   usada sem declaracao e erro.
  Operadores + - * / % e comparacoes: exatamente 2 operandos numericos.
             Aritmetica: INT op INT = INT (/ e divisao inteira); com algum FLOAT = FLOAT.
             % exige INT. Comparacoes (> < == != <= >=) dao BOOL. Divisor literal 0 e erro.
  if         2 ou 3 argumentos. Condicao BOOL ou INT. Sem else = VOID.
  while      condicao + ao menos 1 expressao no corpo. Tipo VOID.
  begin      ao menos 1 expressao; o tipo e o da ultima.
  print      exatamente 1 argumento com valor. Tipo VOID.
  let        (let ((a e1) (b e2)) corpo...): e1, e2 sao avaliadas no escopo EXTERNO; nomes repetidos sao erro.
  defun      so no nivel global. Nome unico. Parametros sao INT e nao podem se repetir.
             Tipo de retorno = tipo da ultima expressao do corpo. Funcao precisa ser definida antes do uso.
  Chamada    (f a1 ... an): f deve ser funcao, n igual a aridade, cada argumento compativel com INT.

ENDERECOS (para o gerador de codigo MEPA): par (nivel lexico, deslocamento).
    global: nivel 0, deslocamentos 0,1,2...      funcao: nivel 1.
    locais e variaveis de let: 0,1,2... dentro do quadro (a posicao de um let e reaproveitada ao sair dele).
    parametros: -(n + PALAVRAS_LIGACAO) + i, i = 0..n-1 (o ultimo fica logo abaixo da area de ligacao).
*/

#ifndef SEMANTICO_HPP
#define SEMANTICO_HPP

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include "ast.hpp"
using namespace std;

// palavras de ligacao entre parametros e variaveis locais no registro de ativacao
// (endereco de retorno + valor salvo do display). Ajuste aqui se o seu MEPA usar outra convencao.
const int PALAVRAS_LIGACAO = 2;

enum class Tipo { INT, FLOAT, BOOL, VOID, DESCONHECIDO };

inline const char* nomeTipo(Tipo t) {
    switch (t) {
        case Tipo::INT:          return "INT";
        case Tipo::FLOAT:        return "FLOAT";
        case Tipo::BOOL:         return "BOOL";
        case Tipo::VOID:         return "VOID";
        case Tipo::DESCONHECIDO: return "?";
    }
    return "?";
}

enum class Categoria { VARIAVEL, PARAMETRO, FUNCAO };

inline const char* nomeCategoria(Categoria c) {
    switch (c) {
        case Categoria::VARIAVEL:  return "variavel";
        case Categoria::PARAMETRO: return "parametro";
        case Categoria::FUNCAO:    return "funcao";
    }
    return "?";
}

// uma linha da tabela de simbolos.
struct Simbolo {
    string nome;
    Categoria cat = Categoria::VARIAVEL;
    Tipo tipo = Tipo::DESCONHECIDO;   // tipo da variavel/parametro; tipo de retorno da funcao
    string escopo;                    // caminho do escopo: "global", "soma", "global/let#1"
    int nivel = 0;                    // nivel lexico (0 = global, 1 = dentro de funcao)
    int desloc = 0;                   // deslocamento no quadro (variaveis e parametros)
    int aridade = 0;                  // so para funcoes
    int tamQuadro = 0;                // so para funcoes: posicoes de variaveis locais
    vector<Tipo> tiposParam;          // so para funcoes: tipo de cada parametro
    int linha = 0;                    // onde foi declarado
    int coluna = 0;
};

struct ErroSemantico {
    int linha;
    int coluna;
    string msg;
};

class AnalisadorSemantico {
private:
    // um escopo visivel: nomes declarados nele.
    struct Escopo {
        string caminho;
        map<string, int> nomes;   // nome -> indice em 'tabela'
        int proxSalvo = 0;        // valor do contador do quadro ao entrar (restaurado ao sair de um let)
    };

    // quadro de ativacao: o programa principal ou uma funcao. Os lets usam o quadro de quem os contem.
    struct Quadro {
        int nivel;
        int proximo;   // proximo deslocamento livre
        int maximo;    // maior numero de posicoes usadas ao mesmo tempo
    };

    vector<Simbolo> tabela;           // historico: nenhum simbolo e removido ao sair do escopo
    vector<Escopo> pilha;             // escopos visiveis (pilha[0] = global)
    vector<Quadro> quadros;
    vector<ErroSemantico> listaErros;
    int contLet = 0;

    // ----------------------------------------------------------------- utilitarios

    void erro(const No& n, const string& msg) {
        ErroSemantico e;
        e.linha = n.linha;
        e.coluna = n.coluna;
        e.msg = msg;
        listaErros.push_back(e);
    }

    static bool ehNumerico(Tipo t) { return t == Tipo::INT || t == Tipo::FLOAT; }

    // valor de tipo 'origem' pode ser guardado em algo de tipo 'destino'?
    static bool compativel(Tipo destino, Tipo origem) {
        if (destino == Tipo::DESCONHECIDO || origem == Tipo::DESCONHECIDO) return true;
        if (destino == origem) return true;
        return destino == Tipo::FLOAT && origem == Tipo::INT;
    }

    // tipo de um if com else: o tipo comum dos dois ramos, ou VOID se nao houver.
    static Tipo unificar(Tipo a, Tipo b) {
        if (a == Tipo::DESCONHECIDO) return b;
        if (b == Tipo::DESCONHECIDO) return a;
        if (a == b) return a;
        if (ehNumerico(a) && ehNumerico(b)) return Tipo::FLOAT;
        return Tipo::VOID;
    }

    static bool ehZeroLiteral(const No& n) {
        return (n.tipo == NoTipo::INT || n.tipo == NoTipo::FLOAT) && strtod(n.lexema.c_str(), nullptr) == 0.0;
    }

    // ----------------------------------------------------------------- tabela e escopos

    // procura o nome do escopo mais interno ao global; devolve o indice na tabela ou -1.
    int buscar(const string& nome) const {
        for (int i = (int)pilha.size() - 1; i >= 0; i--) {
            map<string, int>::const_iterator it = pilha[i].nomes.find(nome);
            if (it != pilha[i].nomes.end()) return it->second;
        }
        return -1;
    }

    // cria o simbolo na tabela (ainda sem registrar o nome em nenhum escopo).
    int novoSimbolo(int idxEscopo, const string& nome, Categoria cat, Tipo tipo, const No& onde) {
        Simbolo s;
        s.nome = nome;
        s.cat = cat;
        s.tipo = tipo;
        s.escopo = pilha[idxEscopo].caminho;
        s.nivel = (cat == Categoria::FUNCAO) ? 0 : quadros.back().nivel;
        s.linha = onde.linha;
        s.coluna = onde.coluna;
        if (cat == Categoria::VARIAVEL) {
            Quadro& q = quadros.back();
            s.desloc = q.proximo++;
            if (q.proximo > q.maximo) q.maximo = q.proximo;
        }
        tabela.push_back(s);
        return (int)tabela.size() - 1;
    }

    void registrar(int idxEscopo, int idxSimbolo) {
        pilha[idxEscopo].nomes[tabela[idxSimbolo].nome] = idxSimbolo;
    }

    void entrarFuncao(const string& nome) {
        Escopo e;
        e.caminho = nome;
        pilha.push_back(e);
        Quadro q;
        q.nivel = 1;
        q.proximo = 0;
        q.maximo = 0;
        quadros.push_back(q);
    }

    // devolve o tamanho do quadro (variaveis locais) da funcao que terminou.
    int sairFuncao() {
        int tam = quadros.back().maximo;
        quadros.pop_back();
        pilha.pop_back();
        return tam;
    }

    void entrarLet() {
        Escopo e;
        e.caminho = pilha.back().caminho + "/let#" + to_string(++contLet);
        e.proxSalvo = quadros.back().proximo;
        pilha.push_back(e);
    }

    void sairLet() {
        quadros.back().proximo = pilha.back().proxSalvo;   // libera as posicoes do let
        pilha.pop_back();
    }

    // ----------------------------------------------------------------- expressoes

    // analisa n e devolve seu tipo.
    Tipo expr(const No& n) {
        switch (n.tipo) {
            case NoTipo::INT:   return Tipo::INT;
            case NoTipo::FLOAT: return Tipo::FLOAT;

            case NoTipo::ID: {
                int idx = buscar(n.lexema);
                if (idx < 0) {
                    erro(n, "variavel '" + n.lexema + "' usada sem declaracao (atribua com set antes de usar)");
                    return Tipo::DESCONHECIDO;
                }
                if (tabela[idx].cat == Categoria::FUNCAO) {
                    erro(n, "'" + n.lexema + "' e uma funcao e nao pode ser usada como valor");
                    return Tipo::DESCONHECIDO;
                }
                return tabela[idx].tipo;
            }

            case NoTipo::OP:
            case NoTipo::KW:
                erro(n, "'" + n.lexema + "' so pode aparecer como primeiro elemento de uma lista");
                return Tipo::DESCONHECIDO;

            case NoTipo::LISTA:  return lista(n);
            case NoTipo::DEFUN:  return defun(n);
            case NoTipo::LET:    return let(n);

            case NoTipo::PROGRAMA:
            case NoTipo::BINDING:
                break;   // nunca chegam aqui: sao tratados por quem os contem
        }
        return Tipo::DESCONHECIDO;
    }

    // igual a expr(), mas exige que a expressao produza valor.
    Tipo exprComValor(const No& n, const string& contexto) {
        Tipo t = expr(n);
        if (t == Tipo::VOID) {
            erro(n, "expressao sem valor usada como " + contexto);
            return Tipo::DESCONHECIDO;
        }
        return t;
    }

    // condicao de if/while: BOOL (comparacao) ou INT.
    void condicao(const No& n, const string& forma) {
        Tipo t = exprComValor(n, "condicao de '" + forma + "'");
        if (t != Tipo::DESCONHECIDO && t != Tipo::BOOL && t != Tipo::INT) {
            erro(n, "condicao de '" + forma + "' deve ser uma comparacao ou um inteiro, mas e " + nomeTipo(t));
        }
    }

    // analisa os filhos de n a partir do indice 'de' (para achar mais erros depois de um erro de forma).
    void analisarResto(const No& n, size_t de) {
        for (size_t i = de; i < n.filhos.size(); i++) {
            expr(*n.filhos[i]);
        }
    }

    Tipo lista(const No& n) {
        if (n.filhos.empty()) {
            return Tipo::VOID;   // lista vazia "()"
        }

        const No& cabeca = *n.filhos[0];
        switch (cabeca.tipo) {
            case NoTipo::KW: {
                const string& k = cabeca.lexema;
                if (k == "set")   return formaSet(n);
                if (k == "print") return formaPrint(n);
                if (k == "if")    return formaIf(n);
                if (k == "while") return formaWhile(n);
                if (k == "begin") return formaBegin(n);
                break;
            }
            case NoTipo::OP: return operador(n);
            case NoTipo::ID: return chamada(n);
            default: break;
        }

        erro(n, "o primeiro elemento da lista deve ser um operador, uma palavra-chave ou o nome de uma funcao");
        analisarResto(n, 0);
        return Tipo::DESCONHECIDO;
    }

    // (set id expr)
    Tipo formaSet(const No& n) {
        size_t nargs = n.filhos.size() - 1;
        if (nargs != 2) {
            erro(n, "'set' espera 2 argumentos (identificador e valor), mas recebeu " + to_string(nargs));
            if (nargs >= 1 && n.filhos[1]->tipo != NoTipo::ID) analisarResto(n, 1);
            else analisarResto(n, 2);
            return Tipo::DESCONHECIDO;
        }

        const No& alvo = *n.filhos[1];
        if (alvo.tipo != NoTipo::ID) {
            erro(alvo, "'set' espera um identificador como primeiro argumento");
            expr(*n.filhos[2]);
            return Tipo::DESCONHECIDO;
        }

        // o valor e avaliado antes de declarar: (set x (+ x 1)) com x inexistente e erro.
        Tipo valor = exprComValor(*n.filhos[2], "valor atribuido a '" + alvo.lexema + "'");

        int idx = buscar(alvo.lexema);
        if (idx >= 0) {
            const Simbolo& s = tabela[idx];
            if (s.cat == Categoria::FUNCAO) {
                erro(alvo, "'" + alvo.lexema + "' e uma funcao e nao pode receber atribuicao");
                return Tipo::DESCONHECIDO;
            }
            if (!compativel(s.tipo, valor)) {
                erro(alvo, "atribuicao incompativel: '" + alvo.lexema + "' e " + nomeTipo(s.tipo) +
                           " (declarada na linha " + to_string(s.linha) + "), mas o valor e " + nomeTipo(valor));
            }
            return s.tipo;
        }

        // primeira atribuicao: declara no escopo atual
        int novo = novoSimbolo((int)pilha.size() - 1, alvo.lexema, Categoria::VARIAVEL, valor, alvo);
        registrar((int)pilha.size() - 1, novo);
        return valor;
    }

    // (print expr)
    Tipo formaPrint(const No& n) {
        size_t nargs = n.filhos.size() - 1;
        if (nargs != 1) {
            erro(n, "'print' espera 1 argumento, mas recebeu " + to_string(nargs));
            analisarResto(n, 1);
            return Tipo::DESCONHECIDO;
        }
        exprComValor(*n.filhos[1], "argumento de 'print'");
        return Tipo::VOID;
    }

    // (if cond then [else])
    Tipo formaIf(const No& n) {
        size_t nargs = n.filhos.size() - 1;
        if (nargs != 2 && nargs != 3) {
            erro(n, "'if' espera 2 ou 3 argumentos (condicao, then e, opcionalmente, else), mas recebeu " + to_string(nargs));
            analisarResto(n, 1);
            return Tipo::DESCONHECIDO;
        }
        condicao(*n.filhos[1], "if");
        Tipo ramoThen = expr(*n.filhos[2]);
        if (nargs == 2) {
            return Tipo::VOID;
        }
        Tipo ramoElse = expr(*n.filhos[3]);
        return unificar(ramoThen, ramoElse);
    }

    // (while cond corpo...)
    Tipo formaWhile(const No& n) {
        size_t nargs = n.filhos.size() - 1;
        if (nargs < 2) {
            erro(n, "'while' espera uma condicao e ao menos 1 expressao no corpo, mas recebeu " + to_string(nargs) + " argumento(s)");
            analisarResto(n, 1);
            return Tipo::DESCONHECIDO;
        }
        condicao(*n.filhos[1], "while");
        for (size_t i = 2; i < n.filhos.size(); i++) {
            expr(*n.filhos[i]);
        }
        return Tipo::VOID;
    }

    // (begin expr...)
    Tipo formaBegin(const No& n) {
        if (n.filhos.size() < 2) {
            erro(n, "'begin' espera ao menos 1 expressao");
            return Tipo::DESCONHECIDO;
        }
        Tipo ultimo = Tipo::VOID;
        for (size_t i = 1; i < n.filhos.size(); i++) {
            ultimo = expr(*n.filhos[i]);
        }
        return ultimo;
    }

    // (op a b)
    Tipo operador(const No& n) {
        const string& o = n.filhos[0]->lexema;
        size_t nargs = n.filhos.size() - 1;
        bool relacional = (o == ">" || o == "<" || o == "==" || o == "!=" || o == "<=" || o == ">=");

        vector<Tipo> t;
        for (size_t i = 1; i < n.filhos.size(); i++) {
            t.push_back(exprComValor(*n.filhos[i], "operando de '" + o + "'"));
        }
        if (nargs != 2) {
            erro(n, "operador '" + o + "' espera 2 operandos, mas recebeu " + to_string(nargs));
            return Tipo::DESCONHECIDO;
        }

        bool ok = true;
        for (int i = 0; i < 2; i++) {
            const No& op = *n.filhos[i + 1];
            if (t[i] != Tipo::DESCONHECIDO && !ehNumerico(t[i])) {
                erro(op, "operando " + to_string(i + 1) + " de '" + o + "' deve ser numerico, mas e " + nomeTipo(t[i]));
                ok = false;
            } else if (o == "%" && t[i] == Tipo::FLOAT) {
                erro(op, "operador '%' exige operandos inteiros, mas o operando " + to_string(i + 1) + " e FLOAT");
                ok = false;
            }
        }
        if ((o == "/" || o == "%") && ehZeroLiteral(*n.filhos[2])) {
            erro(*n.filhos[2], "divisao por zero");
        }

        if (relacional) return Tipo::BOOL;
        if (!ok) return Tipo::DESCONHECIDO;

        if (t[0] == Tipo::DESCONHECIDO) return t[1];
        if (t[1] == Tipo::DESCONHECIDO) return t[0];
        return (t[0] == Tipo::FLOAT || t[1] == Tipo::FLOAT) ? Tipo::FLOAT : Tipo::INT;
    }

    // (f a1 ... an)
    Tipo chamada(const No& n) {
        const No& cabeca = *n.filhos[0];
        size_t nargs = n.filhos.size() - 1;

        vector<Tipo> targs;
        for (size_t i = 1; i < n.filhos.size(); i++) {
            targs.push_back(exprComValor(*n.filhos[i], "argumento de '" + cabeca.lexema + "'"));
        }

        int idx = buscar(cabeca.lexema);   // depois dos argumentos: eles podem ter declarado variaveis
        if (idx < 0) {
            erro(cabeca, "funcao '" + cabeca.lexema + "' nao declarada (defina com defun antes de chamar)");
            return Tipo::DESCONHECIDO;
        }
        const Simbolo& f = tabela[idx];
        if (f.cat != Categoria::FUNCAO) {
            erro(cabeca, "'" + cabeca.lexema + "' e uma " + nomeCategoria(f.cat) + ", nao uma funcao");
            return Tipo::DESCONHECIDO;
        }
        if ((int)nargs != f.aridade) {
            erro(n, "funcao '" + f.nome + "' espera " + to_string(f.aridade) + " argumento(s), mas recebeu " + to_string(nargs));
            return f.tipo;
        }
        for (size_t i = 0; i < nargs; i++) {
            if (!compativel(f.tiposParam[i], targs[i])) {
                erro(*n.filhos[i + 1], "argumento " + to_string(i + 1) + " de '" + f.nome + "': esperado " +
                                       nomeTipo(f.tiposParam[i]) + ", encontrado " + nomeTipo(targs[i]));
            }
        }
        return f.tipo;
    }

    // (defun nome (params...) corpo...)
    Tipo defun(const No& n) {
        const No& nomeNo = *n.cab[0];
        const string& nome = nomeNo.lexema;
        size_t nparams = n.cab.size() - 1;

        if (pilha.size() != 1) {
            erro(n, "'defun' so e permitido no nivel global (fora de funcoes e de let)");
        }

        // a funcao sempre vai para o escopo global, e antes do corpo, para permitir recursao.
        map<string, int>::const_iterator jaExiste = pilha[0].nomes.find(nome);
        bool duplicada = (jaExiste != pilha[0].nomes.end());
        if (duplicada) {
            erro(nomeNo, "'" + nome + "' ja foi declarado na linha " + to_string(tabela[jaExiste->second].linha));
        }
        int idxFn = novoSimbolo(0, nome, Categoria::FUNCAO, Tipo::DESCONHECIDO, nomeNo);
        tabela[idxFn].aridade = (int)nparams;
        tabela[idxFn].tiposParam = vector<Tipo>(nparams, Tipo::INT);
        if (!duplicada) registrar(0, idxFn);

        entrarFuncao(nome);
        int idxEsc = (int)pilha.size() - 1;
        for (size_t i = 1; i < n.cab.size(); i++) {
            const No& p = *n.cab[i];
            if (pilha[idxEsc].nomes.count(p.lexema)) {
                erro(p, "parametro '" + p.lexema + "' duplicado na funcao '" + nome + "'");
                continue;
            }
            int idx = novoSimbolo(idxEsc, p.lexema, Categoria::PARAMETRO, Tipo::INT, p);
            tabela[idx].desloc = -(int)nparams - PALAVRAS_LIGACAO + (int)(i - 1);
            registrar(idxEsc, idx);
        }

        Tipo retorno = Tipo::VOID;
        for (size_t i = 0; i < n.filhos.size(); i++) {
            retorno = expr(*n.filhos[i]);
        }

        int tam = sairFuncao();
        tabela[idxFn].tipo = retorno;
        tabela[idxFn].tamQuadro = tam;
        return Tipo::VOID;
    }

    // (let ((a e1) (b e2)) corpo...)
    Tipo let(const No& n) {
        // os valores sao avaliados FORA do escopo do let
        vector<Tipo> tipos;
        for (size_t i = 0; i < n.cab.size(); i++) {
            const No& b = *n.cab[i];
            tipos.push_back(exprComValor(*b.filhos[0], "valor de '" + b.lexema + "' no let"));
        }

        entrarLet();
        int idxEsc = (int)pilha.size() - 1;
        for (size_t i = 0; i < n.cab.size(); i++) {
            const No& b = *n.cab[i];
            if (pilha[idxEsc].nomes.count(b.lexema)) {
                erro(b, "variavel '" + b.lexema + "' repetida no mesmo let");
                continue;
            }
            int idx = novoSimbolo(idxEsc, b.lexema, Categoria::VARIAVEL, tipos[i], b);
            registrar(idxEsc, idx);
        }

        Tipo ultimo = Tipo::VOID;
        for (size_t i = 0; i < n.filhos.size(); i++) {
            ultimo = expr(*n.filhos[i]);
        }

        sairLet();
        return ultimo;
    }

public:
    // analisa o programa inteiro.
    void analisar(const No& programa) {
        Escopo global;
        global.caminho = "global";
        pilha.push_back(global);

        Quadro q;
        q.nivel = 0;
        q.proximo = 0;
        q.maximo = 0;
        quadros.push_back(q);

        for (size_t i = 0; i < programa.filhos.size(); i++) {
            expr(*programa.filhos[i]);
        }

        // erros em ordem de aparicao no texto
        stable_sort(listaErros.begin(), listaErros.end(),
                    [](const ErroSemantico& a, const ErroSemantico& b) {
                        return a.linha != b.linha ? a.linha < b.linha : a.coluna < b.coluna;
                    });
    }

    const vector<Simbolo>& simbolos() const { return tabela; }
    const vector<ErroSemantico>& erros() const { return listaErros; }
    int tamanhoQuadroGlobal() const { return quadros.empty() ? 0 : quadros[0].maximo; }
};

// imprime os erros semanticos e devolve quantos foram.
inline size_t imprimirErrosSemanticos(const vector<ErroSemantico>& erros) {
    if (erros.empty()) return 0;
    printf("%zu erro(s) semantico(s):\n", erros.size());
    for (size_t i = 0; i < erros.size(); i++) {
        printf("  ERRO SEMANTICO (linha %d, coluna %d): %s\n", erros[i].linha, erros[i].coluna, erros[i].msg.c_str());
    }
    return erros.size();
}

// imprime a tabela de simbolos.
inline void imprimirTabelaSimbolos(const vector<Simbolo>& tabela, int tamGlobal) {
    int wNome = 4, wEsc = 6;
    for (size_t i = 0; i < tabela.size(); i++) {
        wNome = max(wNome, (int)tabela[i].nome.size());
        wEsc = max(wEsc, (int)tabela[i].escopo.size());
    }

    printf("Tabela de Simbolos\n");
    printf("%-3s %-*s %-10s %-6s %-*s %-10s %-7s %s\n",
           "#", wNome, "Nome", "Categoria", "Tipo", wEsc, "Escopo", "Endereco", "Decl.", "Detalhes");

    for (size_t i = 0; i < tabela.size(); i++) {
        const Simbolo& s = tabela[i];

        char endereco[32];
        if (s.cat == Categoria::FUNCAO) snprintf(endereco, sizeof endereco, "-");
        else snprintf(endereco, sizeof endereco, "(%d,%d)", s.nivel, s.desloc);

        char decl[32];
        snprintf(decl, sizeof decl, "%d:%d", s.linha, s.coluna);

        string detalhes;
        if (s.cat == Categoria::FUNCAO) {
            detalhes = "aridade=" + to_string(s.aridade) + ", locais=" + to_string(s.tamQuadro) +
                       ", retorno=" + nomeTipo(s.tipo);
        }

        printf("%-3zu %-*s %-10s %-6s %-*s %-10s %-7s %s\n",
               i, wNome, s.nome.c_str(), nomeCategoria(s.cat), nomeTipo(s.tipo),
               wEsc, s.escopo.c_str(), endereco, decl, detalhes.c_str());
    }

    printf("\nQuadro global: %d posicao(oes) de variavel\n", tamGlobal);
}

#endif // SEMANTICO_HPP
