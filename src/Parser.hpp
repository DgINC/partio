#pragma once

#include "Arena.hpp"
#include "ASTNodes.hpp"
#include "Preprocessor.hpp"
#include "StringPool.hpp"
#include "ITokenStream.hpp"


class Parser {
    ITokenStream& stream;
    StringPool& pool;
    Arena& arena;
    Token current_tok;

    // Читаем следующий токен
    void advance() {
        current_tok = stream.next_token();
    }

    // Проверяем токен и съедаем его, если он совпал. Иначе - кидаем ошибку.
    bool expect_and_consume(const TokenKind kind) {
        if (current_tok.kind == kind) {
            advance();
            return true;
        }
        // В реальности тут мы аккуратно логируем ошибку и пытаемся восстановиться
        throw std::runtime_error("Синтаксическая ошибка: неожиданный токен!");
    }

public:
    Parser(ITokenStream &s, StringPool &p, Arena& a) : stream(s), pool(p), arena(a) {
        advance(); // Заряжаем первый токен в ствол
    }

    TranslationUnitNode* parse_translation_unit() {
        // Создаем корень в Арене, передаем её же как memory_resource для вектора
        auto* root = arena.make<TranslationUnitNode>(&arena);

        while (current_tok.kind != TOKEN_EOF) {
            if (current_tok.kind == TOKEN_HASH) {
                parse_directive();
            } else if (current_tok.kind == TOKEN_NEWLINE) {
                advance(); // Скипаем пустые строки
            } else if (is_type_token(current_tok.kind)) {
                root->declarations.push_back(parse_declaration());
            } else {
                // Тут мы будем вызывать DiagnosticEngine, а не кидать throw
                throw std::runtime_error("Unexpected token outside of declaration");
            }
        }
        return root;
    }

private:
    static bool is_type_token(const TokenKind kind) {
        switch (kind) {
            case TOKEN_KW_INT:
            case TOKEN_KW_CHAR:
            case TOKEN_KW_VOID:
            case TOKEN_KW_FLOAT:
            case TOKEN_KW_DOUBLE:
            case TOKEN_KW_BOOL:
                return true;
            default:
                return false;
        }
    }

    ASTNode* parse_declaration() {
        // 1. Тип
        uint32_t type_id = current_tok.string_id;
        advance();

        // 2. Имя
        if (current_tok.kind != TOKEN_IDENTIFIER) {
            throw std::runtime_error("Expected identifier");
        }
        uint32_t name_id = current_tok.string_id;
        advance();

        // 3. РАЗВИЛКА! Смотрим, что идет дальше
        if (current_tok.kind == TOKEN_LPAREN) {
            // Видим '(' — значит это функция!
            advance(); // Съели '('

            // TODO: Позже тут будем парсить аргументы функции
            expect_and_consume(TOKEN_RPAREN); // Съели ')'

            // Парсим тело, если оно есть
            BlockNode* body = nullptr;
            if (current_tok.kind == TOKEN_LBRACE) {
                body = parse_block();
            } else {
                expect_and_consume(TOKEN_SEMICOLON); // Это просто декларация: int foo();
            }

            return arena.make<FunctionDeclNode>(type_id, name_id, body);

        } else {
            // Видим не скобку — значит это просто переменная!
            // TODO: Позже тут добавим поддержку инициализации: = 5;
            expect_and_consume(TOKEN_SEMICOLON);

            return arena.make<VarDeclNode>(type_id, name_id);
        }
    }

    BlockNode* parse_block() {
        expect_and_consume(TOKEN_LBRACE); // Съели '{'

        auto* block = arena.make<BlockNode>(&arena);

        // Крутимся, пока не встретим '}' или конец файла
        while (current_tok.kind != TOKEN_RBRACE && current_tok.kind != TOKEN_EOF) {
            if (ASTNode* stmt = parse_statement()) {
                block->statements.push_back(stmt); // Кладём в вектор на Арене!
            }
        }

        expect_and_consume(TOKEN_RBRACE); // Съели '}'
        return block;
    }

    ASTNode* parse_expression() {
        // Пока мы умеем парсить только голые числа
        if (current_tok.kind == TOKEN_NUMBER) { // Убедись, что у тебя есть такой токен в лексере!
            uint32_t id = current_tok.string_id;
            advance(); // Съели число
            return arena.make<NumberNode>(id);
        }

        // В будущем тут будет вызов функций (std::cout), математика и переменные
        // Если встретили что-то непонятное, пока просто глотаем токен, чтобы не падать
        advance();
        return nullptr;
    }

    ASTNode* parse_statement() {
        if (current_tok.kind == TOKEN_KW_RETURN) { // Добавь TOKEN_RETURN в лексер, если еще нет!
            advance(); // Съели 'return'

            ASTNode* expr = nullptr;
            // Если сразу не идет ';', значит там есть выражение (например, return 0;)
            if (current_tok.kind != TOKEN_SEMICOLON) {
                expr = parse_expression();
            }

            expect_and_consume(TOKEN_SEMICOLON); // Съели ';'
            return arena.make<ReturnNode>(expr);
        }

        // Если это не return, возможно это просто выражение как стейтмент (например, "a + b;")
        // Но пока мы это скипаем, чтобы не усложнять
        advance();
        return nullptr;
    }

    void parse_directive() {
        expect_and_consume(TOKEN_HASH); // Съели '#'

        // Теперь мы в режиме Directive (благодаря фиксу в лексере)
        // Парсим саму команду, например include
        if (current_tok.kind == TOKEN_KW_INCLUDE) {
            advance(); // Съели include

            if (current_tok.kind == TOKEN_SYSTEM_INCLUDE || current_tok.kind == TOKEN_STRING) {
                // Тут мы могли бы сохранить путь к файлу для препроцессора
                advance();
            }
        }

        // Директива ВСЕГДА заканчивается новой строкой или концом файла
        // Лексер сам сбросит режим в Standard, когда дойдет до '\n'
        while (current_tok.kind != TOKEN_NEWLINE && current_tok.kind != TOKEN_EOF) {
            advance(); // Доедаем остатки, если там какой-то мусор
        }

        if (current_tok.kind == TOKEN_NEWLINE) {
            advance(); // Финальный аккорд: съедаем перевод строки
        }
    }
};
