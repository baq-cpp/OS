#include "Assembler.h"

#include <iomanip>
#include <iostream>
#include <sstream>

using namespace std;

Assembler::Assembler() {}

// The entire VM instruction set, as one table. Compare each row against the
// "VM Instruction Set" table in the handout -- opcode numbers, operand
// shape, and I-bit all come straight from there.
const map<string, InstructionSpec>& Assembler::instructionTable()
{
    static const map<string, InstructionSpec> table = {
        // mnemonic     opcode  hasRd  second-operand           I
        {"load",    {0,  true,  SecondOperand::ADDR,  0}},
        {"loadi",   {0,  true,  SecondOperand::CONST, 1}},
        {"store",   {1,  true,  SecondOperand::ADDR,  1}}, // I is forced to 1
        {"add",     {2,  true,  SecondOperand::REG,   0}},
        {"addi",    {2,  true,  SecondOperand::CONST, 1}},
        {"addc",    {3,  true,  SecondOperand::REG,   0}},
        {"addci",   {3,  true,  SecondOperand::CONST, 1}},
        {"sub",     {4,  true,  SecondOperand::REG,   0}},
        {"subi",    {4,  true,  SecondOperand::CONST, 1}},
        {"subc",    {5,  true,  SecondOperand::REG,   0}},
        {"subci",   {5,  true,  SecondOperand::CONST, 1}},
        {"and",     {6,  true,  SecondOperand::REG,   0}},
        {"andi",    {6,  true,  SecondOperand::CONST, 1}},
        {"xor",     {7,  true,  SecondOperand::REG,   0}},
        {"xori",    {7,  true,  SecondOperand::CONST, 1}},
        {"compl",   {8,  true,  SecondOperand::NONE,  0}},
        {"shl",     {9,  true,  SecondOperand::NONE,  0}},
        {"shla",    {10, true,  SecondOperand::NONE,  0}},
        {"shr",     {11, true,  SecondOperand::NONE,  0}},
        {"shra",    {12, true,  SecondOperand::NONE,  0}},
        {"compr",   {13, true,  SecondOperand::REG,   0}},
        {"compri",  {13, true,  SecondOperand::CONST, 1}},
        {"getstat", {14, true,  SecondOperand::NONE,  0}},
        {"putstat", {15, true,  SecondOperand::NONE,  0}},
        {"jump",    {16, false, SecondOperand::ADDR,  1}},
        {"jumpl",   {17, false, SecondOperand::ADDR,  1}},
        {"jumpe",   {18, false, SecondOperand::ADDR,  1}},
        {"jumpg",   {19, false, SecondOperand::ADDR,  1}},
        {"call",    {20, false, SecondOperand::ADDR,  1}},
        {"return",  {21, false, SecondOperand::NONE,  0}},
        {"read",    {22, true,  SecondOperand::NONE,  0}},
        {"write",   {23, true,  SecondOperand::NONE,  0}},
        {"halt",    {24, false, SecondOperand::NONE,  0}},
        {"noop",    {25, false, SecondOperand::NONE,  0}},
    };
    return table;
}

bool Assembler::encodeLine(const string& line, int lineNumber, unsigned& word) const
{
    istringstream tokens(line);
    string mnemonic;
    tokens >> mnemonic;

    const auto& table = instructionTable();
    auto it = table.find(mnemonic);
    if (it == table.end()) {
        cerr << "Assembler error, line " << lineNumber
             << ": unknown instruction \"" << mnemonic << "\"\n";
        return false;
    }
    const InstructionSpec& spec = it->second;

    int rd = 0;
    if (spec.hasRd) {
        tokens >> rd;
        if (rd < 0 || rd > 3) {
            cerr << "Assembler error, line " << lineNumber
                 << ": register " << rd << " out of range (0-3)\n";
            return false;
        }
    }

    // opcode(5) | rd(2) | I(1) | ...
    word = (static_cast<unsigned>(spec.opcode) << 11)
         | (static_cast<unsigned>(rd) << 9)
         | (static_cast<unsigned>(spec.immediateBit) << 8);

    switch (spec.second) {
        case SecondOperand::NONE:
            break;

        case SecondOperand::REG: {
            int rs;
            tokens >> rs;
            if (rs < 0 || rs > 3) {
                cerr << "Assembler error, line " << lineNumber
                     << ": register " << rs << " out of range (0-3)\n";
                return false;
            }
            word |= static_cast<unsigned>(rs) << 6;
            break;
        }

        case SecondOperand::ADDR: {
            int addr;
            tokens >> addr;
            if (addr < 0 || addr > 255) {
                cerr << "Assembler error, line " << lineNumber
                     << ": address " << addr << " out of range (0-255)\n";
                return false;
            }
            word |= static_cast<unsigned>(addr) & 0xFF;
            break;
        }

        case SecondOperand::CONST: {
            int constant;
            tokens >> constant;
            if (constant < -128 || constant > 127) {
                cerr << "Assembler error, line " << lineNumber
                     << ": constant " << constant << " out of range (-128..127)\n";
                return false;
            }
            word |= static_cast<unsigned>(constant) & 0xFF;
            break;
        }
    }

    return true;
}

bool Assembler::assemble(fstream& in, fstream& out)
{
    string line;
    int lineNumber = 0;

    while (getline(in, line)) {
        lineNumber++;

        // Skip blank lines and whole-line comments. A trailing "! comment"
        // after real operands needs no special handling: encodeLine() only
        // reads as many tokens as the instruction requires and never looks
        // at the rest of the line.
        if (line.empty() || line[0] == '!')
            continue;

        unsigned word;
        if (!encodeLine(line, lineNumber, word))
            return false;

        out << setfill('0') << setw(5) << word << '\n';
    }

    return true;
}
