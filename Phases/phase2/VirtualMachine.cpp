#include "VirtualMachine.h"

using namespace std;

VirtualMachine::VirtualMachine()
    : mem(MSIZE, 0), r(RSIZE, 0), pc(0), ir(0), sr(0), sp(MSIZE), clock_(0),
      base(0), limit(0), stackFloor(0)
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

void VirtualMachine::setReturnStatus(Reason reason)
{
    sr = (sr & ~SR_STATUS_MASK) | ((static_cast<int>(reason) << SR_STATUS_SHIFT) & SR_STATUS_MASK);
}

VirtualMachine::Reason VirtualMachine::getReturnStatus() const
{
    return static_cast<Reason>((sr & SR_STATUS_MASK) >> SR_STATUS_SHIFT);
}

int VirtualMachine::getIoRegister() const
{
    return (sr & SR_IOREG_MASK) >> SR_IOREG_SHIFT;
}

int VirtualMachine::loadProgram(fstream& objectCode, int base)
{
    int count = 0;
    int word;
    while (base + count < MSIZE && objectCode >> word) {
        mem[base + count] = word;
        count++;
    }
    return count;
}

VirtualMachine::Reason VirtualMachine::runSlice(int budget)
{
    int used = 0;
    while (used < budget) {
        int before = clock_;
        Reason reason = step();
        used += clock_ - before;
        if (reason != Reason::RUNNING)
            return reason; // step() already recorded this in sr
    }
    setReturnStatus(Reason::TIME_SLICE);
    return Reason::TIME_SLICE;
}

VirtualMachine::Reason VirtualMachine::step()
{
    if (pc < base || pc >= base + limit) {
        setReturnStatus(Reason::OUT_OF_BOUND);
        return Reason::OUT_OF_BOUND;
    }

    ir = mem[pc];
    pc++;

    int opcode = (ir >> 11) & 0x1F;
    int rd     = (ir >> 9)  & 0x3;
    int i      = (ir >> 8)  & 0x1;
    int rs     = (ir >> 6)  & 0x3;
    int addr   = ir & 0xFF;
    int constant = ir & 0xFF;

    clock_++; // baseline: every instruction costs at least 1 tick

    auto inBounds = [&](int a) { return a >= 0 && a < limit; };
    auto fault = [&](Reason reason) { setReturnStatus(reason); return reason; };

    switch (opcode) {
        case 0: // load / loadi
            if (i) {
                r[rd] = signExtend8(constant) & 0xFFFF;
            } else {
                if (!inBounds(addr)) return fault(Reason::OUT_OF_BOUND);
                r[rd] = mem[base + addr];
                clock_ += 3; // load: 4 ticks total
            }
            break;

        case 1: // store
            if (!inBounds(addr)) return fault(Reason::OUT_OF_BOUND);
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

        case 10: { // shla
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

        case 12: { // shra
            int signBit = r[rd] & 0x8000;
            setFlag(SR_CARRY, (r[rd] & 0x1) != 0);
            r[rd] = ((r[rd] & 0xFFFF) >> 1) | signBit;
            break;
        }

        case 13: { // compr / compri
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
            // Preserve the I/O-register and VM-Return-Status fields -- a
            // user program can only see/set the low 5 condition-code bits
            // through getstat/putstat, never the OS-only trap-status bits.
            sr = (sr & (SR_STATUS_MASK | SR_IOREG_MASK)) | (r[rd] & 0x1F);
            break;

        case 16: // jump
            if (!inBounds(addr)) return fault(Reason::OUT_OF_BOUND);
            pc = base + addr;
            break;

        case 17: // jumpl
            if (!inBounds(addr)) return fault(Reason::OUT_OF_BOUND);
            if (sr & SR_LESS) pc = base + addr;
            break;

        case 18: // jumpe
            if (!inBounds(addr)) return fault(Reason::OUT_OF_BOUND);
            if (sr & SR_EQUAL) pc = base + addr;
            break;

        case 19: // jumpg
            if (!inBounds(addr)) return fault(Reason::OUT_OF_BOUND);
            if (sr & SR_GREATER) pc = base + addr;
            break;

        case 20: // call: push {pc, r0, r1, r2, r3, sr}, jump to addr
            if (!inBounds(addr)) return fault(Reason::OUT_OF_BOUND);
            if (sp - 6 < stackFloor) return fault(Reason::STACK_OVERFLOW);
            mem[--sp] = pc;
            for (int j = 0; j < 4; j++) mem[--sp] = r[j];
            mem[--sp] = sr;
            pc = base + addr;
            clock_ += 3; // call: 4 ticks total
            break;

        case 21: // return: pop {sr, r3, r2, r1, r0, pc} (reverse of call)
            if (sp + 6 > MSIZE) return fault(Reason::STACK_UNDERFLOW);
            sr = mem[sp++];
            for (int j = 3; j >= 0; j--) r[j] = mem[sp++];
            pc = mem[sp++];
            clock_ += 3; // return: 4 ticks total
            break;

        case 22: // read -- trap to the OS; it owns *.in and knows the deadline
            setReturnStatus(Reason::READ);
            sr = (sr & ~SR_IOREG_MASK) | ((rd << SR_IOREG_SHIFT) & SR_IOREG_MASK);
            return Reason::READ;

        case 23: // write -- trap to the OS; it owns *.out
            setReturnStatus(Reason::WRITE);
            sr = (sr & ~SR_IOREG_MASK) | ((rd << SR_IOREG_SHIFT) & SR_IOREG_MASK);
            return Reason::WRITE;

        case 24: // halt
            return fault(Reason::HALTED);

        case 25: // noop
            break;

        default:
            return fault(Reason::INVALID_OPCODE);
    }

    return Reason::RUNNING;
}
