#include "VirtualMachine.h"

#include <iostream>

using namespace std;

VirtualMachine::VirtualMachine()
    : mem(MSIZE, 0), r(RSIZE, 0), pc(0), ir(0), sr(0), sp(MSIZE), clock_(0), base(0), limit(0)
{
}

int VirtualMachine::signExtend8(int v)
{
    return static_cast<int>(static_cast<int8_t>(v & 0xFF));
}

int VirtualMachine::signed16(int v)
{
    return static_cast<int>(static_cast<int16_t>(v & 0xFFFF));
}

void VirtualMachine::setFlag(int mask, bool value)
{
    if (value) sr |= mask;
    else       sr &= ~mask;
}

void VirtualMachine::run(fstream& objectCode, fstream& in, fstream& out)
{
    base = 0;
    for (limit = 0; objectCode >> mem[limit]; limit++) {}

    pc = 0;
    sr = 0;
    sp = MSIZE;
    clock_ = 0;

    for (;;) {
        Step s = step(in, out);
        if (s == Step::HALTED) {
            break;
        } else if (s == Step::OUT_OF_BOUND) {
            out << "Runtime error: out-of-bound memory reference.\n";
            break;
        } else if (s == Step::STACK_OVERFLOW) {
            out << "Runtime error: stack overflow.\n";
            break;
        } else if (s == Step::STACK_UNDERFLOW) {
            out << "Runtime error: stack underflow.\n";
            break;
        } else if (s == Step::INVALID_OPCODE) {
            out << "Runtime error: invalid opcode.\n";
            break;
        }
        // Step::CONTINUE -> loop again
    }
}

VirtualMachine::Step VirtualMachine::step(fstream& in, fstream& out)
{
    if (pc < base || pc >= base + limit)
        return Step::OUT_OF_BOUND;

    ir = mem[pc];
    pc++;

    int opcode = (ir >> 11) & 0x1F;
    int rd     = (ir >> 9)  & 0x3;
    int i      = (ir >> 8)  & 0x1;
    int rs     = (ir >> 6)  & 0x3;
    int addr   = ir & 0xFF;          // used when i == 0 and instruction is address-based
    int constant = ir & 0xFF;        // used when i == 1 (sign-extended by the consumer)

    clock_++; // baseline: every instruction costs at least 1 tick

    auto inBounds = [&](int a) { return a >= 0 && a < limit; };

    switch (opcode) {
        case 0: // load / loadi
            if (i) {
                r[rd] = signExtend8(constant) & 0xFFFF;
            } else {
                if (!inBounds(addr)) return Step::OUT_OF_BOUND;
                r[rd] = mem[base + addr];
                clock_ += 3; // load: 4 ticks total
            }
            break;

        case 1: // store
            if (!inBounds(addr)) return Step::OUT_OF_BOUND;
            mem[base + addr] = r[rd];
            clock_ += 3; // store: 4 ticks total
            break;

        case 2:   // add / addi
        case 3: { // addc / addci
            bool withCarry = (opcode == 3);
            long a = signed16(r[rd]);
            long b = i ? signExtend8(constant) : signed16(r[rs]);
            long carryIn = withCarry ? (sr & SR_CARRY) : 0;
            long signedResult = a + b + carryIn;

            unsigned long ua = static_cast<unsigned long>(r[rd] & 0xFFFF);
            unsigned long ub = i ? static_cast<unsigned long>(signExtend8(constant) & 0xFFFF)
                                  : static_cast<unsigned long>(r[rs] & 0xFFFF);
            unsigned long unsignedResult = ua + ub + static_cast<unsigned long>(carryIn);

            r[rd] = static_cast<int>(unsignedResult & 0xFFFF);
            setFlag(SR_CARRY, (unsignedResult & 0x10000) != 0);
            setFlag(SR_OVERFLOW, signedResult < -32768 || signedResult > 32767);
            break;
        }

        case 4:   // sub / subi
        case 5: { // subc / subci
            bool withBorrow = (opcode == 5);
            long a = signed16(r[rd]);
            long b = i ? signExtend8(constant) : signed16(r[rs]);
            long borrowIn = withBorrow ? (sr & SR_CARRY) : 0;
            long signedResult = a - b - borrowIn;

            long ua = r[rd] & 0xFFFF;
            long ub = (i ? (signExtend8(constant) & 0xFFFF) : (r[rs] & 0xFFFF));
            bool borrowOccurred = (ua - ub - borrowIn) < 0;

            r[rd] = static_cast<int>(((ua - ub - borrowIn) & 0xFFFF));
            setFlag(SR_CARRY, borrowOccurred);
            setFlag(SR_OVERFLOW, signedResult < -32768 || signedResult > 32767);
            break;
        }

        case 6: // and / andi
            r[rd] = (i ? (r[rd] & (signExtend8(constant) & 0xFFFF))
                       : (r[rd] & r[rs])) & 0xFFFF;
            break;

        case 7: // xor / xori
            r[rd] = (i ? (r[rd] ^ (signExtend8(constant) & 0xFFFF))
                       : (r[rd] ^ r[rs])) & 0xFFFF;
            break;

        case 8: // compl
            r[rd] = (~r[rd]) & 0xFFFF;
            break;

        case 9: { // shl
            int shifted = (r[rd] << 1);
            setFlag(SR_CARRY, (shifted & 0x10000) != 0);
            r[rd] = shifted & 0xFFFF;
            break;
        }

        case 10: { // shla (arithmetic left shift, sign-preserving)
            int signBit = r[rd] & 0x8000;
            int shifted = (r[rd] << 1);
            setFlag(SR_CARRY, (shifted & 0x10000) != 0);
            r[rd] = (shifted & 0x7FFF) | signBit;
            break;
        }

        case 11: // shr
            setFlag(SR_CARRY, (r[rd] & 0x1) != 0);
            r[rd] = (r[rd] & 0xFFFF) >> 1;
            break;

        case 12: { // shra (arithmetic right shift, sign-preserving)
            int signBit = r[rd] & 0x8000;
            setFlag(SR_CARRY, (r[rd] & 0x1) != 0);
            r[rd] = ((r[rd] & 0xFFFF) >> 1) | signBit;
            break;
        }

        case 13: { // compr / compri (signed comparison)
            int a = signed16(r[rd]);
            int b = i ? signExtend8(constant) : signed16(r[rs]);
            setFlag(SR_LESS, a < b);
            setFlag(SR_EQUAL, a == b);
            setFlag(SR_GREATER, a > b);
            break;
        }

        case 14: // getstat
            r[rd] = sr & 0xFFFF;
            break;

        case 15: // putstat
            sr = r[rd] & 0xFFFF;
            break;

        case 16: // jump
            if (!inBounds(addr)) return Step::OUT_OF_BOUND;
            pc = base + addr;
            break;

        case 17: // jumpl
            if (!inBounds(addr)) return Step::OUT_OF_BOUND;
            if (sr & SR_LESS) pc = base + addr;
            break;

        case 18: // jumpe
            if (!inBounds(addr)) return Step::OUT_OF_BOUND;
            if (sr & SR_EQUAL) pc = base + addr;
            break;

        case 19: // jumpg
            if (!inBounds(addr)) return Step::OUT_OF_BOUND;
            if (sr & SR_GREATER) pc = base + addr;
            break;

        case 20: // call: push {pc, r0, r1, r2, r3, sr}, jump to addr
            if (!inBounds(addr)) return Step::OUT_OF_BOUND;
            if (sp - 6 < base + limit) return Step::STACK_OVERFLOW;
            mem[--sp] = pc;
            for (int j = 0; j < 4; j++) mem[--sp] = r[j];
            mem[--sp] = sr;
            pc = base + addr;
            clock_ += 3; // call: 4 ticks total
            break;

        case 21: // return: pop {sr, r3, r2, r1, r0, pc} (reverse of call)
            if (sp + 6 > MSIZE) return Step::STACK_UNDERFLOW;
            sr = mem[sp++];
            for (int j = 3; j >= 0; j--) r[j] = mem[sp++];
            pc = mem[sp++];
            clock_ += 3; // return: 4 ticks total
            break;

        case 22: // read
            in >> r[rd];
            r[rd] &= 0xFFFF;
            clock_ += 27; // read: 28 ticks total
            break;

        case 23: // write
            out << signed16(r[rd]) << '\n';
            clock_ += 27; // write: 28 ticks total
            break;

        case 24: // halt
            return Step::HALTED;

        case 25: // noop
            break;

        default:
            return Step::INVALID_OPCODE;
    }

    return Step::CONTINUE;
}
