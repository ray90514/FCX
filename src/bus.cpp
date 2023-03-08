
#include <bus.h>
#include <cstdlib>
#include <cstdio>

Bus::Bus(PPU& ppu, APU& apu, Cartridge& cartridge, Controller& controller) : ppu(ppu), apu(apu), cartridge(cartridge), controller(controller){
    ram = new Byte[0x0800];
    apu_reg = new Byte[0x0020];
    prg_rom = cartridge.getPRGRom();
    prg_size = cartridge.getPRGSize();
}

Bus::Byte Bus::readByte(Word address) { 
    if (address < 0x2000) {
        return ram[address % 0x0800];
    }
    else if (address < 0x4000) {
        return ppu.readReg((address - 0x2000) % 0x0008);
    }
    else if (address < 0x4020) {
        if (address == 0x4014) {

        }
        else if(address == 0x4016) {
            return controller.readInput();
        }
        else {
            return apu.readReg(address - 0x4000);
        }
    }
    else if (address < 0x6000) {

    }
    else if (address < 0x8000) {

    }
    else {
        return prg_rom[(address - 0x8000) % prg_size];
    }

    return 0;
}

void Bus::writeByte(Word address, Byte data) { 
    if (address < 0x2000) {
        ram[address % 0x0800] = data;
    }
    else if (address < 0x4000) {
        ppu.writeReg((address - 0x2000) % 0x0008, data);
    }
    else if (address < 0x4020) {
        //apu_reg[address - 0x4000] = data;
        if (address == 0x4014) {
            getOAMBuffer(data);
            ppu.writeOAM(oam_buffer);
        }
        else if (address == 0x4016) {
            controller.setMode(data);
        }
        else {
            return apu.writeReg(address - 0x4000, data);
        }
    }
    else if (address < 0x6000) {
        
    }
    else if (address < 0x8000) {

    }
    else {
        //TODO
    }
}

Bus::Word Bus::readWord(Word address) {
    return (readByte(address + 1) << 8) | readByte(address);
}
void Bus::writeWord(Word address, Word data) {
    writeByte(address, (Byte)data);
    writeByte(address + 1, (Byte)(data >> 8));
}



bool Bus::isNMI() {
    return ppu.isNMI();
}

bool Bus::isIRQ() {
    return apu.isIRQ();
}

void Bus::getOAMBuffer(Byte address) {
    Word start_addr = (Word)address << 8;
    for (int i = 0; i < 256; i++) {
        oam_buffer[i] = readByte(start_addr + i);
    }

}

Bus::~Bus() {

}