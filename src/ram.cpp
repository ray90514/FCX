#include <ram.h>

RAM::RAM() {
	mem = new RAM::byte[2048];
}

RAM::RAM(int size) {
	mem = new RAM::byte[size];
}

void RAM::write(word adress, byte data) {
	
}

RAM::byte RAM::read(word adress) {
	
}

RAM::~RAM() {
	delete mem;
}