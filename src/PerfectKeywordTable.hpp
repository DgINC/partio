#pragma once

#include <algorithm>
#include <array>
#include <string_view>
#include <optional>

#include "Token.hpp"

struct KeywordEntry {
    std::string_view name;
    TokenKind kind;
};

template<size_t TableSize>
class PerfectKeywordTable {
    std::array<std::optional<KeywordEntry>, TableSize> table{};
    uint32_t magic_seed = 0;

    // Простенький constexpr хэш
    static constexpr uint32_t hash(const std::string_view s, const uint32_t seed) {
        uint32_t h = seed;
        for (const char c : s) h = h * 33 + static_cast<uint8_t>(c);
        return h;
    }

public:
    // Конструктор, который найдет идеальный seed при компиляции
    template<size_t N>
    explicit constexpr PerfectKeywordTable(const std::array<KeywordEntry, N>& keywords) {
        bool collision = true;
        while (collision) {
            collision = false;
            table.fill(std::nullopt);
            magic_seed++;

            for (const auto& kw : keywords) {
                uint32_t h = hash(kw.name, magic_seed) % TableSize;
                if (table[h].has_value()) {
                    collision = true;
                    break;
                }
                table[h] = kw;
            }

            // Если мы перебрали слишком много и не нашли (маленький размер таблицы),
            // компилятор выдаст ошибку "constexpr loop limit exceeded".
            // Это хорошо — значит нужно увеличить TableSize.
        }
    }

    constexpr TokenKind lookup(std::string_view s) const {
        uint32_t h = hash(s, magic_seed) % TableSize;
        if (table[h].has_value() && table[h]->name == s) {
            return table[h]->kind;
        }
        return TOKEN_IDENTIFIER;
    }
};

constexpr bool compare_keywords(const KeywordEntry& a, const KeywordEntry& b) {
    return a.name < b.name;
}

template<size_t N>
class BinaryKeywordTable {
    std::array<KeywordEntry, N> table;

public:
    // Конструктор просто копирует и сортирует массив при компиляции
    explicit constexpr BinaryKeywordTable(std::array<KeywordEntry, N> arr) : table(arr) {
        // constexpr std::sort доступен с C++20
        std::sort(table.begin(), table.end(), compare_keywords);
    }

    // Бинарный поиск за O(log N)
    constexpr TokenKind lookup(std::string_view s) const {
        int left = 0;
        int right = N - 1;

        while (left <= right) {
            int mid = left + (right - left) / 2;
            const int cmp = table[mid].name.compare(s);

            if (cmp == 0) return table[mid].kind;
            if (cmp < 0) left = mid + 1;
            else right = mid - 1;
        }

        return TOKEN_IDENTIFIER; // Не нашли
    }
};
