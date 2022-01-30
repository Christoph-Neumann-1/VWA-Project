#include <GenerateByteCode.hpp>

namespace vwa
{

    void discard(NodeResult result, std::vector<bc::BcToken> &bc, Cache *cache)
    {
        auto size = result.pointerDepth ? getSizeOfType(result.type, cache->structs) : 8;
        if (size)
        {
            bc.push_back({bc::Pop});
            pushToBc(bc, size);
        }
    }

    // The exact pointer depth is irrelevant
    void typeCast(const uint64_t resultT, const bool rIsPtr, const uint64_t argT, const bool aIsPtr, std::vector<bc::BcToken> &bc, Logger &log)
    {
        // FIXME: implement pointers
        if (rIsPtr || aIsPtr)
            throw std::runtime_error("Pointers not implemented");
        if (resultT == argT)
            return;
        if (resultT == PrimitiveTypes::Void || argT == PrimitiveTypes::Void)
        {
            log << Logger::Error << "Invalid cast casting from/to void is invalid\n";
            throw std::runtime_error("Cannot cast void");
        }
        switch (argT)
        {
        case PrimitiveTypes::F64:
            switch (resultT)
            {
            case PrimitiveTypes::I64:
                bc.push_back({bc::FtoI});
                break;
            case PrimitiveTypes::U8:
                bc.push_back({bc::FtoC});
                break;
            default:
                log << Logger::Error << "Casting is only possible for builtin types\n";
                throw std::runtime_error("Casting is only possible for builtin types");
            }
            return;
        case PrimitiveTypes::I64:
            switch (argT)
            {
            case PrimitiveTypes::F64:
                bc.push_back({bc::ItoF});
                break;
            case PrimitiveTypes::U8:
                bc.push_back({bc::CtoI});
                break;
            default:
                log << Logger::Error << "Casting is only possible for builtin types\n";
                throw std::runtime_error("Casting is only possible for builtin types");
            }
            return;
        case PrimitiveTypes::U8:
            switch (argT)
            {
            case PrimitiveTypes::F64:
                bc.push_back({bc::ItoF});
                bc.push_back({bc::FtoC});
                break;
            case PrimitiveTypes::I64:
                bc.push_back({bc::ItoC});
                break;
            default:
                log << Logger::Error << "Casting is only possible for builtin types\n";
                throw std::runtime_error("Casting is only possible for builtin types");
            }
            return;
        }
    }
    void compileFunc(Linker::Module *module, Cache *cache, const Pass1Result::Function &func, std::vector<uint8_t> &constPool, std::vector<bc::BcToken> &bc, Logger &log)
    {
        log << Logger::Info << "Compiling function " << func.name << '\n';
        if (func.name == "main")
        {
            if (func.returnType.name != "int" || func.returnType.pointerDepth)
            {
                log << Logger::Error << "Main function must return int\n";
                throw std::runtime_error("Main function must return int");
            }
            log << Logger::Info << "Found main function\n";
            if (module->main)
            {
                log << Logger::Error << "Multiple main functions found\n";
                throw std::runtime_error("Multiple main functions found");
            }
            else
                module->main = bc.size() + 1;
        }
        auto cached = cache->map.find(func.name);
        if (cached == cache->map.end() || cached->second.second != Cache::Type::Function)
        {
            log << Logger::Error << "Function not found in cache";
            throw std::runtime_error("Function not found in cache");
        }
        auto &funcData = cache->functions[cached->second.first];
        std::vector<Scope> scopes;
        Scope scope;
        if (funcData.args.size() != func.parameters.size())
        {
            log << Logger::Error << "Function invalid parameter count missmatch between cache and original parameters";
            throw std::runtime_error("Function has wrong number of parameters");
        }
        for (size_t i = 0; i < funcData.args.size(); i++)
        {
            scope.variables.insert({func.parameters[i].name, {scope.size, funcData.args[i].type}});
            scope.size += funcData.args[i].pointerDepth ? getSizeOfType(funcData.args[i].type, cache->structs) : 9;
        }
        scopes.push_back(scope);
        // TODO: store the offsets somewhere What was I thinking?
        auto res = compileNode(module, cache, &func.body, NodeResult{funcData.returnType.type, funcData.returnType.pointerDepth}, constPool, bc, scopes, log);
        if (res.type != PrimitiveTypes::Void || res.pointerDepth)
        {
            log << Logger::Info << "Function body evaluates to value, adding implicit return\n";
            typeCast(funcData.returnType.type, funcData.returnType.pointerDepth, res.type, res.pointerDepth, bc, log);
        }
        bc.push_back(bc::BcToken{bc::Return});
        // TODO: include pointer computation in getSizeOfType
        pushToBc(bc, uint64_t{res.pointerDepth ? getSizeOfType(res.type, cache->structs) : 8});
    }

    NodeResult promoteType(const uint64_t type, const bool isPtr, const uint64_t otherType, const bool otherIsPtr, std::vector<bc::BcToken> &bc, Logger &log)
    {
        if (type == otherType && isPtr == otherIsPtr)
            return NodeResult{type, isPtr};
        if (isPtr || otherIsPtr)
            throw std::runtime_error("Pointers not implemented");
        if (type == PrimitiveTypes::F64 || otherType == PrimitiveTypes::F64)
        {
            typeCast(PrimitiveTypes::F64, false, type, false, bc, log);
            typeCast(PrimitiveTypes::F64, false, otherType, false, bc, log);
            return NodeResult{PrimitiveTypes::F64, false};
        }
        if (type == PrimitiveTypes::I64 || otherType == PrimitiveTypes::I64)
        {
            typeCast(PrimitiveTypes::I64, false, type, false, bc, log);
            typeCast(PrimitiveTypes::I64, false, otherType, false, bc, log);
            return NodeResult{PrimitiveTypes::I64, false};
        }
        if (type == PrimitiveTypes::U8 || otherType == PrimitiveTypes::U8)
        {
            typeCast(PrimitiveTypes::U8, false, type, false, bc, log);
            typeCast(PrimitiveTypes::U8, false, otherType, false, bc, log);
            return NodeResult{PrimitiveTypes::U8, false};
        }
        log << Logger::Error << "Cannot promote types\n";
        throw std::runtime_error("Cannot promote types");
    }

    // TODO: return the type the expression evaluates to
    // TODO: keep track of current stack top, or implement instructions relative to stack top
    NodeResult compileNode(Linker::Module *module, Cache *cache, const Node *node, const NodeResult fRetT, std::vector<uint8_t> &constPool, std::vector<bc::BcToken> &bc, std::vector<Scope> &scopes, Logger &log)
    {
        // TODO: consider another pass to generate scopes beforehand
        switch (node->type)
        {
        case Node::Type::Unassigned:
            log << Logger::Error << "Unassigned node";
            throw std::runtime_error("Unassigned node");
        case Node::Type::Return:
            if (node->children.size())
            {
                auto res = compileNode(module, cache, &node->children.back(), fRetT, constPool, bc, scopes, log);
                typeCast(fRetT.type, fRetT.pointerDepth, res.type, res.pointerDepth, bc, log);
            }
            bc.push_back({bc::Return});
            pushToBc(bc, uint64_t{fRetT.pointerDepth ? getSizeOfType(fRetT.type, cache->structs) : 8});
            return {};
        case Node::Type::LiteralI:
            bc.push_back({bc::Push64});
            pushToBc(bc, std::get<int64_t>(node->value));
            return {PrimitiveTypes::I64};
        case Node::Type::LiteralF:
            bc.push_back({bc::Push64});
            pushToBc(bc, std::get<double>(node->value));
            return {PrimitiveTypes::F64};
        case Node::Type::Block:
            // I do not know why, but this entered a infinite loop at runtime
            // for (auto &child : node->children)
            for (size_t i = 0; i < node->children.size(); ++i)
                discard(compileNode(module, cache, &node->children[i], fRetT, constPool, bc, scopes, log), bc, cache);
            return {};
        case Node::Type::Plus:
        {
            auto lhs = compileNode(module, cache, &node->children[0], fRetT, constPool, bc, scopes, log);
            auto rhs = compileNode(module, cache, &node->children[1], fRetT, constPool, bc, scopes, log);
            auto ret = promoteType(lhs.type, lhs.pointerDepth, rhs.type, rhs.pointerDepth, bc, log);
            switch (ret.type)
            {
            case PrimitiveTypes::I64:
                bc.push_back({bc::AddI});
                break;
            }
            return ret;
        }
        }
    }
}