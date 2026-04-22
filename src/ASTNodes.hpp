#pragma once
#include <cstdint>
#include <vector>

struct ASTNode {
    static constexpr bool is_arena_safe = false;
};

struct BlockNode : ASTNode {
    static constexpr bool is_arena_safe = true;

    // Вектор стейтментов. PMR, чтобы жил в Арене!
    std::pmr::vector<ASTNode*> statements;

    explicit BlockNode(std::pmr::memory_resource* mr)
        : statements(mr) {}
};

// Узел для функций: int main() { ... }
struct FunctionDeclNode : ASTNode {
    static constexpr bool is_arena_safe = true;

    std::uint32_t return_type_id;
    std::uint32_t name_id;
    BlockNode* body; // Указатель на тело функции (может быть nullptr, если это просто int main();)

    FunctionDeclNode(const uint32_t ret, const uint32_t name, BlockNode* b = nullptr)
        : return_type_id(ret), name_id(name), body(b) {}
};

struct VarDeclNode : ASTNode {
    static constexpr bool is_arena_safe = true;

    uint32_t type_name_id;
    uint32_t var_name_id;
    ASTNode* initializer; // Просто указатель!

    VarDeclNode(const uint32_t type, const uint32_t name, ASTNode* init = nullptr)
        : type_name_id(type), var_name_id(name), initializer(init) {}
};

// Узел для чисел (например, 0, 42)
struct NumberNode : ASTNode {
    static constexpr bool is_arena_safe = true;
    uint32_t value_string_id; // Храним ID строки из пула, чтобы потом перевести в int

    explicit NumberNode(const uint32_t id) : value_string_id(id) {}
};

// Узел для return
struct ReturnNode : ASTNode {
    static constexpr bool is_arena_safe = true;
    ASTNode* expression; // То, что мы возвращаем (может быть nullptr)

    explicit ReturnNode(ASTNode* expr = nullptr) : expression(expr) {}
};

struct TranslationUnitNode : ASTNode {
    static constexpr bool is_arena_safe = true;

    std::pmr::vector<ASTNode*> declarations;

    explicit TranslationUnitNode(std::pmr::memory_resource* mr)
        : declarations(mr) {}
};