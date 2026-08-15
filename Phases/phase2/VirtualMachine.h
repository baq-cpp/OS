#ifndef VIRTUALMACHINE_H
#define VIRTUALMACHINE_H

#include <cstdint>
#include <fstream>
#include <vector>

// ---------------------------------------------------------------------------
// Phase 2 status register layout (extends phase 1's low 5 bits):
//
//     d ... d  I/O-Reg  VM-Return-Status  V  L  E  G  C
//     15    10 9:8      7:5               4  3  2  1  0
//
// The VM no longer runs a program to completion by itself. Instead the OS
// hands it a time-slice budget (runSlice) and the VM executes until either
// the budget is spent or something happens that only the OS can handle:
// a halt, a runtime error, or a read/write "trap" (the VM does not touch
// *.in/*.out itself anymore -- see OS::handleIO). Whatever stopped it is
// recorded in both the return value AND the VM-Return-Status bits of sr,
// because the assignment models this as a real trap: the OS's first move
// after a stop is to read sr, exactly like an interrupt handler reading a
// hardware status register.
//
// VirtualMachine keeps its registers private, the same as phase 1. OS is
// declared a friend (per the handout) specifically so it can reach in and
// copy state to/from a PCB during a context switch -- that is the one
// legitimate reason to break encapsulation here, and it is worth noticing
// that PCB, which is a pure data record with no behavior of its own, is
// simply a struct with public fields instead.
// ---------------------------------------------------------------------------

class OS;

class VirtualMachine {
public:
    VirtualMachine();

    // Reads whitespace-separated decimal words from 'objectCode' into
    // memory starting at 'base'. Returns the number of words read (the
    // program's logical size, i.e. its limit).
    int loadProgram(std::fstream& objectCode, int base);

    // The shared call/return stack lives at the *top* of the 256-word
    // memory and grows downward. 'floor' is the first address a stack is
    // not allowed to fall below -- the high-water mark of all resident
    // programs' code, so one process's stack can never smash another
    // process's (or its own) instructions.
    void setStackFloor(int floor) { stackFloor = floor; }

    int get_clock() const { return clock_; }

    // Reasons the VM can hand control back to the OS. Values 0-7 double as
    // the literal 3-bit encoding written into sr bits 7:5 (see the table in
    // the phase 2 handout) -- RUNNING is a pure sentinel used only inside
    // step()/runSlice() and is never written into sr.
    enum class Reason {
        TIME_SLICE      = 0,
        HALTED          = 1,
        OUT_OF_BOUND    = 2,
        STACK_OVERFLOW  = 3,
        STACK_UNDERFLOW = 4,
        INVALID_OPCODE  = 5,
        READ            = 6,
        WRITE           = 7,
        RUNNING         = -1,
    };

    // Runs at most 'budget' clock ticks worth of instructions (a single
    // in-flight load/store/call/return is always finished, so the actual
    // ticks spent can overshoot budget by up to 3 -- see the handout).
    // Returns the reason execution stopped and writes that same reason into
    // sr's VM-Return-Status field.
    Reason runSlice(int budget);

    Reason getReturnStatus() const;
    int getIoRegister() const; // valid only when getReturnStatus() is READ or WRITE

private:
    friend class OS;

    static constexpr int SR_CARRY    = 0x01;
    static constexpr int SR_GREATER  = 0x02;
    static constexpr int SR_EQUAL    = 0x04;
    static constexpr int SR_LESS     = 0x08;
    static constexpr int SR_OVERFLOW = 0x10;
    static constexpr int SR_STATUS_SHIFT = 5;
    static constexpr int SR_STATUS_MASK  = 0x7 << SR_STATUS_SHIFT;
    static constexpr int SR_IOREG_SHIFT  = 8;
    static constexpr int SR_IOREG_MASK   = 0x3 << SR_IOREG_SHIFT;

    static const int MSIZE = 256;
    static const int RSIZE = 4;

    std::vector<int> mem;   // 256 words shared by every resident process
    std::vector<int> r;     // the 4 registers of whichever process is running

    int pc, ir, sr, sp, clock_;
    int base, limit;        // the *currently dispatched* process's window
    int stackFloor;         // lowest address the shared stack may reach

    static int signExtend8(int v);
    static int signed16(int v);
    void setFlag(int mask, bool value);
    void setReturnStatus(Reason r);

    Reason step(); // executes exactly one instruction
};

#endif
