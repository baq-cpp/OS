#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <fstream>
#include <map>
#include <string>

// ---------------------------------------------------------------------------
// Assembler for the 16-bit toy CPU used throughout this lecture.
//
// Every instruction word is laid out as either:
//
//   Format 1:  OP(5) RD(2) I(1) RS(2) UNUSED(6)     bits 15..0
//   Format 2:  OP(5) RD(2) I(1) ADDR/CONST(8)       bits 15..0
//
// Rather than hand-coding 26 near-identical if/else branches (one per
// mnemonic), we describe each mnemonic once as a row in a table
// (see instructionTable() in Assembler.cpp). The encoder is then a single
// ~15 line function that looks the mnemonic up and follows its recipe. This
// is exactly how real assemblers are built: the ISA is DATA, the encoder is
// generic logic that walks that data.
// ---------------------------------------------------------------------------

// What, besides RD, an instruction expects on the rest of the line.
enum class SecondOperand {
    NONE,   // no second operand:            halt, noop, return
    REG,    // a source register (RS):       add, sub, and, xor, compr ...
    ADDR,   // an unsigned 8-bit address:     load, store, jump, jumpl, ...
    CONST   // a signed 8-bit constant:       loadi, addi, subi, ...
};

struct InstructionSpec {
    int opcode;             // 5-bit opcode (0-25)
    bool hasRd;             // does this instruction encode an RD field?
    SecondOperand second;   // what (if anything) follows RD on the line
    int immediateBit;       // the literal value of the "I" bit for this row
};

class Assembler {
public:
    Assembler();

    // Reads assembly source from 'in' line by line and writes one 5-digit,
    // zero-padded decimal object word per line to 'out'. Comments start
    // with '!' -- either a whole comment line, or trailing text after the
    // operands the instruction needed (the line parser simply never reads
    // that far, so it is ignored "for free").
    //
    // Returns true on success, false if any line fails to assemble (bad
    // mnemonic, register out of 0..3, address out of 0..255, or constant
    // out of -128..127). On failure, no further lines are processed.
    bool assemble(std::fstream& in, std::fstream& out);

private:
    static const std::map<std::string, InstructionSpec>& instructionTable();

    // Assembles a single non-comment, non-blank line into 'word'.
    // Returns false and prints a diagnostic to std::cerr on error.
    bool encodeLine(const std::string& line, int lineNumber, unsigned& word) const;
};

#endif
