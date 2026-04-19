#include "Interpreter.hpp"

#include <utility>
#include "Value.hpp"
#include "YACSLexer.h"

std::regex patternToRegex(const std::string& pattern) {
    std::string r = pattern;
    size_t pos = 0;
    while ((pos = r.find('.', pos)) != std::string::npos) { r.replace(pos, 1, "\\."); pos += 2; }
    pos = 0;
    while ((pos = r.find('*', pos)) != std::string::npos) { r.replace(pos, 1, ".*"); pos += 2; }
    return std::regex(r, std::regex_constants::ECMAScript | std::regex_constants::icase);
}

void Interpreter::set_project_ctx(std::shared_ptr<ProjectContext> _project_ctx) {
    project_ctx = std::move(_project_ctx);
}

std::any Interpreter::visitArg_list(YACSParser::Arg_listContext *ctx) {
    std::vector<Value> parameters;

    // Получаем все дочерние узлы 'arg'

    for (const auto args = ctx->arg(); auto* arg_node : args) {
        if (!arg_node || !arg_node->expr()) continue;

        // Выполняем обход выражения

        // Проверяем, не пустой ли результат, прежде чем кастить
        if (antlrcpp::Any visited_result = visit(arg_node->expr()); visited_result.has_value()) {
            try {
                auto val = std::any_cast<Value>(visited_result);
                parameters.push_back(val);
            } catch (const std::bad_any_cast& e) {
                // Если в visit(expr) что-то пошло не так и вернулся не Value
                std::cerr << "Ошибка типа в аргументе: " << e.what() << std::endl;
            }
        }
    }

    // Отдаем собранный массив наверх
    return parameters;
}

std::any Interpreter::visitQualified_id(YACSParser::Qualified_idContext *ctx) {
    std::string result; // Создаем явный объект

    const auto ids = ctx->IDENTIFIER();
    for (size_t i = 0; i < ids.size(); ++i) {
        result += ids[i]->getText();
        if (i + 1 < ids.size()) result += ":";
    }

    Value test_val(result);
    antlrcpp::Any any_val = test_val;
    return any_val;
}

std::any Interpreter::visitFor_stmt(YACSParser::For_stmtContext *ctx) {
    // 1. Вычисляем коллекцию
    auto container = std::any_cast<Value>(visit(ctx->expr()));

    // Достаем имена переменных из param_list (например, k, v)
    const auto params = ctx->param_list()->param();
    const std::string varName1 = params[0]->IDENTIFIER()->getText();
    const std::string varName2 = params.size() > 1 ? params[1]->IDENTIFIER()->getText() : "";

    auto executeBody = [&](const Value& k, const Value& v) {
        std::map<std::string, Value> loopScope;
        if (varName2.empty()) {
            loopScope[varName1] = v; // for (val in list)
        } else {
            loopScope[varName1] = k; // for (key, val in map)
            loopScope[varName2] = v;
        }

        scopes.push_back(loopScope);
        try {
            for (auto* stmt : ctx->statement()) visit(stmt);
        } catch (...) {
            scopes.pop_back();
            throw; // Пробрасываем Break/Continue/Return дальше
        }
        scopes.pop_back();
    };

    try {
        if (container.is<ValueList>()) {
            const auto& list = container.as<ValueList>();
            for (size_t i = 0; i < list.size(); ++i) {
                executeBody(Value(static_cast<int>(i)), list[i]);
            }
        } else if (container.is<ValueMap>()) {
            for (auto const& [k, v] : container.as<ValueMap>()) {
                executeBody(Value(k), v);
            }
        }
    } catch (const BreakSignal&) { /* тихо выходим из цикла */ }
    catch (const ContinueSignal&) { /* ловим внутри итерации (в блоке выше) */ }

    return {};
}

std::any Interpreter::visitIndexAccess(YACSParser::IndexAccessContext *ctx) {
    auto container = std::any_cast<Value>(visit(ctx->expr(0)));
    auto index = std::any_cast<Value>(visit(ctx->expr(1)));

    if (container.is<ValueMap>()) {
        return container.as<ValueMap>().at(index.to_string());
    }
    if (container.is<ValueList>()) {
        return container.as<ValueList>().at(index.as<int>());
    }
    throw std::runtime_error("Trying to index something that is not a container");
}

std::any Interpreter::visitProject_file(YACSParser::Project_fileContext *ctx) {
    for (const auto block : ctx->project_block()) {
        if (!block->project_body()) continue;

        for (auto* child : block->project_body()->children) {
            if (auto* t_ctx = dynamic_cast<YACSParser::Target_blockContext*>(child)) {
                // Если его нет — создаем пустой объект
                if (std::string name = t_ctx->IDENTIFIER()->getText(); !project_ctx->project_data->targets.contains(name)) {
                    auto ptr = std::make_shared<Target>(name);
                    project_ctx->project_data->targets.try_emplace(name, std::move(ptr));
                }
            }
        }
    }

    auto result = visitChildren(ctx);

    if (hasCycles()) {
        throw std::runtime_error("Build aborted due to circular dependencies.");
    }

    buildExecutionPlans();

    return result;
}

std::any Interpreter::visitImport_stmt(YACSParser::Import_stmtContext *ctx) {
    const std::string fileName = ctx->IDENTIFIER(0)->getText() + ".yacs";
    const std::string alias = ctx->IDENTIFIER().size() > 1
                        ? ctx->IDENTIFIER(1)->getText()
                        : ctx->IDENTIFIER(0)->getText();

    std::filesystem::path importPath = project_ctx->root / fileName;

    if (!std::filesystem::exists(importPath)) {
        throw std::runtime_error("File not found: " + importPath.string());
    }

    const auto importContext = std::make_shared<ProjectContext>();
    importContext->root = project_ctx->root;
    importContext->project_data = project_ctx->project_data;

    // Создаем хранилище
    const auto ast = std::make_shared<ParsedFile>();
    ast->stream = std::make_unique<std::ifstream>(importPath);
    ast->input = std::make_unique<antlr4::ANTLRInputStream>(*ast->stream);
    ast->lexer = std::make_unique<YACSLexer>(ast->input.get());
    ast->tokens = std::make_unique<antlr4::CommonTokenStream>(ast->lexer.get());
    ast->parser = std::make_unique<YACSParser>(ast->tokens.get());

    // Сохраняем В КОНТЕКСТ импортируемого проекта
    importContext->ast_cache.push_back(ast);

    // Выполняем
    Interpreter importInterpreter(_config, project_ctx->project_data);
    importInterpreter.set_project_ctx(importContext);
    importInterpreter.visit(ast->parser->project_file());

    // Регистрируем проект в текущем проекте
    project_ctx->globals[alias] = Value(importContext);

    return nullptr;
}

std::any Interpreter::visitAdditiveExpr(YACSParser::AdditiveExprContext *ctx) {
    const auto left = std::any_cast<Value>(visit(ctx->left));
    const auto right = std::any_cast<Value>(visit(ctx->right));

    if (ctx->PLUS()) {
        // Сложение строк
        if (std::holds_alternative<std::string>(left.data) && std::holds_alternative<std::string>(right.data)) {
            return std::make_any<Value>(Value(std::get<std::string>(left.data) + std::get<std::string>(right.data)));
        }
        // Сложение списков (ValueList + ValueList)
        if (std::holds_alternative<ValueList>(left.data) && std::holds_alternative<ValueList>(right.data)) {
            auto res = std::get<ValueList>(left.data);
            const auto& rList = std::get<ValueList>(right.data);
            res.insert(res.end(), rList.begin(), rList.end());
            return std::make_any<Value>(Value(res));
        }
        // Сложение чисел
        if (std::holds_alternative<int>(left.data) && std::holds_alternative<int>(right.data)) {
            return std::make_any<Value>(Value(std::get<int>(left.data) + std::get<int>(right.data)));
        }
    }
    // Для "-" можно добавить логику удаления элементов из списка или вычитания чисел

    throw std::runtime_error("Incompatible types for operation '");
}

std::any Interpreter::visitReturn_stmt(YACSParser::Return_stmtContext *ctx) {
    Value val;
    if (ctx->expr()) {
        val = std::any_cast<Value>(visit(ctx->expr()));
    }
    throw ReturnSignal(val);
}

std::any Interpreter::visitFunction_def(YACSParser::Function_defContext *ctx) {
    std::string name = ctx->IDENTIFIER()->getText();
    if (!currentNamespace.empty()) {
        name = currentNamespace + ":" + name;
    }
    Function func;
    func.name = name;
    func.context = ctx; // Сохраняем указатель на дерево

    // Разбираем параметры
    if (ctx->param_list()) {
        for (auto* paramCtx : ctx->param_list()->param()) {
            func.params.push_back({
                paramCtx->type_spec()->getText(),
                paramCtx->IDENTIFIER()->getText()
            });
        }
    }

    project_ctx->functions[name] = func;
    return nullptr;
}

std::any Interpreter::visitNamespace_block(YACSParser::Namespace_blockContext *ctx) {
    const std::string nsName = ctx->IDENTIFIER()->getText();

    // Сохраняем старый префикс, чтобы восстановить его в конце
    const std::string oldNamespace = currentNamespace;

    // Формируем новый префикс. Если мы уже в неймспейсе, добавляем через двоеточие
    if (currentNamespace.empty()) {
        currentNamespace = nsName;
    } else {
        currentNamespace += ":" + nsName;
    }

    visitChildren(ctx);

    currentNamespace = oldNamespace;

    return nullptr;
}

std::any Interpreter::visitComparisonExpr(YACSParser::ComparisonExprContext *ctx) {
    // 1. Используем именованные метки из грамматики!
    // ANTLR автоматически сгенерирует методы left и right()
    const auto leftVal = std::any_cast<Value>(visit(ctx->left));
    const auto rightVal = std::any_cast<Value>(visit(ctx->right));

    // 2. Определяем тип оператора
    // Мы можем проверить наличие конкретного токена через ctx->EQUAL_TO() и т.д.
    bool result = false;

    // Для == и != используем встроенную магию std::variant
    if (ctx->EQUAL_TO()) {
        result = leftVal == rightVal;
    }
    else if (ctx->NOT_EQUAL_TO()) {
        result = leftVal != rightVal;
    }
    // Для сравнений <, >, <=, >= проверяем, что оба операнда — числа
    else {
        if (std::holds_alternative<int>(leftVal.data) && std::holds_alternative<int>(rightVal.data)) {
            const int l = std::get<int>(leftVal.data);
            const int r = std::get<int>(rightVal.data);

            if (ctx->LESS_THAN())                result = l < r;
            else if (ctx->GREATER_THAN())           result = l > r;
            else if (ctx->LESS_THAN_OR_EQUAL_TO())  result = l <= r;
            else if (ctx->GREATER_THAN_OR_EQUAL_TO()) result = l >= r;
        } else {
            throw std::runtime_error("Comparison operators (except == and !=) apply only to numbers.");
        }
    }

    return std::make_any<Value>(Value(result));
}

std::any Interpreter::visitIf_stmt(YACSParser::If_stmtContext *ctx) {
    // 1. Вычисляем условие внутри 'if (expr)'
    const auto condVal = std::any_cast<Value>(visit(ctx->expr()));

    // 2. Достаем логическое значение из Value
    bool condition = false;
    if (std::holds_alternative<bool>(condVal.data)) {
        condition = std::get<bool>(condVal.data);
    } else {
        // Если в условии не bool (например, if ("строка")), кидаем ошибку
        throw std::runtime_error("The condition in 'if' must be of logical type (bool)!");
    }

    // 3. Умный обход дочерних элементов (чтобы отделить if-блок от else-блока)
    int blockLevel = 0;      // 1 = мы в блоке if, 2 = мы в блоке else
    bool executeMode = false; // Флаг: нужно ли выполнять текущий statement

    for (auto* child : ctx->children) {
        // Проверяем, является ли узел "терминальным" (то есть ключевым словом или скобкой)

        if (auto* terminalNode = dynamic_cast<antlr4::tree::TerminalNode*>(child)) {
            if (const std::string text = terminalNode->getText(); text == "{") {
                blockLevel++; // Зашли в новые фигурные скобки

                // Если мы в первом блоке (if) и условие ИСТИННО -> выполняем
                if (blockLevel == 1 && condition) {
                    executeMode = true;
                }
                // Если мы во втором блоке (else) и условие ЛОЖНО -> выполняем
                else if (blockLevel == 2 && !condition) {
                    executeMode = true;
                }
                else {
                    executeMode = false; // Иначе пропускаем
                }
            }
            else if (text == "}") {
                executeMode = false; // Вышли из блока, выключаем выполнение
            }
        }
        // Если это не терминальный узел (значит это statement) И флаг включен
        else if (executeMode) {
            visit(child); // Выполняем полезную нагрузку (переменные, присваивания и т.д.)
        }
    }

    return nullptr;
}

std::any Interpreter::visitBoolLiteral(YACSParser::BoolLiteralContext *ctx) {
    const std::string text = ctx->getText();

    const bool boolValue = text == "true";

    return std::make_any<Value>(Value(boolValue));
}

Interpreter::Interpreter(const std::shared_ptr<ProjectConfig>& config, const std::shared_ptr<ProjectData>& _project_data) :
        _config(config) {

    project_ctx = std::make_shared<ProjectContext>();
    project_ctx->project_data = _project_data;
    project_ctx->globals["ARCH"] = Value(config->arch);
    project_ctx->globals["MODE"] = Value(config->build_mode);

    project_ctx->globals["executable"] = Value(TargetType::Executable);
    project_ctx->globals["library"]    = Value(TargetType::Library);
    project_ctx->globals["sources"]    = Value(TargetType::Sources);
    project_ctx->globals["generator"]  = Value(TargetType::Generator);
    project_ctx->globals["dumb"]       = Value(TargetType::Dumb);

    scopes.emplace_back();

    builtins["message"] = wrap_builtin([](const std::map<std::string, Value>& args) -> Value {
        std::string level = "LOG";
        if (args.contains("level")) level = args.at("level").to_string();

        if (args.contains("text")) {
            std::cout << "[" << level << "] " << args.at("text").to_string() << std::endl;
        }
        return Value(true);
    });

    builtins["print"] = wrap_builtin([](const std::map<std::string, Value>& args) {
        if (args.empty()) {
            throw std::runtime_error("print() ожидает аргумент");
        }

        // Ищем команду по приоритету: "exec" -> "0"
        auto it = args.find("text");
        if (it == args.end()) it = args.find("0");

        if (it == args.end()) {
            throw std::runtime_error("No string passed to print() (needs a positional argument or 'text=...')");
        }

        const std::string cmd = it->second.to_string();

        std::cout << cmd << std::endl;
    });
    // Обновленная регистрация glob
    builtins["glob"] = wrap_builtin([](const std::map<std::string, Value>& args) -> Value {
        ValueList results;
        std::vector<std::string> patterns;

        // 1. Извлекаем паттерны из аргумента "patterns" или "0" (первый позиционный)
        Value val;
        if (args.contains("patterns")) val = args.at("patterns");
        else if (args.contains("0")) val = args.at("0");

        if (val.is<ValueList>()) {
            for (const auto& item : std::get<ValueList>(val.data)) patterns.push_back(item.as<std::string>());
        } else if (val.is<std::string>()) {
            patterns.push_back(val.as<std::string>());
        }

        // 2. Сканируем
        for (const auto& pStr : patterns) {
            std::filesystem::path p(pStr);
            std::string mask = p.filename().string();
            std::string dir = p.parent_path().empty() ? "." : p.parent_path().string();

            if (!std::filesystem::exists(dir)) continue;

            auto re = patternToRegex(mask);
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (std::filesystem::is_regular_file(entry)) {
                    if (std::regex_match(entry.path().filename().string(), re)) {
                        results.emplace_back(entry.path().generic_string());
                    }
                }
            }
        }

        // 3. Сортировка для детерминизма
        std::ranges::sort(results, [](const Value& a, const Value& b) {
            return a.as<std::string>() < b.as<std::string>();
        });

        return Value(results);
    });

    builtins["fetch"] = wrap_builtin([this](const std::map<std::string, Value>& args) -> Value {
        if (!args.contains("url")) {
            throw std::runtime_error("fetch(): 'url' is required");
        }

        auto url = args.at("url").as<std::string>();
        std::string version = args.contains("version") ? args.at("version").as<std::string>() : "";
        std::string sources_dir = args.contains("sources_dir") ? args.at("sources_dir").as<std::string>() : "";

        std::filesystem::path final_path;

        // 1. Определяем: это URL или локальный путь?

        if (url.find("://") != std::string::npos || url.find("git@") == 0) {
            // Логика для удаленного репозитория
            std::string name = args.contains("name") ? args.at("name").as<std::string>() : "";
            if (name.empty()) {
                name = url.substr(url.find_last_of('/') + 1);
                if (name.ends_with(".git")) name.erase(name.size() - 4);
            }

            final_path = std::filesystem::current_path() / ".yacs" / "deps" / name;

            if (!std::filesystem::exists(final_path)) {
                std::string cmd = "git clone " + url + " " + final_path.string();
                if (!version.empty()) cmd += " --branch " + version;
                cmd += " --depth 1";

                if (std::system(cmd.c_str()) != 0) {
                    throw std::runtime_error("fetch(): Failed to clone " + url);
                }
            }
        } else {
            // Логика для локального пути
            final_path = std::filesystem::absolute(url);
            if (!std::filesystem::exists(final_path)) {
                throw std::runtime_error("fetch(): Local path not found: " + url);
            }
        }

        // 2. Учитываем sources_dir, если он указан
        std::filesystem::path project_root = final_path;
        if (!sources_dir.empty()) {
            project_root /= sources_dir;
        }

        // 3. Создаем контекст модуля
        auto moduleContext = std::make_shared<ProjectContext>();
        moduleContext->root = project_root; // Запоминаем корень!
        moduleContext->project_data = project_ctx->project_data;

        // 4. Ищем и запускаем build.yacs внутри этого модуля
        if (std::filesystem::path build_file = project_root / "build.yacs"; std::filesystem::exists(build_file)) {
            std::ifstream stream(build_file);
            antlr4::ANTLRInputStream input(stream);
            YACSLexer lexer(&input);
            antlr4::CommonTokenStream tokens(&lexer);
            YACSParser parser(&tokens);

            // Рекурсивно запускаем интерпретатор для подпроекта
            // Важно: передаем новый moduleContext как основной проект для той копии
            Interpreter subInterpreter(_config, project_ctx->project_data);
            subInterpreter.set_project_ctx(moduleContext);
            subInterpreter.visit(parser.project_file());
        }

        return Value(moduleContext);
    });

    builtins["shell"] = wrap_builtin([](const std::map<std::string, Value>& args) -> Value {
        if (args.empty()) {
            throw std::runtime_error("shell() ожидает аргумент");
        }

        // Ищем команду по приоритету: "exec" -> "0"
        auto it = args.find("exec");
        if (it == args.end()) it = args.find("0");

        if (it == args.end()) {
            throw std::runtime_error("В shell() не передана строка команды (нужен позиционный аргумент или 'exec=...') ");
        }

        const std::string cmd = it->second.to_string();

        if (cmd.empty()) {
            return Value(0); // Или ошибка, если пустые команды запрещены
        }

        std::cout << "[SHELL EXEC] " << cmd << std::endl;

        const int res = std::system(cmd.c_str());

        return Value(res);
    });
}

std::any Interpreter::visitVariable_decl(YACSParser::Variable_declContext *ctx) {
    std::string name = ctx->IDENTIFIER()->getText();
    Value val;
    if (ctx->expr()) {
        val = std::any_cast<Value>(visit(ctx->expr()));
    }

    // Если мы внутри функции (scopes > 1), это локальная переменная.
    // Если scopes == 1, это переменная таргета.
    if (scopes.size() == 1 && !currentNamespace.empty()) {
        name = currentNamespace + ":" + name;
    }

    // Сохраняем в текущий scope (самый верхний на стеке)
    // Именно отсюда resolveVariable будет её забирать!
    scopes.back()[name] = val;

    // Дополнительно дублируем в target, если нужно для компиляции
    if (currentTarget != nullptr) {
        (*currentTarget)[name] = val;
    }

    return nullptr;
}

antlrcpp::Any Interpreter::visitFunctionCall(YACSParser::FunctionCallContext *ctx) {
    std::string fullName = ctx->qualified_id()->getText();
    auto idents = ctx->qualified_id()->IDENTIFIER();

    // 1. СБОР АРГУМЕНТОВ
    std::map<std::string, Value> finalArgs;
    if (ctx->arg_list()) {
        int posCounter = 0;
        for (auto* argCtx : ctx->arg_list()->arg()) {
            auto val = std::any_cast<Value>(visit(argCtx->expr()));
            if (argCtx->IDENTIFIER()) {
                // Именованный: func(mask="*.cpp")
                finalArgs[argCtx->IDENTIFIER()->getText()] = val;
            } else {
                // Позиционный: func("*.cpp")
                finalArgs[std::to_string(posCounter++)] = val;
            }
        }
    }

    // --- 2. РЕЗОЛВИНГ ФУНКЦИИ И КОНТЕКСТА ---
    Function* targetFunc = nullptr;
    std::shared_ptr<ProjectContext> targetProjectScope = project_ctx;

    if (idents.size() == 1) {
        std::string name = idents[0]->getText();
        if (builtins.contains(name)) return std::make_any<Value>(builtins[name](finalArgs));
        if (project_ctx->functions.contains(name)) targetFunc = &project_ctx->functions[name];
    }
    else {
        // Случай lib:foo:bar или lib:bar
        std::string first = idents[0]->getText();

        try {
            if (Value base = resolveVariable(first); base.is<std::shared_ptr<ProjectContext>>()) {
                targetProjectScope = base.as<std::shared_ptr<ProjectContext>>();

                // Склеиваем ВЕСЬ остаток в одну строку через двоеточие
                // Если вызвали lib:foo:bar, то tail будет "foo:bar"
                std::string tail;
                for (size_t i = 1; i < idents.size(); ++i) {
                    tail += (i > 1 ? ":" : "") + idents[i]->getText();
                }

                // Твой visitFunction_def сохранил функцию именно так!
                if (targetProjectScope->functions.contains(tail)) {
                    targetFunc = &targetProjectScope->functions[tail];
                }
            }
        } catch (...) {
            // Если через resolveVariable не нашли, пробуем полное имя в текущем проекте
            if (project_ctx->functions.contains(fullName)) {
                targetFunc = &project_ctx->functions[fullName];
                targetProjectScope = project_ctx;
            }
        }
    }

    // Если путь не сработал, ищем "плоское" имя в текущем контексте
    if (!targetFunc) {
        if (project_ctx->functions.contains(fullName)) {
            targetFunc = &project_ctx->functions[fullName];
            targetProjectScope = project_ctx;
        }
    }

    if (!targetFunc) {
        throw std::runtime_error("Unknown function: " + fullName);
    }

    // --- 3. ПОДГОТОВКА ЛОКАЛЬНОГО СКОУПА И ПРОВЕРКА ТИПОВ ---
    auto&[name, params, context] = *targetFunc;
    std::map<std::string, Value> localScope;

    for (size_t i = 0; i < params.size(); ++i) {
        std::string paramName = params[i].name;
        std::string expectedType = params[i].type;
        std::string posKey = std::to_string(i);

        Value argVal;
        bool isProvided = false;

        if (finalArgs.contains(paramName)) {
            argVal = finalArgs[paramName];
            isProvided = true;
        } else if (finalArgs.contains(posKey)) {
            argVal = finalArgs[posKey];
            isProvided = true;
        }

        if (!isProvided) {
            throw std::runtime_error("ОШИБКА: Функция '" + fullName + "' требует аргумент '" + paramName + "'!");
        }

        // Проверка типов
        bool typeMatch = false;
        if (expectedType == "string" && argVal.is<std::string>()) typeMatch = true;
        else if (expectedType == "list" && argVal.is<ValueList>()) typeMatch = true;
        else if (expectedType == "int" && argVal.is<int>()) typeMatch = true;
        else if (expectedType == "bool" && argVal.is<bool>()) typeMatch = true;
        else if (expectedType == "auto" || expectedType == "any") typeMatch = true;

        if (!typeMatch) {
            throw std::runtime_error("ОШИБКА ТИПА: Функция '" + fullName + "' ожидает '" +
                                     expectedType + "' для '" + paramName + "'");
        }

        localScope[paramName] = argVal;
    }

    // --- 4. ВЫПОЛНЕНИЕ С ПРАВИЛЬНЫМ КОНТЕКСТОМ ---
    std::shared_ptr<ProjectContext> oldProject = project_ctx;
    std::string oldNamespace = currentNamespace;

    project_ctx = targetProjectScope;
    // Определяем неймспейс функции (например, из "lib:foo:bar" получаем "lib:foo")
    if (name.find(':') != std::string::npos) {
        currentNamespace = name.substr(0, name.find_last_of(':'));
    } else {
        currentNamespace = "";
    }

    scopes.push_back(localScope);
    Value result;

    try {
        auto* funcCtx = context;
        const auto& children = funcCtx->children;

        // Находим границы тела функции (между { и })
        size_t start = 0;
        size_t end = children.size();

        for (size_t i = 0; i < children.size(); ++i) {
            if (children[i]->getText() == "{") start = i + 1;
            if (children[i]->getText() == "}") end = i;
        }

        // Итерируемся по детям внутри скобок
        for (size_t i = start; i < end; ++i) {
            auto* child = children[i];

            // 1. Если это обычный statement (if, присваивание и т.д.)
            if (auto* stmt = dynamic_cast<YACSParser::StatementContext*>(child)) {
                visit(stmt);
            }
            // 2. Если мы наткнулись на токен 'return'
            else if (child->getText() == "return") {
                // В твоей грамматике: 'return' expr ';'
                // Значит, следующий ребенок (i + 1) — это само выражение expr
                if (i + 1 < end) {
                    if (auto* exprNode = dynamic_cast<YACSParser::ExprContext*>(children[i + 1])) {
                        result = std::any_cast<Value>(visit(exprNode));
                        throw ReturnSignal(result); // Прерываем выполнение функции
                    }
                }
            }
        }
    } catch (const ReturnSignal& sig) {
        result = sig.getValue();
    }

    // --- 4. ВОЗВРАТ КОНТЕКСТА ---
    scopes.pop_back();
    currentNamespace = oldNamespace;
    project_ctx = oldProject;

    return result;
}

Value Interpreter::evaluateExpression(const std::string& expression) {
    // 1. Создаем поток ввода из строки
    antlr4::ANTLRInputStream input(expression);

    // 2. Настраиваем лексер и парсер специально для этого кусочка
    YACSLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    YACSParser parser(&tokens);

    // Удаляем стандартные обработчики ошибок, чтобы не мусорить в консоль
    parser.removeErrorListeners();

    // 3. Парсим именно как выражение (правило expr в твоей грамматике)
    auto tree = parser.expr();

    // 4. ГЛАВНЫЙ МОМЕНТ: Используем текущий экземпляр визитора (this),
    // чтобы контекст переменных (scopes) остался тем же самым!
    return std::any_cast<Value>(visit(tree));
}

bool Interpreter::hasCycles() const {
    // Состояния: 0 = не были, 1 = в процессе (в стеке), 2 = всё проверено
    // В качестве ключа используем сырой указатель (Target*), так как он уникален
    std::unordered_map<Target*, int> state;
    const auto& targetsMap = project_ctx->project_data->targets;

    // Заполняем начальные состояния
    for (const auto& target_ptr : targetsMap | std::views::values) {
        state[target_ptr.get()] = 0;
    }

    // Лямбда для рекурсивного обхода (DFS)
    // path теперь хранит shared_ptr, чтобы в случае ошибки вытащить имена
    auto dfs = [&](this auto& self, const std::shared_ptr<Target>& node, std::vector<std::shared_ptr<Target>>& path) -> bool {
        Target* raw_ptr = node.get();
        state[raw_ptr] = 1; // Зашли (пометили как "в обработке")
        path.push_back(node);

        // Теперь итерируемся по ValueList depends
        for (const auto& dep_val : node->depends) {
            // Проверяем, что в Value реально лежит таргет
            if (!dep_val.is<std::shared_ptr<Target>>()) {
                continue;
            }

            auto dep_ptr = dep_val.as<std::shared_ptr<Target>>();
            Target* raw_dep_ptr = dep_ptr.get();

            // Если таргет уже в стеке текущего обхода — мы нашли цикл
            if (state[raw_dep_ptr] == 1) {
                path.push_back(dep_ptr);

                std::cerr << "[FATAL] Circular dependency detected:\nTrace: ";
                for (size_t i = 0; i < path.size(); ++i) {
                    // Здесь предполагается, что у Target есть поле name.
                    // Если нет — можно выводить адреса или искать имя в targetsMap
                    std::cerr << path[i]->name << (i == path.size() - 1 ? " [RECURSION!]" : " -> ");
                }
                std::cerr << std::endl;
                return true;
            }

            // Если еще не проверяли этот таргет — идем вглубь
            if (state[raw_dep_ptr] == 0) {
                if (self(dep_ptr, path)) return true;
            }
        }

        state[raw_ptr] = 2; // Пометили как полностью проверенный
        path.pop_back();
        return false;
    };

    std::vector<std::shared_ptr<Target>> path;
    // Прогоняем каждый таргет из карты проекта
    for (const auto& target_ptr : targetsMap | std::views::values) {
        if (state[target_ptr.get()] == 0) {
            if (dfs(target_ptr, path)) return true;
        }
    }

    return false;
}

void Interpreter::buildExecutionPlans() const {
    const auto& data = project_ctx->project_data;
    data->execution_plan.clear();

    for (const auto& [name, root_target_ptr] : data->targets) {

        std::unordered_set<Target*> visited; // Используем сырой указатель как уникальный ID объекта
        std::queue<std::shared_ptr<Target>> current_plan;

        // Рекурсивный DFS по указателям
        std::function<void(const std::shared_ptr<Target>&)> dfs = [&](const std::shared_ptr<Target>& t) {
            if (!t || visited.contains(t.get())) return;

            for (const auto& dep_value : t->depends) {
                if (dep_value.is<std::shared_ptr<Target>>()) {
                    current_plan.push(dep_value.as<std::shared_ptr<Target>>());
                }
            }
            visited.insert(t.get());
        };

        dfs(root_target_ptr);
        data->execution_plan[name] = std::move(current_plan);
    }
}
