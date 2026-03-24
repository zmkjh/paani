// Naming.inl - Implementation
#pragma once

#include "Naming.hpp"
#include <algorithm>

namespace paani {

// C standard library reserved names
const std::unordered_set<std::string> Naming::C_RESERVED_NAMES = {
    // stdio.h
    "printf", "fprintf", "sprintf", "snprintf", "scanf", "sscanf", "fscanf",
    "fopen", "fclose", "fread", "fwrite", "fseek", "ftell", "rewind", "fflush",
    "getchar", "putchar", "gets", "puts", "perror", "remove", "rename", "tmpfile",
    // stdlib.h
    "malloc", "calloc", "realloc", "free", "exit", "abort", "system", "getenv",
    "atoi", "atol", "atof", "strtol", "strtoul", "strtod", "strtof", "strtold",
    "rand", "srand", "qsort", "bsearch", "abs", "labs", "llabs",
    "div", "ldiv", "lldiv", "mblen", "mbtowc", "wctomb", "mbstowcs", "wcstombs",
    // string.h
    "strlen", "strcpy", "strncpy", "strcat", "strncat", "strcmp", "strncmp",
    "strchr", "strrchr", "strstr", "strtok", "strerror", "strcoll", "strxfrm",
    "memcpy", "memmove", "memset", "memcmp", "memchr",
    // math.h
    "sin", "cos", "tan", "asin", "acos", "atan", "atan2", "sinh", "cosh", "tanh",
    "sqrt", "pow", "exp", "log", "log10", "log2", "ceil", "floor", "fabs", "fmod",
    "round", "trunc", "nearbyint", "remainder", "fmax", "fmin", "fma",
    // time.h
    "time", "clock", "difftime", "mktime", "strftime", "localtime", "gmtime",
    "asctime", "ctime", "timespec_get",
    // ctype.h
    "isalpha", "isdigit", "isalnum", "isspace", "isupper", "islower",
    "toupper", "tolower", "isblank", "iscntrl", "isgraph", "isprint", "ispunct",
    // stdint.h / stddef.h
    "intptr_t", "uintptr_t", "intmax_t", "uintmax_t", "ptrdiff_t", "size_t",
    "offsetof", "NULL",
    // Keywords and special
    "main", "sizeof", "typeof", "alignof", "static_assert",
    // Common macros
    "assert", "errno", "EOF", "BUFSIZ", "FILENAME_MAX", "FOPEN_MAX",
    // signal.h
    "signal", "raise", "SIG_DFL", "SIG_IGN", "SIG_ERR",
    // setjmp.h
    "setjmp", "longjmp",
    // locale.h
    "setlocale", "localeconv",
    // wchar.h
    "wprintf", "wscanf", "fgetwc", "fputwc", "fgetws", "fputws",
    // C11 additions
    "static_assert", "alignas", "alignof", "noreturn", "thread_local"
};

// C reserved prefixes (user identifiers should not start with these)
const std::unordered_set<std::string> Naming::C_RESERVED_PREFIXES = {
    "_",        // Leading underscore
    "__",       // Double underscore (compiler reserved)
    "_",        // Underscore + uppercase (compiler reserved)
    "is",       // ctype.h functions
    "to",       // ctype.h functions
    "str",      // string.h functions
    "mem",      // memory functions
    "wcs",      // wide char string functions
};

// Paani compiler reserved prefixes
const std::unordered_set<std::string> Naming::COMPILER_RESERVED_PREFIXES = {
    "__paani_gen_",  // All compiler-generated identifiers
    "s_ctype_",      // Component type IDs
    "s_trait_",      // Trait type IDs
};

// ========== Core Symbol Generation ==========
inline std::string Naming::symbol(SymbolCategory cat, 
                                   const std::string& package, 
                                   const std::string& name) {
    switch (cat) {
        case SymbolCategory::InternalFunction:
            return internalFunction(package, name);
        case SymbolCategory::ExportFunction:
            return exportFunction(package, name);
        case SymbolCategory::ExternFunction:
            return externFunction(name);
        case SymbolCategory::Component:
            return component(package, name);
        case SymbolCategory::Trait:
            return trait(package, name);
        case SymbolCategory::System:
            return system(package, name);
        case SymbolCategory::SystemFunction:
            return systemFunction(package, name);
        case SymbolCategory::TemplateSpawn:
            return templateSpawn(package, name);
        case SymbolCategory::ComponentTypeId:
            return componentTypeId(package, name);
        case SymbolCategory::TraitTypeId:
            return traitTypeId(package, name);
        case SymbolCategory::WorldVar:
            return worldVar(package);
        case SymbolCategory::ModuleInit:
            return moduleInit(package);
        case SymbolCategory::ExternType:
            return externType(package, name);
        case SymbolCategory::HandleType:
            return handleType(package, name);
    }
    return name; // Fallback
}

// ========== Component Naming ==========
inline std::string Naming::component(const std::string& package, const std::string& name) {
    return withPackage(package, name);
}

inline std::string Naming::componentTypeId(const std::string& package, const std::string& name) {
    return "s_ctype_" + withPackage(package, name);
}

// ========== Trait Naming ==========
inline std::string Naming::trait(const std::string& package, const std::string& name) {
    return withPackage(package, name);
}

inline std::string Naming::traitTypeId(const std::string& package, const std::string& name) {
    return "s_trait_" + withPackage(package, name);
}

// ========== System Naming ==========
inline std::string Naming::system(const std::string& package, const std::string& name) {
    return withPackage(package, name);
}

inline std::string Naming::systemFunction(const std::string& package, const std::string& name) {
    return withPackage(package, "system_" + name);
}

// ========== Function Naming ==========
inline std::string Naming::internalFunction(const std::string& package, const std::string& name) {
    return withPackage(package, name);  // All functions are prefixed with package
}

inline std::string Naming::exportFunction(const std::string& package, const std::string& name) {
    return withPackage(package, name);  // Same as internal - both are prefixed
}

inline std::string Naming::externFunction(const std::string& name) {
    return name;  // No prefix for extern functions
}

inline std::string Naming::function(const std::string& package, const std::string& name,
                                     bool isExport, bool isExtern) {
    if (isExtern) {
        return externFunction(name);
    } else if (isExport) {
        return exportFunction(package, name);
    } else {
        return internalFunction(package, name);
    }
}

// ========== Template Naming ==========
inline std::string Naming::templateSpawn(const std::string& package, const std::string& name) {
    return withPackage(package, "spawn_" + name);
}

// ========== Module & World Naming ==========
inline std::string Naming::moduleInit(const std::string& package) {
    return withPackage(package, "paani_module_init");
}

inline std::string Naming::worldVar(const std::string& package) {
    return "__paani_gen_" + package + "_w";
}

inline std::string Naming::thisWorldVar() {
    return "__paani_gen_this_w";
}

// ========== Extern & Handle Types ==========
inline std::string Naming::externType(const std::string& package, const std::string& name) {
    return withPackage(package, name);
}

inline std::string Naming::handleType(const std::string& package, const std::string& name) {
    return withPackage(package, name);
}

// ========== Generic Helpers ==========
inline std::string Naming::withPackage(const std::string& package, const std::string& name) {
    return package + "__" + name;
}

inline std::string Naming::withPrefix(const std::string& prefix, const std::string& name) {
    return prefix + name;
}

// ========== Reserved Names Checking ==========
inline bool Naming::isReserved(const std::string& name) {
    return isCReserved(name) || isCompilerReserved(name);
}

inline bool Naming::isCReserved(const std::string& name) {
    if (C_RESERVED_NAMES.count(name) > 0) {
        return true;
    }
    // Check reserved prefixes
    for (const auto& prefix : C_RESERVED_PREFIXES) {
        if (name.find(prefix) == 0) {
            return true;
        }
    }
    return false;
}

inline bool Naming::isCompilerReserved(const std::string& name) {
    for (const auto& prefix : COMPILER_RESERVED_PREFIXES) {
        if (name.find(prefix) == 0) {
            return true;
        }
    }
    return false;
}

inline std::string Naming::suggestAlternative(const std::string& name) {
    // Simple suggestion: add package-like prefix
    return "my_" + name;
}

// ========== C Standard Library Header Lookup ==========
inline std::string Naming::getCHeaderForFunction(const std::string& name) {
    // stdio.h functions
    static const std::unordered_set<std::string> stdio_funcs = {
        "printf", "fprintf", "sprintf", "snprintf", "scanf", "sscanf", "fscanf",
        "fopen", "fclose", "fread", "fwrite", "fseek", "ftell", "rewind", "fflush",
        "getchar", "putchar", "gets", "puts", "perror", "remove", "rename", "tmpfile",
        "tmpnam", "fgetc", "fputc", "fgets", "fputs", "getc", "putc", "ungetc",
        "clearerr", "feof", "ferror", "fileno", "freopen", "setbuf", "setvbuf"
    };
    
    // stdlib.h functions
    static const std::unordered_set<std::string> stdlib_funcs = {
        "malloc", "calloc", "realloc", "free", "exit", "abort", "system", "getenv",
        "atoi", "atol", "atof", "strtol", "strtoul", "strtod", "strtof", "strtold",
        "rand", "srand", "qsort", "bsearch", "abs", "labs", "llabs",
        "div", "ldiv", "lldiv", "mblen", "mbtowc", "wctomb", "mbstowcs", "wcstombs"
    };
    
    // string.h functions
    static const std::unordered_set<std::string> string_funcs = {
        "strlen", "strcpy", "strncpy", "strcat", "strncat", "strcmp", "strncmp",
        "strchr", "strrchr", "strstr", "strtok", "strerror", "strcoll", "strxfrm",
        "memcpy", "memmove", "memset", "memcmp", "memchr"
    };
    
    // math.h functions
    static const std::unordered_set<std::string> math_funcs = {
        "sin", "cos", "tan", "asin", "acos", "atan", "atan2", "sinh", "cosh", "tanh",
        "sqrt", "pow", "exp", "log", "log10", "log2", "ceil", "floor", "fabs", "fmod",
        "round", "trunc", "nearbyint", "remainder", "fmax", "fmin", "fma"
    };
    
    // time.h functions
    static const std::unordered_set<std::string> time_funcs = {
        "time", "clock", "difftime", "mktime", "strftime", "localtime", "gmtime",
        "asctime", "ctime", "timespec_get"
    };
    
    // ctype.h functions
    static const std::unordered_set<std::string> ctype_funcs = {
        "isalpha", "isdigit", "isalnum", "isspace", "isupper", "islower",
        "toupper", "tolower", "isblank", "iscntrl", "isgraph", "isprint", "ispunct"
    };
    
    // signal.h functions
    static const std::unordered_set<std::string> signal_funcs = {
        "signal", "raise"
    };
    
    // locale.h functions
    static const std::unordered_set<std::string> locale_funcs = {
        "setlocale", "localeconv"
    };
    
    // wchar.h functions
    static const std::unordered_set<std::string> wchar_funcs = {
        "wprintf", "wscanf", "fgetwc", "fputwc", "fgetws", "fputws"
    };
    
    if (stdio_funcs.count(name)) return "stdio.h";
    if (stdlib_funcs.count(name)) return "stdlib.h";
    if (string_funcs.count(name)) return "string.h";
    if (math_funcs.count(name)) return "math.h";
    if (time_funcs.count(name)) return "time.h";
    if (ctype_funcs.count(name)) return "ctype.h";
    if (signal_funcs.count(name)) return "signal.h";
    if (locale_funcs.count(name)) return "locale.h";
    if (wchar_funcs.count(name)) return "wchar.h";
    
    return "";  // Unknown function
}

} // namespace paani