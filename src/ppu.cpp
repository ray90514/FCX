#include "ppu.h"
#include <cstdio>
PPU::PPU(Cartridge &cartridge) : cartridge(cartridge){
    chr_rom = cartridge.getCHRRom();
    chr_size = cartridge.getCHRSize();
    mirror_type = (Mirror)cartridge.getMirrorType();
    pattern_table = chr_rom;
    powerup();
}

void PPU::test() {

}

void PPU::runCycles(int cycles) {
    for (int i = 0; i < cycles; i++, nextPos()) {
        render_pos_x = scan_pos_x - 1;
        render_pos_y = scan_pos_y;
        if ((scan_pos_y == 0 && scan_pos_x == 0) && 
            ((reg[Reg::Mask] & Mask::ShowSprites) || (reg[Reg::Mask] & Mask::ShowBackground)) && 
            (odd_frame)) {
            continue;
        }
        // visible scanline
        else if (scan_pos_y < 240) {
            accessLine();
            if (scan_pos_x > 0 && scan_pos_x < 257) {
                renderPixel();
            }
            // sprite evaluation  every visible scanline
            evalSprites();
        }
        else if (scan_pos_y == 241 && scan_pos_x == 1) {
            if (reg[Reg::Ctrl] & Ctrl::GenerateNMI) {
                sendNMI();
            }
            reg[Reg::Status] |= Status::VerticalBlank;
            frame_ready = true;
        }
        // pre-render scanline
        else if (scan_pos_y == 261) {
            accessLine();
            if (scan_pos_x == 0) {
                reg[Reg::OAMaddr] = 0;
            }
            else if (scan_pos_x == 1) {
                reg[Reg::Status] &= ~Status::VerticalBlank;
                reg[Reg::Status] &= ~Status::SpriteOverflow;
                reg[Reg::Status] &= ~Status::SpriteZeroHit;
            }
            else if (scan_pos_x > 279 && scan_pos_x < 305) {
                if ((reg[Reg::Mask] & Mask::ShowSprites) || (reg[Reg::Mask] & Mask::ShowBackground)) {
                    vram_addr = (vram_addr & ~0x7BE0) | (temp_vram_addr & 0x7BE0);
                }
            }
            else if (scan_pos_x == 340) {
                odd_frame = !odd_frame;
            }
        }
    }
}

void PPU::powerup() {
    write_toggle = false;
    odd_frame = false;
    reg[Reg::Ctrl] = 0;
    reg[Reg::Mask] = 0;
    reg[Reg::Status] = 0b10100000;
    reg[Reg::OAMaddr] = 0;
    reg[Reg::Scroll] = 0;
    reg[Reg::PPUaddr] = 0;
    read_buffer = 0;
    scan_pos_x = 0;
    scan_pos_y = 0;
}

void PPU::reset() {
    // TODO : write ignore
    write_toggle = false;
    odd_frame = false;
    reg[Reg::Ctrl] = 0;
    reg[Reg::Mask] = 0;
    reg[Reg::Scroll] = 0;
    reg[Reg::PPUaddr] = 0;
    read_buffer = 0;
    scan_pos_x = 0;
    scan_pos_y = 0;
    clocks_after_reset = 0;
}

void PPU::accessLine() {
    // idle
    if (scan_pos_x == 0) {
    }
    // visible clock
    else if (scan_pos_x < 257) {
        if (render_pos_x != 0) {
            shiftReg();
        }
        if (render_pos_x % 8 == 0 && render_pos_x != 0) {
            loadShiftReg();
        }
        prefetchReg();
        if (scan_pos_x % 8 == 0 && scan_pos_x != 0) {
            incCoarseX();
        }
        if (scan_pos_x == 256) {
            incY();
        }
    }
    else if (scan_pos_x == 257) {
        shiftReg();
        if ((reg[Reg::Mask] & Mask::ShowSprites) || (reg[Reg::Mask] & Mask::ShowBackground)) {
            vram_addr = (vram_addr & ~0x041F) | (temp_vram_addr & 0x041F);
        }
    }
    else if (scan_pos_x > 320 && scan_pos_x < 338) {
        shiftReg();
        if (scan_pos_x == 329 || scan_pos_x == 337) {
            loadShiftReg();
        }
        prefetchReg();
        if (scan_pos_x == 328 || scan_pos_x == 336) {
            incCoarseX();
        }
    }
}

void PPU::renderPixel() {
    renderBackground();
    renderSprites();
    //evaluation pixel
    if (background_pattern == 0 && sprite_pattern == 0) {
        render_buffer[render_pos_y * width + render_pos_x] = palette[palette_indexes[0]];
    }
    // sprite pixel non-transparent and foreground priority
    else if (sprite_pattern && (background_pattern == 0 || !(eval_sprite_attribute[sprite_index] & SpriteAttributes::Priority))) {
        render_buffer[render_pos_y * width + render_pos_x] = palette[sprite_color];
    }
    else {
        render_buffer[render_pos_y * width + render_pos_x] = palette[background_color];
    }
    // sprite 0 hit detect 
    if ((current_sprite_zero && sprite_index == 0) && 
        (background_pattern && sprite_pattern) && 
        render_pos_x != 255) {
        reg[Reg::Status] |= Status::SpriteZeroHit;
    }
}

void PPU::renderBackground() {
    background_pattern = 0;
    if (!(reg[Reg::Mask] & Mask::ShowBackground)) {
        return;
    }
    if (render_pos_x < 8 && !(reg[Reg::Mask] & Mask::ShowBackgroundLeft)) {
        return;
    }
    background_pattern_low = (reg_pattern_low >> (15 - fine_x_scroll)) & 0b01;
    background_pattern_high = ((reg_pattern_high >> (15 - fine_x_scroll)) << 1) & 0b10;
    background_attribute_low = (reg_attribute_low >> (7 - fine_x_scroll)) & 0b01;
    background_attribute_high = ((reg_attribute_high >> (7 - fine_x_scroll)) << 1) & 0b10;
    background_pattern = background_pattern_high | background_pattern_low;
    background_attribute = background_attribute_high | background_attribute_low;
    background_color = palette_indexes[background_attribute << 2 | background_pattern];        
}

void PPU::renderSprites() {
    sprite_pattern = 0;
    if (!(reg[Reg::Mask] & Mask::ShowSprites)) {
        return;
    }
    if (render_pos_x < 8 && !(reg[Reg::Mask] & Mask::ShowSpritesLeft)) {
        return;
    }
    if (render_pos_y == 0) {
        return;
    }
    for (sprite_index = 0; sprite_index < 8; sprite_index++) {
        // active
        sprite_x_offet = render_pos_x - (int)eval_sprite_x[sprite_index];
        if (sprite_x_offet >= 0 && sprite_x_offet < 8) {
            // flip horizontally
            if (eval_sprite_attribute[sprite_index] & SpriteAttributes::FlipHorizontally) {
                sprite_pattern_low = (eval_sprite_low[sprite_index] >> sprite_x_offet) & 0b01;
                sprite_pattern_high = ((eval_sprite_high[sprite_index] >> sprite_x_offet) << 1) & 0b10;
                sprite_pattern = sprite_pattern_high | sprite_pattern_low;
            }
            else {
                sprite_pattern_low = (eval_sprite_low[sprite_index] >> (7 - sprite_x_offet)) & 0b01;
                sprite_pattern_high = ((eval_sprite_high[sprite_index] >> (7 - sprite_x_offet)) << 1) & 0b10;
                sprite_pattern = sprite_pattern_high | sprite_pattern_low;
            }
            if (sprite_pattern) {
                sprite_color = palette_indexes[1 << 4 | (eval_sprite_attribute[sprite_index] & 0b11) << 2 | sprite_pattern];
                break;
            }
        }
    }
}

void PPU::evalSprites() {
    if (!(reg[Reg::Mask] & Mask::ShowSprites) && !(reg[Reg::Mask] & Mask::ShowBackground )) {
        return;
    }
    if (scan_pos_x == 1) {
        eval_n = 0;
        eval_m = 0;
        sprite_found_count = 0;
        eval_sprite_index = 0;
        current_sprite_zero = next_sprite_zero;
        next_sprite_zero = false;
    }
    // idle and new scanline
    if (scan_pos_x == 1) {

    }
    // clear secondary oam
    else if (scan_pos_x < 65) {
        //secondary_oam[(scan_pos_x - 1) / 2] = 0xFF;
    }
    // sprite evaluation
    else if (scan_pos_x < 257) {
        if (scan_pos_x == 65) {
            eval_start = reg[Reg::OAMaddr];
        }
        //TODO : improve accuracy
        // even cycle
        if (scan_pos_x % 2 == 0 && eval_n < 64) {
            //secondary oam remains
            if (sprite_found_count < 8) {
                // render y in sprite scope
                sprite_y_offet = scan_pos_y - (int)oam[4 * eval_n + eval_start];
                if (sprite_y_offet >= 0 && sprite_y_offet < ((reg[Reg::Ctrl] & Ctrl::SpriteSize) ? 16 : 8)) {
                    for (int i = 0; i < 4; i++) {
                        secondary_oam[4 * sprite_found_count + i] = oam[4 * eval_n + i + eval_start];
                    }
                    sprite_found_count++;
                    if (eval_n == 0) {
                        next_sprite_zero = true;
                    }
                }
                eval_n++;
            }
            else {
                eval_sprite_y = oam[4 * eval_n + eval_m + eval_start];
                sprite_y_offet = scan_pos_y - (int)oam[4 * eval_n + eval_start];
                if (sprite_y_offet >= 0 && sprite_y_offet < ((reg[Reg::Ctrl] & Ctrl::SpriteSize) ? 16 : 8)) {
                    reg[Reg::Status] |= Status::SpriteOverflow;
                    eval_n = 64;
                }
                else {
                    eval_n++;
                    eval_m++;
                }
            }
            // exceed  oam
            if (4 * eval_n + eval_m + eval_start > 255) {
                eval_n = 64;
            }
        }
    }
    // TODO : PPU address bus contents
    // TODO : For the first empty sprite slot, this will consist of sprite #63's Y-coordinate followed by 3 $FF bytes
    // sprite fetch
    else if (scan_pos_x < 321) {
        reg[Reg::OAMaddr] = 0;
        if ((scan_pos_x - 257) % 8 == 0) {
            eval_sprite_index = (scan_pos_x - 257) / 8;
            if (eval_sprite_index < sprite_found_count) {
                eval_sprite_y = secondary_oam[4 * eval_sprite_index];
                eval_sprite_attribute[eval_sprite_index] = secondary_oam[4 * eval_sprite_index + 2];
                eval_sprite_x[eval_sprite_index] = secondary_oam[4 * eval_sprite_index + 3];
                if (reg[Reg::Ctrl] & Ctrl::SpriteSize) {
                    sprite_pattern_addr = (Word)(secondary_oam[4 * eval_sprite_index + 1] & 0b1) << 12 | (Word)(secondary_oam[4 * eval_sprite_index + 1] & ~0b1) << 4;
                }
                else {
                    sprite_pattern_addr = (Word)(reg[Reg::Ctrl] & Ctrl::SpriteHalf) << 9 | (Word)secondary_oam[4 * eval_sprite_index + 1] << 4;
                }
            }
        }
        else if ((scan_pos_x - 257) % 8 == 4) {
            if (eval_sprite_index < sprite_found_count) {
                // flip vertically
                if (eval_sprite_attribute[eval_sprite_index] & SpriteAttributes::FlipVertically) {
                    sprite_pattern_addr += ((reg[Reg::Ctrl] & Ctrl::SpriteSize) ? 23 : 7) - (scan_pos_y - eval_sprite_y) - ((scan_pos_y - eval_sprite_y) & 0b1000);
                    eval_sprite_low[eval_sprite_index] = readMem(sprite_pattern_addr);
                    eval_sprite_high[eval_sprite_index] = readMem(sprite_pattern_addr | 0b1000);
                }
                else {
                    sprite_pattern_addr += (scan_pos_y - eval_sprite_y) + ((scan_pos_y - eval_sprite_y) & 0b1000);
                    eval_sprite_low[eval_sprite_index] = readMem(sprite_pattern_addr);
                    eval_sprite_high[eval_sprite_index] = readMem(sprite_pattern_addr | 0b1000);
                }
            }
            // Unused sprites are loaded with an all-transparent set of values
            else {
                eval_sprite_low[eval_sprite_index] = 0;
                eval_sprite_high[eval_sprite_index] = 0;
            }
        }
    }
    // background render pipeline initialization
    else {

    }
}

void PPU::prefetchReg() {
    // TODO : PPU address bus contents
    switch (render_pos_x % 8) {
        case 0:
        {
            background_name = readMem(0x2000 | (vram_addr & 0x0FFF));
            break;
        }
        case 2:
        {
            prefetch_attribute = readMem(0x23C0 | (vram_addr & 0x0C00) | ((vram_addr >> 4) & 0x38) | ((vram_addr >> 2) & 0x07));
            if (((vram_addr >> 5) & 0x1F) & 0b10) {
                prefetch_attribute >>= 4;
            }
            if ((vram_addr & 0x1F) & 0b10) {
                prefetch_attribute >>= 2;
            }
            prefetch_attribute &= 0b11;
            break;
        }
        case 4:
        {
            background_pattern_addr = (Word)(reg[Reg::Ctrl] & Ctrl::BackgroundHalf) << 8 | background_name << 4 | (vram_addr & 0x7000) >> 12;
            prefetch_pattern_low = readMem(background_pattern_addr);
            break;
        }
        case 6:
        {
            prefetch_pattern_high = readMem(background_pattern_addr | 0b1000);
            break;
        }
    }
}

void PPU::loadShiftReg() {
    reg_pattern_low |= prefetch_pattern_low;
    reg_pattern_high |= prefetch_pattern_high;
    reg_attribute = prefetch_attribute;
}

void PPU::shiftReg() {
    reg_pattern_low <<= 1;
    reg_pattern_high <<= 1;
    reg_attribute_low <<= 1;
    reg_attribute_high <<= 1;
    reg_attribute_low |= (reg_attribute & 0b01);
    reg_attribute_high |= reg_attribute >> 1;
}

void PPU::incCoarseX() {
    if ((reg[Reg::Mask] & Mask::ShowSprites) || (reg[Reg::Mask] & Mask::ShowBackground)) {
        if ((vram_addr & 0b11111) == 0b11111) {
            vram_addr &= ~0b11111;
            vram_addr ^= 0x0400;
        }
        else {
            vram_addr += 0b1;
        }
    }
}

void PPU::incY() {
    if ((reg[Reg::Mask] & Mask::ShowSprites) || (reg[Reg::Mask] & Mask::ShowBackground)) {
        if ((vram_addr & 0x7000) != 0x7000) {
            vram_addr += 0x1000;
        }
        else {
            vram_addr &= ~0x7000;
            Word y = (vram_addr & 0x03E0) >> 5;
            if (y == 29) {
                y = 0;
                vram_addr ^= 0x0800;
            }
            else if (y == 31) {
                y = 0;
            }
            else {
                y += 1;
            }
            vram_addr = (vram_addr & ~0x03E0) | (y << 5);
        }
    }
}

PPU::Byte PPU::readReg(int address) {
    switch (address) {
        // TODO : race condition
        case Reg::Status : {
            io_latch = reg[Reg::Status];
            reg[Reg::Status] &= ~Status::VerticalBlank;
            write_toggle = false;
            break;
        }
        case Reg::OAMdata : {
            io_latch = oam[reg[Reg::OAMaddr]];
            break;
        }
        case Reg::PPUdata : {
            io_latch = vram_addr < 0x3F00 ? read_buffer : readMem(vram_addr);
            read_buffer = readMem(vram_addr);
            vram_addr += ((reg[Reg::Ctrl] & Ctrl::VramAddrIncrement) ? 32 : 1);
            break;
        }
    }
    return io_latch;
}

void PPU::writeReg(int address, Byte data) {
    switch (address) {
        case Reg::Ctrl : {
            if ((reg[Reg::Status] & Status::VerticalBlank) && !(reg[Reg::Ctrl] & Ctrl::GenerateNMI) && (data & Ctrl::GenerateNMI)) {
                sendNMI();
            }
            temp_vram_addr = (temp_vram_addr & ~0x0C00) | (((Word)data & 0b11) << 10);
            reg[Reg::Ctrl] = data;
            break;
        }
        case Reg::Mask : {
            reg[Reg::Mask] = data;
            break;
        }
        case Reg::OAMaddr : {
            reg[Reg::OAMaddr] = data;
            break;
        }
        case Reg::OAMdata : {
            if ((scan_pos_y > 239 && scan_pos_y < 261) || (!(reg[Reg::Mask] & Mask::ShowSprites) && !(reg[Reg::Mask] & Mask::ShowBackground))) {
                oam[reg[Reg::OAMaddr]++] = data;
            }
            else {
                reg[Reg::OAMaddr] += 4;
            }
            break;
        }
        case Reg::Scroll : {
            if (!write_toggle) {
                temp_vram_addr = (temp_vram_addr & ~0b11111) | ((Word)(data & 0b11111000) >> 3);
                fine_x_scroll = data & 0b111;
                write_toggle = true;
            }
            else {
                temp_vram_addr = (temp_vram_addr & ~0x73E0) | (((Word)data & 0b111) << 12) | (((Word)data & 0b11111000) << 2);
                write_toggle = false;
            }
            break;
        }
        case Reg::PPUaddr : {
            if (!write_toggle) {
                temp_vram_addr = (temp_vram_addr & ~0x7F00) | (((Word)data & 0b111111) << 8);
                write_toggle = true;
            }
            else {
                temp_vram_addr = (temp_vram_addr & ~0xFF) | (Word)data;
                vram_addr = temp_vram_addr;
                write_toggle = false;
            }
            break;
        }
        case Reg::PPUdata : {
            writeMem(vram_addr, data);
            vram_addr += ((reg[Reg::Ctrl] & Ctrl::VramAddrIncrement) ? 32 : 1);
            break;
        }
    }
    io_latch = data;
    reg[Reg::Status] = (reg[Reg::Status] & ~0b11111) | (data & 0b11111);
}

void PPU::writeOAM(Byte * mem) {
    for (int i = 0; i < 256; i++) {
        oam[(reg[Reg::OAMaddr] + i) % 256] = mem[i];
    }
}

PPU::Byte PPU::readMem(Word address) {
    if (address < 0x2000) {
        return pattern_table[address];
    }
    else if (address < 0x3F00) {
        if (mirror_type == Mirror::Horizantal) {
            if (address < 0x2800) {
                return name_table[(address - 0x2000) % 0x400];
            }
            else {
                return name_table[0x400 + ((address - 0x2400) % 0x400)];
            }
        }
        else if (mirror_type == Mirror::Vertical) {
            if (address < 0x2800) {
                return name_table[(address - 0x2000) % 0x800];
            }
            else {
                return name_table[(address - 0x2800) % 0x800];
            }
        }
    }
    else if (address < 0x4000) {
        if (address % 4 == 0) {
            return palette_indexes[((address - 0x3F00) % 0x20) & 0b1111 ];
        }
        else {
            return palette_indexes[(address - 0x3F00) % 0x20];
        }
    }
    return 0;
}

void PPU::writeMem(Word address, Byte data) {
    if (address < 0x2000) {
        if (cartridge.isCHRRam()) {
            pattern_table[address] = data;
        }
    }
    else if (address < 0x3F00) {
        if (mirror_type == Mirror::Horizantal) {
            if (address < 0x2800) {
                name_table[(address - 0x2000) % 0x400] = data;
            }
            else {
                name_table[0x400 + ((address - 0x2400) % 0x400)] = data;
            }
        }
        else if (mirror_type == Mirror::Vertical) {
            if (address < 0x2800) {
                name_table[(address - 0x2000) % 0x800] = data;
            }
            else {
                name_table[(address - 0x2800) % 0x800] = data;
            }
        }
    }
    else if (address < 0x4000) {
        if (address % 4 == 0) {
            palette_indexes[((address - 0x3F00) & 0b1111) % 0x20] = data;
        }
        else {
            palette_indexes[(address - 0x3F00) % 0x20] = data;
        }
    }
    else {
    }
}

void PPU::nextPos() {
    scan_pos_x++;
    if (scan_pos_x == 341) {
        scan_pos_x = 0;
        scan_pos_y++;
        if (scan_pos_y == 262) {
            scan_pos_y = 0;
        }
    }
}

bool PPU::isNMI() {
    if (nmi) {
        nmi = false;
        return true;
    }
    return nmi;
}

void PPU::sendNMI() {
    nmi = true;
}

bool PPU::checkFrameReady() {
    if (frame_ready) {
        frame_ready = false;
        return true;
    }
    return frame_ready;
}

void PPU::renderFromBuffer(Pixel * output_buffer) {
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            output_buffer[i * width + j] = render_buffer[i * width + j];
        }
    }
}

void PPU::switchPatternTable(int index) {
    pattern_table = chr_rom + 0x2000 * (index & 0b11);
}