#ifndef _CARTRIDGE_H
#define _CARTRIDGE_H
#include <cstdint>
#include <fstream>
class Cartridge {
public:
    using Byte = uint8_t;
    using Word = uint16_t;
    Cartridge();
    Cartridge(const char *file_name);
    Byte* getPRGRom();
    Byte* getCHRRom();
    int getPRGSize();
    int getCHRSize();
    int getMirrorType();
    bool isCHRRam();
    ~Cartridge();
private:
    int mapper;
    int prg_size;
    int chr_size;
    int mirror_type;
    bool is_chr_ram;
    Byte *prg_rom;
    Byte *chr_rom;
    Byte* loadFile(const char *file_name);
    void loadRom(const char *file_name);
};
#endif

