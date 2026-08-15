#ifndef OS_H
#define OS_H

#include <list>
#include <queue>
#include <string>

#include "Assembler.h"
#include "PCB.h"
#include "VirtualMachine.h"

// ---------------------------------------------------------------------------
// Phase 2: a round-robin, time-sharing OS built on top of the phase 1 VM.
//
// One VirtualMachine and one Assembler are shared by every process. At any
// instant at most one process is "loaded into" the VM's registers; all
// others sit, fully described, in a PCB. Moving a process's state between
// its PCB and the VM is exactly what a real context switch does -- see
// dispatch()/save() below.
// ---------------------------------------------------------------------------

class OS {
public:
    OS();

    // Discovers every *.s file in the current directory, assembles it,
    // loads it into the VM, and round-robins all of them to completion.
    void run();

private:
    static const int TIME_SLICE = 15;
    static const int CONTEXT_SWITCH_TIME = 5;
    static const int IO_DURATION = 27; // ticks from trap to data-ready

    VirtualMachine vm;
    Assembler assembler;

    std::list<PCB*> jobs;        // every process, for final reporting
    std::queue<PCB*> readyQ;
    std::queue<PCB*> waitQ;
    PCB* running;

    int idleTime;
    int contextSwitchTime;

    void discoverAndLoadPrograms();
    void dispatchNextReady();                  // pop readyQ -> running, update waitTime, dispatch()
    void dispatch(PCB* p);                     // PCB -> VM registers (+ stack, if any)
    void save(PCB* p);                         // VM registers -> PCB (+ stack, if any)
    void loadStack(PCB* p);
    void saveStack(PCB* p);

    void handleIO(PCB* p, int trapClock);      // perform the actual read/write immediately
    int promoteCompletedIO();                  // waitQ -> readyQ for finished I/O; returns
                                                // earliest remaining completion time (INT_MAX if none)
    void terminate(PCB* p, const std::string& reason);

    void writeProcessAccounting(PCB* p);
    void writeSystemAccounting();              // appended to every *.out, once the system halts
};

#endif
