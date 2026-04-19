#pragma once
#include <cstdint>

enum TokenKind : uint32_t {
    TOKEN_EOF = 0,
    TOKEN_HASH,
    TOKEN_UNKNOWN,

    // Идентификаторы и литералы
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_CHAR,

    // Ключевые слова (добавим парочку для примера)
    TOKEN_KW_INT, TOKEN_KW_RETURN, TOKEN_KW_IF, TOKEN_KW_FOR, TOKEN_KW_WHILE, TOKEN_KW_VOID,

    // Операторы
    TOKEN_PLUS, TOKEN_PLUS_PLUS, TOKEN_PLUS_EQUAL,       // + ++ +=
    TOKEN_MINUS, TOKEN_MINUS_MINUS, TOKEN_MINUS_EQUAL,   // - -- -=
    TOKEN_STAR, TOKEN_STAR_EQUAL,                        // * *=
    TOKEN_SLASH, TOKEN_SLASH_EQUAL,                      // / /=
    TOKEN_PERCENT, TOKEN_PERCENT_EQUAL,                  // % %=
    TOKEN_ASSIGN, TOKEN_EQUAL,                           // = ==
    TOKEN_NOT, TOKEN_NOT_EQUAL,                          // ! !=
    TOKEN_LESS, TOKEN_LESS_EQUAL, TOKEN_LSHIFT,          // < <= <<
    TOKEN_GREATER, TOKEN_GREATER_EQUAL, TOKEN_RSHIFT,    // > >= >>

    // Пунктуация
    TOKEN_LPAREN, TOKEN_RPAREN,                          // ( )
    TOKEN_LBRACE, TOKEN_RBRACE,                          // { }
    TOKEN_LBRACKET, TOKEN_RBRACKET,                      // [ ]
    TOKEN_SEMICOLON, TOKEN_COMMA, TOKEN_DOT,              // ; , .

    TOKEN_NEWLINE,
    TOKEN_SYSTEM_INCLUDE
};

struct Token {
    TokenKind kind;
    uint32_t string_id; // ID из StringPool (или 0)

    // Упакованные координаты (чтобы влезть в 16 байт)
    uint32_t line;
    uint32_t column;

    // Флаги (например: 1 - это float, 2 - hex, 3 - содержит ошибки)
    uint32_t flags;
};
