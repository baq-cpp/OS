#include "OS.h"

#include <climits>
#include <cstdlib>
#include <fstream>
#include <iostream>

using namespace std;

OS::OS() : running(nullptr), idleTime(0), contextSwitchTime(0) {}

void OS::discoverAndLoadPrograms()
{
    // Exactly what the handout specifies: let the shell enumerate the
    // programs so the OS's own code stays filesystem-listing-agnostic.
    system("ls *.s > progs 2>/dev/null");

    ifstream progs("progs");
    string fileName;
    int nextBase = 0;

    while (progs >> fileName) {
        string name = fileName.substr(0, fileName.size() - 2); // strip ".s"

        fstream src(fileName, ios::in);
        fstream obj(name + ".o", ios::out);
        if (!src.is_open() || !obj.is_open()) {
            cerr << "OS: could not open " << fileName << " or " << name << ".o, skipping\n";
            continue;
        }
        if (!assembler.assemble(src, obj)) {
            cerr << "OS: assembler error in " << fileName << ", skipping\n";
            continue;
        }
        src.close();
        obj.close();

        obj.open(name + ".o", ios::in);
        int limit = vm.loadProgram(obj, nextBase);
        obj.close();

        PCB* p = new PCB(name, nextBase, limit);
        p->inFile.open(name + ".in", ios::in);
        p->outFile.open(name + ".out", ios::out);
        p->stackFileName = name + ".st";

        jobs.push_back(p);
        readyQ.push(p);

        nextBase += limit;
    }

    // The shared call/return stack lives above the highest resident
    // program and must never be pushed down into anyone's code.
    vm.setStackFloor(nextBase);
}

void OS::dispatch(PCB* p)
{
    vm.pc = p->pc;
    for (int i = 0; i < 4; i++) vm.r[i] = p->r[i];
    vm.sr = p->sr;
    vm.sp = p->sp;
    vm.base = p->base;
    vm.limit = p->limit;
    if (p->sp < 256) loadStack(p);
    running = p;
}

void OS::save(PCB* p)
{
    p->pc = vm.pc;
    for (int i = 0; i < 4; i++) p->r[i] = vm.r[i];
    p->sr = vm.sr;
    p->sp = vm.sp;
    if (p->sp < 256) saveStack(p);
}

void OS::saveStack(PCB* p)
{
    fstream st(p->stackFileName, ios::out | ios::trunc);
    for (int addr = p->sp; addr < 256; addr++)
        st << vm.mem[addr] << '\n';
}

void OS::loadStack(PCB* p)
{
    fstream st(p->stackFileName, ios::in);
    for (int addr = p->sp; addr < 256 && st; addr++)
        st >> vm.mem[addr];
}

void OS::dispatchNextReady()
{
    running = readyQ.front();
    readyQ.pop();
    running->waitTime += vm.get_clock() - running->readyEnteredAt;
    dispatch(running);
}

void OS::handleIO(PCB* p, int trapClock)
{
    int reg = vm.getIoRegister();
    if (vm.getReturnStatus() == VirtualMachine::Reason::READ) {
        int value = 0;
        p->inFile >> value;
        p->r[reg] = value & 0xFFFF;
    } else { // WRITE
        int16_t value = static_cast<int16_t>(p->r[reg] & 0xFFFF);
        p->outFile << value << '\n';
    }
    // 1 tick was already charged by the VM to decode/trap the instruction;
    // the transfer itself takes the remaining 27.
    p->ioCompletionTime = trapClock + IO_DURATION;
}

int OS::promoteCompletedIO()
{
    int n = static_cast<int>(waitQ.size());
    int earliest = INT_MAX;
    for (int i = 0; i < n; i++) {
        PCB* p = waitQ.front();
        waitQ.pop();
        if (p->ioCompletionTime <= vm.get_clock()) {
            p->ioTime += vm.get_clock() - p->waitEnteredAt;
            p->readyEnteredAt = vm.get_clock();
            readyQ.push(p);
        } else {
            earliest = min(earliest, p->ioCompletionTime);
            waitQ.push(p);
        }
    }
    return earliest;
}

void OS::terminate(PCB* p, const string& reason)
{
    if (!reason.empty())
        p->outFile << reason << '\n';
    p->turnaroundTime = vm.get_clock();
    p->finished = true;
    writeProcessAccounting(p);
}

void OS::writeProcessAccounting(PCB* p)
{
    p->outFile << "\n--- Process Accounting: " << p->name << " ---\n"
               << "CPU Time        = " << p->cpuTime << '\n'
               << "Waiting Time    = " << p->waitTime << '\n'
               << "I/O Time        = " << p->ioTime << '\n'
               << "Turnaround Time = " << p->turnaroundTime << '\n';
}

void OS::writeSystemAccounting()
{
    int finalClock = vm.get_clock();
    int totalCpu = 0;
    for (PCB* p : jobs) totalCpu += p->cpuTime;

    int systemTime = idleTime + contextSwitchTime;
    double systemUtil = finalClock ? 100.0 * (finalClock - idleTime) / finalClock : 0.0;
    double userUtil   = finalClock ? 100.0 * totalCpu / finalClock : 0.0;
    double throughput = finalClock ? 1000.0 * static_cast<int>(jobs.size()) / finalClock : 0.0;

    for (PCB* p : jobs) {
        p->outFile << "\n--- System Accounting (system halted at clock " << finalClock << ") ---\n"
                   << "System Time            = " << systemTime
                   << " (idle " << idleTime << " + context switch " << contextSwitchTime << ")\n"
                   << "System CPU Utilization = " << systemUtil << "%\n"
                   << "User CPU Utilization   = " << userUtil << "%\n"
                   << "Throughput             = " << throughput << " processes/sec\n";
        p->outFile.close();
    }
}

void OS::run()
{
    discoverAndLoadPrograms();
    if (jobs.empty()) {
        cerr << "OS: no *.s programs found in the current directory\n";
        return;
    }

    dispatchNextReady();

    while (true) {
        int clockBeforeSlice = vm.get_clock();
        VirtualMachine::Reason reason = vm.runSlice(TIME_SLICE);
        int trapClock = vm.get_clock();
        running->cpuTime += trapClock - clockBeforeSlice;
        save(running);

        // Context switch: 5 ticks of pure OS/scheduler overhead.
        vm.clock_ += CONTEXT_SWITCH_TIME;
        contextSwitchTime += CONTEXT_SWITCH_TIME;

        // 1st: anyone whose I/O finished while we were running rejoins readyQ.
        promoteCompletedIO();

        // 2nd: file the process that just relinquished the VM.
        PCB* justRan = running;
        running = nullptr;
        switch (reason) {
            case VirtualMachine::Reason::HALTED:
                terminate(justRan, "");
                break;
            case VirtualMachine::Reason::OUT_OF_BOUND:
                terminate(justRan, "Runtime error: out-of-bound memory reference.");
                break;
            case VirtualMachine::Reason::STACK_OVERFLOW:
                terminate(justRan, "Runtime error: stack overflow.");
                break;
            case VirtualMachine::Reason::STACK_UNDERFLOW:
                terminate(justRan, "Runtime error: stack underflow.");
                break;
            case VirtualMachine::Reason::INVALID_OPCODE:
                terminate(justRan, "Runtime error: invalid opcode.");
                break;
            case VirtualMachine::Reason::READ:
            case VirtualMachine::Reason::WRITE:
                handleIO(justRan, trapClock);
                justRan->waitEnteredAt = vm.get_clock();
                waitQ.push(justRan);
                break;
            case VirtualMachine::Reason::TIME_SLICE:
            default:
                justRan->readyEnteredAt = vm.get_clock();
                readyQ.push(justRan);
                break;
        }

        // 3rd: if nobody is ready but someone is waiting on I/O, the CPU
        // goes idle until the earliest pending I/O completes.
        if (readyQ.empty() && !waitQ.empty()) {
            int earliest = promoteCompletedIO(); // nothing is due yet; just peek
            if (earliest != INT_MAX && earliest > vm.get_clock()) {
                int idle = earliest - vm.get_clock();
                vm.clock_ += idle;
                idleTime += idle;
                promoteCompletedIO(); // now actually promote it
            }
        }

        if (readyQ.empty())
            break; // nothing ready, nothing waiting -> every process is done

        dispatchNextReady();
    }

    writeSystemAccounting();
    for (PCB* p : jobs) delete p;
}
