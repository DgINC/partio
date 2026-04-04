#include <fstream>
#include <thread>

#include "ANTLRInputStream.h"
#include "YACSLexer.h"
#include "YACSParser.h"
#include "Interpreter.hpp"
#include <argparse/argparse.hpp>


int main(int argc, char *argv[]) {
    argparse::ArgumentParser program("partio", "1.0", argparse::default_arguments::none);

    program.add_argument("-a", "--arch")
    .required()
    .action([=](const std::string& value) {
        static const std::set<std::string> choices = {"x64", "arm64", "riscv"};
        if (!choices.contains(value)) {
            throw std::runtime_error("Unknown architecture: " + value );
        }
        return value;
    })
    .help("Target architecture.");

    program.add_argument("-b", "--build-mode")
    .help("Build mode.")
    .default_value(std::string("release"))
    .choices("debug", "release");

    auto default_jobs = std::thread::hardware_concurrency();
    if (default_jobs == 0) default_jobs = 1;

    program.add_argument("-j", "--jobs")
    .help("Jobs count")
    .default_value(static_cast<int>(default_jobs))
    .scan<'i', int>();

    program.add_argument("-t", "--target")
    .required()
    .help("Build target");

    program.add_argument("-v", "--verbose")
        .help("increase output verbosity")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("-h", "--help")
    .action([&](const std::string& s) {
        std::cout << program << std::endl;
        std::exit(0);
    })
    .default_value(false)
    .help("shows help message")
    .implicit_value(true)
    .nargs(0);

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
        std::cerr << "Error: " << err.what() << std::endl;
        std::cerr << "Use --help for help. LOL" << std::endl;
        std::exit(1);
    }

    ProjectConfig config;
    config.arch = program.get<std::string>("--arch");
    config.build_mode = program.get<std::string>("--build-mode");
    config.thread_count = program.get<int>("--jobs");
    config.verbose = program.get<bool>("--verbose");
    config.target = program.get<std::string>("--target");

    auto root_project = std::make_shared<ProjectContext>();
    root_project->root = std::filesystem::current_path();

    std::ifstream stream("build.yacs");
    antlr4::ANTLRInputStream input(stream);
    YACSLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    YACSParser parser(&tokens);

    auto* tree = parser.project_file(); // Корень твоей грамматики
    Interpreter visitor(config, root_project);

    try {
        visitor.visit(tree);
    } catch (const std::exception& err) {
        std::cerr << "Error: " << err.what() << std::endl;
    }

    return 0;
}
