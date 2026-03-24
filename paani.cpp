// paani.cpp - Paani Compiler
// New simplified CLI design

#include "paani_lexer.hpp"
#include "paani_parser.hpp"
#include "paani_codegen.hpp"
#include "paani_sema.hpp"
#include "paani_package.hpp"
#include "paani_embedded_rt.hpp"
#include "paani_colors.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <map>
#include <set>
#include <algorithm>
#include <cctype>

// Platform detection
#ifdef _WIN32
    #define PLATFORM_WINDOWS
    #define DEFAULT_EXE_EXT ".exe"
#else
    #define PLATFORM_UNIX
    #define DEFAULT_EXE_EXT ""
#endif

// Compiler toolchain detection
struct Toolchain {
    std::string cc;
    std::string ar;
    std::string exeExt;
    std::string mkdirCmd;
    std::string rmCmd;
};

Toolchain detectToolchain() {
    Toolchain tc;
    tc.exeExt = DEFAULT_EXE_EXT;
    
    // Detect C compiler
    if (std::system("gcc --version >nul 2>&1") == 0 || std::system("gcc --version >/dev/null 2>&1") == 0) {
        tc.cc = "gcc";
        tc.ar = "ar";
    } else if (std::system("clang --version >nul 2>&1") == 0 || std::system("clang --version >/dev/null 2>&1") == 0) {
        tc.cc = "clang";
        tc.ar = "llvm-ar";
    } else if (std::system("cc --version >nul 2>&1") == 0 || std::system("cc --version >/dev/null 2>&1") == 0) {
        tc.cc = "cc";
        tc.ar = "ar";
    } else {
        tc.cc = "gcc";  // fallback
        tc.ar = "ar";
    }
    
    // Detect platform-specific commands
#ifdef PLATFORM_WINDOWS
    tc.mkdirCmd = "mkdir";
    tc.rmCmd = "rm -f";
#else
    tc.mkdirCmd = "mkdir -p";
    tc.rmCmd = "rm -f";
#endif
    
    return tc;
}

using namespace paani;
namespace fs = std::filesystem;

static const char* argv0 = nullptr;

std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void writeFile(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot write file: " + filename);
    }
    file << content;
}

std::string pathToString(const fs::path& path) {
    std::string result = path.string();
    std::replace(result.begin(), result.end(), '\\', '/');
    if (!result.empty() && result.back() == '/') {
        result.pop_back();
    }
    return result;
}

struct CliPackageInfo {
    std::string name;
    fs::path path;
    std::vector<fs::path> sourceFiles;
    std::vector<fs::path> staticLibs;
    std::vector<std::string> dependencies;
};

class Project {
public:
    fs::path rootDir;
    fs::path outputDir;
    fs::path buildDir;
    std::map<std::string, CliPackageInfo> packages;
    CliPackageInfo mainPackage;
    bool hasMainPackage = false;

    Project(const fs::path& root) : rootDir(root) {
        outputDir = root / "output";
        buildDir = root / "build";
    }

    void discover() {
        for (const auto& entry : fs::directory_iterator(rootDir)) {
            if (entry.is_directory()) {
                std::string dirName = entry.path().filename().string();
                if (dirName == "output" || dirName == "build" || dirName == "paani_rt") {
                    continue;
                }
                auto pkg = discoverPackage(dirName, entry.path());
                if (!pkg.sourceFiles.empty()) {
                    if (dirName == "main") {
                        mainPackage = std::move(pkg);
                        hasMainPackage = true;
                    } else {
                        packages[dirName] = std::move(pkg);
                    }
                }
            }
        }
    }

    CliPackageInfo discoverPackage(const std::string& name, const fs::path& path) {
        CliPackageInfo pkg;
        pkg.name = name;
        pkg.path = path;
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".paani") {
                pkg.sourceFiles.push_back(entry.path());
            }
        }
        std::sort(pkg.sourceFiles.begin(), pkg.sourceFiles.end());
        // Discover libs directory (optional) - supports static and dynamic libraries
        fs::path libsDir = path / "libs";
        if (fs::exists(libsDir)) {
            for (const auto& entry : fs::directory_iterator(libsDir)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    // Support:
                    // - Static libraries (.a, .lib)
                    // - Dynamic libraries (.dll, .so, .dylib)
                    // - Object files (.o, .obj) - can be directly linked
                    if (ext == ".a" || ext == ".lib" || ext == ".dll" || ext == ".so" || ext == ".dylib" ||
                        ext == ".o" || ext == ".obj") {
                        pkg.staticLibs.push_back(entry.path());
                    }
                }
            }
        }
        return pkg;
    }

    void analyzeDependencies() {
        for (auto& [name, pkg] : packages) {
            std::string mergedSource;
            for (const auto& file : pkg.sourceFiles) {
                try {
                    mergedSource += readFile(file.string()) + "\n";
                } catch (...) {}
            }
            size_t pos = 0;
            while ((pos = mergedSource.find("use ", pos)) != std::string::npos) {
                pos += 4;
                while (pos < mergedSource.size() && std::isspace(mergedSource[pos])) pos++;
                size_t start = pos;
                while (pos < mergedSource.size() && (std::isalnum(mergedSource[pos]) || mergedSource[pos] == '_')) pos++;
                if (pos > start) {
                    std::string usedPkg = mergedSource.substr(start, pos - start);
                    if (packages.count(usedPkg)) {
                        pkg.dependencies.push_back(usedPkg);
                    }
                }
            }
            std::sort(pkg.dependencies.begin(), pkg.dependencies.end());
            pkg.dependencies.erase(std::unique(pkg.dependencies.begin(), pkg.dependencies.end()), pkg.dependencies.end());
        }
    }

    std::vector<std::string> getBuildOrder() {
        std::vector<std::string> order;
        std::set<std::string> visited;
        std::set<std::string> visiting;
        std::function<void(const std::string&)> visit = [&](const std::string& name) {
            if (visited.count(name)) return;
            if (visiting.count(name)) return;
            visiting.insert(name);
            auto it = packages.find(name);
            if (it != packages.end()) {
                for (const auto& dep : it->second.dependencies) {
                    if (packages.count(dep)) visit(dep);
                }
            }
            visiting.erase(name);
            visited.insert(name);
            order.push_back(name);
        };
        for (const auto& [name, pkg] : packages) visit(name);
        return order;
    }
};

bool compilePackage(CliPackageInfo& pkg, const fs::path& outputDir,
                   const std::map<std::string, CliPackageInfo>& allPackages,
                   bool verbose, bool isEntryPoint = false, const fs::path& projectRoot = "") {
    try {
        if (verbose) {
            ConsoleColor::printDim("  Compiling package: ");
            ConsoleColor::printHighlight(pkg.name + "\n");
        }
        std::string mergedSource;
        for (const auto& file : pkg.sourceFiles) {
            mergedSource += "// ===== " + file.filename().string() + " =====\n";
            mergedSource += readFile(file.string());
            mergedSource += "\n\n";
        }
        Parser parser(mergedSource, pkg.name);
        parser.setEntryPoint(isEntryPoint);
        auto ast = parser.parse();
        if (!ast) {
            ConsoleColor::printErrorPrefix();
            std::cerr << " Parse failed for package: " << pkg.name << "\n";
            return false;
        }
        SemanticAnalyzer sema;
        // Set project root as search path for package imports
        if (!projectRoot.empty()) {
            sema.packageManager.searchPaths.clear();
            sema.packageManager.searchPaths.push_back(projectRoot);
        }
        try {
            sema.analyze(*ast, mergedSource);
        } catch (const std::runtime_error& e) {
            std::cerr << e.what();
            return false;
        }
        CodeGenerator codegen;
        std::string header = codegen.generateHeader(*ast);
        std::string impl = codegen.generateImpl(*ast);
        fs::path headerFile = outputDir / (pkg.name + ".h");
        fs::path implFile = outputDir / (pkg.name + ".c");
        writeFile(headerFile.string(), header);
        writeFile(implFile.string(), impl);
        if (verbose) {
            ConsoleColor::printDim("    Generated: ");
            ConsoleColor::printSuccess(pkg.name + ".h, " + pkg.name + ".c\n");
        }
        return true;
    } catch (const std::exception& e) {
        ConsoleColor::printErrorPrefix();
        std::cerr << " Error compiling " << pkg.name << ": " << e.what() << "\n";
        return false;
    }
}

void generateMakefile(Project& project) {
    Toolchain tc = detectToolchain();
    std::ostringstream mf;
    mf << "# Auto-generated Makefile\n";
    mf << "# Cross-platform: Works on Windows (MinGW) and Unix-like systems\n\n";
    mf << "OUTPUT_DIR = " << pathToString(project.outputDir) << "\n";
    mf << "BUILD_DIR = " << pathToString(project.buildDir) << "\n";
    mf << "PAANI_RT = " << pathToString(project.rootDir / "paani_rt") << "\n\n";

    mf << "CC = " << tc.cc << "\n";
    mf << "CFLAGS = -Wall -O2 -Wno-implicit-function-declaration -Wno-builtin-declaration-mismatch -I$(OUTPUT_DIR) -I$(PAANI_RT)\n";
    mf << "AR = " << tc.ar << "\n";
    mf << "ARFLAGS = rcs\n\n";
    
    // Collect all external libraries from libs directories
    std::vector<std::pair<std::string, std::string>> externalLibs; // (package name, lib filename)
    for (const auto& [name, pkg] : project.packages) {
        for (const auto& lib : pkg.staticLibs) {
            externalLibs.push_back({name, lib.filename().string()});
        }
    }
    
    // Define external library variables
    for (size_t i = 0; i < externalLibs.size(); i++) {
        mf << "EXT_LIB" << i << " = $(BUILD_DIR)/" << externalLibs[i].second << "\n";
    }
    if (!externalLibs.empty()) {
        mf << "\n";
    }
    
    for (const auto& [name, pkg] : project.packages) {
        mf << name << "_SRC = $(OUTPUT_DIR)/" << name << ".c\n";
        mf << name << "_OBJ = $(BUILD_DIR)/" << name << ".o\n";
        mf << name << "_LIB = $(BUILD_DIR)/lib" << name << ".a\n\n";
    }
    mf << "all: $(PAANI_RT_LIB)";
    for (const auto& [name, pkg] : project.packages) mf << " $(" << name << "_LIB)";
    for (size_t i = 0; i < externalLibs.size(); i++) {
        mf << " $(EXT_LIB" << i << ")";
    }
    mf << "\n\n";
    mf << "PAANI_RT_LIB = $(BUILD_DIR)/libpaani_rt.a\n\n";
    mf << "$(PAANI_RT_LIB): $(PAANI_RT)/paani_ecs.c $(PAANI_RT)/paani_ecs.h\n";
    mf << "\t@mkdir $(BUILD_DIR) 2>nul || true\n";
    mf << "\t$(CC) $(CFLAGS) -c $(PAANI_RT)/paani_ecs.c -o $(BUILD_DIR)/paani_ecs.o\n";
    mf << "\t$(AR) $(ARFLAGS) $@ $(BUILD_DIR)/paani_ecs.o\n";
    mf << "\t@rm -f $(BUILD_DIR)/paani_ecs.o 2>nul || true\n\n";
    
    // Rules to copy external libraries from libs directories to build directory
    size_t libIndex = 0;
    for (const auto& [name, pkg] : project.packages) {
        // Object and library rules
        mf << "$(" << name << "_OBJ): $(" << name << "_SRC) $(OUTPUT_DIR)/" << name << ".h\n";
        mf << "\t@mkdir $(BUILD_DIR) 2>nul || true\n";
        mf << "\t$(CC) $(CFLAGS) -c $< -o $@\n\n";
        mf << "$(" << name << "_LIB): $(" << name << "_OBJ)\n";
        mf << "\t@mkdir $(BUILD_DIR) 2>nul || true\n";
        mf << "\t$(AR) $(ARFLAGS) $@ $(" << name << "_OBJ)\n\n";
        
        // External library copy rules
        for (const auto& lib : pkg.staticLibs) {
            mf << "$(EXT_LIB" << libIndex << "): " << pathToString(lib) << "\n";
            mf << "\t@mkdir $(BUILD_DIR) 2>nul || true\n";
            mf << "\t@cp $< $@\n\n";
            libIndex++;
        }
    }
    mf << "clean:\n";
    mf << "\t-rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/*.a 2>nul || true\n\n";
    mf << "distclean: clean\n";
    mf << "\t-rm -f $(OUTPUT_DIR)/*.h $(OUTPUT_DIR)/*.c 2>nul || true\n\n";
    mf << ".PHONY: all clean distclean\n";
    writeFile((project.outputDir / "Makefile").string(), mf.str());
}

void generateExecutableMakefile(Project& project) {
    Toolchain tc = detectToolchain();
    std::ostringstream mf;
    mf << "# Auto-generated Makefile for Executable\n";
    mf << "# Cross-platform: Works on Windows (MinGW) and Unix-like systems\n\n";
    mf << "OUTPUT_DIR = " << pathToString(project.outputDir) << "\n";
    mf << "BUILD_DIR = " << pathToString(project.buildDir) << "\n";
    mf << "PAANI_RT = " << pathToString(project.rootDir / "paani_rt") << "\n\n";
    mf << "# Platform settings\n";
    mf << "EXE_EXT = " << tc.exeExt << "\n\n";
    mf << "CC = " << tc.cc << "\n";
    mf << "CFLAGS = -Wall -O2 -Wno-implicit-function-declaration -Wno-builtin-declaration-mismatch -I$(OUTPUT_DIR) -I$(PAANI_RT)\n";
    mf << "LDFLAGS =\n";
    mf << "AR = " << tc.ar << "\n";
    mf << "ARFLAGS = rcs\n\n";
    
    // Collect all external libraries from libs directories
    std::vector<std::pair<std::string, std::string>> externalLibs; // (package name, lib filename)
    for (const auto& [name, pkg] : project.packages) {
        for (const auto& lib : pkg.staticLibs) {
            externalLibs.push_back({name, lib.filename().string()});
        }
    }
    for (const auto& lib : project.mainPackage.staticLibs) {
        externalLibs.push_back({"main", lib.filename().string()});
    }
    
    // Define external library variables
    for (size_t i = 0; i < externalLibs.size(); i++) {
        mf << "EXT_LIB" << i << " = $(BUILD_DIR)/" << externalLibs[i].second << "\n";
    }
    if (!externalLibs.empty()) {
        mf << "\n";
    }
    
    for (const auto& [name, pkg] : project.packages) {
        mf << name << "_OBJ = $(BUILD_DIR)/" << name << ".o\n";
        mf << name << "_LIB = $(BUILD_DIR)/lib" << name << ".a\n";
    }
    mf << "\nMAIN_SRC = $(OUTPUT_DIR)/main.c\n";
    mf << "MAIN_OBJ = $(BUILD_DIR)/main.o\n";
    mf << "MAIN_LIB = $(BUILD_DIR)/libmain.a\n\n";
    mf << "ALL_LIBS =";
    for (const auto& [name, pkg] : project.packages) mf << " $(" << name << "_LIB)";
    mf << " $(MAIN_LIB)\n\n";
    mf << "PAANI_RT_LIB = $(BUILD_DIR)/libpaani_rt.a\n\n";
    mf << "all: exe\n\n";
    mf << "exe: $(BUILD_DIR)/main$(EXE_EXT)\n\n";
    mf << "$(BUILD_DIR)/main$(EXE_EXT): $(ALL_LIBS) $(PAANI_RT_LIB)";
    for (size_t i = 0; i < externalLibs.size(); i++) {
        mf << " $(EXT_LIB" << i << ")";
    }
    mf << "\n";
    mf << "\t@mkdir $(BUILD_DIR) 2>nul || true\n";
    mf << "\t$(CC) $(LDFLAGS) -o $@ $(MAIN_OBJ)";
    for (const auto& [name, pkg] : project.packages) mf << " $(" << name << "_OBJ)";
    mf << " $(PAANI_RT_LIB)";
    for (size_t i = 0; i < externalLibs.size(); i++) {
        mf << " $(EXT_LIB" << i << ")";
    }
    mf << " -lm\n\n";
    
    // Rules to copy external libraries from libs directories to build directory
    size_t libIndex = 0;
    for (const auto& [name, pkg] : project.packages) {
        for (const auto& lib : pkg.staticLibs) {
            mf << "$(EXT_LIB" << libIndex << "): " << pathToString(lib) << "\n";
            mf << "\t@mkdir $(BUILD_DIR) 2>nul || true\n";
            mf << "\t@cp $< $@\n\n";
            libIndex++;
        }
    }
    for (const auto& lib : project.mainPackage.staticLibs) {
        mf << "$(EXT_LIB" << libIndex << "): " << pathToString(lib) << "\n";
        mf << "\t@mkdir $(BUILD_DIR) 2>nul || true\n";
        mf << "\t@cp $< $@\n\n";
        libIndex++;
    }
    
    mf << "$(MAIN_OBJ): $(MAIN_SRC) $(OUTPUT_DIR)/main.h\n";
    mf << "\t@mkdir $(BUILD_DIR) 2>nul || true\n";
    mf << "\t$(CC) $(CFLAGS) -c $< -o $@\n\n";
    mf << "$(MAIN_LIB): $(MAIN_OBJ)\n";
    mf << "\t@mkdir $(BUILD_DIR) 2>nul || true\n";
    mf << "\t$(AR) $(ARFLAGS) $@ $<\n\n";
    for (const auto& [name, pkg] : project.packages) {
        mf << "$(" << name << "_OBJ): $(OUTPUT_DIR)/" << name << ".c $(OUTPUT_DIR)/" << name << ".h\n";
        mf << "\t@mkdir $(BUILD_DIR) 2>nul || true\n";
        mf << "\t$(CC) $(CFLAGS) -c $< -o $@\n\n";
        mf << "$(" << name << "_LIB): $(" << name << "_OBJ)\n";
        mf << "\t@mkdir $(BUILD_DIR) 2>nul || true\n";
        mf << "\t$(AR) $(ARFLAGS) $@ $<\n\n";
    }
    mf << "$(PAANI_RT_LIB): $(PAANI_RT)/paani_ecs.c $(PAANI_RT)/paani_ecs.h\n";
    mf << "\t@mkdir $(BUILD_DIR) 2>nul || true\n";
    mf << "\t$(CC) $(CFLAGS) -c $(PAANI_RT)/paani_ecs.c -o $(BUILD_DIR)/paani_ecs.o\n";
    mf << "\t$(AR) $(ARFLAGS) $@ $(BUILD_DIR)/paani_ecs.o\n";
    mf << "\t@rm -f $(BUILD_DIR)/paani_ecs.o 2>nul || true\n\n";
    mf << "clean:\n";
    mf << "\t-rm -f $(BUILD_DIR)/main$(EXE_EXT) $(BUILD_DIR)/*.o $(BUILD_DIR)/*.a 2>nul || true\n\n";
    mf << ".PHONY: all clean exe\n";
    writeFile((project.outputDir / "Makefile").string(), mf.str());
}

void printHelp() {
    ConsoleColor::printHighlight("Paani Compiler");
    std::cout << " - ECS Programming Language\n\n";
    
    ConsoleColor::printInfo("USAGE:\n");
    std::cout << "    paani <command> [options]\n\n";
    
    ConsoleColor::printInfo("COMMANDS:\n");
    ConsoleColor::printHighlight("    new <name>      ");
    std::cout << "Create a new Paani project\n";
    ConsoleColor::printHighlight("    build [pkg]     ");
    std::cout << "Build all packages or specific package\n";
    ConsoleColor::printHighlight("    link            ");
    std::cout << "Build executable from 'main' package\n";
    ConsoleColor::printHighlight("    run             ");
    std::cout << "Build and run executable\n";
    ConsoleColor::printHighlight("    clean           ");
    std::cout << "Clean build artifacts\n\n";
    
    ConsoleColor::printInfo("PROJECT STRUCTURE:\n");
    ConsoleColor::printDim("    ./\n");
    ConsoleColor::printDim("    |-- paani_rt/          # Paani runtime\n");
    ConsoleColor::printDim("    |-- pkg1/              # Library package\n");
    ConsoleColor::printDim("    |-- main/              # Entry point package\n");
    ConsoleColor::printDim("    |-- output/            # Generated C code\n");
    ConsoleColor::printDim("    `-- build/             # Compiled artifacts\n\n");
    
    ConsoleColor::printInfo("EXAMPLES:\n");
    ConsoleColor::printDim("    paani new mygame         # Create new project\n");
    ConsoleColor::printDim("    paani build              # Build all library packages\n");
    ConsoleColor::printDim("    paani link               # Build executable\n");
    ConsoleColor::printDim("    paani run                # Build and run\n");
    ConsoleColor::printDim("    paani clean              # Clean build artifacts\n\n");
    
    ConsoleColor::printInfo("OPTIONS:\n");
    ConsoleColor::printDim("    -v, --verbose    Show detailed output\n");
    ConsoleColor::printDim("    -h, --help       Show this help message\n");
}

int cmdNew(const std::vector<std::string>& args, bool verbose) {
    if (args.empty()) {
        ConsoleColor::printErrorPrefix();
        std::cerr << " Package name required\n";
        return 1;
    }
    std::string projectName = args[0];
    try {
        fs::path projectDir = projectName;
        if (fs::exists(projectDir)) {
            ConsoleColor::printErrorPrefix();
            std::cerr << " Directory '" << projectName << "' already exists\n";
            return 1;
        }
        fs::create_directory(projectDir);
        fs::create_directory(projectDir / "main");
        fs::create_directory(projectDir / "main" / "utils");
        fs::create_directory(projectDir / "output");
        fs::create_directory(projectDir / "build");
        std::string mainContent = R"(// Main package for )" + projectName + R"(

data Timer {
    elapsed: f32
}

trait AutoExit [Timer];

// Global init: create timer entity
let e = spawn;
give e Timer: {elapsed: 0.0};
tag e as AutoExit;

for AutoExit in exitSystem(dt: f32) as e {
    e.Timer.elapsed += dt;
    if (e.Timer.elapsed >= 3.0) {
        exit;
    }
}
)";
        writeFile((projectDir / "main" / "main.paani").string(), mainContent);
        std::string helpersContent = R"(// Utility functions for )" + projectName + R"(

export fn clamp(x: f64, min: f64, max: f64) -> f64 {
    if (x < min) { return min; }
    if (x > max) { return max; }
    return x;
}
)";
        writeFile((projectDir / "main" / "utils" / "helpers.paani").string(), helpersContent);
        fs::path paaniRtDest = projectDir / "paani_rt";
        fs::create_directory(paaniRtDest);
        // Write SoA runtime header and implementation separately
        writeFile((paaniRtDest / "paani_ecs.h").string(), PAANI_ECS_H_CONTENT);
        writeFile((paaniRtDest / "paani_ecs.c").string(), PAANI_ECS_C_CONTENT);
        Toolchain tc = detectToolchain();
        std::string compileCmd = tc.cc + " -c -O2 -I" + pathToString(paaniRtDest) + " " + pathToString(paaniRtDest / "paani_ecs.c") + " -o " + pathToString(projectDir / "build" / "paani_ecs.o");
        if (verbose) {
            ConsoleColor::printDim("  Compiling paani_rt runtime...\n    ");
            std::cout << compileCmd << "\n";
        }
        int compileResult = std::system(compileCmd.c_str());
        if (compileResult != 0) {
            ConsoleColor::printErrorPrefix();
            std::cerr << " Failed to compile paani_rt\n";
            return 1;
        }
        std::string arCmd = tc.ar + " rcs " + pathToString(projectDir / "build" / "libpaani_rt.a") + " " + pathToString(projectDir / "build" / "paani_ecs.o");
        int arResult = std::system(arCmd.c_str());
        if (arResult != 0) {
            ConsoleColor::printErrorPrefix();
            std::cerr << " Failed to create paani_rt library\n";
            return 1;
        }
        fs::remove(projectDir / "build" / "paani_ecs.o");
        if (verbose) {
            ConsoleColor::printDim("  ");
            ConsoleColor::printSuccessPrefix();
            std::cout << " Pre-compiled libpaani_rt.a\n";
        }
        if (!verbose) {
            ConsoleColor::printSuccessPrefix();
            std::cout << " Project '";
            ConsoleColor::printHighlight(projectName);
            std::cout << "' created\n";
            ConsoleColor::printDim("  Location: ");
            std::cout << pathToString(fs::absolute(projectDir)) << "\n";
            std::cout << "\n";
            ConsoleColor::printInfo("Next steps:\n");
            ConsoleColor::printDim("  cd " + projectName + "\n");
            ConsoleColor::printDim("  paani build\n");
        }
        return 0;
    } catch (const std::exception& e) {
        ConsoleColor::printErrorPrefix();
        std::cerr << " " << e.what() << "\n";
        return 1;
    }
}

int cmdBuild(const std::vector<std::string>& args, bool verbose) {
    try {
        auto startTime = std::chrono::high_resolution_clock::now();
        Project project(fs::current_path());
        ConsoleColor::printInfo("Discovering packages...\n");
        project.discover();
        if (project.packages.empty() && !project.hasMainPackage) {
            ConsoleColor::printErrorPrefix();
            std::cerr << " No packages found in current directory\n";
            return 1;
        }
        std::cout << "Found " << project.packages.size() << " package(s):\n";
        for (const auto& [name, pkg] : project.packages) {
            ConsoleColor::printDim("  - ");
            ConsoleColor::printHighlight(name);
            ConsoleColor::printDim(" (" + std::to_string(pkg.sourceFiles.size()) + " files)\n");
        }
        if (project.hasMainPackage) {
            ConsoleColor::printDim("  - ");
            ConsoleColor::printHighlight("main");
            ConsoleColor::printDim(" (" + std::to_string(project.mainPackage.sourceFiles.size()) + " files) [entry point - use 'paani link']\n");
        }
        std::cout << "\n";
        project.analyzeDependencies();
        std::vector<std::string> buildOrder;
        if (args.empty()) {
            buildOrder = project.getBuildOrder();
        } else {
            std::string target = args[0];
            if (!project.packages.count(target)) {
                ConsoleColor::printErrorPrefix();
                std::cerr << " Package not found: " << target << "\n";
                return 1;
            }
            std::set<std::string> needed;
            std::function<void(const std::string&)> addDeps = [&](const std::string& name) {
                if (needed.count(name)) return;
                needed.insert(name);
                auto it = project.packages.find(name);
                if (it != project.packages.end()) {
                    for (const auto& dep : it->second.dependencies) addDeps(dep);
                }
            };
            addDeps(target);
            for (const auto& name : project.getBuildOrder()) {
                if (needed.count(name)) buildOrder.push_back(name);
            }
        }
        fs::create_directories(project.outputDir);
        ConsoleColor::printInfo("Compiling packages...\n");
        int successCount = 0;
        for (const auto& name : buildOrder) {
            auto& pkg = project.packages[name];
            if (compilePackage(pkg, project.outputDir, project.packages, verbose, false, project.rootDir)) {
                successCount++;
            } else {
                ConsoleColor::printErrorPrefix();
                std::cerr << " Failed to compile package: " << name << "\n";
                return 1;
            }
        }
        generateMakefile(project);
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        std::cout << "\n";
        ConsoleColor::printSuccessPrefix();
        std::cout << " Build successful\n";
        ConsoleColor::printDim("  Packages: " + std::to_string(successCount) + "/" + std::to_string(buildOrder.size()) + "\n");
        ConsoleColor::printDim("  Output: " + pathToString(project.outputDir) + "/\n");
        ConsoleColor::printDim("  Time: " + std::to_string(duration.count()) + "ms\n");
        return 0;
    } catch (const std::exception& e) {
        ConsoleColor::printErrorPrefix();
        std::cerr << " " << e.what() << "\n";
        return 1;
    }
}

int cmdLink(bool verbose) {
    try {
        auto startTime = std::chrono::high_resolution_clock::now();
        Project project(fs::current_path());
        ConsoleColor::printInfo("Discovering packages...\n");
        project.discover();
        if (!project.hasMainPackage) {
            ConsoleColor::printErrorPrefix();
            std::cerr << " No 'main' package found. Create a 'main/' directory.\n";
            return 1;
        }
        ConsoleColor::printDim("Found main package (" + std::to_string(project.mainPackage.sourceFiles.size()) + " files)\n");
        ConsoleColor::printDim("Found " + std::to_string(project.packages.size()) + " library package(s)\n\n");
        if (!project.packages.empty()) {
            ConsoleColor::printInfo("Building library packages...\n");
            project.analyzeDependencies();
            fs::create_directories(project.outputDir);
            for (const auto& name : project.getBuildOrder()) {
                auto& pkg = project.packages[name];
                if (!compilePackage(pkg, project.outputDir, project.packages, verbose, false, project.rootDir)) {
                    ConsoleColor::printErrorPrefix();
                    std::cerr << " Failed to compile package: " << name << "\n";
                    return 1;
                }
            }
        }
        ConsoleColor::printInfo("\nCompiling main package (entry point)...\n");
        if (!compilePackage(project.mainPackage, project.outputDir, project.packages, verbose, true, project.rootDir)) {
            ConsoleColor::printErrorPrefix();
            std::cerr << " Failed to compile main package\n";
            return 1;
        }
        generateExecutableMakefile(project);
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        std::cout << "\n";
        ConsoleColor::printSuccessPrefix();
        std::cout << " Link successful\n";
        ConsoleColor::printDim("  Libraries: " + std::to_string(project.packages.size()) + "\n");
        ConsoleColor::printDim("  Output: " + pathToString(project.outputDir) + "/\n");
        ConsoleColor::printDim("  Time: " + std::to_string(duration.count()) + "ms\n");
        std::cout << "\n";
        ConsoleColor::printInfo("Next steps:\n");
        ConsoleColor::printDim("  cd " + pathToString(project.outputDir) + "\n");
        ConsoleColor::printDim("  make exe\n");
        return 0;
    } catch (const std::exception& e) {
        ConsoleColor::printErrorPrefix();
        std::cerr << " " << e.what() << "\n";
        return 1;
    }
}

int cmdRun(bool verbose) {
    try {
        ConsoleColor::printHighlight("=== Building executable ===\n");
        int linkResult = cmdLink(verbose);
        if (linkResult != 0) return linkResult;
        ConsoleColor::printHighlight("\n=== Compiling executable ===\n");
        std::string makeCmd = "cd " + pathToString(fs::current_path() / "output") + " && make exe";
        if (verbose) {
            ConsoleColor::printDim("Running: " + makeCmd + "\n");
        }
        int makeResult = std::system(makeCmd.c_str());
        if (makeResult != 0) {
            ConsoleColor::printErrorPrefix();
            std::cerr << " Failed to build executable\n";
            return 1;
        }
        ConsoleColor::printHighlight("\n=== Running executable ===\n");
        Toolchain tcRun = detectToolchain();
        std::string exePath = pathToString(fs::current_path() / "build" / ("main" + tcRun.exeExt));
        if (!fs::exists(exePath)) {
            ConsoleColor::printErrorPrefix();
            std::cerr << " Executable not found: " << exePath << "\n";
            return 1;
        }
        if (verbose) {
            ConsoleColor::printDim("Running: " + exePath + "\n\n");
        }
        int runResult = std::system(exePath.c_str());
        std::cout << "\n";
        ConsoleColor::printSuccessPrefix();
        std::cout << " Program exited with code: " << runResult << "\n";
        return runResult;
    } catch (const std::exception& e) {
        ConsoleColor::printErrorPrefix();
        std::cerr << " " << e.what() << "\n";
        return 1;
    }
}

int cmdClean(bool verbose) {
    try {
        Project project(fs::current_path());
        int count = 0;
        if (fs::exists(project.buildDir)) {
            for (const auto& entry : fs::directory_iterator(project.buildDir)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    if (ext == ".o" || ext == ".a" || ext == ".obj" || ext == ".exe") {
                        if (verbose) {
                            ConsoleColor::printDim("  Removing: " + pathToString(entry.path()) + "\n");
                        }
                        fs::remove(entry.path());
                        count++;
                    }
                }
            }
        }
        if (fs::exists(project.outputDir)) {
            for (const auto& entry : fs::directory_iterator(project.outputDir)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    if (ext == ".h" || ext == ".c") {
                        if (verbose) {
                            ConsoleColor::printDim("  Removing: " + pathToString(entry.path()) + "\n");
                        }
                        fs::remove(entry.path());
                        count++;
                    }
                }
            }
        }
        ConsoleColor::printSuccessPrefix();
        std::cout << " Cleaned " << count << " file(s)\n";
        return 0;
    } catch (const std::exception& e) {
        ConsoleColor::printErrorPrefix();
        std::cerr << " " << e.what() << "\n";
        return 1;
    }
}

int main(int argc, char* argv[]) {
    argv0 = argv[0];
    
    // Initialize color support
    ConsoleColor::init();
    
    if (argc < 2) {
        printHelp();
        return 1;
    }
    std::string command = argv[1];
    std::vector<std::string> args;
    bool verbose = false;
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-v" || arg == "--verbose") verbose = true;
        else if (arg == "-h" || arg == "--help") { printHelp(); return 0; }
        else if (arg[0] != '-') args.push_back(arg);
    }
    if (command == "new") return cmdNew(args, verbose);
    else if (command == "build") return cmdBuild(args, verbose);
    else if (command == "link") return cmdLink(verbose);
    else if (command == "run") return cmdRun(verbose);
    else if (command == "clean") return cmdClean(verbose);
    else if (command == "help" || command == "-h" || command == "--help") { printHelp(); return 0; }
    else {
        ConsoleColor::printErrorPrefix();
        std::cerr << " Unknown command: " << command << "\n";
        ConsoleColor::printDim("Use 'paani help' for usage information.\n");
        return 1;
    }
}