/* Não terminais à esquerda viram funções, não terminais à direita viram chamdas de funções. */
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include "lexico_claude.cpp"
using namespace std;

// tipode de tokens que o lexer pode retornar.
enum class TokenType {
    DEFUN, LET, ID,
    INT, FLOAT, OP,
    KW, LPAREN, RPAREN,
    END, INVALID
};

// representação do token atual
struct Token {
    TokenType tipo;
    string lexema;
};

// interface com o analisador léxico
// deve retornar o próximo token da entrada.
Token prox();
Token token;

// declarações das funções do parcer
void Erro(const string& mensagem){
    cerr << "Erro sintatico: "<< mensagem
    << ". Token encontrado: " << token.lexema << endl;

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

    if (token.tipo != TokenType::END) {
        Erro("Esperado fim de entrada.");
    }
}

void EXPR_LIST(){
     if (token.tipo == TokenType::RPAREN || token.tipo == TokenType::END) {
        // EXPR_LIST -> epsilom
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
        Accept(TokenType::ID, "Esperado nome da função");
        Accept(TokenType::LPAREN, "Esperado (");

        PARAM_LIST();

        Accept(TokenType::RPAREN, "Esperado ) apos os parametros");

        EXPR_LIST();

        Accept(TokenType::RPAREN, "Esperado ) apos corpo da funcao");
    } else if (token.tipo == TokenType::LET) {
        Accept(TokenType::LET, "Esperado let");
        Accept(TokenType::LPAREN, "Esperado (");

        BINDING_LIST();

        Accept(TokenType::RPAREN, "Esperado ) apos as declaracoes");

        EXPR_LIST();

        Accept(TokenType::RPAREN, "Esperado ) apos o corpo let");
    } else {
        EXPR_LIST();

        Accept(TokenType::RPAREN, "Esperado ) para fecahr a lista");
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
    Accept(TokenType::ID, "Esprado nome da variavel");
    EXPR();
    Accept(TokenType::RPAREN, "Esperado ) no binding");
}

int main(){
    token = prox();

    PROGRAM();

    cout << "Entrada sintaticamente valida" << endl;
    
    return 0;
}