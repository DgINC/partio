#pragma once

#include <memory>

#include "Interpreter.hpp"
#include "SourceManager.hpp"
#include "StringPool.hpp"
#include "TypeRegistry.hpp"

class Builder {
    std::shared_ptr<ProjectConfig> config;
    std::shared_ptr<ProjectData> project_data;

    std::shared_ptr<SourceManager> source_manager;
    std::shared_ptr<StringPool> string_pool;
    std::shared_ptr<TypeRegistry>  type_registry;
public:
    Builder(const std::shared_ptr<ProjectConfig>& config, const std::shared_ptr<ProjectData>& _project_data);

    void build() const;

    void compile_target(const std::shared_ptr<Target>& target) const;
};
