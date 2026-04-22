#pragma once

#include <string_view>

#include "ITokenStream.hpp"
#include "Lexer.hpp"
#include "ProjectContext.hpp"
#include "SourceManager.hpp"
#include "StringPool.hpp"

struct IncludeEntry {
    std::unique_ptr<Lexer> lexer;
    const SourceBuffer* buffer; // Здесь лежит наш путь и данные
};

class Preprocessor : public ITokenStream {
    std::shared_ptr<SourceManager> src_mgr;
    std::shared_ptr<StringPool> pool;
    std::shared_ptr<Target> target;

    // Стек лексеров. unique_ptr сам уничтожит лексер при удалении из вектора.
    std::vector<IncludeEntry> stack;

public:
    Preprocessor(const std::weak_ptr<SourceManager>& mgr,const std::weak_ptr<StringPool>& p, const std::shared_ptr<Target> &target)
    : src_mgr(mgr), pool(p), target(target) {}

    bool push_file(const fs::path& filepath, const bool is_system = false) {
        fs::path current_dir = "";
        const SourceBuffer* buffer = nullptr;

        // Если стек не пуст, берем путь файла, который сейчас "сверху"
        if (!stack.empty()) {
            current_dir = stack.back().buffer->filepath.parent_path();
        }

        buffer = src_mgr->load_file(filepath, current_dir, is_system);

        if (!buffer) {
            // Либо файл не найден, либо SourceManager решил его не давать (например, #pragma once)
            return false;
        }

        // Собираем запись: новый лексер + ссылка на буфер
        stack.push_back({
            std::make_unique<Lexer>(buffer, pool),
            buffer
        });

        return true;
    }

    Token next_token() override {
        while (!stack.empty()) {
            // Работаем с лексером из последней записи в стеке
            const Token t = stack.back().lexer->next_token();

            if (t.kind == TOKEN_EOF) {
                stack.pop_back(); // unique_ptr сам приберет за собой лексер
                continue;
            }

            if (t.kind == TOKEN_HASH) {
                stack.back().lexer->set_mode(LexerMode::Directive);
                if (handle_directive()) continue;
            }

            return t;
        }
        return { TOKEN_EOF };
    }

private:
    bool handle_directive() {
        // Мы только что "съели" символ '#'
        const Token directive = stack.back().lexer->next_token();
        
        if (directive.kind != TOKEN_IDENTIFIER) return false;

        if (const std::string_view dir_name = pool->get_string(directive.string_id); dir_name == "include") {
            return handle_include();
        }

        // Если это не include, возвращаем false (например, пока не умеем #define)
        return false;
    }

    bool handle_include() {
        const Token next_tok = stack.back().lexer->next_token();
        
        std::string filepath;
        bool is_system = false;

        // В лексере нам нужно будет отличать строки "file.h" (TOKEN_STRING)
        // от системных путей <math.h> (TOKEN_SYSTEM_INCLUDE)
        if (next_tok.kind == TOKEN_STRING) {
            filepath = pool->get_string(next_tok.string_id);
        } 
        else if (next_tok.kind == TOKEN_SYSTEM_INCLUDE) {
            filepath = pool->get_string(next_tok.string_id);
            is_system = true;
        } 
        else {
            // Синтаксическая ошибка: ожидалась строка или <...>
            return false; 
        }

        // Ставим текущий файл на паузу и кладем новый лексер ПОВЕРХ стека
        push_file(filepath, is_system);
        return true;
    }
};
