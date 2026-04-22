#pragma once

#include <filesystem>
#include <ranges>
#include <regex>
#include <functional>

#include "ProjectContext.hpp"
#include "YACSParserBaseVisitor.h"
#include "Value.hpp"

struct ProjectConfig {
    std::string arch;
    std::string build_mode;
    std::string target;
    int thread_count;
    bool verbose;
    std::vector<std::filesystem::path> include_paths;
};

using BuiltinFunc = std::move_only_function<Value(const std::map<std::string, Value>&)>;

template<typename F>
BuiltinFunc wrap_builtin(F&& func) {
    // Используем захват с перемещением (init-capture)
    return [f = std::forward<F>(func)](const std::map<std::string, Value>& args) mutable -> Value {
        // Проверяем результат вызова f с аргументами
        using RetType = std::invoke_result_t<F&, const std::map<std::string, Value>&>;

        if constexpr (std::is_void_v<RetType>) {
            f(args);
            return {}; // Возвращаем наш "null" (std::monostate)
        } else {
            return f(args);
        }
    };
}

class Interpreter : public YACSParserBaseVisitor {
public:

    std::any visitFor_stmt(YACSParser::For_stmtContext *ctx) override;

    std::any visitIndexAccess(YACSParser::IndexAccessContext *ctx) override;

    std::any visitProject_file(YACSParser::Project_fileContext *ctx) override;

    std::any visitImport_stmt(YACSParser::Import_stmtContext *ctx) override;

    std::any visitAdditiveExpr(YACSParser::AdditiveExprContext *ctx) override;

    std::any visitReturn_stmt(YACSParser::Return_stmtContext *ctx) override;

    std::any visitFunction_def(YACSParser::Function_defContext *ctx) override;

    std::any visitNamespace_block(YACSParser::Namespace_blockContext *ctx) override;

    std::any visitComparisonExpr(YACSParser::ComparisonExprContext *ctx) override;

    std::any visitIf_stmt(YACSParser::If_stmtContext *ctx) override;

    std::any visitBoolLiteral(YACSParser::BoolLiteralContext *ctx) override;

    // Поиск переменной (от локальной к глобальной)
    Value& getVar(const std::string& name) {
        // 1. Формируем список имен для поиска
        std::vector<std::string> candidates;

        // Если имя без двоеточия и мы в неймспейсе — сначала ищем с префиксом
        if (name.find(':') == std::string::npos && !currentNamespace.empty()) {
            candidates.push_back(currentNamespace + ":" + name);
        }
        candidates.push_back(name); // Затем ищем как есть (глобально или локально)

        // 2. Проходим по стеку областей видимости сверху вниз (от локальной к глобальной)
        for (auto & scope : std::views::reverse(scopes)) {
            for (const auto& candidate : candidates) {
                if (scope.contains(candidate)) {
                    return scope[candidate];
                }
            }
        }

        throw std::runtime_error("ОШИБКА: Переменная '" + name + "' не определена!");
    }

    void setVar(const std::string& name, const Value& val, const bool onlyLocal = false) {
        if (onlyLocal) {
            scopes.back()[name] = val;
            return;
        }
        // Если переменная уже есть где-то выше, обновляем её там
        for (auto & scope : std::views::reverse(scopes)) {
            if (scope.contains(name)) {
                scope[name] = val;
                return;
            }
        }
        // Если нигде нет — создаем в локальной
        scopes.back()[name] = val;
    }

private:
    std::shared_ptr<ProjectConfig> _config;
    std::shared_ptr<ProjectContext> project_ctx;
    std::shared_ptr<Target> currentTarget = nullptr;
    std::string currentNamespace; // Пусто по умолчанию (глобальная область)
    std::vector<std::map<std::string, Value>> scopes;
    std::map<std::string, BuiltinFunc> builtins;
    std::string current_target_name{};
public:
    explicit Interpreter(const std::shared_ptr<ProjectConfig>& config, const std::shared_ptr<ProjectData>& _project_data);
    Interpreter(const Interpreter&) = delete;            // Удаляем копирование
    Interpreter& operator=(const Interpreter&) = delete; // Удаляем присваивание

    Interpreter(Interpreter&&) = default;                // Оставляем перемещение
    Interpreter& operator=(Interpreter&&) = default;

    void set_project_ctx(std::shared_ptr<ProjectContext> project_ctx);

    std::any visitArg_list(YACSParser::Arg_listContext *ctx) override;

    std::any visitQualified_id(YACSParser::Qualified_idContext *ctx) override;

    std::any visitStringLiteral(YACSParser::StringLiteralContext *ctx) override {
        std::string result{};

        for (const auto* literal = ctx->string_literal(); auto* child : literal->children) {
            // 1. Если это текстовый токен или эскейп-последовательность
            if (auto* terminal = dynamic_cast<antlr4::tree::TerminalNode*>(child)) {
                if (const auto type = terminal->getSymbol()->getType(); type == YACSLexer::STRING_TEXT) {
                    result += terminal->getText();
                }
                else if (type == YACSLexer::ESCAPE) {
                    if (std::string esc = terminal->getText(); esc == "\\n") result += "\n";
                    else if (esc == "\\t") result += "\t";
                    else if (esc == "\\\"") result += "\"";
                    else if (esc == "\\$") result += "$";
                    else result += esc.substr(1);
                }
            }
            else if (auto* interp = dynamic_cast<YACSParser::InterpolationContext*>(child)) {
                auto val = std::any_cast<Value>(visit(interp->expr()));
                result += val.to_string();
            }
        }

        return Value(result);
    }

    std::any visitNumberLiteral(YACSParser::NumberLiteralContext *ctx) override {
        const int value = std::stoi(ctx->NUMBER()->getText());
        return Value(value);
    }

    std::any visitMapLiteral(YACSParser::MapLiteralContext *ctx) override {
        std::map<std::string, Value> m;
        if (ctx->map_entries()) {
            for (auto* entry : ctx->map_entries()->map_entry()) {
                auto key = std::any_cast<Value>(visit(entry->expr(0)));
                auto val = std::any_cast<Value>(visit(entry->expr(1)));
                m[key.to_string()] = val; // Ключ мапы всегда храним как строку для простоты
            }
        }
        return Value(m);
    }

    std::any visitListLiteral(YACSParser::ListLiteralContext *ctx) override {
        ValueList list_elements;

        // Проверяем, есть ли вообще элементы внутри скобок (не пустой ли это список [])
        if (ctx->expr_list() != nullptr) {

            // Проходим в цикле по всем выражениям внутри списка
            for (const auto expr_ctx : ctx->expr_list()->expr()) {

                // Вычисляем значение отдельного элемента
                auto result = visit(expr_ctx);

                if (!result.has_value()) {
                    std::cerr << "[Ошибка] Элемент списка вернул пустоту." << std::endl;
                    continue;
                }

                try {
                    // Извлекаем наше значение и добавляем в массив
                    auto val = std::any_cast<Value>(result);
                    list_elements.push_back(val);
                } catch (const std::bad_any_cast& e) {
                    std::cerr << "[Ошибка] Не удалось определить тип элемента списка." << std::endl;
                    std::cerr << e.what() << std::endl;
                }
            }
        }

        // Возвращаем собранный массив, обернув его в универсальный тип Value
        return Value(list_elements);
    }

    std::any visitVariable_decl(YACSParser::Variable_declContext *ctx) override;

    std::any visitStatement(YACSParser::StatementContext *ctx) override {
        return visitChildren(ctx);
    }

    std::any visitProject_block(YACSParser::Project_blockContext *ctx) override {
        return visit(ctx->project_body());
    }

    std::any visitProject_body(YACSParser::Project_bodyContext *ctx) override {
        for (const auto stmt : ctx->statement()) {
            visit(stmt); // Выполняем каждую строку (присваивания, if и т.д.)
        }
        for (const auto target : ctx->target_block()) {
            visit(target); // Затем заходим в каждую цель
        }
        return nullptr;
    }

    std::any visitTarget_body(YACSParser::Target_bodyContext *ctx) override {
        for (const auto stmt : ctx->statement()) {
            visit(stmt);
        }
        return nullptr;
    }

    // Обход блока цели
    std::any visitTarget_block(YACSParser::Target_blockContext *ctx) override {
        const std::string name = ctx->IDENTIFIER()->getText();

        const auto& targetPtr = project_ctx->project_data->targets.at(name);

        // 2. Проверяем, не пытаемся ли мы описать один и тот же таргет второй раз
        if (targetPtr->is_defined) {
            throw std::runtime_error("The target '" + name + "' is described more than once!");
        }

        // 3. Помечаем, что теперь этот таргет "в работе" (наполняется данными)
        targetPtr->is_defined = true;

        if (ctx->ASYNC_KW()) {
            targetPtr->async = true;
        }

        // 4. Переключаем контекст интерпретатора
        const std::shared_ptr<Target> oldTarget = currentTarget;
        currentTarget = targetPtr;

        scopes.emplace_back();
        current_target_name = name;

        visit(ctx->target_body()); // Наполняем цель свойствами

        scopes.pop_back();
        current_target_name.clear();

        currentTarget = oldTarget; // Возвращаемся в корень проекта
        return nullptr;
    }

    std::any visitAssignment(YACSParser::AssignmentContext *ctx) override {
        const auto val = std::any_cast<Value>(visit(ctx->expr()));
        std::string name = ctx->qualified_id()->getText();

        // Учитываем namespace для глобальных, если имя написано без двоеточия
        if (scopes.size() == 1 && !currentNamespace.empty() && name.find(':') == std::string::npos) {
            name = currentNamespace + ":" + name;
        }

        if (currentTarget != nullptr) {
            try {
                if (ctx->op->getType() == YACSParser::ASSIGN) {
                    (*currentTarget)[name] = val;  // Вызовет setProperty
                }
                else if (ctx->op->getType() == YACSParser::PLUS_ASSIGN) {
                    (*currentTarget)[name] += val; // Вызовет addProperty (склейка списков!)
                }
            } catch (const std::exception& e) {
                // Перехватываем ошибку из Proxy и добавляем номер строки
                const size_t line = ctx->start->getLine();
                const size_t col = ctx->start->getCharPositionInLine();

                throw std::runtime_error("Строка " + std::to_string(line) + ":" + std::to_string(col) + " -> " + e.what());
            }
        } else {
            if (ctx->op->getType() == YACSParser::ASSIGN) {
                setVar(name, val, false);
            } else if (ctx->op->getType() == YACSParser::PLUS_ASSIGN) {
                // Для += сначала получаем ссылку на существующую переменную
                Value& existing = getVar(name);
                existing += val; // Вызываем твою перегрузку оператора +=
            }
        }
        return nullptr;
    }

    std::any visitVarReference(YACSParser::VarReferenceContext *ctx) override {
        // Получаем текст, например "$version"
        std::string name = ctx->IDENTIFIER()->getText();

        // Если в токене остался '$', убираем его (зависит от того, как описан лексер)
        if (name[0] == '$') name.erase(0, 1);

        // Достаем значение из нашего ScopeStack
        Value& val = getVar(name);

        return std::make_any<Value>(val);
    }

    std::any visitIdReference(YACSParser::IdReferenceContext *ctx) override {
        const auto idents = ctx->qualified_id()->IDENTIFIER();

        // Сценарий 1: Просто одиночное имя (например: my_var или gtest)
        if (idents.size() == 1) {
            const std::string name = idents[0]->getText();
            return std::make_any<Value>(resolveVariable(name));
        }

        // Сценарий 2: Составное имя (например: antlr:runtime)
        if (idents.size() == 2) {
            const std::string prefix = idents[0]->getText(); // "antlr"
            const std::string target = idents[1]->getText(); // "runtime"

            // Пытаемся найти, что скрывается за префиксом

            // Если префикс — это другой проект (результат fetch)
            if (const Value prefixVal = resolveVariable(prefix); prefixVal.is<std::shared_ptr<ProjectContext>>()) {
                // Проверяем, есть ли такой таргет в том проекте
                const auto& subProject = prefixVal.as<std::shared_ptr<ProjectContext>>();
                if ( subProject->project_data->targets.contains(target)) {
                    // Возвращаем полное имя "префикс:цель"
                    const auto res = subProject->project_data->targets.at(target);
                    return std::make_any<Value>(res);
                }
                throw std::runtime_error("Target '" + target + "' not found in project '" + prefix + "'!");
            }

            // Если это не проект, возможно это просто переменная с двоеточием в имени
            // (например, глобальная переменная внутри неймспейса)
            return std::make_any<Value>(resolveVariable(ctx->qualified_id()->getText()));
        }

        throw std::runtime_error("Path too complex (more than one ':'):" + ctx->qualified_id()->getText());
    }

    std::any visitFunctionCall(YACSParser::FunctionCallContext *ctx) override;

    Value evaluateExpression(const std::string &expression);

    [[nodiscard]] bool hasCycles() const;

    void buildExecutionPlans() const;

    Value resolveVariable(const std::string& name) {
        std::vector<std::string> candidates;
        if (name.find(':') == std::string::npos && !currentNamespace.empty()) {
            candidates.push_back(currentNamespace + ":" + name);
        }
        candidates.push_back(name);

        for (auto const& scope : std::views::reverse(scopes)) {
            for (const auto& candidate : candidates) {
                if (scope.contains(candidate)) {
                    return scope.at(candidate);
                }
            }
        }

        for (const auto& candidate : candidates) {
            if (project_ctx->globals.contains(candidate)) {
                return project_ctx->globals.at(candidate);
            }
        }

        for (const auto& candidate : candidates) {
            if (project_ctx->project_data->targets.contains(candidate)) {
                const auto res = project_ctx->project_data->targets.at(candidate);
                return Value(res);
            }
        }

        throw std::runtime_error("Identifier '" + name + "' was not found in either variables or targets!");
    }
};