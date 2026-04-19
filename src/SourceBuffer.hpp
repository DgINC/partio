#pragma once

#include <filesystem>
#include <stdexcept>
#include <iostream>

// Специфичные заголовки для mmap (POSIX: Linux / macOS)
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace fs = std::filesystem;

class SourceBuffer {
public:
    fs::path filepath;
    const uint8_t* data = nullptr;
    size_t size = 0;

private:
    int fd = -1; // Дескриптор файла

public:
    // Конструктор: открываем файл и мапим в память
    explicit SourceBuffer(const fs::path& path) : filepath(path) {
        // 1. Открываем файл только для чтения
        fd = open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::runtime_error("Не удалось открыть файл: " + path.string());
        }

        // 2. Узнаем размер файла
        struct stat sb;
        if (fstat(fd, &sb) == -1) {
            close(fd);
            throw std::runtime_error("Не удалось получить размер файла: " + path.string());
        }
        size = sb.st_size;

        // Если файл пустой, mmap может вернуть ошибку, обрабатываем это
        if (size == 0) {
            data = nullptr;
            return;
        }

        // 3. Проецируем файл в память (mmap)
        // PROT_READ - только чтение, MAP_PRIVATE - изменения не пишутся на диск
        void* mapped = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mapped == MAP_FAILED) {
            close(fd);
            throw std::runtime_error("Ошибка mmap для файла: " + path.string());
        }

        data = static_cast<const uint8_t*>(mapped);
    }

    // Деструктор: подчищаем за собой
    ~SourceBuffer() {
        if (data != nullptr && data != MAP_FAILED) {
            munmap(const_cast<uint8_t*>(data), size);
        }
        if (fd != -1) {
            close(fd);
        }
    }

    // Запрещаем копирование (чтобы не было двойного munmap)
    SourceBuffer(const SourceBuffer&) = delete;
    SourceBuffer& operator=(const SourceBuffer&) = delete;

    // Разрешаем перемещение (полезно для unique_ptr и контейнеров)
    SourceBuffer(SourceBuffer&& other) noexcept {
        filepath = std::move(other.filepath);
        data = other.data;
        size = other.size;
        fd = other.fd;

        other.data = nullptr;
        other.size = 0;
        other.fd = -1;
    }
};