#include <GenerateByteCode.hpp>
// FIXME: if any code for casting is inserted, I NEED to make sure there are no problems with reads relative to the instruction ptr. I'll need that should I ever implement statement expressions.
//  TODO: assert, at some point that primitive types are not overriden
//  TODO: output more useful format and use it for optimization
namespace vwa
{

    auto &getMember(const std::string &name, const size_t type, const Cache &cache)
    {
        if (type < numReservedIndices)
            throw std::runtime_error("Attempted member access on primitive type");
        auto &typeInfo = cache.structs[type - numReservedIndices].symbol->data;
        if (auto val = std::get_if<1>(&typeInfo))
        {
            if (auto it = std::find_if(val->fields.begin(), val->fields.end(), [&name](const auto &field)
                                       { return field.name == name; });
                it != val->fields.end())
            {
                auto idx = std::distance(val->fields.begin(), it);
                return cache.structs[type - numReservedIndices].members[idx];
            }
            else
            {
                throw std::runtime_error("Field " + name + " not found in struct " + cache.structs[type - numReservedIndices].symbol->name.name);
            }
        }
        else
        {
            throw std::runtime_error("Attempted member access on non-struct type");
        }
    }

    void discard(NodeResult result, std::vector<bc::BcToken> &bc, Cache *cache)
    {
        auto size = getSizeOfType(result.type, result.pointerDepth, cache->structs);
        if (size)
        {
            bc.push_back({bc::Pop});
            pushToBc(bc, size);
        }
    }

    // The exact pointer depth is irrelevant
    [[nodiscard]] std::optional<bc::BcInstruction> typeCast(const uint64_t resultT, const bool rIsPtr, const uint64_t argT, const bool aIsPtr, Logger &log)
    {
        // FIXME: implement pointers
        if (resultT == argT && rIsPtr == aIsPtr)
            return std::nullopt;
        if (rIsPtr && aIsPtr)
            return std::nullopt;
        if (rIsPtr)
        {
            if (argT == PrimitiveTypes::I64)
                return std::nullopt;
            log << Logger::Error << "Pointer cast to non-pointer type\n";
            throw std::runtime_error("Pointer cast to non-pointer type");
        }
        if (aIsPtr)
        {
            if (resultT == PrimitiveTypes::I64)
                return std::nullopt;
            log << Logger::Error << "Non-pointer cast to pointer type\n";
            throw std::runtime_error("Non-pointer cast to pointer type");
        }
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
                return bc::FtoI;
            case PrimitiveTypes::U8:
                return bc::FtoC;
            default:
                log << Logger::Error << "Casting is only possible for builtin types\n";
                throw std::runtime_error("Casting is only possible for builtin types");
            }
            return std::nullopt;
        case PrimitiveTypes::I64:
            switch (resultT)
            {
            case PrimitiveTypes::F64:
                return bc::ItoF;
            case PrimitiveTypes::U8:
                return bc::ItoC;
            default:
                log << Logger::Error << "Casting is only possible for builtin types\n";
                throw std::runtime_error("Casting is only possible for builtin types");
            }
            return std::nullopt;
        case PrimitiveTypes::U8:
            switch (resultT)
            {
                // TODO: implement this
            case PrimitiveTypes::F64:
                return bc::CtoF;
            case PrimitiveTypes::I64:
                return bc::CtoI;
            default:
                log << Logger::Error << "Casting is only possible for builtin types\n";
                throw std::runtime_error("Casting is only possible for builtin types");
            }
            return std::nullopt;
        default:
            log << Logger::Error << "Casting is only possible for builtin types\n";
            throw std::runtime_error("Casting is only possible for builtin types");
        }
    }
    void compileFunc(Linker::Module *module, Cache *cache, const Pass1Result::Function &func, std::vector<uint8_t> &constPool, std::vector<bc::BcToken> &bc, Logger &log)
    {
        log << Logger::Info << "Compiling function " << func.name << '\n';
        if (func.name == "main")
        {
            if (func.returnType.name.name != "int" || func.returnType.pointerDepth)
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
        auto cached = cache->map.find({func.name, module->name});
        if (cached == cache->map.end() || cached->second.second != Cache::Type::Function)
        {
            log << Logger::Error << "Function not found in cache, this should never happen";
            throw std::runtime_error("Function not found in cache");
        }
        auto &funcData = cache->functions[cached->second.first];
        std::vector<Scope> scopes;
        Scope scope{0, 16};
        if (funcData.args.size() != func.parameters.size())
        {
            log << Logger::Error << "Function invalid parameter count missmatch between cache and original parameters";
            throw std::runtime_error("Function has wrong number of parameters");
        }
        for (size_t i = 0; i < funcData.args.size(); i++)
        {
            scope.variables.insert({func.parameters[i].name, {scope.size + scope.offset, funcData.args[i].type, funcData.args[i].pointerDepth}});
            scope.size += getSizeOfType(funcData.args[i].type, funcData.args[i].pointerDepth, cache->structs);
        }
        scopes.push_back(scope);
        funcData.address = bc.size();
        funcData.finished = true; // Indicates that the adress is final.
        // TODO: store the offsets somewhere What was I thinking?
        // FIXME: consider all outcomes, like having a value at the end of a void function and such
        auto res = compileNode(module, cache, &func.body, NodeResult{funcData.returnType.type, funcData.returnType.pointerDepth}, constPool, bc, scopes, log);
        if (res.type != PrimitiveTypes::Void || res.pointerDepth)
        {
            log << Logger::Info << "Function body evaluates to value, adding implicit return\n";
            if (auto instr = typeCast(funcData.returnType.type, funcData.returnType.pointerDepth, res.type, res.pointerDepth, log); instr)
                bc.push_back({*instr});
        }
        // Even if there is no implicit return, add this to make sure the function returns at all
        bc.push_back(bc::BcToken{bc::Return});
        pushToBc(bc, uint64_t{getSizeOfType(res.type, res.pointerDepth, cache->structs)});
    }

    // First pos is needed if the first args is to be cast
    NodeResult promoteType(const uint64_t type, const bool isPtr, const uint64_t otherType, const bool otherIsPtr, std::vector<bc::BcToken> &bc, size_t firstPos, Logger &log)
    {
        if (type == otherType && isPtr == otherIsPtr)
            return NodeResult{type, isPtr};
        if (isPtr || otherIsPtr)
            throw std::runtime_error("Pointers not implemented");
        if (type == PrimitiveTypes::F64 || otherType == PrimitiveTypes::F64)
        {
            if (auto instr = typeCast(PrimitiveTypes::F64, false, type, false, log); instr)
                bc.insert(bc.begin() + firstPos, {*instr});
            // typeCast(PrimitiveTypes::F64, false, otherType, false, log);
            else if (auto instr = typeCast(PrimitiveTypes::F64, false, otherType, false, log); instr)
                bc.push_back({*instr});
            // typeCast(PrimitiveTypes::F64, false, type, false, log);
            return NodeResult{PrimitiveTypes::F64, false};
        }
        if (type == PrimitiveTypes::I64 || otherType == PrimitiveTypes::I64)
        {
            // typeCast(PrimitiveTypes::I64, false, type, false, log);
            // typeCast(PrimitiveTypes::I64, false, otherType, false, log);

            if (auto instr = typeCast(PrimitiveTypes::I64, false, type, false, log); instr)
                bc.insert(bc.begin() + firstPos, {*instr});
            else if (auto instr = typeCast(PrimitiveTypes::I64, false, otherType, false, log); instr)
                bc.push_back({*instr});

            return NodeResult{PrimitiveTypes::I64, false};
        }
        // Is there any point in doing this?
        if (type == PrimitiveTypes::U8 || otherType == PrimitiveTypes::U8)
        {
            // typeCast(PrimitiveTypes::U8, false, type, false, log);
            // typeCast(PrimitiveTypes::U8, false, otherType, false, log);
            if (auto instr = typeCast(PrimitiveTypes::U8, false, type, false, log); instr)
                bc.insert(bc.begin() + firstPos, {*instr});
            else if (auto instr = typeCast(PrimitiveTypes::U8, false, otherType, false, log); instr)
                bc.push_back({*instr});
            return NodeResult{PrimitiveTypes::U8, false};
        }
        log << Logger::Error << "Cannot promote types\n";
        throw std::runtime_error("Cannot promote types");
    }

    Scope generateBlockScope(const Node &node, const Cache &cache, Logger &log, size_t offset)
    {
        // Iterate over all children and add declvars to scope.
        Scope scope{0, offset};
        for (auto &child : node.children)
            if (child.type == Node::Type::DeclareVar)
            {
                auto [_, res] = scope.variables.insert({std::get<std::string>(child.children[0].value), {scope.size + scope.offset, std::get<Node::VarTypeCached>(child.children[1].value).index, std::get<Node::VarTypeCached>(child.children[1].value).pointerDepth}});
                if (!res)
                {
                    log << Logger::Error << "Variable " << std::get<std::string>(child.children[0].value) << " already declared\n";
                    throw std::runtime_error("Variable already declared");
                }
                scope.size += getSizeOfType(std::get<Node::VarTypeCached>(child.children[1].value).index, std::get<Node::VarTypeCached>(child.children[1].value).pointerDepth, cache.structs);
            }
        return scope;
    }

    // TODO: member access
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
                if (auto instr = typeCast(fRetT.type, fRetT.pointerDepth, res.type, res.pointerDepth, log); instr)
                    bc.push_back({*instr});
            }
            bc.push_back({bc::Return});
            pushToBc(bc, uint64_t{getSizeOfType(fRetT.type, fRetT.pointerDepth, cache->structs)});
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
        {
            auto scope = generateBlockScope(*node, *cache, log, scopes.back().size + scopes.back().offset);
            scopes.push_back(scope);
            if (scope.size)
            {
                bc.push_back({bc::Push});
                pushToBc(bc, uint64_t{scope.size});
            }
            // I do not know why, but this entered a infinite loop, but it works now so I won't touch it
            // for (auto &child : node->children)
            for (size_t i = 0; i < node->children.size() - 1; ++i)
                // Discard means that the value is not used, it is just dropped from the stack
                // TODO: consider not discarding last value
                discard(compileNode(module, cache, &node->children[i], fRetT, constPool, bc, scopes, log), bc, cache);
            auto res = compileNode(module, cache, &node->children.back(), fRetT, constPool, bc, scopes, log);
            if (scope.size)
            {
                bc.push_back({bc::Pop});
                pushToBc(bc, uint64_t{scope.size});
            }
            scopes.pop_back();
            return res;
        }
        case Node::Type::Plus:
        {
            auto lhs = compileNode(module, cache, &node->children[0], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            auto rhs = compileNode(module, cache, &node->children[1], fRetT, constPool, bc, scopes, log);
            NodeResult ret;
            if (!lhs.pointerDepth && !rhs.pointerDepth)
                ret = promoteType(lhs.type, lhs.pointerDepth, rhs.type, rhs.pointerDepth, bc, pos, log);
            else if (lhs.pointerDepth)
                if (rhs.pointerDepth)
                    throw std::runtime_error("Cannot add pointers");
                else if (rhs.type == PrimitiveTypes::I64)
                {
                    ret = lhs;
                    bc.push_back({bc::AddI});
                    return ret;
                }
                else
                    throw std::runtime_error("Cannot add pointers");
            else if (rhs.pointerDepth)
                if (lhs.type == PrimitiveTypes::I64)
                {
                    ret = rhs;
                    // TODO this is a horrible solution, I really need to find out if this is a safe thing to do
                    bc.push_back({bc::AddI});
                    return ret;
                }
                else
                    throw std::runtime_error("Cannot add pointers");
            switch (ret.type)
            {
            case PrimitiveTypes::I64:
                bc.push_back({bc::AddI});
                break;
            case PrimitiveTypes::F64:
                bc.push_back({bc::AddF});
                break;
            default:
                log << Logger::Error << "Cannot add types\n";
                throw std::runtime_error("Cannot add types");
            }
            return ret;
        }
        case Node::Type::Minus:
        {
            auto lhs = compileNode(module, cache, &node->children[0], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            auto rhs = compileNode(module, cache, &node->children[1], fRetT, constPool, bc, scopes, log);
            auto ret = promoteType(lhs.type, lhs.pointerDepth, rhs.type, rhs.pointerDepth, bc, pos, log);
            switch (ret.type)
            {
            case PrimitiveTypes::I64:
                bc.push_back({bc::SubI});
                break;
            case PrimitiveTypes::F64:
                bc.push_back({bc::SubF});
                break;
            default:
                log << Logger::Error << "Cannot subtract types\n";
                throw std::runtime_error("Cannot subtract types");
            }
            return ret;
        }
        case Node::Type::UnaryMinus:
        {
            // TODO: promote chars to int for this and similar ops
            auto res = compileNode(module, cache, &node->children[0], fRetT, constPool, bc, scopes, log);
            if (res.pointerDepth)
                throw std::runtime_error("Pointer not implemented");
            switch (res.type)
            {
            case PrimitiveTypes::I64:
                bc.push_back({bc::Push64});
                pushToBc(bc, int64_t{-1});
                bc.push_back({bc::MulI});
                break;
            case PrimitiveTypes::F64:
                bc.push_back({bc::NegF});
                break;
            default:
                log << Logger::Error << "Cannot negate type\n";
                throw std::runtime_error("Cannot negate type");
            }
            return res;
        }
        case Node::Type::Multiply:
        {
            auto lhs = compileNode(module, cache, &node->children[0], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            auto rhs = compileNode(module, cache, &node->children[1], fRetT, constPool, bc, scopes, log);
            auto ret = promoteType(lhs.type, lhs.pointerDepth, rhs.type, rhs.pointerDepth, bc, pos, log);
            switch (ret.type)
            {
            case PrimitiveTypes::I64:
                bc.push_back({bc::MulI});
                break;
            case PrimitiveTypes::F64:
                bc.push_back({bc::MulF});
                break;
            default:
                log << Logger::Error << "Cannot multiply types\n";
                throw std::runtime_error("Cannot multiply types");
            }
            return ret;
        }
        case Node::Type::Divide:
        {
            auto lhs = compileNode(module, cache, &node->children[0], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            auto rhs = compileNode(module, cache, &node->children[1], fRetT, constPool, bc, scopes, log);
            auto ret = promoteType(lhs.type, lhs.pointerDepth, rhs.type, rhs.pointerDepth, bc, pos, log);
            switch (ret.type)
            {
            case PrimitiveTypes::I64:
                bc.push_back({bc::DivI});
                break;
            case PrimitiveTypes::F64:
                bc.push_back({bc::DivF});
                break;
            default:
                log << Logger::Error << "Cannot divide types\n";
                throw std::runtime_error("Cannot divide types");
            }
            return ret;
        }
        case Node::Type::Modulo:
        {
            auto lhs = compileNode(module, cache, &node->children[0], fRetT, constPool, bc, scopes, log);
            if (auto instr = typeCast(PrimitiveTypes::I64, false, lhs.type, lhs.pointerDepth, log); instr)
                bc.push_back({*instr});
            auto rhs = compileNode(module, cache, &node->children[1], fRetT, constPool, bc, scopes, log);
            if (auto instr = typeCast(PrimitiveTypes::I64, false, rhs.type, rhs.pointerDepth, log); instr)
                bc.push_back({*instr});
            bc.push_back({bc::ModI});
            return {PrimitiveTypes::I64};
        }
        case Node::Type::Power:
        {
            auto lhs = compileNode(module, cache, &node->children[0], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            auto rhs = compileNode(module, cache, &node->children[1], fRetT, constPool, bc, scopes, log);
            auto ret = promoteType(lhs.type, lhs.pointerDepth, rhs.type, rhs.pointerDepth, bc, pos, log);
            switch (ret.type)
            {
            case PrimitiveTypes::I64:
                bc.push_back({bc::PowerI});
                break;
            case PrimitiveTypes::F64:
                bc.push_back({bc::PowerF});
                break;
            default:
                log << Logger::Error << "Cannot power types\n";
                throw std::runtime_error("Cannot power types");
            }
            return ret;
        }
        case Node::Type::DeclareVar:
        {
            if (node->children.size() == 4)
            {
                auto &name = std::get<std::string>(node->children[0].value);
                auto it = scopes.back().variables.find(name);
                auto init = compileNode(module, cache, &node->children[3], fRetT, constPool, bc, scopes, log);
                if (auto instr = typeCast(it->second.type, it->second.pointerDepth, init.type, init.pointerDepth, log); instr)
                    bc.push_back({*instr});
                bc.push_back({bc::WriteRel});
                pushToBc<intptr_t>(bc, it->second.offset);
                pushToBc<uint64_t>(bc, getSizeOfType(init.type, init.pointerDepth, cache->structs));
            }
            return {};
        }
        case Node::Type::Assign:
        {
            auto expr = compileNode(module, cache, &node->children[1], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            auto &what = node->children[0];
            NodeResult rt;
            if (what.type == Node::Type::Dereference)
            {
                rt = compileNode(module, cache, &what.children[0], fRetT, constPool, bc, scopes, log);
                bc.push_back({bc::WriteAbs});
            }
            else if (what.type == Node::Type::Variable)
            {
                bc.push_back({bc::WriteRel});
                auto &name = std::get<Identifier>(what.value).name; // This is ok, as there are no global variables
                for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
                {
                    auto it2 = it->variables.find(name);
                    if (it2 != it->variables.end())
                    {
                        rt.type = it2->second.type;
                        rt.pointerDepth = it2->second.pointerDepth;
                        pushToBc<intptr_t>(bc, it2->second.offset);
                    }
                }
            }
            // TODO: operator -> either here or by rewriting it earlier
            else if (what.type == Node::Type::MemberAccess)
            {
                auto evalTree = [&](const Node &node, auto &self) -> std::tuple<size_t, size_t, size_t, bool>
                {
                    switch (node.type)
                    {
                    case Node::Type::Variable:
                    {
                        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
                        {
                            auto it2 = it->variables.find(std::get<Identifier>(node.value).name);
                            if (it2 != it->variables.end())
                                return {it2->second.type, it2->second.pointerDepth, it2->second.offset, false};
                        }
                        log << Logger::Error << "Cannot access member\n";
                        throw std::runtime_error("Cannot find variable");
                    }
                    case Node::Type::MemberAccess:
                    {
                        auto prev = self(node.children[0], self);
                        if (std::get<1>(prev))
                        {
                            log << Logger::Error << "Cannot access member of ptr\n";
                            throw std::runtime_error("Cannot access member of ptr");
                        }
                        auto &mem = getMember(std::get<std::string>(node.children[1].value), std::get<0>(prev), *cache);
                        return {mem.type, mem.ptrDepth, mem.offset + std::get<2>(prev), std::get<3>(prev)};
                    }
                    case Node::Type::Dereference:
                    {
                        auto res = compileNode(module, cache, &node.children[0], fRetT, constPool, bc, scopes, log);
                        if (!res.pointerDepth)
                        {
                            log << Logger::Error << "Cannot dereference non-pointer\n";
                            throw std::runtime_error("Cannot dereference non-pointer");
                        }
                        return {res.type, res.pointerDepth - 1, 0, true};
                    }
                    default:
                        log << Logger::Error << "Cannot access member of temporary\n";
                        throw std::runtime_error("Cannot access member of temporary");
                    }
                };
                auto res = evalTree(what, evalTree);
                switch (std::get<3>(res))
                {
                case false:
                    bc.push_back({bc::WriteRel});
                    pushToBc<intptr_t>(bc, std::get<2>(res));
                    break;
                case true:
                    bc.push_back({bc::Push64});
                    // This is probably the wrong sign, but since integers are twos complement this should be fine
                    pushToBc<uint64_t>(bc, std::get<2>(res));
                    bc.push_back({bc::AddI});
                    bc.push_back({bc::WriteAbs});
                    break;
                }
                rt.type = std::get<0>(res);
                rt.pointerDepth = std::get<1>(res);
            }
            else
            {
                log << Logger::Error << "Cannot assign to non-variable yet\n";
                throw std::runtime_error("Cannot assign to non-variable yet");
            }
            if (!rt.type)
                throw std::runtime_error("Assignment failed");
            if (auto instr = typeCast(rt.type, rt.pointerDepth, expr.type, expr.pointerDepth, log); instr)
                bc.insert(bc.begin() + pos - 1, {*instr});
            pushToBc<uint64_t>(bc, getSizeOfType(rt.type, rt.pointerDepth, cache->structs));
            return {};
        }
        case Node::Type::Variable:
        {
            auto &name = std::get<Identifier>(node->value).name;
            for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
            {
                auto it2 = it->variables.find(name);
                if (it2 != it->variables.end())
                {
                    bc.push_back({bc::ReadRel});
                    pushToBc<intptr_t>(bc, it2->second.offset);
                    pushToBc<uint64_t>(bc, getSizeOfType(it2->second.type, it2->second.pointerDepth, cache->structs));
                    return {it2->second.type, it2->second.pointerDepth};
                }
            }
            log << Logger::Error << "Variable " << name << " not found\n";
            throw std::runtime_error("Variable not found");
        }
        case Node::Type::If:
        {
            auto cond = compileNode(module, cache, &node->children[0], fRetT, constPool, bc, scopes, log);
            if (auto instr = typeCast(PrimitiveTypes::I64, false, cond.type, cond.pointerDepth, log); instr)
                bc.push_back({*instr});
            bc.push_back({bc::JumpRelIfFalse});
            auto pos = bc.size();
            pushToBc<int64_t>(bc, 0);
            // TODO: consider evaluating to value
            discard(compileNode(module, cache, &node->children[1], fRetT, constPool, bc, scopes, log), bc, cache);
            if (node->children.size() == 3)
            {
                bc.push_back({bc::JumpRel});
                auto pos2 = bc.size();
                pushToBc<int64_t>(bc, 0);
                *reinterpret_cast<int64_t *>(&bc[pos]) = bc.size() - pos + 1;
                discard(compileNode(module, cache, &node->children[2], fRetT, constPool, bc, scopes, log), bc, cache);
                *reinterpret_cast<int64_t *>(&bc[pos2]) = bc.size() - pos2 + 1;
            }
            else
                *reinterpret_cast<int64_t *>(&bc[pos]) = bc.size() - pos + 1;
            return {};
        }
        case Node::Type::While:
        {
            auto pos = bc.size();
            auto cond = compileNode(module, cache, &node->children[0], fRetT, constPool, bc, scopes, log);
            if (auto instr = typeCast(PrimitiveTypes::I64, false, cond.type, cond.pointerDepth, log); instr)
                bc.push_back({*instr});
            bc.push_back({bc::JumpRelIfFalse});
            auto pos2 = bc.size();
            pushToBc<int64_t>(bc, 0);
            discard(compileNode(module, cache, &node->children[1], fRetT, constPool, bc, scopes, log), bc, cache);
            bc.push_back({bc::JumpRel});
            pushToBc<int64_t>(bc, pos - bc.size() + 1);
            *reinterpret_cast<int64_t *>(&bc[pos2]) = bc.size() - pos2 + 1;
            // TODO: replace continue and break with correct instruction. This requires having a function which allows skipping instructions.
            return {};
        }
        case Node::Type::Dereference:
        {
            auto expr = compileNode(module, cache, &node->children[0], fRetT, constPool, bc, scopes, log);
            // TODO: cast int to ptr
            bc.push_back({bc::ReadAbs});
            pushToBc<uint64_t>(bc, getSizeOfType(expr.type, expr.pointerDepth - 1, cache->structs));
            return {expr.type, expr.pointerDepth - 1};
        }
        // Only works on local variables
        case Node::Type::AddressOf:
        {
            auto evalTree = [&](const Node &node, auto &self) -> std::tuple<size_t, size_t, size_t, bool>
            {
                switch (node.type)
                {
                case Node::Type::Variable:
                {
                    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
                    {
                        auto it2 = it->variables.find(std::get<Identifier>(node.value).name);
                        if (it2 != it->variables.end())
                            return {it2->second.type, it2->second.pointerDepth, it2->second.offset, false};
                    }
                    log << Logger::Error << "Cannot access member\n";
                    throw std::runtime_error("Cannot find variable");
                }
                case Node::Type::MemberAccess:
                {
                    auto prev = self(node.children[0], self);
                    if (std::get<1>(prev))
                    {
                        log << Logger::Error << "Cannot access member of ptr\n";
                        throw std::runtime_error("Cannot access member of ptr");
                    }
                    auto &mem = getMember(std::get<std::string>(node.children[1].value), std::get<0>(prev), *cache);
                    return {mem.type, mem.ptrDepth, mem.offset + std::get<2>(prev), std::get<3>(prev)};
                }
                case Node::Type::Dereference:
                {
                    auto res = compileNode(module, cache, &node.children[0], fRetT, constPool, bc, scopes, log);
                    if (!res.pointerDepth)
                    {
                        log << Logger::Error << "Cannot dereference non-pointer\n";
                        throw std::runtime_error("Cannot dereference non-pointer");
                    }
                    return {res.type, res.pointerDepth - 1, 0, true};
                }
                default:
                    log << Logger::Error << "Cannot access member of temporary\n";
                    throw std::runtime_error("Cannot access member of temporary");
                }
            };
            auto res = evalTree(node->children[0], evalTree);
            switch (std::get<3>(res))
            {
            case false:
                bc.push_back({bc::AbsOf});
                pushToBc<intptr_t>(bc, std::get<2>(res));
                break;
            case true:
                bc.push_back({bc::Push64});
                // This is probably the wrong sign, but since integers are twos complement this should be fine
                pushToBc<uint64_t>(bc, std::get<2>(res));
                bc.push_back({bc::AddI});
                break;
            }
            return {std::get<0>(res), std::get<1>(res) + 1};
            // TODO: support structs
            // auto &name = std::get<Identifier>(node->children[0].value).name;
            // for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
            // {
            //     auto it2 = it->variables.find(name);
            //     if (it2 != it->variables.end())
            //     {
            //         bc.push_back({bc::AbsOf});
            //         pushToBc<uint64_t>(bc, it2->second.offset);
            //         return {it2->second.type, 1};
            //     }
            // }
            // log << Logger::Error << "Cannot take address of non-variable yet\n";
            // throw std::runtime_error("Did not find variable");
        }
        case Node::Type::CallFunc:
        {
            // No support for function pointers yet
            auto &name = std::get<Identifier>(node->children[0].value);
            auto it = cache->map.find(name);
            it = it == cache->map.end() ? cache->map.find({name.name, module->name}) : it;
            if (it == cache->map.end())
            {
                log << Logger::Error << "Function " << name.name << " not found\n";
                throw std::runtime_error("Function not found");
            }
            if (it->second.second == Cache::Type::Struct)
            {
                log << Logger::Error << "Symbol " << name.name << " is a struct, not a function\n";
                throw std::runtime_error("Symbol is a struct, not a function");
            }
            auto &func = cache->functions[it->second.first];
            auto nArgs = node->children.size() - 1;
            if (nArgs != func.args.size())
            {
                log << Logger::Error << "Function " << name.name << " expects " << func.args.size() << " arguments, but got " << nArgs << "\n";
                throw std::runtime_error("Function expects different number of arguments");
            }
            size_t argSize = 0;
            for (size_t i = 0; i < nArgs; i++)
            {
                argSize += getSizeOfType(func.args[i].type, func.args[i].pointerDepth, cache->structs);
                auto arg = compileNode(module, cache, &node->children[i + 1], fRetT, constPool, bc, scopes, log);
                if (auto instr = typeCast(func.args[i].type, func.args[i].pointerDepth, arg.type, arg.pointerDepth, log); instr)
                    bc.push_back({*instr});
            }
            bc.push_back({bc::CallFunc});
            pushToBc<uint64_t>(bc, it->second.first); // Gets replaced by adress or more permanent index later}
            pushToBc<uint64_t>(bc, argSize);
            return {func.returnType.type, func.returnType.pointerDepth};
        }

        // TODO: implement something like decltype to reduce code length
        case Node::Type::SizeOf:
        {
            auto t = std::get<Node::VarTypeCached>(node->value);
            auto size = getSizeOfType(t.index, t.pointerDepth, cache->structs);
            bc.push_back({bc::Push64});
            pushToBc<int64_t>(bc, size);
            return {PrimitiveTypes::I64, 0};
        }
        case Node::Type::MemberAccess:
        {
            enum Type
            {
                ReadAbs, // Root is a pointer
                ReadRel, // Root is a local variable
                ReadTmp, // Root is a temporary value
            };

            auto walkTree = [&](const Node &node, auto &self) -> std::tuple<Node::VarTypeCached, Type, size_t>
            {
                switch (node.type)
                {
                case Node::Type::Variable:
                    // TODO I really need to extract variable lookup into a function
                    {
                        auto &name = std::get<Identifier>(node.value).name;
                        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
                        {
                            auto it2 = it->variables.find(name);
                            if (it2 != it->variables.end())
                                return {{it2->second.type, it2->second.pointerDepth}, ReadRel, it2->second.offset};
                        }
                        log << Logger::Error << "Cannot access non-variable yet\n";
                        throw std::runtime_error("Did not find variable");
                    }
                case Node::Type::Dereference:
                {
                    auto val = compileNode(module, cache, &node.children[0], fRetT, constPool, bc, scopes, log);
                    if (val.pointerDepth == 0)
                    {
                        log << Logger::Error << "Cannot dereference non-pointer\n";
                        throw std::runtime_error("Cannot dereference non-pointer");
                    }
                    return {Node::VarTypeCached{val.type, val.pointerDepth - 1}, ReadAbs, 0};
                }
                case Node::Type::MemberAccess:
                {
                    auto [t, type, offset] = self(node.children[0], self);
                    auto &name = std::get<std::string>(node.children[1].value);
                    if (t.pointerDepth)
                    {
                        log << Logger::Error << "Cannot access member ptr\n";
                        throw std::runtime_error("Cannot access member ptr");
                    }
                    auto &mem = getMember(name, t.index, *cache);
                    return {{mem.type, mem.ptrDepth}, type, offset + mem.offset};
                }
                default:
                {
                    auto val = compileNode(module, cache, &node, fRetT, constPool, bc, scopes, log);
                    bc.push_back({bc::ReadMember});
                    pushToBc<uint64_t>(bc, getSizeOfType(val.type, val.pointerDepth, cache->structs));
                    return {{val.type, val.pointerDepth}, ReadTmp, 0};
                }
                }
            };
            auto [t, type, offset] = walkTree(*node, walkTree);
            switch (type)
            {
            case ReadAbs:
                bc.push_back({bc::Push64});
                pushToBc<uint64_t>(bc, offset);
                bc.push_back({bc::AddI});
                bc.push_back({bc::ReadAbs});
                break;
            case ReadRel:
                bc.push_back({bc::ReadRel});
                pushToBc<uint64_t>(bc, offset);
                break;
            case ReadTmp:
                pushToBc<uint64_t>(bc, offset);
            }
            pushToBc<uint64_t>(bc, getSizeOfType(t.index, t.pointerDepth, cache->structs));
            return {t.index, t.pointerDepth};
            // auto &root = node->children[0];
            // int64_t offset = 0;
            // auto rootRes = compileNode(module, cache, &root, fRetT, constPool, bc, scopes, log);
            // auto rootRes2 = rootRes;
            // for (uint i = 1; i < node->children.size(); ++i)
            // {
            //     if (rootRes.pointerDepth)
            //     {
            //         log << "Cannot use operator . on a pointer\n";
            //         throw std::runtime_error("Cannot use operator . on a pointer");
            //     }
            //     auto &child = node->children[i];
            //     if (child.type != Node::Type::Variable)
            //     {
            //         throw std::runtime_error("Invalid node");
            //     }
            //     auto &name = std::get<Identifier>(child.value).name;
            //     auto &type = cache->structs[rootRes.type - numReservedIndices];
            //     auto &sym = std::get<Linker::Module::Symbol::Struct>(type.symbol->data);
            //     auto field = std::find_if(sym.fields.begin(), sym.fields.end(), [&name](const Linker::Module::Symbol::Struct::Field &f)
            //                               { return f.name == name; });
            //     if (field == sym.fields.end())
            //     {
            //         log << Logger::Error << "Field " << name << " not found in struct "
            //             << "\n";
            //         throw std::runtime_error("Field not found");
            //     }
            //     rootRes = {std::get<1>(field->type), field->pointerDepth};
            //     if (i < node->children.size() - 1)
            //         offset += getSizeOfType(std::get<1>(field->type), field->pointerDepth, cache->structs);
            // }
            // auto totalSize = getSizeOfType(rootRes2.type, rootRes2.pointerDepth, cache->structs);
            // auto resultSize = getSizeOfType(rootRes.type, rootRes.pointerDepth, cache->structs);
            // bc.push_back({bc::ReadMember});
            // pushToBc<uint64_t>(bc, totalSize);
            // pushToBc<uint64_t>(bc, offset);
            // pushToBc<uint64_t>(bc, resultSize);
            // return rootRes;
            // // TODO: reduce instruction count
        }
        case Node::Type::Break:
        {
            bc.push_back({bc::Break});
            pushToBc<uint64_t>(bc, 0);
        }
        case Node::Type::Continue:
        {
            bc.push_back({bc::Continue});
            pushToBc<uint64_t>(bc, 0);
        }
        }

        log << Logger::Error << "Unhandled node type " << static_cast<int>(node->type) << "\n";
        throw std::runtime_error("Unhandled node type");
    }
}