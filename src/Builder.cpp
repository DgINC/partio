#include "Builder.hpp"

#include "Preprocessor.hpp"

Builder::Builder(const std::shared_ptr<ProjectConfig> &config, const std::shared_ptr<ProjectData> &_project_data)
    : config(config), project_data(_project_data) {
    source_manager = std::make_shared<SourceManager>();
}

void Builder::compile_target() {
    const auto &target = project_data->targets.at(config->target);
    auto &plan = project_data->execution_plan.at(config->target);

    for (auto &module = plan.front(); !plan.empty(); plan.pop()) {

    }
    // 2. Создаем препроцессор, передав ему только ссылки на данные
    Preprocessor preproc(source_manager, string_pool, target->include_dirs);

    // 3. Поехали!
    preproc.push_file(target->main_source_file);

    Parser parser(preproc, type_registry);
    parser.parse_to_hlir();
}
