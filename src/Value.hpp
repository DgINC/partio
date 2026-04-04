#pragma once

#include <variant>
#include <string>
#include <vector>
#include <map>
#include <memory>


struct Function;
enum class TargetType;
struct Value;
class ProjectContext;

using ValueList = std::vector<Value>;
using ValueMap = std::map<std::string, Value>;

struct Value {
    // Храним всё: от булевых значений до списков и словарей
    std::variant<
        std::monostate,
        bool,
        int,
        std::string,
        ValueList,
        ValueMap,
        TargetType,
        std::shared_ptr<ProjectContext>,
        std::shared_ptr<Function>> data;

    // Удобные конструкторы
    Value() : data(std::monostate{}) {}
    explicit Value(bool v) : data(v) {}
    explicit Value(int v) : data(v) {}
    explicit Value(std::string v) : data(v) {}
    explicit Value(const char* v) : data(std::string(v)) {}
    explicit Value(ValueList v) : data(v) {}
    explicit Value(const ValueMap & map) : data(map) {};
    explicit Value(TargetType type) : data(type) {}
    explicit Value(std::shared_ptr<ProjectContext> context) : data(std::move(context)) {};
    explicit Value(std::shared_ptr<Function> func) : data(std::move(func)) {}

    Value& operator+=(const Value& other) {
        // 1. Если левая часть пустая (еще не инициализирована)
        if (std::holds_alternative<std::monostate>(data)) {
            data = other.data;
            return *this;
        }

        // 2. Если левая часть — список
        if (std::holds_alternative<ValueList>(data)) {
            auto& myList = std::get<ValueList>(data);

            // Добавляем список к списку
            if (std::holds_alternative<ValueList>(other.data)) {
                const auto& otherList = std::get<ValueList>(other.data);
                myList.insert(myList.end(), otherList.begin(), otherList.end());
            }
            // Добавляем одиночный элемент к списку
            else {
                myList.push_back(other);
            }
            return *this;
        }

        // 3. Если левая часть — строка (поддержка склейки строк)
        if (std::holds_alternative<std::string>(data) && std::holds_alternative<std::string>(other.data)) {
            std::get<std::string>(data) += std::get<std::string>(other.data);
            return *this;
        }

        throw std::runtime_error("Ошибка типов: невозможно объединить данные этих типов.");
    }

    bool operator==(const Value& other) const {
        return data == other.data;
    }

    bool operator!=(const Value& other) const {
        return data != other.data;
    }

    [[nodiscard]] std::string to_string() const;

    template<typename T>
    T* get_if() { return std::get_if<T>(&data); }

    // Удобные конвертеры
    [[nodiscard]] std::string as_string() const {
        if (const auto s = std::get_if<std::string>(&data)) return *s;
        return ""; // Или кидать ошибку
    }

    ValueList& as_list() {
        if (auto l = std::get_if<ValueList>(&data)) return *l;
        static ValueList empty; return empty;
    }

    // Проверка типа
    [[nodiscard]] bool is_list() const { return std::holds_alternative<ValueList>(data); }
    [[nodiscard]] bool is_string() const { return std::holds_alternative<std::string>(data); }

    [[nodiscard]] bool is_project() const {
        return std::holds_alternative<std::shared_ptr<ProjectContext>>(data);
    }

    [[nodiscard]] std::shared_ptr<ProjectContext> as_project() const {
        return std::get<std::shared_ptr<ProjectContext>>(data);
    }

    [[nodiscard]] bool is_function() const {
        return std::holds_alternative<std::shared_ptr<Function>>(data);
    }

    [[nodiscard]] std::shared_ptr<Function> as_function() const {
        if (auto f = std::get_if<std::shared_ptr<Function>>(&data)) return *f;
        throw std::runtime_error("ОШИБКА: Значение не является функцией!");
    }

    [[nodiscard]] bool is_map() const {
        return std::holds_alternative<ValueMap>(data);
    }

    [[nodiscard]] ValueMap as_map() const {
        if (const auto l = std::get_if<ValueMap>(&data)) return *l;
        return ValueMap{};
    }

    [[nodiscard]] bool is_int() const {
        return std::holds_alternative<int>(data);
    }

    [[nodiscard]] int as_int() const {
        if (const auto l = std::get_if<int>(&data)) return *l;
        return 0;
    }
};

class ReturnSignal : public std::exception {
    Value value;
public:
    explicit ReturnSignal(Value value) : value(std::move(value)) {};

    [[nodiscard]] constexpr Value getValue() const {
        return value;
    }
};

class BreakSignal : public std::exception {};

class ContinueSignal : public std::exception {};