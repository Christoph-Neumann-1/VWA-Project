#include <InstructionNames.hpp>
#include <sstream>
#include <vector>
#include <iostream>

namespace vwa::bc
{
    std::unordered_map<std::string, BcInstruction> strInstrMap = []
    {
        std::unordered_map<std::string, BcInstruction> map;
        for (size_t i = 0; i < LastInstr; i++)
        {
            map[instrStrMap[i]] = static_cast<BcInstruction>(i);
        }
        return map;
    }();
    std::string disassemble(const std::vector<bc::BcToken> &bc)
    {
        std::stringstream ss;
        for (size_t i = 0; i < bc.size(); i += getInstructionSize(bc[i].instruction))
        {
            ss << instrStrMap[bc[i].instruction];
            switch (bc[i].instruction)
            {
                default:
                    break;
                }
        }
        return ss.str();
    }
}

#ifndef BC_DISASM_NO_MAIN
int main(void)
{
    //TODO: load from module file
   std::cout<< vwa::bc::disassemble(std::vector<vwa::bc::BcToken>{{vwa::bc::BcInstruction::AbsOf}});
}
#endif