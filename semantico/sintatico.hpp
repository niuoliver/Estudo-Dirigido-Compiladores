/*
Analisador Sintatico - Linguagem Mini-Lisp  (cabecalho; antes era sintatico.cpp)
Alunos:
    Oliver Neumann Brosselin

Descendente recursivo (LL(1)) sobre a gramatica fatorada G':
    program      -> expr_list
    expr_list    -> expr expr_list | epsilon
    expr         -> atom | "(" list_tail
    list_tail    -> "defun" id "(" param_list ")" expr_list ")"
                  | "let" "(" binding_list ")" expr_list ")"
                  | expr_list ")"
    atom         -> number | symbol
    number       -> int | float
    symbol       -> op | kw | id
    param_list   -> id param_list | epsilon
    binding_list -> binding binding_list | epsilon
    binding      -> "(" id expr ")"

Nao terminais a esquerda viram funcoes, nao terminais a direita viram chamadas de funcoes.
Terminais viram chamadas a Accept().

Diferenca em relacao ao validador anterior: cada funcao agora devolve um no da AST (ast.hpp)
em vez de so verificar a entrada. As regras e as mensagens de erro sao as mesmas.

Ponto de entrada:  NoPtr analisarSintaxe(const vector<Token>& lista)
    Recebe a lista de tokens do lexico (sem erros lexicos) e devolve a raiz da arvore.
    Em caso de erro sintatico imprime a mensagem em stderr e encerra com exit(1).
*/

#ifndef SINTATICO_HPP
#define SINTATICO_HPP

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include "lexico.hpp"
#include "ast.hpp"
using namespace std;

// terminais da gramatica (o lexico tem tokens mais finos; veja categoria()).
enum class TokenType {
    DEFUN, LET, ID,
    INT, FLOAT, OP,
    KW, LPAREN, RPAREN,
    END, INVALID
};

// representação do token atual, na visão do parser.
struct TokenSint {
    TokenType tipo;
    string lexema;
    int linha;
    int coluna;
};

// ---------------------------------------------------------------------------
// interface com o analisador léxico
// ---------------------------------------------------------------------------

// tokens produzidos pelo léxico e posição do próximo a ser entregue.
static vector<Token> tokens;
static size_t posToken = 0;

// converte o token fino do léxico no terminal da gramática.
static TokenType categoria(TipoToken t) {
    switch (t) {
        case INT:    return TokenType::INT;
        case FLOAT:  return TokenType::FLOAT;
        case LPAREN: return TokenType::LPAREN;
        case RPAREN: return TokenType::RPAREN;
        case OP_SOMA: case OP_SUBT: case OP_MULT: case OP_DIVI: case OP_MOD:
        case OP_IG:   case OP_DF:
        case OP_MA:   case OP_ME:   case OP_MAI:  case OP_MEI:
            return TokenType::OP;
        case IF: case WHILE: case BEGIN: case SET: case PRINT:
            return TokenType::KW;
        case DEFUN:  return TokenType::DEFUN;
        case LET:    return TokenType::LET;
        case ID:     return TokenType::ID;
        case FIM:    return TokenType::END;
        case ERRO:
        default:     return TokenType::INVALID;
    }
}

// retorna o próximo token da entrada. Depois do FIM, continua devolvendo FIM.
static TokenSint prox() {
    const Token& t = tokens[posToken];
    if (t.tipo != FIM) {
        posToken++;
    }
    return { categoria(t.tipo), t.lexema, t.linha, t.coluna };
}

static TokenSint token;   // token atual (lookahead)

// ---------------------------------------------------------------------------
// declarações das funções do parser
// ---------------------------------------------------------------------------
static NoPtr PROGRAM();
static void  EXPR_LIST(vector<NoPtr>& destino);
static NoPtr EXPR();
static NoPtr LIST_TAIL(const TokenSint& abre);
static NoPtr ATOM();
static NoPtr NUMBER();
static NoPtr SYMBOL();
static void  PARAM_LIST(vector<NoPtr>& destino);
static void  BINDING_LIST(vector<NoPtr>& destino);
static NoPtr BINDING();

[[noreturn]] static void Erro(const string& mensagem) {
    fflush(stdout);
    cerr << "Erro sintatico (linha " << token.linha << ", coluna " << token.coluna << "): "
         << mensagem << ". Token encontrado: ";
    if (token.tipo == TokenType::END) {
        cerr << "fim da entrada";
    } else {
        cerr << "'" << token.lexema << "'";
    }
    cerr << endl;

    exit(EXIT_FAILURE);
}

static void Accept(TokenType esperado, const string& mensagem) {
    if (token.tipo == esperado) {
        token = prox();
    } else {
        Erro(mensagem);
    }
}

static NoPtr PROGRAM(){
    NoPtr raiz = novoNo(NoTipo::PROGRAMA, "", 1, 1);

    EXPR_LIST(raiz->filhos);

    if (token.tipo == TokenType::RPAREN) {
        Erro("')' sem '(' correspondente");
    }
    if (token.tipo != TokenType::END) {
        Erro("Esperado fim de entrada");
    }
    return raiz;
}

static void EXPR_LIST(vector<NoPtr>& destino){
     if (token.tipo == TokenType::RPAREN || token.tipo == TokenType::END) {
        // EXPR_LIST -> epsilon   (FOLLOW(expr_list) = { ")", "$" })
        return;
     }

     destino.push_back(EXPR());
     EXPR_LIST(destino);
}

static NoPtr EXPR(){
    if (token.tipo == TokenType::LPAREN) {
        TokenSint abre = token;     // a lista herda a posicao do '('
        Accept(TokenType::LPAREN, "Esperado (");
        return LIST_TAIL(abre);
    }
    return ATOM();
}

static NoPtr LIST_TAIL(const TokenSint& abre){
    if (token.tipo == TokenType::DEFUN) {
        NoPtr n = novoNo(NoTipo::DEFUN, "defun", abre.linha, abre.coluna);

        Accept(TokenType::DEFUN, "Esperado defun");

        TokenSint nome = token;
        Accept(TokenType::ID, "Esperado nome da funcao");
        n->cab.push_back(novoNo(NoTipo::ID, nome.lexema, nome.linha, nome.coluna));

        Accept(TokenType::LPAREN, "Esperado ( antes dos parametros");

        PARAM_LIST(n->cab);

        Accept(TokenType::RPAREN, "Esperado ) apos os parametros");

        EXPR_LIST(n->filhos);

        Accept(TokenType::RPAREN, "Esperado ) apos corpo da funcao");
        return n;
    } else if (token.tipo == TokenType::LET) {
        NoPtr n = novoNo(NoTipo::LET, "let", abre.linha, abre.coluna);

        Accept(TokenType::LET, "Esperado let");
        Accept(TokenType::LPAREN, "Esperado ( antes das declaracoes");

        BINDING_LIST(n->cab);

        Accept(TokenType::RPAREN, "Esperado ) apos as declaracoes");

        EXPR_LIST(n->filhos);

        Accept(TokenType::RPAREN, "Esperado ) apos o corpo do let");
        return n;
    } else {
        NoPtr n = novoNo(NoTipo::LISTA, "", abre.linha, abre.coluna);

        EXPR_LIST(n->filhos);

        Accept(TokenType::RPAREN, "Esperado ) para fechar a lista");
        return n;
    }
}

static NoPtr ATOM(){
    if (token.tipo == TokenType::INT || token.tipo == TokenType::FLOAT) {
        return NUMBER();
    }
    return SYMBOL();
}

static NoPtr NUMBER(){
    TokenSint t = token;
    if (token.tipo == TokenType::INT) {
        Accept(TokenType::INT, "Esperado inteiro");
        return novoNo(NoTipo::INT, t.lexema, t.linha, t.coluna);
    } else if (token.tipo == TokenType::FLOAT) {
        Accept(TokenType::FLOAT, "Esperado numero real");
        return novoNo(NoTipo::FLOAT, t.lexema, t.linha, t.coluna);
    }
    Erro("Esperado um numero inteiro ou real");
}

static NoPtr SYMBOL(){
    TokenSint t = token;
    if (token.tipo == TokenType::OP) {
        Accept(TokenType::OP, "Esperado operador");
        return novoNo(NoTipo::OP, t.lexema, t.linha, t.coluna);
    } else if (token.tipo == TokenType::KW) {
        Accept(TokenType::KW, "Esperada palavra-chave");
        return novoNo(NoTipo::KW, t.lexema, t.linha, t.coluna);
    } else if (token.tipo == TokenType::ID) {
        Accept(TokenType::ID, "Esperado identificador");
        return novoNo(NoTipo::ID, t.lexema, t.linha, t.coluna);
    }
    Erro("Esperado operador, palavra-chave ou identificador");
}

static void PARAM_LIST(vector<NoPtr>& destino){
    if (token.tipo == TokenType::ID) {
        TokenSint t = token;
        Accept(TokenType::ID, "Esperado identificador");
        destino.push_back(novoNo(NoTipo::ID, t.lexema, t.linha, t.coluna));
        PARAM_LIST(destino);
    }
    // caso contrario deriva em epsilon
}

static void BINDING_LIST(vector<NoPtr>& destino){
    if (token.tipo == TokenType::LPAREN) {
        destino.push_back(BINDING());
        BINDING_LIST(destino);
    }
    // caso contrario deriva em epsilon
}

static NoPtr BINDING(){
    Accept(TokenType::LPAREN, "Esperado ( no binding");

    TokenSint nome = token;
    Accept(TokenType::ID, "Esperado nome da variavel");
    NoPtr b = novoNo(NoTipo::BINDING, nome.lexema, nome.linha, nome.coluna);

    b->filhos.push_back(EXPR());

    Accept(TokenType::RPAREN, "Esperado ) no binding");
    return b;
}

// ---------------------------------------------------------------------------
// ponto de entrada do sintatico
// ---------------------------------------------------------------------------
inline NoPtr analisarSintaxe(const vector<Token>& lista) {
    tokens = lista;
    posToken = 0;
    token = prox();
    return PROGRAM();
}

#endif // SINTATICO_HPP
