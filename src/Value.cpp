#include "Value.hpp"

#include <format>
#include <ranges>

std::string Value::to_string() const {
    // 1. Если у тебя внутри std::variant (самый частый случай)
    return std::visit([]<typename T0>(T0&& arg) -> std::string {
        using T = std::decay_t<T0>;

        if constexpr (std::is_same_v<T, int>) {
            return std::to_string(arg);
        }
        else if constexpr (std::is_same_v<T, double>) {
            // Чтобы не было лишних нулей, как в обычном to_string(double)
            std::string s = std::to_string(arg);
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if (s.back() == '.') s.pop_back();
            return s;
        }
        else if constexpr (std::is_same_v<T, bool>) {
            return arg ? "true" : "false";
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return arg; // Возвращаем саму строку
        }
        else if constexpr (std::is_same_v<T, std::monostate>) {
            return "null"; // Или "undefined"
        }
        else if constexpr (std::is_same_v<T, ValueList>) {
            if (arg.empty()) return "[]";

            std::string inner = arg
            // 1. Превращаем каждый элемент (Value) в строку
            | std::views::transform([](const auto& item) { return item.to_string(); })
            // 2. Склеиваем их, вставляя ", " между элементами
            | std::views::join_with(std::string_view(", "))
            // 3. Вычисляем весь этот ленивый конвейер и складываем в std::string
            | std::ranges::to<std::string>();

            return std::format("[{}]", inner);
        }
        else if constexpr (std::is_same_v<T, ValueMap>) {
            if (arg.empty()) return "{}";

            std::string inner = arg
                // 1. У мапы элемент — это пара ключ-значение. Делаем из неё строку "key": value
                | std::views::transform([](const auto& kv) {
                    return std::format(R"("{}": {})", kv.first, kv.second.to_string());
                })
                // 2. Склеиваем пары через запятую
                | std::views::join_with(std::string_view(", "))
                // 3. Собираем в строку
                | std::ranges::to<std::string>();

            return std::format("{{{}}}", inner); // {{ и }} в format экранируют фигурные скобки
        }
        else {
            return "<unknown_type>";
        }
    }, data); // 'data' — это твой std::variant
}