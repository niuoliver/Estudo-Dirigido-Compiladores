/*
Compilador Mini-Lisp -> MEPA  (fases ate agora: lexica, sintatica e semantica)
Alunos:
    Oliver Neumann Brosselin

Pipeline:  texto -> lexico (tokens) -> sintatico (AST) -> semantico (tabela de simbolos)

Uso:
    g++ -std=c++17 -Wall -Wextra -o minilisp minilisp.cpp
    ./minilisp [opcoes] programa.lisp        (ou ./minilisp < programa.lisp)

Opcoes:
    --sem-tokens   nao imprime a lista de tokens
    --ast          imprime a arvore sintatica
    --so-sintatico para depois da analise sintatica

Codigo de saida: 0 = sem erros, 1 = erro lexico, sintatico ou semantico, 2 = uso incorreto / arquivo nao abre.
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
#include "lexico.hpp"
#include "ast.hpp"
#include "sintatico.hpp"
#include "semantico.hpp"
using namespace std;

static void uso(const char* prog) {
    fprintf(stderr, "Uso: %s [--sem-tokens] [--ast] [--so-sintatico] [arquivo.lisp]\n", prog);
}

int main(int argc, char* argv[]) {
    bool mostrarTokens = true;
    bool mostrarArvore = false;
    bool soSintatico = false;
    const char* caminho = nullptr;

    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "--sem-tokens")        mostrarTokens = false;
        else if (a == "--ast")          mostrarArvore = true;
        else if (a == "--so-sintatico") soSintatico = true;
        else if (a.size() > 1 && a[0] == '-' && a[1] == '-') { uso(argv[0]); return 2; }
        else                            caminho = argv[i];
    }

    // leitura do texto-fonte
    stringstream buffer;
    if (caminho != nullptr) {
        ifstream arq(caminho);
        if (!arq) {
            fprintf(stderr, "Nao foi possivel abrir o arquivo '%s'\n", caminho);
            return 2;
        }
        buffer << arq.rdbuf();
    } else {
        buffer << cin.rdbuf();
    }
    string texto = buffer.str();

    // 1- analise lexica
    printf("=== ANALISE LEXICA ===\n");
    vector<Token> lista = tokenizar(texto);
    if (mostrarTokens) {
        imprimirTokens(lista);
    }
    if (imprimirErrosLexicos(lista) > 0) {
        printf("\nAnalise interrompida: corrija os erros lexicos.\n");
        return 1;
    }
    printf("Analise lexica concluida sem erros.\n");

    // 2- analise sintatica (em caso de erro, encerra com a mensagem em stderr)
    printf("\n=== ANALISE SINTATICA ===\n");
    NoPtr raiz = analisarSintaxe(lista);
    printf("Analise sintatica concluida: entrada valida.\n");
    if (mostrarArvore) {
        printf("\nArvore sintatica:\n");
        imprimirArvore(*raiz);
    }
    if (soSintatico) {
        return 0;
    }

    // 3- analise semantica
    printf("\n=== ANALISE SEMANTICA ===\n");
    AnalisadorSemantico sem;
    sem.analisar(*raiz);
    if (imprimirErrosSemanticos(sem.erros()) > 0) {
        printf("\nAnalise interrompida: corrija os erros semanticos.\n");
        return 1;
    }
    printf("Analise semantica concluida sem erros.\n\n");
    imprimirTabelaSimbolos(sem.simbolos(), sem.tamanhoQuadroGlobal());

    return 0;
}
