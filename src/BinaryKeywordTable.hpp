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
static constexpr std::array<KeywordEntry, 103> raw_keywords = {{
    {"alignas", TOKEN_KW_ALIGNAS},
    {"alignof", TOKEN_KW_ALIGNOF},
    {"asm", TOKEN_KW_ASM},
    {"atomic_cancel", TOKEN_KW_ATOMIC_CANCEL},
    {"atomic_commit", TOKEN_KW_ATOMIC_COMMIT},
    {"atomic_noexcept", TOKEN_KW_ATOMIC_NOEXCEPT},
    {"auto", TOKEN_KW_AUTO},
    {"bool", TOKEN_KW_BOOL},
    {"break", TOKEN_KW_BREAK},
    {"case", TOKEN_KW_CASE},
    {"catch",  TOKEN_KW_CATCH},
    {"char", TOKEN_KW_CHAR},
    {"class", TOKEN_KW_CLASS},
    {"concept", TOKEN_KW_CONCEPT},
    {"const", TOKEN_KW_CONST},
    {"consteval", TOKEN_KW_CONSTEVAL},
    {"constexpr", TOKEN_KW_CONSTEXPR},
    {"constinit", TOKEN_KW_CONSTINIT},
    {"const_cast", TOKEN_KW_CONST_CAST},
    {"continue", TOKEN_KW_CONTINUE},
    {"contract_assert", TOKEN_KW_CONTRACT_ASSERT},
    {"co_await", TOKEN_KW_CO_AWAIT},
    {"co_return", TOKEN_KW_CO_RETURN},
    {"co_yield", TOKEN_KW_CO_YIELD},
    {"decltype", TOKEN_KW_DECLTYPE},
    {"default", TOKEN_KW_DEFAULT},
    {"define", TOKEN_KW_DEFINE},
    {"defined", TOKEN_KW_DEFINED},
    {"delete", TOKEN_KW_DELETE},
    {"do",     TOKEN_KW_DO},
    {"double", TOKEN_KW_DOUBLE},
    {"dynamic_cast", TOKEN_KW_DYNAMIC_CAST},
    {"elif", TOKEN_KW_ELIF},
    {"elifdef", TOKEN_KW_ELIFDEF},
    {"elifndef", TOKEN_KW_ELIFNDEF},
    {"else", TOKEN_KW_ELSE},
    {"endif", TOKEN_KW_ENDIF},
    {"embed", TOKEN_KW_EMBED},
    {"enum", TOKEN_KW_ENUM},
    {"error", TOKEN_KW_ERROR},
    {"explicit", TOKEN_KW_EXPLICIT},
    {"export", TOKEN_KW_EXPORT},
    {"extern", TOKEN_KW_EXTERN},
    {"false", TOKEN_KW_FALSE},
    {"final", TOKEN_KW_FINAL},
    {"float", TOKEN_KW_FLOAT},
    {"for", TOKEN_KW_FOR},
    {"friend", TOKEN_KW_FRIEND},
    {"goto", TOKEN_KW_GOTO},
    {"if",     TOKEN_KW_IF},
    {"ifdef", TOKEN_KW_IFDEF},
    {"ifndef", TOKEN_KW_IFNDEF},
    {"import", TOKEN_KW_IMPORT},
    {"include", TOKEN_KW_INCLUDE},
    {"inline", TOKEN_KW_INLINE},
    {"int",    TOKEN_KW_INT},
    {"long",   TOKEN_KW_LONG},
    {"line", TOKEN_KW_LINE},
    {"module", TOKEN_KW_MODULE},
    {"mutable", TOKEN_KW_MUTABLE},
    {"namespace", TOKEN_KW_NAMESPACE},
    {"new", TOKEN_KW_NEW},
    {"noexcept", TOKEN_KW_NOEXCEPT},
    {"nullptr", TOKEN_KW_NULL},
    {"operator", TOKEN_KW_OPERATOR},
    {"override", TOKEN_KW_OVERRIDE},
    {"pragma", TOKEN_KW_PRAGMA},
    {"pre", TOKEN_KW_PRE},
    {"private", TOKEN_KW_PRIVATE},
    {"protected", TOKEN_KW_PROTECTED},
    {"post", TOKEN_KW_POST},
    {"public", TOKEN_KW_PUBLIC},
    {"reflexpr", TOKEN_KW_REFLEXPR},
    {"reinterpret_cast", TOKEN_KW_REINTERPRET},
    {"requires", TOKEN_KW_REQUIRES},
    {"return", TOKEN_KW_RETURN},
    {"short", TOKEN_KW_SHORT},
    {"signed", TOKEN_KW_SIGNED},
    {"sizeof", TOKEN_KW_SIZEOF},
    {"static", TOKEN_KW_STATIC},
    {"static_assert", TOKEN_KW_STATIC_ASSERT},
    {"static_cast", TOKEN_KW_STATIC_CAST},
    {"struct", TOKEN_KW_STRUCT},
    {"switch", TOKEN_KW_SWITCH},
    {"synchronized", TOKEN_KW_SYNCHRONIZED},
    {"template", TOKEN_KW_TEMPLATE},
    {"this", TOKEN_KW_THIS},
    {"thread_local", TOKEN_KW_TREAD_LOCAL},
    {"throw", TOKEN_KW_THROW},
    {"true", TOKEN_KW_TRUE},
    {"try", TOKEN_KW_TRY},
    {"typedef", TOKEN_KW_TYPEDEF},
    {"typeid", TOKEN_KW_TYPEID},
    {"typename", TOKEN_KW_TYPENAME},
    {"undef", TOKEN_KW_UNDEF},
    {"union", TOKEN_KW_UNION},
    {"unsigned", TOKEN_KW_UNSIGNED},
    {"using", TOKEN_KW_USING},
    {"virtual", TOKEN_KW_VIRTUAL},
    {"void", TOKEN_KW_VOID},
    {"volatile", TOKEN_KW_VOLATILE},
    {"warning", TOKEN_KW_WARNING},
    {"while",  TOKEN_KW_WHILE},
}};

// Магия: компилятор сам создаст отсортированную таблицу
static constexpr BinaryKeywordTable<raw_keywords.size()> KEYWORDS(raw_keywords);
