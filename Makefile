INCLUDE = src

CXX = g++

CXXFLAGS = -g -Wall

OBJ = apu.o bus.o cartridge.o controller.o cpu.o ppu.o fcx.o

.PHONY: clean

all: FCX

FCX: $(OBJ)
	$(CXX) $(CXXFLAGS) -o FCX $(OBJ) -lSDL2 -L.
	rm -f *.o
%.o: $(INCLUDE)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
	
clean: 
	rm -f *.o ./FCX
