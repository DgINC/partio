#pragma once

#include <memory>

#include "Interpreter.hpp"
#include "SourceManager.hpp"

class Builder {
    std::shared_ptr<ProjectConfig> config;
    std::shared_ptr<ProjectData> project_data;
    std::shared_ptr<SourceManager> source_manager;
public:
    Builder(const std::shared_ptr<ProjectConfig>& config, const std::shared_ptr<ProjectData>& _project_data);

    void compile_target();
};
