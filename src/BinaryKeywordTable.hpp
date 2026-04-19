#pragma once

#include <array>
#include <string_view>
#include <algorithm>

#include "Token.hpp"

// --- 1. Ключевые слова и Таблица ---

struct KeywordEntry {
    std::string_view name;
    TokenKind kind;
};

template<size_t N>
class BinaryKeywordTable {
    std::array<KeywordEntry, N> table;

public:
    explicit constexpr BinaryKeywordTable(std::array<KeywordEntry, N> arr) : table(arr) {
        // Сортируем при компиляции, чтобы работал бинарный поиск
        std::sort(table.begin(), table.end(), [](const KeywordEntry& a, const KeywordEntry& b) {
            return a.name < b.name;
        });
    }

    constexpr TokenKind lookup(std::string_view s) const {
        auto it = std::lower_bound(table.begin(), table.end(), s,
            [](const KeywordEntry& entry, const std::string_view val) {
                return entry.name < val;
            });

        if (it != table.end() && it->name == s) {
            return it->kind;
        }
        return TOKEN_IDENTIFIER;
    }
};

// Наш список "вшитых" слов
static constexpr std::array<KeywordEntry, 6> raw_keywords = {{
    {"if",     TOKEN_KW_IF},
    {"int",    TOKEN_KW_INT},
    {"return", TOKEN_KW_RETURN},
    {"for",    TOKEN_KW_FOR},
    {"while",  TOKEN_KW_WHILE},
    {"void",   TOKEN_KW_VOID}
}};

// Магия: компилятор сам создаст отсортированную таблицу
static constexpr BinaryKeywordTable<raw_keywords.size()> KEYWORDS(raw_keywords);
