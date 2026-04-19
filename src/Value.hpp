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
struct Target;

using ValueList = std::vector<Value>;
using ValueMap = std::map<std::string, Value>;

struct Value {
    std::variant<
        std::monostate,
        bool,
        int,
        std::string,
        ValueList,
        ValueMap,
        TargetType,
        std::shared_ptr<ProjectContext>,
        std::shared_ptr<Function>,
        std::shared_ptr<Target>> data;

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
    explicit Value(std::shared_ptr<Target> target) : data(std::move(target)) {}

    Value& operator+=(const Value& other) {
        std::visit([this, &other]<typename T0>(T0& lhs_val) {
            using T = std::decay_t<T0>;

            if constexpr (std::is_same_v<T, std::monostate>) {
                this->data = other.data;
            }
            else if constexpr (std::is_same_v<T, ValueList>) {
                if (other.is<ValueList>()) {
                    const auto& rhs_list = other.as<ValueList>();
                    lhs_val.insert(lhs_val.end(), rhs_list.begin(), rhs_list.end());
                } else {
                    lhs_val.push_back(other);
                }
            }
            else if constexpr (std::is_same_v<T, std::string>) {
                if (other.is<std::string>()) {
                    lhs_val += other.as<std::string>();
                } else {
                    throw std::runtime_error("Cannot append non-string to string.");
                }
            }
            else if constexpr (std::is_same_v<T, int>) {
                if (other.is<int>()) lhs_val += other.as<int>();
                else throw std::runtime_error("Math requires two integers.");
            }
            else {
                throw std::runtime_error("+= operator not supported for this type.");
            }
        }, data);

        return *this;
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

    template<typename T>
    [[nodiscard]] bool is() const noexcept {
        return std::holds_alternative<T>(data);
    }

    template<typename T>
    [[nodiscard]] const T& as() const {
        if (!is<T>()) {
            throw std::runtime_error("Type mismatch when retrieving value");
        }
        return std::get<T>(data);
    }

    template<typename T>
    [[nodiscard]] T& as() {
        if (!is<T>()) {
            throw std::runtime_error("Type mismatch when attempting to modify");
        }
        return std::get<T>(data);
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