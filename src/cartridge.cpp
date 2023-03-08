#include <cartridge.h>

Cartridge::Cartridge() {
    loadRom("test.nes");
}

Cartridge::Cartridge(const char * file_name) {
    loadRom(file_name);
}

Cartridge::Byte * Cartridge::getPRGRom() {
    return prg_rom;
}

Cartridge::Byte * Cartridge::getCHRRom() {
    return chr_rom;
}

int Cartridge::getPRGSize() {
    return prg_size;
}

int Cartridge::getCHRSize() {
    return chr_size;
}

int Cartridge::getMirrorType() {
    return mirror_type;
}

bool Cartridge::isCHRRam() {
    return is_chr_ram;
}

Cartridge::~Cartridge() {
}

Cartridge::Byte* Cartridge::loadFile(const char *file_name) {
    int length;
    Byte *buffer;
    std::fstream file;
    file.open(file_name, std::ios::binary | std::ios::in);
    file.seekg(0, std::ios::end);
    length = file.tellg();
    file.seekg(0, std::ios::beg);
    buffer = new Byte[length];
    file.read((char*)buffer, length);
    file.close();
    return buffer;
}

void Cartridge::loadRom(const char *file_name) {
    Byte *buffer = loadFile(file_name);
    if (buffer[0] != 'N' || buffer[1] != 'E' || buffer[2] != 'S' || buffer[3] != 0x1A) return;
    prg_size = buffer[4] * 0x4000;
    chr_size = buffer[5] * 0x2000;
    prg_rom = new Byte[prg_size];
    chr_rom = chr_size ? new Byte[chr_size] : new Byte[0x2000];
    if (chr_size == 0) {
        is_chr_ram = true;
    }
    int i;
    for (i = 0; i < buffer[4] * 0x4000; i++) {
        prg_rom[i] = buffer[16 + i];
    }
    for (int j = 0; j < buffer[5] * 0x2000; j++) {
        chr_rom[j] = buffer[16 + i + j];
    }
    mirror_type = buffer[6] & 0b1;
    mapper = buffer[7] & 0xF0 | buffer[6] >> 4;
    delete[] buffer;

}
