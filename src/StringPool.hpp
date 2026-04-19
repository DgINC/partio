#pragma once

#include <cstdint>
#include <string_view>
#include <vector>
#include <cstring>
#include <iostream>

class StringPool {
    // Упакованная структура для хэш-таблицы (8 байт)
    struct Entry {
        uint32_t offset; // Смещение начала строки в Арене
        uint32_t length; // Длина строки
    };

    std::vector<char> arena;           // Гигантский склад символов
    std::vector<Entry> hash_table;     // Наша таблица
    uint32_t active_entries = 0;

    // Пустая строка (ничего не найдено) будет обозначаться как 0
    const uint32_t EMPTY_SLOT = 0xFFFFFFFF;

    // Классический супербыстрый хэш FNV-1a
    static uint32_t hash_fnv1a(const std::string_view str) {
        uint32_t hash = 2166136261u;
        for (const char c : str) {
            hash ^= static_cast<uint8_t>(c);
            hash *= 16777619u;
        }
        return hash;
    }

    void grow_table() {
        // Увеличиваем таблицу (всегда степень двойки для быстрого взятия остатка)
        const size_t new_size = hash_table.empty() ? 4096 : hash_table.size() * 2;
        std::vector<Entry> new_table(new_size, {EMPTY_SLOT, 0});

        // Перехэшируем старые элементы
        for (const auto& entry : hash_table) {
            if (entry.offset != EMPTY_SLOT) {
                const std::string_view s(&arena[entry.offset], entry.length);
                uint32_t idx = hash_fnv1a(s) & new_size - 1; // Быстрое деление по модулю

                while (new_table[idx].offset != EMPTY_SLOT) {
                    idx = idx + 1 & new_size - 1;
                }
                new_table[idx] = entry;
            }
        }
        hash_table = std::move(new_table);
    }

public:
    StringPool() {
        // Резервируем память, чтобы избежать лишних переаллокаций
        arena.reserve(1024 * 1024); // 1 МБ символов со старта
        grow_table();

        // Положим пустую строку под индексом 0 на всякий случай
        intern("");
    }

    // Главная функция: скармливаешь текст, получаешь ID
    uint32_t intern(const std::string_view str) {
        // Если таблица заполнена больше чем на 60%, расширяем
        if (active_entries * 100 / hash_table.size() > 60) {
            grow_table();
        }

        const uint32_t mask = hash_table.size() - 1;
        uint32_t idx = hash_fnv1a(str) & mask;

        // Ищем место или существующую строку (Linear Probing)
        while (hash_table[idx].offset != EMPTY_SLOT) {
            const uint32_t existing_offset = hash_table[idx].offset;

            // Если длины совпали, проверяем сами символы
            if (const uint32_t existing_len = hash_table[idx].length;
                existing_len == str.length() &&
                std::memcmp(&arena[existing_offset], str.data(), existing_len) == 0) {
                // Нашли! Возвращаем её смещение как уникальный ID
                return existing_offset;
            }

            // Коллизия: идем в следующую ячейку
            idx = idx + 1 & mask;
        }

        // Если мы тут, значит строки нет. Добавляем её!
        const uint32_t new_offset = static_cast<uint32_t>(arena.size());
        const uint32_t new_length = static_cast<uint32_t>(str.length());

        // Копируем символы в конец Арены
        arena.insert(arena.end(), str.begin(), str.end());

        // Записываем в таблицу
        hash_table[idx] = {new_offset, new_length};
        active_entries++;

        // Возвращаем смещение в Арене как ID
        // (Оно гарантированно уникально для каждой строки)
        return new_offset;
    }

    // Обратное преобразование: по ID получить строку (нужно для дебага и вывода ошибок)
    std::string_view get_string(const uint32_t id) const {
        // ID - это просто смещение в арене, но нам нужно узнать длину.
        // Чтобы не искать по хэш-таблице, можно использовать небольшую хитрость,
        // но для простоты здесь предполагается, что лексер сам знает длину,
        // либо мы можем хранить длину прямо перед строкой в Арене.
        // Для текущего примера возвращаем указатель (чуть позже допилим структуру ID).
        return std::string_view(&arena[id]);
    }
};
