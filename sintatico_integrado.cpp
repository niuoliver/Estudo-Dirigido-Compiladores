/*
Analisador Sintatico - Linguagem Mini-Lisp
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

Pipeline: o lexico (lexico.hpp) le o texto inteiro e produz a lista de tokens;
se houver erro lexico o sintatico nem comeca. Senao, prox() entrega os tokens ao
parser, um por vez, ja convertidos para os terminais da gramatica (TokenType).

Uso:
    g++ -std=c++17 -Wall -Wextra -o sintatico sintatico.cpp
    ./sintatico programa.lisp      (le de arquivo)
    ./sintatico < programa.lisp    (le da entrada padrao)

Codigo de saida: 0 = entrada valida, 1 = erro lexico ou sintatico, 2 = arquivo nao aberto.
*/
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include "lexico_integrado.hpp"
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
TokenSint prox() {
    const Token& t = tokens[posToken];
    if (t.tipo != FIM) {
        posToken++;
    }
    return { categoria(t.tipo), t.lexema, t.linha, t.coluna };
}

TokenSint token;   // token atual (lookahead)

// ---------------------------------------------------------------------------
// declarações das funções do parser
// ---------------------------------------------------------------------------
void PROGRAM();
void EXPR_LIST();
void EXPR();
void LIST_TAIL();
void ATOM();
void NUMBER();
void SYMBOL();
void PARAM_LIST();
void BINDING_LIST();
void BINDING();

[[noreturn]] void Erro(const string& mensagem) {
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

void Accept(TokenType esperado, const string& mensagem) {
    if (token.tipo == esperado) {
        token = prox();
    } else {
        Erro(mensagem);
    }
}

void PROGRAM(){
    EXPR_LIST();

    if (token.tipo == TokenType::RPAREN) {
        Erro("')' sem '(' correspondente");
    }
    if (token.tipo != TokenType::END) {
        Erro("Esperado fim de entrada");
    }
}

void EXPR_LIST(){
     if (token.tipo == TokenType::RPAREN || token.tipo == TokenType::END) {
        // EXPR_LIST -> epsilon   (FOLLOW(expr_list) = { ")", "$" })
        return;
     }

     EXPR();
     EXPR_LIST();
}

void EXPR(){
    if (token.tipo == TokenType::LPAREN) {
        Accept(TokenType::LPAREN, "Esperado (");
        LIST_TAIL();
    } else {
        ATOM();
    }
}

void LIST_TAIL(){
    if (token.tipo == TokenType::DEFUN) {
        Accept(TokenType::DEFUN, "Esperado defun");
        Accept(TokenType::ID, "Esperado nome da funcao");
        Accept(TokenType::LPAREN, "Esperado ( antes dos parametros");

        PARAM_LIST();

        Accept(TokenType::RPAREN, "Esperado ) apos os parametros");

        EXPR_LIST();

        Accept(TokenType::RPAREN, "Esperado ) apos corpo da funcao");
    } else if (token.tipo == TokenType::LET) {
        Accept(TokenType::LET, "Esperado let");
        Accept(TokenType::LPAREN, "Esperado ( antes das declaracoes");

        BINDING_LIST();

        Accept(TokenType::RPAREN, "Esperado ) apos as declaracoes");

        EXPR_LIST();

        Accept(TokenType::RPAREN, "Esperado ) apos o corpo do let");
    } else {
        EXPR_LIST();

        Accept(TokenType::RPAREN, "Esperado ) para fechar a lista");
    }
}

void ATOM(){
    if (token.tipo == TokenType::INT || token.tipo == TokenType::FLOAT) {
        NUMBER();
    } else {
        SYMBOL();
    }
}

void NUMBER(){
    if (token.tipo == TokenType::INT) {
        Accept(TokenType::INT, "Esperado inteiro");
    } else if (token.tipo == TokenType::FLOAT) {
        Accept(TokenType::FLOAT, "Esperado numero real");
    } else {
        Erro("Esperado um numero inteiro ou real");
    }
}

void SYMBOL(){
    if (token.tipo == TokenType::OP) {
        Accept(TokenType::OP, "Esperado operador");
    } else if (token.tipo == TokenType::KW) {
        Accept(TokenType::KW, "Esperada palavra-chave");
    } else if (token.tipo == TokenType::ID) {
        Accept(TokenType::ID, "Esperado identificador");
    } else {
        Erro("Esperado operador, palavra-chave ou identificador");
    }
}

void PARAM_LIST(){
    if (token.tipo == TokenType::ID) {
        Accept(TokenType::ID, "Esperado identificador");
        PARAM_LIST();
    }
    // caso contrario deriva em epsilon
}

void BINDING_LIST(){
    if (token.tipo == TokenType::LPAREN) {
        BINDING();
        BINDING_LIST();
    }
    // caso contrario deriva em epsilon
}

void BINDING(){
    Accept(TokenType::LPAREN, "Esperado ( no binding");
    Accept(TokenType::ID, "Esperado nome da variavel");
    EXPR();
    Accept(TokenType::RPAREN, "Esperado ) no binding");
}

// ---------------------------------------------------------------------------
// main: lê o texto, roda o léxico, mostra os tokens e, se não houver erro léxico, o parser.
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]){
    string texto;
    stringstream buffer;

    if (argc > 1) {
        ifstream arq(argv[1]);
        if (!arq) {
            fprintf(stderr, "Nao foi possivel abrir o arquivo '%s'\n", argv[1]);
            return 2;
        }
        buffer << arq.rdbuf();
    } else {
        buffer << cin.rdbuf();
    }
    texto = buffer.str();

    // 1- análise léxica sobre o texto inteiro
    tokens = tokenizar(texto);
    imprimirTokens(tokens);

    if (imprimirErrosLexicos(tokens) > 0) {
        printf("\nAnalise sintatica nao executada: corrija os erros lexicos.\n");
        return 1;
    }

    // 2- análise sintática
    token = prox();

    PROGRAM();

    cout << "\nEntrada sintaticamente valida" << endl;

    return 0;
}
