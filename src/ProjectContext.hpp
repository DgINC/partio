#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <queue>
#include <unordered_map>

#include "Value.hpp"
#include "YACSLexer.h"
#include "YACSParser.h"

struct Parameter {
    std::string type;
    std::string name;
};

struct Function {
    std::string name;
    std::vector<Parameter> params;
    // Храним контекст тела функции для последующего выполнения
    YACSParser::Function_defContext* context;
};

using FunctionMap = std::map<std::string, Function>;

enum class TargetType {
    Executable,
    Library,
    Sources,
    Generator,
    Dumb
};

struct Target;

// Умный посредник для перехвата операторов = и +=
struct TargetPropertyProxy {
    Target& target;
    std::string key;

    // Перехватываем: target["key"] = val;
    TargetPropertyProxy& operator=(const Value& val);

    // Перехватываем: target["key"] += val;
    TargetPropertyProxy& operator+=(const Value& val);

    explicit operator Value() const;
};

struct Target {
    explicit Target(std::string name)
        : name(std::move(name)) {
    }

    friend TargetPropertyProxy;
private:
    static inline const std::set<std::string> ValidProperties = {
        "type", "sources", "flags", "depends", "includes", "output", "input", "vars"
    };
    std::unordered_map<std::string, ValueList*> get_list_map() {
        return {{"sources", &sources}, {"flags", &flags}, {"depends", &depends}, {"includes", &includes}};
    }

    void handle_type_assignment(const Value & val);
public:
    std::string name;
    TargetType type = TargetType::Dumb; // executable, library, generator
    ValueList sources;
    ValueList flags;
    ValueList depends;
    ValueList includes;
    std::string output;
    std::string input;
    ValueMap custom_vars; // Для всяких доп. настроек

    bool is_defined = false;
    bool async = false;
    std::vector<std::string> dependencies;

    TargetPropertyProxy operator[](const std::string& key) {
        return TargetPropertyProxy{*this, key};
    }

    // Метод для жесткого присваивания (=)
    // Единая точка входа для всех операций записи
    void set_property(const std::string& key, const Value& val, const bool is_append) {
        // 1. Обработка списковых полей (самое частое)
        if (auto lists = get_list_map(); lists.contains(key)) {
            auto* target_list = lists[key];
            if (const auto* src_list = std::get_if<ValueList>(&val.data)) {
                if (is_append) {
                    target_list->insert(target_list->end(), src_list->begin(), src_list->end());
                } else {
                    *target_list = *src_list;
                }
            } else {
                // Если прибавили одиночный элемент к списку - просто пушим его
                target_list->push_back(val);
            }
            return;
        }

        // 2. Обработка типа (enum)
        if (key == "type") {
            handle_type_assignment(val);
            return;
        }

        // 3. Всё остальное - в custom_vars
        if (is_append) {
            custom_vars[key] += val; // Используем твой оператор += из Value
        } else {
            custom_vars[key] = val;
        }
    }

    [[nodiscard]] Value get_property(const std::string& key) const {
        // 1. Возвращаем списки
        if (key == "sources") return Value(sources);
        if (key == "flags") return Value(flags);
        if (key == "depends") return Value(depends);
        if (key == "includes") return Value(includes);

        // 2. Возвращаем строки
        if (key == "output") return Value(output);
        if (key == "input") return Value(input);

        // 3. Возвращаем тип (тут зависит от того, как у тебя Value хранит Enums)
        if (key == "type") return Value(type); // Если Value поддерживает TargetType

        // 4. Ищем в custom_vars
        if (const auto it = custom_vars.find(key); it != custom_vars.end()) {
            return it->second;
        }

        // Если свойства нет, можно либо вернуть "пустой" Value, либо кинуть ошибку
        throw std::runtime_error("Свойство '" + key + "' не найдено у таргета '" + name + "'");
    }
};

// Реализация методов Proxy (должна быть после полного определения Target)
inline TargetPropertyProxy& TargetPropertyProxy::operator=(const Value& val) {
    target.set_property(key, val, false);
    return *this;
}

inline TargetPropertyProxy& TargetPropertyProxy::operator+=(const Value& val) {
    target.set_property(key, val, true);
    return *this;
}

inline TargetPropertyProxy::operator Value() const {
    // Логика чтения: сначала из полей, потом из custom_vars
    // Но для этого нужно дописать геттер в Target
    return target.get_property(key);
}

inline void Target::handle_type_assignment(const Value &val) {
    // Статическая мапа создастся один раз при первом вызове
    static const std::unordered_map<std::string, TargetType> TargetTypeMap {
            {"executable", TargetType::Executable},
            {"library",    TargetType::Library},
            {"sources",    TargetType::Sources},
            {"generator",  TargetType::Generator},
            {"dumb",       TargetType::Dumb}
    };

    // 1. Если пользователь передал строку (например: type = "executable")
    if (const auto* str_ptr = std::get_if<std::string>(&val.data)) {
        if (const auto it = TargetTypeMap.find(*str_ptr); it != TargetTypeMap.end()) {
            type = it->second;
        } else {
            throw std::runtime_error("ОШИБКА: Неизвестный тип таргета: '" + *str_ptr + "'");
        }
    }
    // 2. Если значение уже является типом TargetType (внутренняя передача)
    else if (const auto* type_ptr = std::get_if<TargetType>(&val.data)) {
        type = *type_ptr;
    }
    // 3. Защита от дурака (если передали число или список)
    else {
        throw std::runtime_error("ОШИБКА: Свойство 'type' должно быть строкой (например, \"executable\")!");
    }
}

// Структура, которая будет держать AST в памяти
struct ParsedFile {
    std::unique_ptr<std::ifstream> stream;
    std::unique_ptr<antlr4::ANTLRInputStream> input;
    std::unique_ptr<YACSLexer> lexer;
    std::unique_ptr<antlr4::CommonTokenStream> tokens;
    std::unique_ptr<YACSParser> parser;
};

class ProjectContext {
public:
    std::filesystem::path root;
    ValueMap globals; // Тут лежат $version, $source_files и т.д.
    std::map<std::string, std::shared_ptr<Target>> targets; // Тут храним все наши цели
    FunctionMap functions;
    std::vector<std::shared_ptr<ParsedFile>> ast_cache;
    std::queue<std::shared_ptr<Target>> execution_plan;
};
