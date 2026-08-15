#include <fstream>
#include <iostream>
#include <string>

#include "Assembler.h"
#include "VirtualMachine.h"

using namespace std;

// Assembles 'name.s' into 'name.o', then loads and runs it on the VM,
// reading from 'name.in' and writing to 'name.out'.
int main(int argc, char *argv[])
{
    if (argc != 2) {
        cerr << "usage: " << argv[0] << " program.s\n";
        return 1;
    }

    string srcName = argv[1];
    const string suffix = ".s";
    if (srcName.size() <= suffix.size() ||
        srcName.compare(srcName.size() - suffix.size(), suffix.size(), suffix) != 0) {
        cerr << "error: " << srcName << " does not end in .s\n";
        return 1;
    }
    string base = srcName.substr(0, srcName.size() - suffix.size());

    fstream src(srcName, ios::in);
    if (!src) {
        cerr << "error: cannot open " << srcName << "\n";
        return 1;
    }

    fstream obj(base + ".o", ios::in | ios::out | ios::trunc);
    Assembler as;
    if (!as.assemble(src, obj)) {
        cerr << "error: assembly of " << srcName << " failed\n";
        return 1;
    }
    obj.clear();
    obj.seekg(0);

    fstream in(base + ".in", ios::in);
    fstream out(base + ".out", ios::out | ios::trunc);

    VirtualMachine vm;
    vm.run(obj, in, out);
    out << "Clock = " << vm.get_clock() << "\n";

    return 0;
} // main
