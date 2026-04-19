#pragma once
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <vector>

#include "SourceBuffer.hpp"

namespace fs = std::filesystem;

class SourceManager {
    // Храним файлы в unique_ptr. Когда SourceManager умрет,
    // все деструкторы SourceBuffer отработают автоматически (munmap).
    std::unordered_map<std::string, std::unique_ptr<SourceBuffer>> loaded_files;
    std::vector<fs::path> include_paths;

public:
    void add_include_path(fs::path path) {
        include_paths.push_back(std::move(path));
    }

    const SourceBuffer* load_file(const fs::path& filepath, const fs::path& current_dir, const bool is_system) {
        fs::path resolved_path;

        // --- Логика поиска пути ---
        if (is_system) {
            for (const auto& inc : include_paths) {
                if (fs::exists(inc / filepath)) {
                    resolved_path = inc / filepath;
                    break;
                }
            }
        } else {
            resolved_path = current_dir / filepath;
            if (!fs::exists(resolved_path)) {
                for (const auto& inc : include_paths) {
                    if (fs::exists(inc / filepath)) {
                        resolved_path = inc / filepath;
                        break;
                    }
                }
            }
        }

        if (resolved_path.empty() || !fs::exists(resolved_path)) {
            return nullptr; // Файл не найден
        }

        // C++17/26: Получаем абсолютный, чистый путь (разрешает симлинки и ../)
        std::string canonical = fs::canonical(resolved_path).string();

        // --- Проверка кэша (Header Guard) ---
        if (loaded_files.contains(canonical)) {
            return loaded_files[canonical].get();
        }

        // --- Загрузка нового файла ---
        try {
            // Сразу кладем в map (std::make_unique создает объект и передает владение)
            loaded_files[canonical] = std::make_unique<SourceBuffer>(canonical);

            // И спокойно возвращаем сырой указатель уже из хранилища
            return loaded_files[canonical].get();

        } catch (const std::exception& e) {
            std::cerr << "Предупреждение препроцессора: " << e.what() << '\n';
            return nullptr;
        }
    }
};