/*
Arvore sintatica (AST) do Mini-Lisp
Alunos:
    Oliver Neumann Brosselin

O parser (sintatico.hpp) monta esta arvore e o analisador semantico (semantico.hpp) a percorre.
Cada no guarda a posicao (linha/coluna) do token que o originou, para as mensagens de erro.

Forma de cada tipo de no:
    PROGRAMA  filhos = expressoes de nivel global
    INT       lexema = "10"
    FLOAT     lexema = "3.14"
    OP        lexema = "+", "<=", ...        (so aparece como 1o elemento de uma lista)
    KW        lexema = "if", "set", ...      (so aparece como 1o elemento de uma lista)
    ID        lexema = nome
    LISTA     filhos = elementos da lista; posicao = a do '('
    DEFUN     cab    = [ID nome, ID param1, ID param2, ...]
              filhos = expressoes do corpo
    LET       cab    = [BINDING, BINDING, ...]
              filhos = expressoes do corpo
    BINDING   lexema = nome da variavel; filhos[0] = expressao do valor
*/

#ifndef AST_HPP
#define AST_HPP

#include <string>
#include <vector>
#include <memory>
#include <cstdio>
using namespace std;

enum class NoTipo { PROGRAMA, INT, FLOAT, OP, KW, ID, LISTA, DEFUN, LET, BINDING };

struct No;
typedef unique_ptr<No> NoPtr;

struct No {
    NoTipo tipo;
    string lexema;
    int linha;
    int coluna;
    vector<NoPtr> cab;      // cabecalho: nome+parametros (DEFUN) ou bindings (LET)
    vector<NoPtr> filhos;   // elementos (LISTA/PROGRAMA), corpo (DEFUN/LET), valor (BINDING)

    No(NoTipo t, const string& lex, int l, int c)
        : tipo(t), lexema(lex), linha(l), coluna(c) {}
};

inline NoPtr novoNo(NoTipo t, const string& lexema, int linha, int coluna) {
    return make_unique<No>(t, lexema, linha, coluna);
}

inline const char* nomeNoTipo(NoTipo t) {
    switch (t) {
        case NoTipo::PROGRAMA: return "PROGRAMA";
        case NoTipo::INT:      return "INT";
        case NoTipo::FLOAT:    return "FLOAT";
        case NoTipo::OP:       return "OP";
        case NoTipo::KW:       return "KW";
        case NoTipo::ID:       return "ID";
        case NoTipo::LISTA:    return "LISTA";
        case NoTipo::DEFUN:    return "DEFUN";
        case NoTipo::LET:      return "LET";
        case NoTipo::BINDING:  return "BINDING";
    }
    return "?";
}

// imprime a arvore com indentacao (uma linha por no), para depuracao e para o caderno.
inline void imprimirArvore(const No& n, int nivel = 0) {
    printf("%*s%s", nivel * 2, "", nomeNoTipo(n.tipo));
    if (!n.lexema.empty()) {
        printf(" %s", n.lexema.c_str());
    }
    printf("  [%d:%d]\n", n.linha, n.coluna);

    if (!n.cab.empty()) {
        printf("%*s%s\n", (nivel + 1) * 2, "",
               n.tipo == NoTipo::DEFUN ? "(nome e parametros)" : "(bindings)");
        for (size_t i = 0; i < n.cab.size(); i++) {
            imprimirArvore(*n.cab[i], nivel + 2);
        }
    }
    if (!n.filhos.empty() && (n.tipo == NoTipo::DEFUN || n.tipo == NoTipo::LET)) {
        printf("%*s(corpo)\n", (nivel + 1) * 2, "");
        for (size_t i = 0; i < n.filhos.size(); i++) {
            imprimirArvore(*n.filhos[i], nivel + 2);
        }
    } else {
        for (size_t i = 0; i < n.filhos.size(); i++) {
            imprimirArvore(*n.filhos[i], nivel + 1);
        }
    }
}

#endif // AST_HPP
