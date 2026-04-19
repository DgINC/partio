#include "../Lexer.hpp"

Token Lexer::next_token() {
    // 1. Пропускаем пробелы и комментарии
    while (true) {
        const char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
        } else if (c == '\n') {
            advance();
            current_line++;
            line_start = cursor;
        } else if (c == '/' && peek_next() == '/') {
            // Однострочный комментарий: глотаем до конца строки
            while (peek() != '\n' && peek() != '\0') advance();
        } else if (c == '/' && peek_next() == '*') {
            // Многострочный комментарий /* ... */
            advance(); advance(); // съедаем /*
            while (peek() != '\0') {
                if (peek() == '*' && peek_next() == '/') {
                    advance(); advance(); // съедаем */
                    break;
                }
                if (advance() == '\n') {
                    current_line++;
                    line_start = cursor;
                }
            }
        } else {
            break; // Полезная нагрузка!
        }
    }

    // Запоминаем, где начинается токен
    const uint8_t* token_start = cursor;
    uint32_t col = static_cast<uint32_t>(cursor - line_start) + 1;

    char c = peek();

    // Конец файла
    if (c == '\0') return { TOKEN_EOF, 0, current_line, col };

    // 2. Идентификаторы и Ключевые слова (a-z, A-Z, _, или UTF-8)
    if (std::isalpha(c) || c == '_' || static_cast<uint8_t>(c) >= 128) {
        return lex_identifier(token_start, col);
    }

    // 3. Числа
    if (std::isdigit(c)) {
        return lex_number(token_start, col);
    }

    // 4. Строки и символы ('', "")
    if (c == '"' || c == '\'') {
        return lex_string(token_start, col);
    }

    // 5. Операторы (+, -, ==, >>=)
    return lex_operator(token_start, col);
}

Token Lexer::lex_identifier(const uint8_t* start, uint32_t col) {
    advance(); // Съедаем первую букву

    while (true) {
        const char c = peek();
        if (std::isalnum(c) || c == '_' || static_cast<uint8_t>(c) >= 128) {
            advance();
        } else {
            break;
        }
    }

    // Создаем string_view прямо на кусок памяти mmap! Без копирования!
    const size_t length = cursor - start;
    const std::string_view text(reinterpret_cast<const char*>(start), length);

    // Получаем уникальный ID строки (Interning)
    uint32_t string_id = pool.intern(text);

    // TODO: Здесь же можно быстро проверить, не является ли string_id ключевым словом
    // (if string_id == pool.intern("int")) return {TOKEN_INT, ...};

    return { TOKEN_IDENTIFIER, string_id, current_line, col, 0 };
}

Token Lexer::lex_operator(const uint8_t* start, uint32_t col) {
    char c = advance();
    switch (c) {
        case '+':
            if (peek() == '+') { advance(); return {TOKEN_PLUS_PLUS, 0, current_line, col}; }
            if (peek() == '=') { advance(); return {TOKEN_PLUS_EQUAL, 0, current_line, col}; }
            return {TOKEN_PLUS, 0, current_line, col};

        case '>':
            if (peek() == '>') {
                advance();
                if (peek() == '=') { advance(); return {TOKEN_RSHIFT_EQUAL, 0, current_line, col}; }
                return {TOKEN_RSHIFT, 0, current_line, col};
            }
            if (peek() == '=') { advance(); return {TOKEN_GREATER_EQUAL, 0, current_line, col}; }
            return {TOKEN_GREATER, 0, current_line, col};

            // ... и так далее для всех символов: &, |, ^, !, =, <, *, /, %

        default:
            return {TOKEN_UNKNOWN, 0, current_line, col};
    }
}
