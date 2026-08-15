CXX = g++
CXXFLAGS = -std=c++17 -Wall -g

all: os

os: os_main.cpp OS.o Assembler.o VirtualMachine.o
	$(CXX) $(CXXFLAGS) -o os os_main.cpp OS.o Assembler.o VirtualMachine.o

OS.o: OS.cpp OS.h PCB.h VirtualMachine.h Assembler.h
	$(CXX) $(CXXFLAGS) -c OS.cpp

Assembler.o: Assembler.cpp Assembler.h
	$(CXX) $(CXXFLAGS) -c Assembler.cpp

VirtualMachine.o: VirtualMachine.cpp VirtualMachine.h
	$(CXX) $(CXXFLAGS) -c VirtualMachine.cpp

clean:
	rm -f os *.o programs/*.o programs/*.st programs/progs
