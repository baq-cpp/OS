CXX = g++
CXXFLAGS = -std=c++17 -Wall -g

all: os

os: os.cpp Assembler.o VirtualMachine.o
	$(CXX) $(CXXFLAGS) -o os os.cpp Assembler.o VirtualMachine.o

Assembler.o: Assembler.cpp Assembler.h
	$(CXX) $(CXXFLAGS) -c Assembler.cpp

VirtualMachine.o: VirtualMachine.cpp VirtualMachine.h
	$(CXX) $(CXXFLAGS) -c VirtualMachine.cpp

clean:
	rm -f os *.o programs/*.o
