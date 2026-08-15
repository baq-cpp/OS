// Phase 2 OS entry point.
//
//   $ ./os [directory]
//
// The handout has the OS discover its workload with `ls *.s > progs` in the
// current directory, so if a directory is given we just chdir() into it
// first -- everything else (Assembler, VirtualMachine, OS) is unaware of
// where it's running.

#include <cstdlib>
#include <iostream>
#include <unistd.h>

#include "OS.h"

int main(int argc, char* argv[])
{
    if (argc > 1 && chdir(argv[1]) != 0) {
        std::cerr << "os: could not chdir to " << argv[1] << '\n';
        return 1;
    }

    OS os;
    os.run();
    return 0;
}
