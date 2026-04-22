#include "Builder.hpp"

#include "Parser.hpp"
#include "Preprocessor.hpp"

Builder::Builder(const std::shared_ptr<ProjectConfig> &config, const std::shared_ptr<ProjectData> &_project_data)
    : config(config), project_data(_project_data) {
    source_manager = std::make_shared<SourceManager>();
    string_pool = std::make_shared<StringPool>();
    type_registry = std::make_shared<TypeRegistry>();

    for (const auto &path : config->include_paths) {
        source_manager->add_include_path(path);
    }
}

void Builder::build() const {
    if (project_data->execution_plan.contains(config->target)) {
        auto queue = project_data->execution_plan.at(config->target);

        while (!queue.empty()) {
            auto target = queue.front();
            queue.pop();

            std::cout << "  -> Собираем таргет: " << target->name << std::endl;
            compile_target(target);
        }
    }

    const auto& project = project_data->targets.at(config->target);

    compile_target(project);
}

void Builder::compile_target(const std::shared_ptr<Target>& target) const {
    for (const auto& file : target->sources) {
        const auto file_path = fs::path(file.as<std::string>());
        Arena file_arena; // На стеке! Быстрее и проще.
        Preprocessor preproc(source_manager, string_pool, target);

        if (!preproc.push_file(file_path)) {
            std::cerr << "Не удалось загрузить: " << file_path << std::endl;
            continue;
        }

        // 3. Парсинг (Токены -> AST)
        Parser parser(preproc, *string_pool, file_arena);
        const auto ast_root = parser.parse_translation_unit();

        std::cout << "AST Root created.\n";
        std::cout << "Nodes in declarations: " << ast_root->declarations.size() << "\n";

        for (auto* node : ast_root->declarations) {
            // Если ты добавил какой-то базовый механизм получения имен
            // или просто проверяешь через dynamic_cast
            if (const auto* func = static_cast<FunctionDeclNode*>(node)) {
                std::cout << "Found function declaration: " << string_pool->get_string(func->name_id) << "\n";
            }
        }

        // 4. Семантический анализ (AST -> Наполнение TypeRegistry)
        //SemanticAnalyzer analyzer(type_registry);
        //analyzer.analyze(ast_root.get());

        std::cout << "Файл " << file_path.filename() << " обработан." << std::endl;
    }
}
