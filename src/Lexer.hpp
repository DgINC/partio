#pragma once

#include <cctype>
#include <cstdint>
#include <string_view>

#include "BinaryKeywordTable.hpp"
#include "ITokenStream.hpp"
#include "SourceBuffer.hpp"
#include "StringPool.hpp"
#include "Token.hpp"

enum class LexerMode { Standard, Directive };

class Lexer : public ITokenStream {
    const uint8_t* buffer;
    const uint8_t* cursor;
    const uint8_t* end;

    uint32_t line = 1;
    uint32_t col = 1;

    const std::weak_ptr<StringPool> pool;
    LexerMode mode = LexerMode::Standard;

public:
    Lexer(const SourceBuffer* sb, const std::weak_ptr<StringPool>& p)
        : buffer(sb->data), cursor(sb->data), end(sb->data + sb->size), pool(p) {}

    void set_mode(const LexerMode m) { mode = m; }

    Token next_token() override {
        skip_whitespace_and_comments();

        if (cursor >= end) return { TOKEN_EOF, 0, line, col, 0 };

        const uint8_t* start = cursor;
        const uint32_t start_col = col;
        const char c = peek();

        // Режим препроцессора для <system_headers.h>
        if (mode == LexerMode::Directive && c == '<') {
            return lex_system_include();
        }

        // Строки "local.h"
        if (c == '"') return lex_string();

        // Идентификаторы и Ключевые слова
        if (std::isalpha(c) || c == '_') return lex_identifier(start, start_col);

        // Числа
        if (std::isdigit(c)) return lex_number(start, start_col);

        // Одиночные символы и операторы
        advance();
        switch (c) {
            case '\n':
                line++; col = 1;
                // Если мы были в режиме директивы, она заканчивается на новой строке
                if (mode == LexerMode::Directive) {
                    mode = LexerMode::Standard;
                    return { TOKEN_NEWLINE, 0, line - 1, start_col, 1 };
                }
                return next_token(); // В обычном режиме просто скипаем \n
            case '#': return { TOKEN_HASH, 0, line, start_col, 1 };
            case '{': return { TOKEN_LBRACE, 0, line, start_col, 1 };
            case '}': return { TOKEN_RBRACE, 0, line, start_col, 1 };
            case ';': return { TOKEN_SEMICOLON, 0, line, start_col, 1 };
            case '<': return { TOKEN_LESS, 0, line, start_col, 1 };
            case '>': return { TOKEN_GREATER, 0, line, start_col, 1 };
            default: return { TOKEN_UNKNOWN, 0, line, start_col, 1 };
        }

        //return { TOKEN_UNKNOWN, 0, line, start_col, 1 };
    }

private:
    char peek() const { return cursor < end ? *cursor : '\0'; }

    void advance() {
        cursor++;
        col++;
    }

    void skip_whitespace_and_comments() {
        while (cursor < end) {
            if (const char c = peek(); c == ' ' || c == '\t' || c == '\r') {
                advance();
            } else if (c == '/' && cursor + 1 < end && cursor[1] == '/') {
                // Однострочный комментарий
                while (cursor < end && peek() != '\n') advance();
            } else {
                break;
            }
        }
    }

    Token lex_identifier(const uint8_t* start, const uint32_t start_col) {
        while (cursor < end && (std::isalnum(peek()) || peek() == '_')) {
            advance();
        }
        const std::string_view text(reinterpret_cast<const char*>(start), cursor - start);

        const TokenKind kind = KEYWORDS.lookup(text);
        uint32_t id = 0;
        {
            const auto& p = pool.lock();
            id = p->intern(text);
        }

        return { kind, id, line, start_col, static_cast<uint32_t>(text.length()) };
    }

    Token lex_system_include() {
        const uint32_t start_col = col;
        advance(); // <
        const uint8_t* start = cursor;
        while (cursor < end && peek() != '>' && peek() != '\n') advance();

        const std::string_view path(reinterpret_cast<const char*>(start), cursor - start);
        if (peek() == '>') advance();

        mode = LexerMode::Standard; // Авто-сброс после прочтения пути
        uint32_t id = 0;
        {
            const auto& p = pool.lock();
            id = p->intern(path);
        }
        return { TOKEN_SYSTEM_INCLUDE, id, line, start_col, static_cast<uint32_t>(path.length()) + 2 };
    }

    Token lex_string() {
        const uint32_t start_col = col;
        advance(); // "
        const uint8_t* start = cursor;
        while (cursor < end && peek() != '"' && peek() != '\n') advance();

        const std::string_view text(reinterpret_cast<const char*>(start), cursor - start);
        if (peek() == '"') advance();

        uint32_t id = 0;
        {
            const auto& p = pool.lock();
            id = p->intern(text);
        }

        return { TOKEN_STRING, id, line, start_col, static_cast<uint32_t>(text.length()) + 2 };
    }

    Token lex_number(const uint8_t* start, const uint32_t start_col) {
        while (cursor < end && std::isdigit(peek())) advance();
        const std::string_view text(reinterpret_cast<const char*>(start), cursor - start);
        uint32_t id = 0;
        {
            const auto& p = pool.lock();
            id = p->intern(text);
        }
        return { TOKEN_NUMBER, id, line, start_col, static_cast<uint32_t>(text.length()) };
    }
};