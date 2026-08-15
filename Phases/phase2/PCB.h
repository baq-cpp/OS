#ifndef PCB_H
#define PCB_H

#include <fstream>
#include <string>

// ---------------------------------------------------------------------------
// Process Control Block: everything the OS needs to remember about a
// process while it is *not* the one running on the (single, shared)
// VirtualMachine.
//
// Unlike VirtualMachine, PCB has no behavior of its own -- it is a plain
// data record, so its fields are simply public. OS is the only class that
// ever touches a PCB.
// ---------------------------------------------------------------------------

struct PCB {
    std::string name;   // e.g. "programs/fact1" -- no suffix

    // Saved VM register file, valid whenever this process is NOT running.
    int pc = 0;
    int r[4] = {0, 0, 0, 0};
    int sr = 0;
    int sp = 256;        // 256 == MSIZE == "no stack yet"
    int base = 0;
    int limit = 0;

    std::fstream inFile;
    std::fstream outFile;
    std::string stackFileName; // opened on demand, see OS::saveStack/loadStack

    bool finished = false;

    // Bookkeeping for a pending read/write: set when the process traps into
    // the OS, consulted once the I/O "completes".
    int ioTargetReg = -1;
    int ioCompletionTime = 0;

    // Timestamps marking the start of the process's current stint in a
    // queue; accounting fields below accumulate the *elapsed* time each
    // time the process leaves that queue.
    int readyEnteredAt = 0;
    int waitEnteredAt = 0;

    // Accounting (Phase 2 handout, "Process Specific").
    int cpuTime = 0;
    int waitTime = 0;
    int ioTime = 0;
    int turnaroundTime = 0;

    PCB(const std::string& n, int b, int l) : name(n), pc(b), base(b), limit(l) {}
};

#endif
