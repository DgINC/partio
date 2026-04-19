#pragma once
#include <vector>
#include <cstdint>
#include <string>

// Виды типов
enum class TypeKind : uint8_t {
    Void,
    Primitive, // int, float, char
    Pointer,   // Указатели (всегда 8 байт на x64)
    Struct     // Пользовательские структуры
};

// Информация о поле внутри структуры
struct FieldInfo {
    uint32_t name_id; // ID имени поля в StringPool
    uint32_t type_id; // ID типа этого поля
    uint32_t offset;  // Вычисленное смещение в байтах
};

// Карточка типа
struct TypeInfo {
    TypeKind kind;
    uint32_t name_id;   // ID имени типа (если есть)
    uint32_t size;      // Общий размер в байтах
    uint32_t alignment; // Требование к выравниванию

    // Заполнено только если kind == Struct
    std::vector<FieldInfo> fields;

    // Заполнено только если kind == Pointer
    uint32_t base_type_id;
};

class TypeRegistry {
private:
    std::vector<TypeInfo> types;

    // Вспомогательная функция для быстрого выравнивания (Битовая магия!)
    // Выравнивает число 'value' до ближайшего числа, кратного 'align'
    // Работает ТОЛЬКО если 'align' - степень двойки (1, 2, 4, 8...)
    static uint32_t align_up(uint32_t value, uint32_t align) {
        return (value + align - 1) & ~(align - 1);
    }

public:
    // Предопределенные ID для базовых типов
    static constexpr uint32_t TYPE_VOID = 0;
    static constexpr uint32_t TYPE_CHAR = 1;
    static constexpr uint32_t TYPE_INT  = 2;
    static constexpr uint32_t TYPE_PTR  = 3; // Универсальный void* для простоты пока

    TypeRegistry() {
        // Инициализируем склад базовыми типами
        // На x64 указатель весит 8 байт и имеет выравнивание 8
        types.push_back({TypeKind::Void,      0, 0, 1, {}, 0}); // 0
        types.push_back({TypeKind::Primitive, 0, 1, 1, {}, 0}); // 1: char (1B)
        types.push_back({TypeKind::Primitive, 0, 4, 4, {}, 0}); // 2: int  (4B)
        types.push_back({TypeKind::Pointer,   0, 8, 8, {}, 0}); // 3: ptr  (8B)
    }

    // Получить тип по ID
    const TypeInfo& get_type(uint32_t id) const {
        return types[id];
    }

    // --- API для Парсера ---

    // 1. Создаем пустую карточку структуры и получаем её будущий ID
    uint32_t begin_struct(uint32_t name_id) {
        uint32_t id = types.size();
        TypeInfo info;
        info.kind = TypeKind::Struct;
        info.name_id = name_id;
        info.size = 0;
        info.alignment = 1; // Минимальное выравнивание
        types.push_back(info);
        return id;
    }

    // 2. Парсер читает поля по одному и добавляет их сюда
    void add_struct_field(uint32_t struct_id, uint32_t field_name_id, uint32_t field_type_id) {
        TypeInfo& struct_info = types[struct_id];
        const TypeInfo& field_type = types[field_type_id];

        // 1. Обновляем выравнивание всей структуры
        // (Оно равно максимальному выравниванию среди всех её полей)
        if (field_type.alignment > struct_info.alignment) {
            struct_info.alignment = field_type.alignment;
        }

        // 2. Вычисляем смещение для текущего поля
        // Смещаем текущий размер до нужного выравнивания (вставляем padding)
        uint32_t offset = align_up(struct_info.size, field_type.alignment);

        // 3. Добавляем поле
        struct_info.fields.push_back({field_name_id, field_type_id, offset});

        // 4. Увеличиваем размер структуры
        struct_info.size = offset + field_type.size;
    }

    // 3. Когда парсер встречает '}', мы финализируем размер структуры
    void finalize_struct(uint32_t struct_id) {
        TypeInfo& struct_info = types[struct_id];

        // Итоговый размер структуры ТОЖЕ должен быть кратен её выравниванию!
        // Это нужно для правильной работы массивов этой структуры.
        struct_info.size = align_up(struct_info.size, struct_info.alignment);
    }
};
