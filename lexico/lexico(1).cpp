/*
Analisador Lexico - Linguagem Mini-Lisp
Alunos:
    Oliver Neumann Brosselin

Tokens reconhecidos:
    INT   [0-9]+
    FLOAT  [0-9]+\.[0-9]+
    LPAREN    (      RPAREN    )
    OP_SOMA   +      OP_SUBT   -      OP_MULT   *      OP_DIVI   /
    OP_IG     ==     OP_DF     !=     OP_MOD    %
    OP_MA     >      OP_ME     <
    OP_MAI    >=     OP_MEI    <=
    IF      if      WHILE       while       BEGIN       begin      
    SET         set         PRINT       print
    DEFUN       defun       LET        let
    ID      [a-zA-Z_][a-zA-Z0-9_]*      se estiver na tabela de reservadas, torna-se uma reservada
    FIM  (fim da entrada)   ERRO (caractere/sequencia invalida)
*/

// #include <bits/stdc++.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cctype>
#include <cstdio>
using namespace std;

// 1- definição dos tipos de Tokens
enum TipoToken {
    INT, FLOAT, LPAREN, RPAREN,
    OP_SOMA, OP_SUBT, OP_MULT,
    OP_DIVI, OP_IG, OP_DF, OP_MOD,
    OP_MA, OP_ME, OP_MAI, OP_MEI,
    IF, WHILE, BEGIN, SET, PRINT,
    DEFUN, LET, ID, FIM, ERRO
};

// vetor estático com nomes legíveis na mesma ordem da enumeração.
static const char* NOME_TOKEN[] = {
    "INT", "FLOAT", "LPAREN", "RPAREN",
    "OP_SOMA", "OP_SUBT", "OP_MULT",
    "OP_DIVI", "OP_IG", "OP_DF", "OP_MOD",
    "OP_MA", "OP_ME", "OP_MAI", "OP_MEI",
    "IF", "WHILE", "BEGIN", "SET", "PRINT",
    "DEFUN", "LET", "ID", "FIM", "ERRO"
};

struct Token {
    TipoToken tipo;
    string lexema;
    int linha = 0;
    int coluna = 0;
    string msg; // mensagem de erro só para o token ERRO
};

// 2- classe que representa o analisador léxico.
class AnalisadorLexico{
//uso interno do analex.
private:
    string entrada;     //texto armazenado no buffer de entrada
    size_t pos = 0;     //cabeça de leitura da string de entrada
    size_t inicio = 0;  //posição do primeiro caracter em relação a string de entrada
    map<string, TipoToken> reservadas; //tabela de palavras reservadas

    // lê o próximo caracter e atualiza a posição da leitura.
    char proxChar(){
        char c;
        if (pos < entrada.size()){
            c = entrada[pos];
            pos++;
        }else {
            c = '\0';
        }
        return c;
    }

    // desfaz a ultima leitura , fazendo o cursor recuar.
    void voltaUm(){
        if (pos>0){
            pos--;
        }
    }

    void erro(const string& msg){
        cerr << "ERRO LEXICO: " << msg << " (posicao " << pos << ")\n";
    }
// uso global.
public:
    // construtor seta a string de entrada para um padrão utilizavel
    AnalisadorLexico(const string& texto) : entrada(texto + '\0') {}

    // implentação do diagrama de transição.
    Token Analex() {
        int est = 1; // estado inicial do automato.
        string lexema = "";
        char ch;

        while (true){
            ch = proxChar();

            switch (est)
            {
            // estado inicial.
            case 1:
                if (ch == ' ' || ch == '\t') {
                    // separador: ignora e permanece no estado 1
                    est = 1;
                }
                else if (ch == '\n' || ch == '\0') {
                    return { FIM, "" };
                }
                else if (isdigit(ch)) {
                    lexema += ch;
                    est = 2;                 // vai acumular um NUM_INT/NUM_REAL
                }
                else if (ch == '+') { 
                    return { OP_SOMA, "+" };
                }
                else if (ch == '-') {
                     return { OP_SUBT, "-" };
                }
                else if (ch == '*') {
                     return { OP_MULT, "*" }; 
                }
                else if (ch == '/') {
                     return { OP_DIVI, "/" };
                }
                else if (ch == '(') {
                     return { LPAREN,  "(" }; 
                }
                else if (ch == ')') {
                     return { RPAREN,  ")" };
                }
                else if (ch == '<') {
                     est = 5; 
                }             // precisa espiar +1 char
                else if (ch == '>') {
                     est = 6; 
                }             // precisa espiar +1 char
                else if (ch == '=') {
                     est = 7; 
                }             // precisa espiar +1 char
                else {
                    erro(string("caractere invalido '") + ch + "'");
                    return { ERRO, string(1, ch) };
                }
                break;
            //estado 2 - acumula números inteiros.
            case 2:
                if (isdigit(ch)) {
                    lexema += ch;
                    est = 2;                 // continua acumulando digitos
                }
                else if (ch == '.') {
                    lexema += ch;
                    est = 3;                 // pode virar NUM_REAL
                }
                else {
                    voltaUm();                // devolve o caractere que nao e digito
                    return { INT, lexema };
                }
                break;
            // estado 3, após ler um "."
            case 3:
                if (isdigit(ch)) {
                    lexema += ch;
                    est = 4;
                }
                else {
                    // ex: "12." sem digito depois -> erro (ERRO2 no slide)
                    erro("numero real malformado (esperava digito apos '.')");
                    return { ERRO, lexema };
                }
                break;
            // estado 4, acumula a parte decimal de um número real.
            case 4:
                if (isdigit(ch)) {
                    lexema += ch;
                    est = 4;
                }
                else {
                    voltaUm();
                    return { FLOAT, lexema };
                }
                break;
            // estado 5 - leu "<"                
            case 5:
                if (ch == '=') { 
                    return { OP_MEI, "<=" };
                }
                if (ch == '>') {
                    return { OP_DF,  "<>" }; 
                }
                voltaUm();
                return { OP_ME, "<" };
            // estado 6 - leu ">"
            case 6:
                if (ch == '=') { 
                    return { OP_MAI, ">=" }; 
                }
                voltaUm();
                return { OP_MA, ">" };
            // estado 7 - leu "="
            case 7:
                if (ch == '=') { 
                    return { OP_IG, "==" }; 
                }
                erro("'=' sozinho nao eh um operador valido (use '==')");
                voltaUm();
                return { ERRO, "=" };
            }
        }
    }
};

//3- main; lê a entrada e chama Analex() até encontrar o FIM.
int main(){
    // faz a leitura de toda a entrada para dentro de uma variável string
    printf("Digite seu texto de entrada: ");
    stringstream buffer;
    string texto;
    getline(cin, texto);

    AnalisadorLexico lex(texto);

    printf("%-10s %s\n", "Lexema", "Token");
    printf("----------------------\n");

    Token t;
    do {
        t = lex.Analex();
        if (t.tipo == FIM) {
            printf("%-10s %s \n", "(fim)", NOME_TOKEN[t.tipo]);
        }else{
            printf("%-10s %s\n", t.lexema.c_str(), NOME_TOKEN[t.tipo]);
        }
    } while (t.tipo != FIM);

    return 0;
}
