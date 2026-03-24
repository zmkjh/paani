inline void CodeGenerator::genGive(const GiveStmt& stmt) {
    std::string entity = genExpr(*stmt.entity);
    std::string compName = packageName_ + "__" + stmt.componentName;
    std::string world = ctx_.getCurrentWorld().empty() ? "__paani_gen_world" : ctx_.getCurrentWorld();

    if (stmt.initFields.empty()) {
        // Simple give without initialization
        ctx_.writeImplLn(ctx_.indent() + "paani_component_add(" + world + ", " + entity + ", s_ctype_" + compName + ", NULL);");
    } else {
        // Give with field initialization
        ctx_.writeImplLn(ctx_.indent() + "{");
        ctx_.pushIndent();
        ctx_.writeImplLn(ctx_.indent() + compName + " __comp_data = {0};");

        // Initialize each field
        for (const auto& field : stmt.initFields) {
            std::string value = genExpr(*field.second);
            ctx_.writeImplLn(ctx_.indent() + "__comp_data." + field.first + " = " + value + ";");
        }

        ctx_.writeImplLn(ctx_.indent() + "paani_component_add(" + world + ", " + entity + ", s_ctype_" + compName + ", &__comp_data);");
        ctx_.popIndent();
        ctx_.writeImplLn(ctx_.indent() + "}");
    }
}

inline void CodeGenerator::genTag(const TagStmt& stmt) {
    std::string entity = genExpr(*stmt.entity);
    std::string traitName = packageName_ + "__" + stmt.traitName;
    std::string world = ctx_.getCurrentWorld().empty() ? "__paani_gen_world" : ctx_.getCurrentWorld();
    ctx_.writeImplLn(ctx_.indent() + "paani_trait_add(" + world + ", " + entity + ", s_trait_" + traitName + ");");
}

inline void CodeGenerator::genUntag(const UntagStmt& stmt) {
    std::string entity = genExpr(*stmt.entity);
    std::string traitName = packageName_ + "__" + stmt.traitName;
    std::string world = ctx_.getCurrentWorld().empty() ? "__paani_gen_world" : ctx_.getCurrentWorld();
    ctx_.writeImplLn(ctx_.indent() + "paani_trait_remove(" + world + ", " + entity + ", s_trait_" + traitName + ");");
}

inline void CodeGenerator::genDestroy(const DestroyStmt& stmt) {
    std::string world = ctx_.getCurrentWorld().empty() ? "__paani_gen_world" : ctx_.getCurrentWorld();
    if (stmt.entity) {
        ctx_.writeImplLn(ctx_.indent() + "paani_entity_destroy(" + world + ", " + genExpr(*stmt.entity) + ");");
    } else {
        ctx_.writeImplLn(ctx_.indent() + "paani_entity_destroy_deferred(" + world + ", e);");
    }
}

inline void CodeGenerator::genExit(const ExitStmt&) {
    emitAllHandleDtors();
    std::string world = ctx_.getCurrentWorld().empty() ? "__paani_gen_world" : ctx_.getCurrentWorld();
    ctx_.writeImplLn(ctx_.indent() + "paani_exit(" + world + ");");
}
