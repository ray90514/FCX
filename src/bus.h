#ifndef _BUS_H_
#define _BUS_H_
#include <cstdint>
#include "ppu.h"
#include "apu.h"
#include "cartridge.h"
#include "controller.h"
class Bus {
public:
    using Byte = uint8_t;
    using Word = uint16_t;
    Bus(PPU& ppu, APU& apu, Cartridge& cartridge, Controller& controller);
    Byte readByte(Word address);
    void writeByte(Word address, Byte data);
    Word readWord(Word address);
    void writeWord(Word address, Word data);
    bool isNMI();
    bool isIRQ();
    ~Bus();
private:
    PPU &ppu;
    APU &apu;
    Cartridge &cartridge;
    Controller &controller;
	Byte *ram;
    Byte *ppu_reg;
    Byte *apu_reg;
    Byte *prg_rom;
    int prg_size;
    int chr_size;
    void getOAMBuffer(Byte address);
    Byte oam_buffer[256];
};
#endif