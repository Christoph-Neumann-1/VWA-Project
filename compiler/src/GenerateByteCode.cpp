#include <GenerateByteCode.hpp>

// FIXME: if any code for casting is inserted, I NEED to make sure there are no problems with reads relative to the instruction ptr. I'll need that should I ever implement statement expressions.
//  TODO: assert, at some point that primitive types are not overriden
//  TODO: output more useful format and use it for optimization
// FIXME: i still need to build the cache and make sure I insert the correct module name everywhere
namespace vwa
{

    using CachedType = Linker::Cache::CachedType;
    using Cache = Linker::Cache;

    void GenModBc(Linker::Module &mod, Linker &linker, Logger &log)
    {
        log << Logger::Info << "Generating bytecode\n";
        // This is used to represent the constant pool. Its relative adress is always known, since it's at the beginning of the module.
        // The pool grows in the opposite direction of the bytecode so as to not introduce additional complexity.
        // It is not merged with bytecode because that would lead to loads of copying data around. Putting the data in the middle of the code is possible, but not easy to work with,
        // this method also makes reuse possible by searching for overlapping data.
        // Data must be pushed in reverse order
        std::vector<uint8_t> constants{};
        std::vector<bc::BcToken> bc;
        for (auto &func : mod.exports)
            if (std::holds_alternative<Linker::Symbol::Function>(func.data))
                compileFunc(mod, linker, func, constants, bc, log);

        auto bcSize = bc.size();
        bc.resize(bcSize + constants.size());
        if (!bcSize)
            log << Logger::Warning << "No code in module\n";
        else
        {
            // FIXME: offset all later function references
            for (int64_t i = bcSize - 1; i >= 0 && constants.size(); --i)
            {
                bc[i + constants.size()] = bc[i];
            }
            {
                int i = constants.size() - 1, j = 0;
                while (i + 1 > 0)
                {
                    bc[j++] = {constants[i--]};
                }
            }
            // TODO: this should really happen somewhere else, like when patching internal function calls
            // Better idea, just ignore it and add the offset to everything
            // for (auto &s : mod.exports)
            //     if (auto f = std::get_if<0>(&s.data))
            //         f->idx += constants.size();
        }
        mod.data = std::move(bc);
        mod.offset = constants.size();
    }

    // This function inserts the token at the specified position and offsets all following instructions.
    // The instruction inserted is not offset. this assumes all following instructions are complete, or at the very least have enough space allocated
    // TODO: make sure this does not cause problems with jumps before this(example an if which jumps past the body if the condition is false)
    // FIXME Did I delete this on accident?
    void bcInsert(std::vector<bc::BcToken> &bc, const size_t pos)
    {
        bc.insert(bc.begin() + pos, bc::BcToken{});
        for (size_t i = pos + 1; i < bc.size(); i += getInstructionSize(bc[i].instruction))
        {
            using namespace bc;
            switch (bc[i].instruction)
            {
            case JumpRel:
            case JumpRelIfFalse:
            case JumpRelIfTrue:
            case JumpFuncRel:
                ++*reinterpret_cast<int64_t *>(&bc[i + 1]);
                continue;
            default:
                continue;
            }
        }
    }

    // auto &getMember(const std::string &name, const size_t type, const Cache &cache)
    // {
    //     if (type < Linker::Cache::reservedIndices)
    //         throw std::runtime_error("Attempted member access on primitive type");
    //     auto &typeInfo = cache.structs[type - Linker::Cache::reservedIndices].symbol->data;
    //     if (auto val = std::get_if<1>(&typeInfo))
    //     {
    //         if (auto it = std::find_if(val->fields.begin(), val->fields.end(), [&name](const auto &field)
    //                                    { return field.name == name; });
    //             it != val->fields.end())
    //         {
    //             auto idx = std::distance(val->fields.begin(), it);
    //             return cache.structs[type - Linker::Cache::reservedIndices].members[idx];
    //         }
    //         else
    //         {
    //             throw std::runtime_error("Field " + name + " not found in struct " + cache.structs[type - Linker::Cache::reservedIndices].symbol->name.name);
    //         }
    //     }
    //     else
    //     {
    //         throw std::runtime_error("Attempted member access on non-struct type");
    //     }
    // }

    void discard(const CachedType result, std::vector<bc::BcToken> &bc, const Cache &cache)
    {
        uint64_t size = cache.getSizeOfType(result);
        if (!size)
            return;
        bc.push_back({bc::Pop});
        pushToBc(bc, size);
    }

    // The exact pointer depth is irrelevant
    [[nodiscard]] std::optional<bc::BcInstruction> typeCast(const CachedType resultT, const CachedType argT, Logger &log)
    {
        auto [rT, rP, rI] = Cache::disassemble(resultT);
        auto [aT, aP, aI] = Cache::disassemble(argT);
        if (rT || aT)
        {
            log << Logger::Error << "Failed to cast types: Is a function\n";
            throw std::runtime_error("Failed to cast types: Is a function");
        }
        // FIXME: implement pointers
        if (resultT == argT || (rP && aP))
            return std::nullopt;
        if ((!rP && rI == Cache::FPtr) || (!aP && aI == Cache::FPtr))
        {
            log << Logger::Error << "Cannot cast function pointers, it has no defined representation";
            throw std::runtime_error("Cannot cast function pointer");
        }
        if (rP)
        {
            if (aI == Cache::I64)
                return std::nullopt;
            log << Logger::Error << "Pointer cast to non-pointer type\n";
            throw std::runtime_error("Pointer cast to non-pointer type");
        }
        if (aP)
        {
            if (rI == Cache::I64)
                return std::nullopt;
            log << Logger::Error << "Non-pointer cast to pointer type\n";
            throw std::runtime_error("Non-pointer cast to pointer type");
        }
        if (rI == Cache::Void || aI == Cache::Void)
        {
            log << Logger::Error << "Invalid cast casting from/to void is invalid\n";
            throw std::runtime_error("Cannot cast void");
        }

        switch (aI)
        {
        case Cache::F64:
            switch (rI)
            {
            case Cache::I64:
                return bc::FtoI;
            case Cache::U8:
                return bc::FtoC;
            default:
                log << Logger::Error << "Casting is only possible for builtin types\n";
                throw std::runtime_error("Casting is only possible for builtin types");
            }
            return std::nullopt;
        case Cache::I64:
            switch (rI)
            {
            case Cache::F64:
                return bc::ItoF;
            case Cache::U8:
                return bc::ItoC;
            default:
                log << Logger::Error << "Casting is only possible for builtin types\n";
                throw std::runtime_error("Casting is only possible for builtin types");
            }
            return std::nullopt;
        case Cache::U8:
            switch (rI)
            {
                // TODO: implement this
            case Cache::F64:
                return bc::CtoF;
            case Cache::I64:
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
    void compileFunc(Linker::Module &module, Linker &linker, Linker::Symbol &func, std::vector<uint8_t> &constPool, std::vector<bc::BcToken> &bc, Logger &log)
    {
        auto &cache = linker.cache;
        auto &f = std::get<Linker::Symbol::Function>(func.data);
        auto cached = cache.ids.find(func.name)->second;
        if (!~cached)
        {
            log << Logger::Error << "Failed to find function " << func.name.name << '\n';
            throw std::runtime_error("Failed to find function");
        }
        auto &cachedF = cache.functions[Cache::getIndex(cached)];
        auto [_, rP, rI] = Cache::disassemble(cachedF.returnType);
        log << Logger::Info << "Compiling function " << func.name.name << '\n';
        bool main = func.name.name == "main";
        if (main)
        {
            if (rP || rI != Cache::I64)
            {
                log << Logger::Error << "Main function must return int\n";
                throw std::runtime_error("Main function must return int");
            }
            if (cachedF.params.size() != 2 || cachedF.params[0] != Cache::I64 || cachedF.params[1] != (Cache::U8 | (2ul << 32)))
            {
                log << Logger::Error << "Main function must have 2 parameters int and string*\n";
                throw std::runtime_error("Main function must have 2 parameters int and string*");
            }
            log << Logger::Info << "Found main function\n";
        }
        // auto &funcData = cache->functions[cached->second.first];
        std::vector<Scope> scopes;
        Scope scope{0, 16};
        if (f.params.size() != cachedF.params.size())
        {
            log << Logger::Error << "Function invalid parameter count missmatch between cache and original parameters";
            throw std::runtime_error("Function has wrong number of parameters");
        }
        for (size_t i = 0; i < f.params.size(); i++)
        {
            scope.variables.insert({f.params[i].name, {cachedF.params[i], scope.size + scope.offset}});
            scope.size += cache.getSizeOfType(cachedF.params[i]);
        }
        scopes.push_back(scope);
        f.idx = bc.size();
        // FIXME: consider all outcomescache having a value at the end of a void function and such
        auto res = compileNode(module, linker, *static_cast<Node *>(f.node), cachedF.returnType, constPool, bc, scopes, log);
        delete static_cast<Node *>(f.node);
        // auto [__, resP, resT] = Cache::disassemble(res);
        // It doesn't really matter if this is unnecessary, there is only one main function so the impact is negligible
        if (main && !res)
        {
            bc.push_back({bc::Push64});
            pushToBc<int64_t>(bc, 0);
            res = Cache::I64;
        }
        // FIXME: by popping the stack, the return value is discarded, it needs to be moved somewhere else
        if (res)
        {
            if (!cachedF.returnType)
                discard(res, bc, cache);
            else
            {
                log << Logger::Info << "Function body evaluates to value, adding implicit return\n";
                if (auto instr = typeCast(cachedF.returnType, res, log); instr)
                    bc.push_back({*instr});
                res = cachedF.returnType;
            }
        }
        // else if (funcData.returnType.type || funcData.returnType.pointerDepth)
        {
            // I should propably fix this at some point
            // log << "Function requires a return value but none was provided\n";
            // throw std::runtime_error("Function requires a return value but none was provided");
        }
        bc.push_back(bc::BcToken{bc::Return});
        pushToBc(bc, uint64_t{cache.getSizeOfType(res)});
        f.type = Linker::Symbol::Function::Compiled;
    }

    // First pos is needed if the first args is to be cast
    // TODO: implement this withou shifting stuff around
    // TODo: update
    CachedType promoteType(const CachedType a, const CachedType b, std::vector<bc::BcToken> &bc, size_t firstPos, Logger &log)
    {
        if (a == b)
            return a;
        auto [_a, aP, aI] = Cache::disassemble(a);
        auto [_b, bP, bI] = Cache::disassemble(b);
        if (aP || bP)
            throw std::runtime_error("Pointers not implemented");
        if (aI == Cache::F64 || bI == Cache::F64)
        {
            if (auto instr = typeCast(Cache::F64, a, log); instr)
                bc.insert(bc.begin() + firstPos, {*instr});
            // typeCast(PrimitiveTypes::F64, false, otherType, false, log);
            else if (auto instr = typeCast(Cache::F64, b, log); instr)
                bc.push_back({*instr});
            // typeCast(PrimitiveTypes::F64, false, type, false, log);
            return Cache::F64;
        }
        if (aI == Cache::I64 || bI == Cache::I64)
        {
            // typeCast(PrimitiveTypes::I64, false, type, false, log);
            // typeCast(PrimitiveTypes::I64, false, otherType, false, log);

            if (auto instr = typeCast(Cache::I64, a, log); instr)
                bc.insert(bc.begin() + firstPos, {*instr});
            else if (auto instr = typeCast(Cache::I64, b, log); instr)
                bc.push_back({*instr});

            return Cache::I64;
        }
        // Is there any point in doing this?
        if (aI == Cache::U8 || bI == Cache::U8)
        {
            // typeCast(PrimitiveTypes::U8, false, type, false, log);
            // typeCast(PrimitiveTypes::U8, false, otherType, false, log);
            if (auto instr = typeCast(Cache::U8, a, log); instr)
                bc.insert(bc.begin() + firstPos, {*instr});
            else if (auto instr = typeCast(Cache::U8, b, log); instr)
                bc.push_back({*instr});
            // FIXME: make this promote everything to int, or remove it all together
            return Cache::U8;
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
                auto [_, res] = scope.variables.insert({std::get<std::string>(child.children[0].value), {std::get<CachedType>(child.children[1].value), scope.size + scope.offset}});
                if (!res)
                {
                    log << Logger::Error << "Variable " << std::get<std::string>(child.children[0].value) << " already declared\n";
                    throw std::runtime_error("Variable already declared");
                }
                scope.size += cache.getSizeOfType(std::get<CachedType>(child.children[1].value));
            }
        return scope;
    }

    // TODO: member access
    CachedType compileNode(Linker::Module &module, Linker &linker, const Node &node, const CachedType fRetT, std::vector<uint8_t> &constPool, std::vector<bc::BcToken> &bc, std::vector<Scope> &scopes, Logger &log)
    {
        auto &cache = linker.cache;
        // TODO: consider another pass to generate scopes beforehand
        switch (node.type)
        {
        case Node::Type::Unassigned:
            log << Logger::Error << "Unassigned node";
            throw std::runtime_error("Unassigned node");
        case Node::Type::Return:
            if (node.children.size())
            {
                auto res = compileNode(module, linker, node.children.back(), fRetT, constPool, bc, scopes, log);
                if (auto instr = typeCast(fRetT, res, log); instr)
                    bc.push_back({*instr});
            }
            bc.push_back({bc::Return});
            pushToBc<uint64_t>(bc, cache.getSizeOfType(fRetT));
            return Cache::Void;
        case Node::Type::LiteralI:
            bc.push_back({bc::Push64});
            pushToBc(bc, std::get<int64_t>(node.value));
            return Cache::I64;
        case Node::Type::LiteralF:
            bc.push_back({bc::Push64});
            pushToBc(bc, std::get<double>(node.value));
            return Cache::F64;
        case Node::Type::Block:
        {

            if (!node.children.size())
                return Cache::Void;
            auto scope = generateBlockScope(node, cache, log, scopes.back().size + scopes.back().offset);
            scopes.push_back(std::move(scope));
            if (scope.size)
            {
                bc.push_back({bc::Push});
                pushToBc(bc, uint64_t{scope.size});
            }
            // I do not know why, but this entered a infinite loop, but it works now so I won't touch it
            // for (auto &child : node.children)
            for (size_t i = 0; i < node.children.size() - 1; ++i)
                // Discard means that the value is not used, it is just dropped from the stack
                // TODO: consider not discarding last value
                discard(compileNode(module, linker, node.children[i], fRetT, constPool, bc, scopes, log), bc, cache);
            auto res = compileNode(module, linker, node.children.back(), fRetT, constPool, bc, scopes, log);
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
            auto lhs = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            auto rhs = compileNode(module, linker, node.children[1], fRetT, constPool, bc, scopes, log);
            auto [_a, aP, aI] = Cache::disassemble(lhs);
            auto [_b, bP, bI] = Cache::disassemble(rhs);
            CachedType ret;
            if (!aP && !bP)
                ret = promoteType(lhs, rhs, bc, pos, log);
            else if (aP)
                if (bP)
                    throw std::runtime_error("Cannot add pointers");
                else if (bI == Cache::I64)
                {
                    bc.push_back({bc::AddI});
                    return lhs;
                }
                else
                    throw std::runtime_error("Cannot add pointers");
            else if (bP)
                // Idk what I am doing
                if (aI == Cache::I64)
                {
                    // TODO this is a horrible solution, I really need to find out if this is a safe thing to do
                    bc.push_back({bc::AddI});
                    return rhs;
                }
                else
                    throw std::runtime_error("Cannot add pointers");
            else
            {
                throw 0; // This is unreachable, but clang kept complaining about it
            }
            // I know ret is not a pointer, so I don't need any bitwise logic
            switch (ret)
            {
            case Cache::I64:
                bc.push_back({bc::AddI});
                break;
            case Cache::F64:
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
            auto lhs = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            auto rhs = compileNode(module, linker, node.children[1], fRetT, constPool, bc, scopes, log);
            auto ret = promoteType(lhs, rhs, bc, pos, log);
            // TODO: pointers
            switch (ret)
            {
            case Cache::I64:
                bc.push_back({bc::SubI});
                break;
            case Cache::F64:
                bc.push_back({bc::SubF});
                break;
            default:
                log << Logger::Error << "Cannot subtract types\n";
                throw std::runtime_error("Cannot subtract types");
            }
            return ret;
        }
        // TODO: builtin type for this and other common ops like increment(provide in-place operations)
        case Node::Type::UnaryMinus:
        {
            // TODO: promote chars to int for this and similar ops
            auto res = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            if (res & Cache::pointerDepthMask)
                throw std::runtime_error("Pointer cannot be negated");
            switch (res)
            {
            case Cache::I64:
                bc.push_back({bc::Push64});
                pushToBc(bc, int64_t{-1});
                bc.push_back({bc::MulI});
                break;
            case Cache::F64:
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
            auto lhs = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            auto rhs = compileNode(module, linker, node.children[1], fRetT, constPool, bc, scopes, log);
            auto ret = promoteType(lhs, rhs, bc, pos, log);
            switch (ret)
            {
            case Cache::I64:
                bc.push_back({bc::MulI});
                break;
            case Cache::F64:
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
            auto lhs = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            auto rhs = compileNode(module, linker, node.children[1], fRetT, constPool, bc, scopes, log);
            auto ret = promoteType(lhs, rhs, bc, pos, log);
            switch (ret)
            {
            case Cache::I64:
                bc.push_back({bc::DivI});
                break;
            case Cache::F64:
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
            auto lhs = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            if (auto instr = typeCast(Cache::I64, lhs, log); instr)
                bc.push_back({*instr});
            auto rhs = compileNode(module, linker, node.children[1], fRetT, constPool, bc, scopes, log);
            if (auto instr = typeCast(Cache::I64, rhs, log); instr)
                bc.push_back({*instr});
            bc.push_back({bc::ModI});
            return Cache::I64;
        }
        case Node::Type::Power:
        {
            auto lhs = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            auto rhs = compileNode(module, linker, node.children[1], fRetT, constPool, bc, scopes, log);
            auto ret = promoteType(lhs, rhs, bc, pos, log);
            switch (ret)
            {
            case Cache::I64:
                bc.push_back({bc::PowerI});
                break;
            case Cache::F64:
                bc.push_back({bc::PowerF});
                break;
            default:
                log << Logger::Error << "Cannot compute exponent\n";
                throw std::runtime_error("Cannot compute exponent");
            }
            return ret;
        }
        case Node::Type::DeclareVar:
        {
            if (node.children.size() == 4)
            {
                auto &name = std::get<std::string>(node.children[0].value);
                auto it = scopes.back().variables.find(name);
                auto init = compileNode(module, linker, node.children[3], fRetT, constPool, bc, scopes, log);
                if (auto instr = typeCast(it->second.type, init, log); instr)
                    bc.push_back({*instr});
                bc.push_back({bc::WriteRel});
                pushToBc<intptr_t>(bc, it->second.offset);
                pushToBc<uint64_t>(bc, cache.getSizeOfType(init));
            }
            return Cache::Void;
        }
        case Node::Type::Assign:
        {
            auto expr = compileNode(module, linker, node.children[1], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            auto &what = node.children[0];
            CachedType rt;
            if (what.type == Node::Type::Dereference)
            {
                rt = compileNode(module, linker, what.children[0], fRetT, constPool, bc, scopes, log);
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
                        rt = it2->second.type;
                        pushToBc<intptr_t>(bc, it2->second.offset);
                    }
                }
            }
            // TODO: convert members to indices early on
            // TODO: can I also handle other variables this way?
            else if (what.type == Node::Type::MemberAccess)
            {
                auto evalTree = [&](const Node &node, auto &self) -> std::tuple<CachedType, size_t, bool>
                {
                    switch (node.type)
                    {
                    case Node::Type::Variable:
                    {
                        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
                        {
                            auto it2 = it->variables.find(std::get<Identifier>(node.value).name);
                            if (it2 != it->variables.end())
                                return {it2->second.type, it2->second.offset, false};
                        }
                        log << Logger::Error << "Cannot access member\n";
                        throw std::runtime_error("Cannot find variable");
                    }
                    case Node::Type::MemberAccess:
                    {
                        auto prev = self(node.children[0], self);
                        auto [_t, ptr, idx] = Cache::disassemble(std::get<0>(prev));
                        if (ptr)
                        {
                            log << Logger::Error << "Cannot access member of ptr\n";
                            throw std::runtime_error("Cannot access member of ptr");
                        }
                        auto mem = cache.getMemberByName(std::get<0>(prev), std::get<std::string>(node.children[1].value));
                        if (!~mem)
                        {
                            log << Logger::Error << "Cannot access member\n";
                            throw std::runtime_error("Cannot access member");
                        }
                        auto &m = cache.structs[idx - Cache::reservedIndicies].members[mem];
                        return {m.type, m.offset + std::get<1>(prev), std::get<2>(prev)};
                    }
                    case Node::Type::Dereference:
                    {
                        auto res = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
                        if (!(res & Cache::pointerDepthMask))
                        {
                            log << Logger::Error << "Cannot dereference non-pointer\n";
                            throw std::runtime_error("Cannot dereference non-pointer");
                        }
                        // This can probably be done more efficiently by messing around with the bits
                        return {Cache::setPointerDepth(res, Cache::getPointerDepth(res) - 1), 0, true};
                    }
                    default:
                        log << Logger::Error << "Cannot access member of temporary\n";
                        throw std::runtime_error("Cannot access member of temporary");
                    }
                };
                auto res = evalTree(what, evalTree);
                if (std::get<2>(res))
                {
                    bc.push_back({bc::Push64});
                    // This is probably the wrong sign, but since integers are twos complement this should be fine
                    pushToBc<uint64_t>(bc, std::get<1>(res));
                    bc.push_back({bc::AddI});
                    bc.push_back({bc::WriteAbs});
                }
                else
                {
                    bc.push_back({bc::WriteRel});
                    pushToBc<intptr_t>(bc, std::get<1>(res));
                }
                rt = std::get<0>(res);
            }
            else
            {
                log << Logger::Error << "Cannot assign to non-variable yet\n";
                throw std::runtime_error("Cannot assign to non-variable yet");
            }
            if (!cache.isTypeValid(rt))
                throw std::runtime_error("Assignment failed");
            if (auto instr = typeCast(rt, expr, log); instr)
                bc.insert(bc.begin() + pos - 1, {*instr});
            pushToBc<uint64_t>(bc, cache.getSizeOfType(rt));
            return Cache::Void;
        }
        case Node::Type::Variable:
        {
            auto &name = std::get<Identifier>(node.value).name;
            for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
            {
                auto it2 = it->variables.find(name);
                if (it2 != it->variables.end())
                {
                    bc.push_back({bc::ReadRel});
                    pushToBc<intptr_t>(bc, it2->second.offset);
                    pushToBc<uint64_t>(bc, cache.getSizeOfType(it2->second.type));
                    return it2->second.type;
                }
            }
            log << Logger::Error << "Variable " << name << " not found\n";
            throw std::runtime_error("Variable not found");
        }
        case Node::Type::If:
        {
            auto cond = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            if (auto instr = typeCast(Cache::I64, cond, log); instr)
                bc.push_back({*instr});
            bc.push_back({bc::JumpRelIfFalse});
            auto pos = bc.size();
            pushToBc<int64_t>(bc, 0);
            // TODO: consider evaluating to value
            discard(compileNode(module, linker, node.children[1], fRetT, constPool, bc, scopes, log), bc, cache);
            if (node.children.size() == 3)
            {
                bc.push_back({bc::JumpRel});
                auto pos2 = bc.size();
                pushToBc<int64_t>(bc, 0);
                *reinterpret_cast<int64_t *>(&bc[pos]) = bc.size() - pos + 1;
                discard(compileNode(module, linker, node.children[2], fRetT, constPool, bc, scopes, log), bc, cache);
                *reinterpret_cast<int64_t *>(&bc[pos2]) = bc.size() - pos2 + 1;
            }
            else
                *reinterpret_cast<int64_t *>(&bc[pos]) = bc.size() - pos + 1;
            return Cache::Void;
        }
        case Node::Type::While:
        {
            auto pos = bc.size();
            auto cond = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            if (auto instr = typeCast(Cache::I64, cond, log); instr)
                bc.push_back({*instr});
            bc.push_back({bc::JumpRelIfFalse});
            auto pos2 = bc.size();
            pushToBc<int64_t>(bc, 0);
            discard(compileNode(module, linker, node.children[1], fRetT, constPool, bc, scopes, log), bc, cache);
            bc.push_back({bc::JumpRel});
            pushToBc<int64_t>(bc, pos - bc.size() + 1);
            *reinterpret_cast<int64_t *>(&bc[pos2]) = bc.size() - pos2 + 1;
            // TODO: replace continue and break with correct instruction. This requires having a function which allows skipping instructions.
            return Cache::Void;
        }
        case Node::Type::Dereference:
        {
            auto expr = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            // TODO: cast int to ptr
            // FIXME: overflow
            bc.push_back({bc::ReadAbs});
            auto newT = Cache::setPointerDepth(expr, Cache::getPointerDepth(expr - 1));
            pushToBc<uint64_t>(bc, cache.getSizeOfType(newT));
            return newT;
        }
        // Only works on local variables
        case Node::Type::AddressOf:
        {
            auto evalTree = [&](const Node &node, auto &self) -> std::tuple<CachedType, size_t, bool>
            {
                switch (node.type)
                {
                case Node::Type::Variable:
                {
                    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
                    {
                        auto it2 = it->variables.find(std::get<Identifier>(node.value).name);
                        if (it2 != it->variables.end())
                            return {it2->second.type, it2->second.offset, false};
                    }
                    log << Logger::Error << "Cannot access member\n";
                    throw std::runtime_error("Cannot find variable");
                }
                case Node::Type::MemberAccess:
                {
                    auto prev = self(node.children[0], self);
                    if (std::get<0>(prev) & Cache::pointerDepthMask)
                    {
                        log << Logger::Error << "Cannot access member of ptr\n";
                        throw std::runtime_error("Cannot access member of ptr");
                    }
                    auto mem = cache.getMemberByName(std::get<0>(prev), std::get<std::string>(node.children[1].value));
                    auto &m = cache.structs[(std::get<0>(prev) & Cache::indexMask) - Cache::reservedIndicies].members[mem];
                    return {m.type, m.offset + std::get<1>(prev), std::get<2>(prev)};
                }
                case Node::Type::Dereference:
                {
                    auto res = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
                    if (!(res & Cache::pointerDepthMask))
                    {
                        log << Logger::Error << "Cannot dereference non-pointer\n";
                        throw std::runtime_error("Cannot dereference non-pointer");
                    }
                    return {Cache::setPointerDepth(res, Cache::getPointerDepth(res) - 1), 0, true};
                }
                default:
                    log << Logger::Error << "Cannot access member of temporary\n";
                    throw std::runtime_error("Cannot access member of temporary");
                }
            };
            auto res = evalTree(node.children[0], evalTree);
            if (std::get<2>(res))
            {
                bc.push_back({bc::Push64});
                // This is probably the wrong sign, but since integers are twos complement this should be fine
                pushToBc<uint64_t>(bc, std::get<1>(res));
                bc.push_back({bc::AddI});
            }
            else
            {
                bc.push_back({bc::AbsOf});
                pushToBc<intptr_t>(bc, std::get<1>(res));
            }
            return Cache::setPointerDepth(std::get<0>(res), Cache::getPointerDepth(std::get<0>(res)) + 1);
            // TODO: support structs
            // auto &name = std::get<Identifier>(node.children[0].value).name;
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
            auto &name = std::get<Identifier>(node.children[0].value);
            auto it = cache.ids.find(name);
            // FIXME: this should be done beforehand
            it = it == cache.ids.end() ? cache.ids.find({name.name, module.name}) : it;
            if (it == cache.ids.end())
            {
                log << Logger::Error << "Function " << name.name << " not found\n";
                throw std::runtime_error("Function not found");
            }
            if (!(it->second & Cache::typeMask))
            {
                log << Logger::Error << "Symbol " << name.name << " is a struct, not a function\n";
                throw std::runtime_error("Symbol is a struct, not a function");
            }
            auto &func = cache.functions[it->second & Cache::indexMask];
            auto nArgs = node.children.size() - 1;
            if (nArgs != func.params.size())
            {
                log << Logger::Error << "Function " << name.name << " expects " << func.params.size() << " arguments, but got " << nArgs << "\n";
                throw std::runtime_error("Function expects different number of arguments");
            }
            size_t argSize = 0;
            for (size_t i = 0; i < nArgs; i++)
            {
                argSize += cache.getSizeOfType(func.params[i]);
                auto arg = compileNode(module, linker, node.children[i + 1], fRetT, constPool, bc, scopes, log);
                if (auto instr = typeCast(func.params[i], arg, log); instr)
                    bc.push_back({*instr});
            }
            // There are two reasons for this:
            // First, it might speed up the linker by having to do less lookups, but I am not sure about that
            // Two, it means I can push back development of the linker
            // FIXME
            // if (func.finished && func.internal)
            // {
            //     bc.push_back({bc::JumpFuncRel});
            //     pushToBc<int64_t>(bc, func.address - bc.size() + 1);
            // }
            // This is not supposed to be done here
            // else
            // if (auto f = std::get<Linker::Symbol::Function>(func.symbol->data); f.type == Linker::Symbol::Function::Type::External)
            // {
            //     bc.push_back({bc::JumpFFI});
            //     pushToBc<Linker::FFIFunc>(bc, f.ffi);
            // }
            // else
            {
                bc.push_back({bc::CallFunc});
                // TODO: make sure this gets replaced by a permanent index
                pushToBc<uint64_t>(bc, it->second & Cache::indexMask); // Gets replaced by adress or more permanent index later}
            }
            pushToBc<uint64_t>(bc, argSize);
            return func.returnType;
        }

        // TODO: implement something like decltype to reduce code length
        case Node::Type::SizeOf:
        {
            auto t = std::get<CachedType>(node.value);
            auto size = cache.getSizeOfType(t);
            bc.push_back({bc::Push64});
            pushToBc<int64_t>(bc, size);
            return Cache::I64;
        }
        case Node::Type::MemberAccess:
        {
            enum Type
            {
                ReadAbs, // Root is a pointer
                ReadRel, // Root is a local variable
                ReadTmp, // Root is a temporary value
            };

            auto walkTree = [&](const Node &node, auto &self) -> std::tuple<CachedType, Type, size_t>
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
                                return {it2->second.type, ReadRel, it2->second.offset};
                        }
                        log << Logger::Error << "Cannot access non-variable yet\n";
                        throw std::runtime_error("Did not find variable");
                    }
                case Node::Type::Dereference:
                {
                    auto val = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
                    if (!(val & Cache::pointerDepthMask))
                    {
                        log << Logger::Error << "Cannot dereference non-pointer\n";
                        throw std::runtime_error("Cannot dereference non-pointer");
                    }
                    return {Cache::setPointerDepth(val, Cache::getPointerDepth(val) - 1), ReadAbs, 0};
                }
                case Node::Type::MemberAccess:
                {
                    auto [t, type, offset] = self(node.children[0], self);
                    auto &name = std::get<std::string>(node.children[1].value);
                    if (t & Cache::pointerDepthMask)
                    {
                        log << Logger::Error << "Cannot access member ptr\n";
                        throw std::runtime_error("Cannot access member ptr");
                    }
                    auto mem = cache.getMemberByName(t, name);
                    auto &m = cache.structs[(t & Cache::indexMask) - Cache::reservedIndicies].members[mem];
                    return {m.type, type, offset + m.offset};
                }
                default:
                {
                    auto val = compileNode(module, linker, node, fRetT, constPool, bc, scopes, log);
                    bc.push_back({bc::ReadMember});
                    pushToBc<uint64_t>(bc, cache.getSizeOfType(val));
                    return {val, ReadTmp, 0};
                }
                }
            };
            auto [t, type, offset] = walkTree(node, walkTree);
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
            pushToBc<uint64_t>(bc, cache.getSizeOfType(t));
            return t;
            // auto &root = node.children[0];
            // int64_t offset = 0;
            // auto rootRes = compileNode(module, cache, &root, fRetT, constPool, bc, scopes, log);
            // auto rootRes2 = rootRes;
            // for (uint i = 1; i < node.children.size(); ++i)
            // {
            //     if (rootRes.pointerDepth)
            //     {
            //         log << "Cannot use operator . on a pointer\n";
            //         throw std::runtime_error("Cannot use operator . on a pointer");
            //     }
            //     auto &child = node.children[i];
            //     if (child.type != Node::Type::Variable)
            //     {
            //         throw std::runtime_error("Invalid node");
            //     }
            //     auto &name = std::get<Identifier>(child.value).name;
            //     auto &type = cache->structs[rootRes.type - Linker::Cache::reservedIndices];
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
            //     if (i < node.children.size() - 1)
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
            return Cache::Void;
        }
        case Node::Type::Continue:
        {
            bc.push_back({bc::Continue});
            pushToBc<uint64_t>(bc, 0);
            return Cache::Void;
        }
        case Node::Type::For:
        {
            // A for statement starts by creating a scope for the loop variable.
            Scope firstScope{0, scopes.back().size + scopes.back().offset};
            // TODO: this is an ugly hack
            firstScope.variables.insert({std::get<std::string>(node.children[0].children[0].value), {std::get<CachedType>(node.children[0].children[1].value), firstScope.offset}});
            firstScope.size += cache.getSizeOfType(std::get<CachedType>(node.children[0].children[1].value));
            scopes.push_back(std::move(firstScope));
            if (firstScope.size)
            {
                bc.push_back({bc::Push});
                pushToBc<uint64_t>(bc, firstScope.size);
            }

            // Then the initializer is compiled.
            discard(compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log), bc, cache);
            // Following that the condition is compiled and it's address is stored for jumping back later.
            auto condAddr = bc.size();
            auto cond = compileNode(module, linker, node.children[1], fRetT, constPool, bc, scopes, log);
            if (auto instr = typeCast(Cache::I64, cond, log))
                bc.push_back({*instr});
            bc.push_back({bc::JumpRelIfFalse});
            auto jumpAddr = bc.size();
            pushToBc<int64_t>(bc, 0); // This is just a placeholder
            // Then the main body is compiled, this does not necessarily require a new scope, but better safe than sorry
            scopes.push_back({0, scopes.back().size + scopes.back().offset});
            compileNode(module, linker, node.children[3], fRetT, constPool, bc, scopes, log);
            scopes.pop_back();
            // Incrementing happens at the end, followed by an unconditional jump back.
            discard(compileNode(module, linker, node.children[2], fRetT, constPool, bc, scopes, log), bc, cache);
            bc.push_back({bc::JumpRel});
            pushToBc<int64_t>(bc, condAddr - bc.size() + 1);
            // Now that the end position is know, update the jump instruction.
            *reinterpret_cast<int64_t *>(&bc[jumpAddr]) = bc.size() - jumpAddr + 1;
            if (firstScope.size)
            {
                bc.push_back({bc::Pop});
                pushToBc<uint64_t>(bc, firstScope.size);
            }
            scopes.pop_back();
            return Cache::Void;
        }
        case Node::Type::LessThan:
        {
            auto lhs = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            auto rhs = compileNode(module, linker, node.children[1], fRetT, constPool, bc, scopes, log);
            auto t = promoteType(lhs, rhs, bc, pos, log);
            if (t & Cache::pointerDepthMask)
                bc.push_back({bc::LessThanI});
            else
                switch (t)
                {
                case Cache::I64:
                    bc.push_back({bc::LessThanI});
                    break;
                case Cache::F64:
                    bc.push_back({bc::LessThanF});
                    break;
                case Cache::U8:
                    throw std::runtime_error("This should not happen");
                default:
                    throw std::runtime_error("Can't compare these types");
                }
            return Cache::I64;
        }
        case Node::Type::GreaterThan:
        {
            auto lhs = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            auto rhs = compileNode(module, linker, node.children[1], fRetT, constPool, bc, scopes, log);
            auto t = promoteType(lhs, rhs, bc, pos, log);
            if (t & Cache::pointerDepthMask)
                bc.push_back({bc::LessThanI});
            else
                switch (t)
                {
                case Cache::I64:
                    bc.push_back({bc::GreaterThanI});
                    break;
                case Cache::F64:
                    bc.push_back({bc::GreaterThanF});
                    break;
                case Cache::U8:
                    throw std::runtime_error("This should not happen");
                default:
                    throw std::runtime_error("Can't compare these types");
                }
            return Cache::I64;
        }
        case Node::Type::GreaterThanEqual:
        {
            auto lhs = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            auto rhs = compileNode(module, linker, node.children[1], fRetT, constPool, bc, scopes, log);
            auto t = promoteType(lhs, rhs, bc, pos, log);
            if (t & Cache::pointerDepthMask)
                bc.push_back({bc::LessThanI});
            else
                switch (t)
                {
                case Cache::I64:
                    bc.push_back({bc::GreaterThanOrEqualI});
                    break;
                case Cache::F64:
                    bc.push_back({bc::GreaterThanOrEqualF});
                    break;
                case Cache::U8:
                    throw std::runtime_error("This should not happen");
                default:
                    throw std::runtime_error("Can't compare these types");
                }
            return Cache::I64;
        }
        case Node::Type::LessThanEqual:
        {
            auto lhs = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            auto rhs = compileNode(module, linker, node.children[1], fRetT, constPool, bc, scopes, log);
            auto t = promoteType(lhs, rhs, bc, pos, log);
            if (t & Cache::pointerDepthMask)
                bc.push_back({bc::LessThanI});
            else
                switch (t)
                {
                case Cache::I64:
                    bc.push_back({bc::LessThanOrEqualI});
                    break;
                case Cache::F64:
                    bc.push_back({bc::LessThanOrEqualF});
                    break;
                case Cache::U8:
                    throw std::runtime_error("This should not happen");
                default:
                    throw std::runtime_error("Can't compare these types");
                }
            return Cache::I64;
        }
        case Node::Type::Cast:
        {
            auto expr = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            auto type = std::get<CachedType>(node.children[1].value);
            if (auto instr = typeCast(type, expr, log))
                bc.push_back({*instr});
            return type;
        }
        case Node::Type::TypePun:
        {
            auto expr = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            auto resT = std::get<CachedType>(node.children[1].value);
            if (cache.getSizeOfType(expr) != cache.getSizeOfType(resT))
            {
                log << Logger::Error << "Failed to type pun, Types must be of the same size\n";
                throw std::runtime_error("Failed to type pun, Types must be of the same size");
            }
            return resT;
        }
        case Node::Type::LiteralS:
        {
            auto &str = std::get<std::string>(node.value);
            pushToConst<char>(constPool, 0);
            for (auto i = str.size() - 1; i + 1 >= 1; --i)
                pushToConst<char>(constPool, str[i]);
            bc.push_back({bc::AbsOfConst});
            pushToBc<uint64_t>(bc, -bc.size() - constPool.size() + 1);
            return Cache::constructType(Cache::SymbolType::Struct, Cache::U8, 1); // Should this be a char instead?
        }
        case Node::Type::And:
        {
            auto lhs = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            if (auto instr = typeCast(Cache::I64, lhs, log); instr)
                bc.insert(bc.begin() + pos - 1, {*instr});
            auto rhs = compileNode(module, linker, node.children[1], fRetT, constPool, bc, scopes, log);
            pos = bc.size();
            if (auto instr = typeCast(Cache::I64, rhs, log); instr)
                bc.insert(bc.begin() + pos - 1, {*instr});
            bc.push_back({bc::And});
            return {Cache::I64};
        }
        case Node::Type::Or:
        {
            auto lhs = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            if (auto instr = typeCast(Cache::I64, lhs, log); instr)
                bc.insert(bc.begin() + pos - 1, {*instr});
            auto rhs = compileNode(module, linker, node.children[1], fRetT, constPool, bc, scopes, log);
            pos = bc.size();
            if (auto instr = typeCast(Cache::I64, rhs, log); instr)
                bc.insert(bc.begin() + pos - 1, {*instr});
            bc.push_back({bc::Or});
            return {Cache::I64};
        }
        case Node::Type::Not:
        {
            auto lhs = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            if (auto instr = typeCast(Cache::I64, lhs, log); instr)
                bc.insert(bc.begin() + pos - 1, {*instr});
            bc.push_back({bc::Not});
            return {Cache::I64};
        }
        case Node::Type::Equal:
        {
            auto lhs = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            auto rhs = compileNode(module, linker, node.children[1], fRetT, constPool, bc, scopes, log);
            auto [_a, aP, aI] = Cache::disassemble(lhs);
            auto [_b, bP, bI] = Cache::disassemble(rhs);
            CachedType ret;
            if (aP)
                lhs = Cache::I64;
            if (bP)
                rhs = Cache::I64;

            ret = promoteType(lhs, rhs, bc, pos, log);
            // I know ret is not a pointer, so I don't need any bitwise logic
            switch (ret)
            {
            case Cache::I64:
                bc.push_back({bc::EqualI});
                break;
            case Cache::F64:
                bc.push_back({bc::EqualF});
                break;
            default:
                log << Logger::Error << "Cannot compare types\n";
                throw std::runtime_error("Cannot compare types");
            }
            return ret;
        }
        case Node::Type::NotEqual:
        {
            auto lhs = compileNode(module, linker, node.children[0], fRetT, constPool, bc, scopes, log);
            auto pos = bc.size();
            auto rhs = compileNode(module, linker, node.children[1], fRetT, constPool, bc, scopes, log);
            auto [_a, aP, aI] = Cache::disassemble(lhs);
            auto [_b, bP, bI] = Cache::disassemble(rhs);
            CachedType ret;
            if (aP)
                lhs = Cache::I64;
            if (bP)
                rhs = Cache::I64;

            ret = promoteType(lhs, rhs, bc, pos, log);
            // I know ret is not a pointer, so I don't need any bitwise logic
            switch (ret)
            {
            case Cache::I64:
                bc.push_back({bc::NotEqualI});
                break;
            case Cache::F64:
                bc.push_back({bc::NotEqualF});
                break;
            default:
                log << Logger::Error << "Cannot compare types\n";
                throw std::runtime_error("Cannot compare types");
            }
            return ret;
        }
        }

        log << Logger::Error << "Unhandled node type " << static_cast<int>(node.type) << "\n";
        throw std::runtime_error("Unhandled node type");
    }
}