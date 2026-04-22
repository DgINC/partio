#pragma once

#include <cstdint>
#include <string_view>
#include <vector>
#include <cstring>
#include <iostream>
#include <memory>

class StringPool {
    // Упакованная структура для хэш-таблицы (8 байт)
    struct Entry {
        std::uint32_t id; // Смещение начала строки в Арене
        std::uint32_t length; // Длина строки
    };

    // Вместо одного вектора используем список блоков фиксированного размера
    // Каждый блок — это 64 КБ (или больше), которые никогда не двигаются
    struct Chunk {
        std::unique_ptr<char[]> data;
        size_t used = 0;
        static constexpr size_t CAPACITY = 64 * 1024; // 64KB

        Chunk() : data(std::make_unique<char[]>(CAPACITY)) {}
    };

    std::vector<std::unique_ptr<Chunk>> chunks;
    std::vector<Entry> hash_table;     // Наша таблица
    std::uint32_t active_entries = 0;

    // Пустая строка (ничего не найдено) будет обозначаться как 0
    const std::uint32_t EMPTY_SLOT = 0xFFFFFFFF;

    // Классический супербыстрый хэш FNV-1a
    static std::uint32_t hash_fnv1a(const std::string_view str) {
        std::uint32_t hash = 2166136261u;
        for (const char c : str) {
            hash ^= static_cast<uint8_t>(c);
            hash *= 16777619u;
        }
        return hash;
    }

    static std::uint32_t encode_id(const std::uint32_t chunk_idx, const std::uint32_t offset) {
        return chunk_idx << 24 | offset & 0xFFFFFF;
    }

    static void decode_id(const std::uint32_t id, std::uint32_t& chunk_idx, std::uint32_t& offset) {
        chunk_idx = id >> 24;
        offset = id & 0xFFFFFF;
    }

    void grow_table() {
        const size_t new_size = hash_table.size() * 2;
        const std::vector<Entry> old_table = std::move(hash_table);

        hash_table.assign(new_size, {EMPTY_SLOT, 0});
        const std::uint32_t mask = static_cast<std::uint32_t>(new_size - 1);

        for (const auto& entry : old_table) {
            if (entry.id != EMPTY_SLOT) {
                // Чтобы не пересчитывать хэш FNV, мы можем достать строку и хэшировать заново,
                // либо хранить хэш в Entry. Для простоты — достанем строку.
                const std::string_view s = get_string(entry.id);
                const std::uint32_t h = hash_fnv1a(s);
                std::uint32_t idx = h & mask;

                while (hash_table[idx].id != EMPTY_SLOT) {
                    idx = idx + 1 & mask;
                }
                hash_table[idx] = entry;
            }
        }
    }

public:
    explicit StringPool(const size_t initial_hash_size = 4096) {
        hash_table.assign(initial_hash_size, {EMPTY_SLOT, 0});
        // Сразу создаем первый чанк
        chunks.push_back(std::make_unique<Chunk>());
    }

    std::uint32_t intern(const std::string_view str) {
        // Проверка заполненности таблицы (60% load factor)
        if (active_entries * 100 / hash_table.size() > 60) {
            grow_table();
        }

        const std::uint32_t mask = static_cast<std::uint32_t>(hash_table.size() - 1);
        const std::uint32_t h = hash_fnv1a(str);
        std::uint32_t idx = h & mask;

        while (hash_table[idx].id != EMPTY_SLOT) {
            const std::uint32_t existing_id = hash_table[idx].id;

            if (const std::uint32_t existing_len = hash_table[idx].length; existing_len == str.length()) {
                std::uint32_t c_idx, c_off;
                decode_id(existing_id, c_idx, c_off);
                // Сравниваем символы (пропускаем 4 байта длины)
                if (const char* existing_data = &chunks[c_idx]->data[c_off + sizeof(std::uint32_t)];
                        std::memcmp(existing_data, str.data(), existing_len) == 0) {
                    return existing_id;
                }
            }
            idx = idx + 1 & mask;
        }

        // Добавляем новую строку
        const size_t needed = str.length() + sizeof(std::uint32_t);

        // Проверяем место в текущем чанке (с учетом выравнивания по 4 байта)
        size_t current_used = chunks.back()->used + 3 & ~3;
        if (current_used + needed > Chunk::CAPACITY) {
            chunks.push_back(std::make_unique<Chunk>());
            current_used = 0;
        }

        Chunk& chunk = *chunks.back();
        const std::uint32_t new_id = encode_id(static_cast<std::uint32_t>(chunks.size() - 1), static_cast<std::uint32_t>(current_used));
        const std::uint32_t len = static_cast<std::uint32_t>(str.length());

        // Пишем длину
        std::memcpy(&chunk.data[current_used], &len, sizeof(std::uint32_t));
        // Пишем данные
        std::memcpy(&chunk.data[current_used + sizeof(std::uint32_t)], str.data(), len);

        chunk.used = current_used + needed;

        hash_table[idx] = {new_id, len};
        active_entries++;

        return new_id;
    }

    [[nodiscard]] std::string_view get_string(const std::uint32_t id) const {
        if (id == EMPTY_SLOT) return "";

        std::uint32_t c_idx, c_off;
        decode_id(id, c_idx, c_off);

        if (c_idx >= chunks.size()) return "<invalid id>";

        const Chunk& chunk = *chunks[c_idx];
        std::uint32_t len;
        std::memcpy(&len, &chunk.data[c_off], sizeof(std::uint32_t));

        return { &chunk.data[c_off + sizeof(std::uint32_t)], len };
    }
};
