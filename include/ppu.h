#ifndef _PPU_H
#define _PPU_H
#include <cstdint>
#include <cartridge.h>
class PPU
{
    enum Reg {
        Ctrl,
        Mask,
        Status,
        OAMaddr,
        OAMdata,
        Scroll,
        PPUaddr,
        PPUdata
        //,OAMDMA
    };
    enum Mirror {
        Horizantal,
        Vertical
    };
    enum Ctrl {
        BaseName = 0b00000011,
        VramAddrIncrement = 0b00000100,
        SpriteHalf = 0b00001000,
        BackgroundHalf = 0b00010000,
        SpriteSize = 0b00100000,
        GenerateNMI = 0b10000000
    };
    //TODO : color emphasize
    enum Mask {
        ShowBackgroundLeft = 0b00000010,
        ShowSpritesLeft = 0b00000100,
        ShowBackground = 0b00001000,
        ShowSprites = 0b00010000
    };
    enum Status {
        SpriteOverflow = 0b00100000,
        SpriteZeroHit = 0b01000000,
        VerticalBlank = 0b10000000
    };
    enum SpriteAttributes {
        Palette = 0b00000011,
        Priority = 0b00100000,
        FlipHorizontally = 0b01000000,
        FlipVertically = 0b10000000
    };
    using Byte = uint8_t;
    using Word = uint16_t;
    using Pixel = uint32_t;
public:
    PPU(Cartridge &cartridge);
    void runCycles(int cycles);
    void powerup();
    void reset();
    void test();
    Byte readReg(int address);
    void writeReg(int address, Byte data);
    void writeOAM(Byte *mem);
    void switchPatternTable(int index);
    bool isNMI();
    bool checkFrameReady();
    void renderFromBuffer(Pixel *output_buffer);
    ~PPU() {}

private:
    static const int width = 256;
    static const int height = 240;
    Cartridge &cartridge;
    // render
    int scan_pos_x;
    int scan_pos_y;
    int render_pos_x;
    int render_pos_y;
    int render_row;
    int render_column;
    int eval_n;
    int eval_m;
    int eval_start;
    int sprite_found_count;
    int sprite_index;
    int eval_sprite_index;
    int sprite_x_offet;
    int sprite_y_offet;
    Word render_start_addr;
    Word background_pattern_addr;
    Byte background_color;
    Byte background_name;
    Byte background_attribute;
    Byte background_attribute_low;
    Byte background_attribute_high;
    Byte background_pattern;
    Byte background_pattern_low;
    Byte background_pattern_high;
    Byte prefetch_attribute;
    Byte prefetch_pattern_low;
    Byte prefetch_pattern_high;
    //background
    Word tile_pattern[2];
    Byte tile_attribute[2];
    //sprite
    Byte oam[256];
    Byte secondary_oam[32];
    Byte eval_sprite_low[8];
    Byte eval_sprite_high[8];
    Byte eval_sprite_attribute[8];
    Byte eval_sprite_x[8];
    Byte eval_sprite_y;
    Byte sprite_pattern;
    Byte sprite_pattern_low;
    Byte sprite_pattern_high;
    Byte sprite_color;
    Word sprite_pattern_addr;
    bool next_sprite_zero;
    bool current_sprite_zero;
    //register
    Byte reg[8];
    Byte io_latch;
    Word vram_addr;
    Word temp_vram_addr;
    Byte read_buffer;
    Byte fine_x_scroll;
    Word reg_pattern_low;
    Word reg_pattern_high;
    Byte reg_attribute;
    Byte reg_attribute_low;
    Byte reg_attribute_high;
    //vram
    Byte name_table[2048];
    Byte palette_indexes[32];
    Byte *pattern_table;
    Byte *chr_rom;
    int chr_size;
    Pixel palette[64] = { 0x6A6A6A, 0x00148F, 0x1E029B, 0x3F008A, 0x600060, 0x660017, 0x570D00, 0x3C1F00,
                          0x1B3300, 0x004200, 0x004500, 0x003C1F, 0x00315C, 0x000000, 0x000000, 0x000000,
                          0xB9B9B9, 0x0F4BD4, 0x412DEB, 0x6C1DD9, 0x9C17AB, 0xA71A4D, 0x993200, 0x7C4A00,
                          0x546400, 0x1A7800, 0x007F00, 0x00763E, 0x00678F, 0x010101, 0x000000, 0x000000, 
                          0xFFFFFF, 0x68A6FF, 0x8C9CFF, 0xB586FF, 0xD975FD, 0xE377B9, 0xE58D68, 0xD49D29, 
                          0xB3AF0C, 0x7BC211, 0x55CA47, 0x46CB81, 0x47C1C5, 0x4A4A4A, 0x000000, 0x000000, 
                          0xFFFFFF, 0xCCEAFF, 0xDDDEFF, 0xECDAFF, 0xF8D7FE, 0xFCD6F5, 0xFDDBCF, 0xF9E7B5,
                          0xF1F0AA, 0xDAFAA9, 0xC9FFBC, 0xC3FBD7, 0xC4F6F6, 0xBEBEBE, 0x000000, 0x000000};
    Pixel render_buffer[width * height];
    int clocks_after_reset;
    bool nmi;
    bool write_toggle;
    bool odd_frame;
    bool frame_ready;
    Mirror mirror_type;
    void incCoarseX();
    void incY();
    void prefetchReg();
    void loadShiftReg();
    void shiftReg();
    void accessLine();
    void renderPixel();
    void renderBackground();
    void renderSprites();
    void evalSprites();
    Byte readMem(Word address);
    void writeMem(Word address, Byte data);
    void nextPos();
    void sendNMI();  
};
#endif
