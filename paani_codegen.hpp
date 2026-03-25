// paani_codegen.hpp - Main code generator interface
// Refactored version: modular design with clear separation of concerns

#pragma once

 // Include all modular components
#include "paani_codegen/core/CodeGenContext.hpp"
#include "paani_codegen/core/TypeMapper.hpp"
#include "paani_codegen/handle/HandleTracker.hpp"
#include "paani_codegen/handle/HandleEmitter.hpp"
#include "paani_package.hpp"  // For PackageInfo and PackageManager
#include "paani_codegen/system/SystemGenerator.hpp"
#include "paani_codegen/system/SystemGenerator.inl"
#include "paani_codegen/decl/TemplateGenerator.hpp"
#include "paani_codegen/decl/TemplateGenerator.inl"
#include "paani_codegen/decl/DependencyGenerator.hpp"
#include "paani_codegen/decl/DependencyGenerator.inl"
#include "paani_codegen/expr/FStringGenerator.hpp"
#include "paani_codegen/expr/FStringGenerator.inl"
#include "paani_codegen/CodeGenerator.hpp"