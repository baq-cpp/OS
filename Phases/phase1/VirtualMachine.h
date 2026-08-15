#ifndef VIRTUALMACHINE_H
#define VIRTUALMACHINE_H

#include <cstdint>
#include <fstream>
#include <vector>

// ---------------------------------------------------------------------------
// A simulated 16-bit CPU: 4 general purpose registers, a 256-word memory,
// and the fetch-decode-execute cycle from the handout.
//
// Status register (sr) bit layout for phase 1:
//     d ... d  V  L  E  G  C
//     15    5  4  3  2  1  0
//
// Registers and memory words are stored as the *bit pattern* a real 16-bit
// register would hold (0..65535). Helpers below reinterpret that pattern as
// signed (int16_t) only where an operation is defined to be signed
// (arithmetic overflow, sign-extension of constants, compr, write). Carry is
// computed from the *unsigned* pattern, mirroring how real ALUs separate
// "did this wrap as unsigned" (carry) from "did this wrap as signed"
// (overflow).
// ---------------------------------------------------------------------------

class VirtualMachine {
public:
    VirtualMachine();

    // Loads the object program from 'objectCode' into memory (base=0,
    // limit=program size), then runs it to completion, reading with
    // `read` from 'in' and writing with `write` to 'out'.
    void run(std::fstream& objectCode, std::fstream& in, std::fstream& out);

    int get_clock() const { return clock_; }

protected:
    // Status register bit masks.
    static constexpr int SR_CARRY    = 0x01;
    static constexpr int SR_GREATER  = 0x02;
    static constexpr int SR_EQUAL    = 0x04;
    static constexpr int SR_LESS     = 0x08;
    static constexpr int SR_OVERFLOW = 0x10;

    static const int MSIZE = 256;
    static const int RSIZE = 4;

    std::vector<int> mem;   // 256 words, each holds a 16-bit pattern (0..65535)
    std::vector<int> r;     // 4 registers, same representation as mem

    int pc, ir, sr, sp, clock_;
    int base, limit;

    // Sign-extends the low 8 bits of 'v' to a full int (two's complement).
    static int signExtend8(int v);
    // Reinterprets the low 16 bits of 'v' as signed.
    static int signed16(int v);

    void setFlag(int mask, bool value);

    // Decodes and executes exactly one instruction (one iteration of the
    // TOP: ir <- mem[pc]; pc++; execute.
    // Returns CONTINUE to keep looping, or the reason the loop must stop.
    enum class Step { CONTINUE, HALTED, OUT_OF_BOUND, STACK_OVERFLOW, STACK_UNDERFLOW, INVALID_OPCODE };
    Step step(std::fstream& in, std::fstream& out);
};

#endif
