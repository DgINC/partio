#pragma once

#include <string>
#include <map>
#include <memory>

#include "ProjectContext.hpp"

// Глобальный объект проекта
class Project {
public:
    std::string name;
    
    // Все цели сборки, индексированные по имени
    std::map<std::string, std::shared_ptr<Target>> targets;
    
    // Глобальные семафоры для управления параллелизмом
    std::map<std::string, int> semaphores;
    
    // Переменные окружения и внутренние переменные проекта
    std::map<std::string, std::string> variables;

    void add_target(const std::shared_ptr<Target> &target) {
        targets[target->name] = target;
    }
};