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
    IF      if      WHILE       while     BEGIN   begin
    SET     set     PRINT   print
    DEFUN   defun   LET     let
    ID      [a-zA-Z_][a-zA-Z0-9_]*   (se estiver na tabela de reservadas vira palavra-chave)
    FIM  (fim da entrada)   ERRO (caractere/sequencia invalida)

Ignorados: espacos, tabs, quebras de linha e comentarios (';' ate o fim da linha).

Uso:
    ./lexico programa.lisp      (le de arquivo)
    ./lexico < programa.lisp    (le da entrada padrao)
*/

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
    int linha = 0;      // linha onde o token começa (1-based)
    int coluna = 0;     // coluna onde o token começa (1-based)
    string msg;         // mensagem de erro (só para tipo ERRO)
};

// 2- classe que representa o analisador léxico.
class AnalisadorLexico {
// uso interno do analex.
private:
    string entrada;                       // texto armazenado no buffer de entrada
    size_t pos = 0;                       // cabeça de leitura da string de entrada
    size_t inicio = 0;                    // posição do primeiro caractere do token atual
    map<string, TipoToken> reservadas;    // tabela de palavras reservadas

    // lê o próximo caracter e atualiza a posição da leitura.
    // Ao passar do fim devolve '\0' mas continua avançando pos (assim pos > size indica fim).
    char proxChar() {
        char c = (pos < entrada.size()) ? entrada[pos] : '\0';
        pos++;
        return c;
    }

    // desfaz a ultima leitura, fazendo o cursor recuar.
    void voltaUm() {
        if (pos > 0) {
            pos--;
        }
    }

    bool chegouAoFim() const {
        return pos > entrada.size();
    }

    static bool ehLetra(char c) {
        return isalpha((unsigned char)c) || c == '_';
    }

    static bool ehDigito(char c) {
        return isdigit((unsigned char)c);
    }

    static bool ehAlnum(char c) {
        return isalnum((unsigned char)c) || c == '_';
    }

    // converte uma posição do buffer em linha/coluna (1-based).
    void linhaColuna(size_t p, int& l, int& c) const {
        l = 1;
        c = 1;
        for (size_t i = 0; i < p && i < entrada.size(); i++) {
            if (entrada[i] == '\n') { 
                l++; 
                c = 1; 
            }
            else { 
                c++;
            }
        }
    }

    // cria um token já com linha/coluna do início do lexema.
    Token cria(TipoToken tipo, const string& lexema, const string& msg = "") {
        Token t;
        t.tipo = tipo;
        t.lexema = lexema;
        t.msg = msg;
        linhaColuna(inicio, t.linha, t.coluna);
        return t;
    }

    // consome o resto de um lexema malformado (letras, dígitos, '_' e '.') para não gerar uma cascata de erros em cima do mesmo pedaço de texto.
    Token malformado(string lexema, const string& msg) {
        while (true) {
            char c = proxChar();
            if (ehAlnum(c) || c == '.') {
                lexema += c;
            } else {
                voltaUm();
                break;
            }
        }
        return cria(ERRO, lexema, msg);
    }

// uso global.
public:
    // construtor guarda o texto e monta a tabela de reservadas.
    AnalisadorLexico(const string& texto) : entrada(texto) {
        reservadas["if"]    = IF;
        reservadas["while"] = WHILE;
        reservadas["begin"] = BEGIN;
        reservadas["set"]   = SET;
        reservadas["print"] = PRINT;
        reservadas["defun"] = DEFUN;
        reservadas["let"]   = LET;
    }

    // implementação do diagrama de transição.
    Token Analex() {
        int est = 1; // estado inicial do automato.
        string lexema = "";
        char ch;

        while (true) {
            ch = proxChar();

            switch (est)
            {
            // estado inicial.
            case 1:
                inicio = pos - 1;   // posição do caractere que acabou de ser lido
                if (chegouAoFim()) {
                    return cria(FIM, "");
                }
                else if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
                    // separador: ignora e permanece no estado 1
                    est = 1;
                }
                else if (ch == ';') {
                    est = 10;                // comentário até o fim da linha
                }
                else if (ehDigito(ch)) {
                    lexema += ch;
                    est = 2;                 // vai acumular um INT/FLOAT
                }
                else if (ehLetra(ch)) {
                    lexema += ch;
                    est = 9;                 // vai acumular um ID/palavra reservada
                }
                else if (ch == '+') { return cria(OP_SOMA, "+"); }
                else if (ch == '-') { return cria(OP_SUBT, "-"); }
                else if (ch == '*') { return cria(OP_MULT, "*"); }
                else if (ch == '/') { return cria(OP_DIVI, "/"); }
                else if (ch == '%') { return cria(OP_MOD,  "%"); }
                else if (ch == '(') { return cria(LPAREN,  "("); }
                else if (ch == ')') { return cria(RPAREN,  ")"); }
                else if (ch == '<') { est = 5; }   // precisa espiar +1 char
                else if (ch == '>') { est = 6; }   // precisa espiar +1 char
                else if (ch == '=') { est = 7; }   // precisa espiar +1 char
                else if (ch == '!') { est = 8; }   // precisa espiar +1 char
                else {
                    return cria(ERRO, string(1, ch),
                                string("caractere invalido '") + ch + "'");
                }
                break;

            // estado 2 - acumula números inteiros.
            case 2:
                if (ehDigito(ch)) {
                    lexema += ch;
                    est = 2;                 // continua acumulando digitos
                }
                else if (ch == '.') {
                    lexema += ch;
                    est = 3;                 // pode virar FLOAT
                }
                else if (ehLetra(ch)) {
                    // ex: "12abc"
                    return malformado(lexema + ch, "numero seguido de letra");
                }
                else {
                    voltaUm();               // devolve o caractere que nao e digito
                    return cria(INT, lexema);
                }
                break;

            // estado 3, após ler um "."
            case 3:
                if (ehDigito(ch)) {
                    lexema += ch;
                    est = 4;
                }
                else {
                    // ex: "12." sem digito depois
                    voltaUm();
                    return cria(ERRO, lexema,
                                "numero real malformado (esperava digito apos '.')");
                }
                break;

            // estado 4, acumula a parte decimal de um número real.
            case 4:
                if (ehDigito(ch)) {
                    lexema += ch;
                    est = 4;
                }
                else if (ch == '.') {
                    // ex: "1.2.3"
                    return malformado(lexema + ch, "numero real malformado (dois pontos)");
                }
                else if (ehLetra(ch)) {
                    // ex: "1.5x"
                    return malformado(lexema + ch, "numero seguido de letra");
                }
                else {
                    voltaUm();
                    return cria(FLOAT, lexema);
                }
                break;

            // estado 5 - leu "<"
            case 5:
                if (ch == '=') {
                    return cria(OP_MEI, "<=");
                }
                voltaUm();
                return cria(OP_ME, "<");

            // estado 6 - leu ">"
            case 6:
                if (ch == '=') {
                    return cria(OP_MAI, ">=");
                }
                voltaUm();
                return cria(OP_MA, ">");

            // estado 7 - leu "="
            case 7:
                if (ch == '=') {
                    return cria(OP_IG, "==");
                }
                voltaUm();
                return cria(ERRO, "=", "'=' sozinho nao eh um operador valido (use '==')");

            // estado 8 - leu "!"
            case 8:
                if (ch == '=') {
                    return cria(OP_DF, "!=");
                }
                voltaUm();
                return cria(ERRO, "!", "'!' sozinho nao eh um operador valido (use '!=')");

            // estado 9 - acumula identificador; no final consulta a tabela de reservadas.
            case 9:
                if (ehAlnum(ch)) {
                    lexema += ch;
                    est = 9;
                }
                else {
                    voltaUm();
                    map<string, TipoToken>::const_iterator it = reservadas.find(lexema);
                    if (it != reservadas.end()) {
                        return cria(it->second, lexema);
                    }
                    return cria(ID, lexema);
                }
                break;

            // estado 10 - comentário: descarta até o fim da linha.
            case 10:
                if (ch == '\n' || chegouAoFim()) {
                    voltaUm();               // o estado 1 trata '\n' / fim de arquivo
                    est = 1;
                }
                break;
            }
        }
    }
};

//3- main; lê a entrada (arquivo ou stdin) e chama Analex() até encontrar o FIM.
int main(int argc, char* argv[]) {
    string texto;

    if (argc > 1) {
        ifstream arq(argv[1]);
        if (!arq) {
            fprintf(stderr, "Nao foi possivel abrir o arquivo '%s'\n", argv[1]);
            return 2;
        }
        stringstream buffer;
        buffer << arq.rdbuf();
        texto = buffer.str();
    } else {
        stringstream buffer;
        buffer << cin.rdbuf();
        texto = buffer.str();
    }

    AnalisadorLexico lex(texto);
    vector<Token> erros;

    printf("%-6s %-4s %-10s %s\n", "Linha", "Col", "Token", "Lexema");
    printf("------------------------------------\n");

    Token t;
    do {
        t = lex.Analex();
        if (t.tipo == FIM) {
            printf("%-6d %-4d %-10s %s\n", t.linha, t.coluna, NOME_TOKEN[t.tipo], "(fim)");
        } else {
            printf("%-6d %-4d %-10s %s\n", t.linha, t.coluna, NOME_TOKEN[t.tipo], t.lexema.c_str());
            if (t.tipo == ERRO) {
                erros.push_back(t);
            }
        }
    } while (t.tipo != FIM);

    if (!erros.empty()) {
        printf("\n%zu erro(s) lexico(s):\n", erros.size());
        for (size_t i = 0; i < erros.size(); i++) {
            printf("  ERRO LEXICO (linha %d, coluna %d): %s [lexema: %s]\n",
                   erros[i].linha, erros[i].coluna,
                   erros[i].msg.c_str(), erros[i].lexema.c_str());
        }
        return 1;
    }

    printf("\nAnalise lexica concluida sem erros.\n");
    return 0;
}
