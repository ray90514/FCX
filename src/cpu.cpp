#define _CRT_SECURE_NO_WARNINGS
#include <cpu.h>
#include <cstdio>

CPU::CPU(Bus &bus) : mem(bus), bus(bus) {
    powerup();
}

void CPU::test() {

}

void CPU::powerup() {
    setByteToStatus(0x34);
    reg_a = 0;
    reg_x = 0;
    reg_y = 0;
    sp = 0xFD;
    pc = mem.readWord(0xFFFC);
    // TODO :
    // $4017 = $00(frame irq enabled)
    // $4015 = $00(all channels disabled)
    // $4000 - $400F = $00
    // $4010 - $4013 = $00
}

void CPU::reset() {
    sp -= 3;
    interrupt_disable = true;
    pc = mem.readWord(0xFFFC);
    mem.writeByte(0x4015, 0);
    // TODO :
    // APU mode in $4017 was unchanged
    // APU was silenced($4015 = 0)
    // APU triangle phase is reset to 0 (i.e.outputs a value of 15, the first step of its waveform)
    // APU DPCM output ANDed with 1 (upper 6 bits cleared)
}

int CPU::run() {
    Byte opcode = 0;
    int count = 0;

    if (bus.isNMI()) {
        interrupt(false);
        pc = mem.readWord(0xFFFA);
    }
    else if (!interrupt_disable && bus.isIRQ()) {
        interrupt(false);
        pc = mem.readWord(0xFFFE);
    }
    is_pc_changed = false;
    is_absolute_index = false;
    is_indirect_y = false;
    operation_cycles = 0;
    opcode = mem.readByte(pc);
    //printf("%4d | PC:%04X %02X A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%ld\n", count, program_counter, opcode, reg_a, reg_x, reg_y, getStatusToByte(false), stack_pointer, total_cycles);
    decode(opcode);
    if (!is_pc_changed) {
        pc++;
    }
    return operation_cycles;
}

void CPU::decode(Byte opcode) {
    Word address = 0;
    Byte value = 0;
    bool is_accumulator = false;
    bool is_immediate = false;
    if ((opcode & 0b1111) == 0b1000) {
        switch (opcode >> 4) {
            case 0x0:
            { 
                PHP();
                break;
            }
            case 0x2:
            {
                PLP();
                break;
            }
            case 0x4:
            {
                PHA();
                break;
            }
            case 0x6:
            {
                PLA();
                break;
            }
            case 0x1:
            {
                clearStatusBit(&carry);
                break;
            }
            case 0x3:
            {
                setStatusBit(&carry);
                break;
            }
            case 0x5:
            {
                clearStatusBit(&interrupt_disable);
                break;
            }
            case 0x7:
            {
                setStatusBit(&interrupt_disable);
                break;
            }
            case 0xB:
            {
                clearStatusBit(&overflow);
                break;
            }
            case 0xD:
            {
                clearStatusBit(&decimal);
                break;
            }
            case 0xF:
            {
                setStatusBit(&decimal);
                break;
            }
            case 0x8:
            {
                decReg(&reg_y);
                break;
            }
            case 0xC:
            {
                incReg(&reg_y);
                break;
            }
            case 0xE:
            {
                incReg(&reg_x);
                break;
            }
            case 0xA:
            {
                transfer(reg_a, &reg_y);
                break;
            }
            case 0x9:
            {
                transfer(reg_y, &reg_a);
                break;
            }
        }
    }
    else if ((opcode & 0b1111) == 0b1010 && opcode > 0x80) {
        switch (opcode >> 4) {
            case 0x8:
            {
                transfer(reg_x, &reg_a);
                break;
            }
            case 0x9:
            { 
                transfer(reg_x, &sp);
                break;
            }
            case 0xA:
            {
                transfer(reg_a, &reg_x);
                break;
            }
            case 0xB:
            {
                transfer(sp, &reg_x);
                break;
            }
            case 0xC:
            { 
                decReg(&reg_x);
                break;
            }
            case 0xE:
            {
                NOP();
                break;
            }
        }
    }
    else if ((opcode & 0b11111) == 0b00000 && opcode < 0x80) {
        is_pc_changed = true;
        switch (opcode >> 4) {
            case 0x0:
            {
                BRK();
                break;
            }
            case 0x2:
            {
                JSR(getAbsolute(nullptr));
                break;
            }
            case 0x4:
            {
                RTI();
                break;
            }
            case 0x6:
            {
                RTS();
                break;
            }
        }
    }
    else if ((opcode & 0b11111) == 0b10000) {
        bool condition;
        // sign extended
        pc++;
        value = mem.readByte(pc++);
        address = ((value & 0x80) ? 0xFF00 : 0x0000) | value;
        switch ((opcode >> 6) & 0b11) {
            case 0b00:
            {
                condition = negative;
                break;
            }
            case 0b01:
            {
                condition = overflow;
                break;
            }
            case 0b10:
            {
                condition = carry;
                break;
            }
            case 0b11:
            {
                condition = zero;
                break;
            }
        }
        if ((opcode & 0x20) == 0) {
            condition = !condition;
        }
        branch(address, condition);
        is_pc_changed = true;
    }
    else if ((opcode & 0b11) == 0b01) {
        switch ((opcode >> 2) & 0b111) {
            //(zero page,X)
            case 0b000:
            {
                address = getXIndirect();
                break;
            }
            // zero page
            case 0b001:
            {
                address = getZeroPage(nullptr);
                break;
            }
            //#immediate, except for STA
            case 0b010:
            {
                is_immediate = true;
                break;
            }
            // absolute
            case 0b011:
            {
                address = getAbsolute(nullptr);
                break;
            }
            //(zero page),Y
            case 0b100:
            {
                address = getIndirectY();
                break;
            }
            // zero page,X
            case 0b101:
            {
                address = getZeroPage(&reg_x);
                break;
            }
            // absolute,Y
            case 0b110:
            {
                address = getAbsolute(&reg_y);
                break;
            }
            // absolute,X
            case 0b111:
            {
                address = getAbsolute(&reg_x);
                break;
            }
        }
        if (is_immediate) {
            value = getImmediate();
        } 
        else if (((opcode >> 5) & 0b111) != 0b100) {
            value = mem.readByte(address);
        }
        switch ((opcode >> 5) & 0b111) {
            case 0b000:
            {
                ORA(value);
                break;
            }
            case 0b001:
            {
                AND(value);
                break;
            }
            case 0b010:
            {
                EOR(value);
                break;
            }
            case 0b011:
            {
                ADC(value);
                break;
            }
            // STA
            case 0b100:
            {
                store(address, reg_a);
                if (is_absolute_index) {
                    operation_cycles = 5;
                }
                else if (is_indirect_y) {
                    operation_cycles = 6;
                }
                break;
            }
            // LDA
            case 0b101:
            {
                load(value, &reg_a);
                break;
            }
            // CMP
            case 0b110:
            {
                compare(value, reg_a);
                break;
            }
            case 0b111:
            {
                SBC(value);
                break;
            }
        }
    }
    else if ((opcode & 0b11) == 0b10) {
        switch ((opcode >> 2) & 0b111) {
            // immediate
            case 0b000:
            {
                is_immediate = true;
                break;
            }
            // zero page
            case 0b001:
            {
                address = getZeroPage(nullptr);
                break;
            }
            // accumulator
            case 0b010:
            {
                is_accumulator = true;
                address = 0;
                break;
            }
            // absolute
            case 0b011:
            {
                address = getAbsolute(nullptr);
                break;
            }
            // zero page,X
            case 0b101:
            {
                // zero page,Y for LDX,STX
                if (((opcode >> 5) & 0b111) == 0b100 ||
                    ((opcode >> 5) & 0b111) == 0b101) {
                    address = getZeroPage(&reg_y);
                }
                else {
                    address = getZeroPage(&reg_x);
                }
                break;
            }
            // absolute,X except for STX
            case 0b111:
            {
                // absolute,Y for LDX
                if (((opcode >> 5) & 0b111) == 0b101) {
                    address = getAbsolute(&reg_y);
                }
                else {
                    address = getAbsolute(&reg_x);
                }
                break;
            }
        }
        switch ((opcode >> 5) & 0b111) {
            case 0b000:
            {
                ASL(address, is_accumulator);
                break;
            }
            case 0b001:
            {
                ROL(address, is_accumulator);
                break;
            }
            case 0b010:
            {
                LSR(address, is_accumulator);
                break;
            }
            case 0b011:
            {
                ROR(address, is_accumulator);
                break;
            }
            // STX
            case 0b100:
            {
                store(address, reg_x);
                break;
            }
            // LDX
            case 0b101:
            {
                if (is_immediate) {
                    value = getImmediate();
                }
                else {
                    value = mem.readByte(address);
                }
                load(value, &reg_x);
                break;
            }
            case 0b110:
            {
                DEC(address);
                break;
               
            } 
            case 0b111:
            {
                INC(address);
                break;
            }
            default:
                break;
        }
    }
    else if ((opcode & 0b11) == 0b00) {
        switch ((opcode >> 2) & 0b111) {
            // immediate
            case 0b000: 
            {
                is_immediate = true;
                break;
            }
            // zero page
            case 0b001:
            {
                address = getZeroPage(nullptr);
                break;
            }
            // absolute
            case 0b011:
            {
                address = getAbsolute(nullptr);
                break;
            }
            // zero page,X
            case 0b101:
            {
                address = getZeroPage(&reg_x);
                break;
            }
            // absolute,X
            case 0b111:
            {
                address = getAbsolute(&reg_x);
                break;
            }
        }
        switch ((opcode >> 5) & 0b111) {
            // BIT
            case 0b001:
            {
                BIT(address);
                break;
            }
            // JMP
            case 0b010:
            {
                is_pc_changed = true;
                JMP(address);
                break;
            }
            // JMP(abs)
            case 0b011:
            {
                is_pc_changed = true;
                JMPI(address);
                break;
            }
            // STY
            case 0b100:
            {
                store(address, reg_y);
                break;
            }
            // LDY
            case 0b101:
            {
                if (is_immediate) {
                    value = getImmediate();
                }
                else {
                    value = mem.readByte(address);
                }
                load(value, &reg_y);
                break;
            }
            // CPY
            case 0b110:
            {
                if (is_immediate) {
                    value = getImmediate();
                }
                else {
                    value = mem.readByte(address);
                }
                compare(value, reg_y);
                break;
            }
            // CPX
            case 0b111:
            {
                if (is_immediate) {
                    value = getImmediate();
                }
                else {
                    value = mem.readByte(address);
                }
                compare(value, reg_x);
                break;
            }
        }
    }
}

void CPU::pushByte(Byte value) {
    mem.writeByte(0x0100 | sp--, value);
}

CPU::Byte CPU::pullByte() {
    return mem.readByte(0x0100 | ++sp);
}

void CPU::pushWord(Word value) {
    mem.writeWord(0x0100 | --sp, value);
    sp--;
}

CPU::Word CPU::pullWord() {
    Word value = mem.readWord(0x0100 | ++sp);
    sp++;
    return value;
}

void CPU::setByteToStatus(Byte value) {
    carry = value & 0b00000001;
    zero = value & 0b00000010;
    interrupt_disable = value & 0b00000100;
    decimal = value & 0b00001000;
    overflow = value & 0b01000000;
    negative = value & 0b10000000;
}

CPU::Byte CPU::getStatusToByte(bool is_instruction_interupt) {
    return (carry << 0) | (zero << 1) | (interrupt_disable << 2) |
        (decimal << 3) | (is_instruction_interupt << 4) | (1 << 5) |
        (overflow << 6) | (negative << 7);
}

CPU::Word CPU::getZeroPage(Byte *reg) {
    Word data = mem.readByte(++pc) + (reg ? *reg : 0);
    operation_cycles = reg ? 4 : 3;
    return data & 0xFF;
}

CPU::Word CPU::getAbsolute(Byte *reg) {
    pc++;
    Word data = mem.readWord(pc++);
    Word value = data + (reg ? *reg : 0);
    operation_cycles = 4 +  (((value ^ data) & 0xFF00) ? 1 : 0);
    is_absolute_index = reg;
    return value;
}

CPU::Word CPU::getXIndirect() {
    Byte data = mem.readByte(++pc);
    operation_cycles = 6;
    return mem.readByte((data + reg_x) & 0x00FF) +
        (mem.readByte((data + reg_x + 1) & 0x00FF) << 8);
}

CPU::Word CPU::getIndirectY() {
    Byte address = mem.readByte(++pc);
    Word data = mem.readByte(address) + (mem.readByte((address + 1) & 0xFF) << 8);
    Word value = data + reg_y;
    operation_cycles = 5 + (((value ^ data) & 0xFF00) ? 1 : 0);
    is_indirect_y = true;
    return value;
}
CPU::Byte CPU::getImmediate() {
    operation_cycles = 2;
    return mem.readByte(++pc);
}

void CPU::ADC(Byte value) {
    Word sum = reg_a + value + carry;
    carry = sum > 0xFF;
    overflow = (~(reg_a ^ value)) & (reg_a ^ (Byte)sum) & 0x80;
    reg_a = (Byte)sum;
    negative = reg_a & 0x80;
    zero = reg_a == 0;
}

void CPU::AND(Byte value) {
    reg_a &= value;
    negative = reg_a & 0x80;
    zero = reg_a == 0;
}

void CPU::ASL(Word address, bool is_accumulator) {
    Byte value = is_accumulator ? reg_a : mem.readByte(address);
    carry = value & 0x80;
    value <<= 1;
    negative = value & 0x80;
    zero = value == 0;
    if (is_accumulator) {
        reg_a = value;
    }
    else {
        mem.writeByte(address, value);
    }
    if (is_absolute_index) {
        operation_cycles = 5;
    }
    operation_cycles += 2;
}

void CPU::branch(Word value, bool condition) {
    operation_cycles = 2;
    if (condition) {
        operation_cycles += (((pc ^ (pc + value)) & 0xFF00) ? 1 : 0);
        pc += value;
    }
}

void CPU::BIT(Word address) {
    Byte value = mem.readByte(address);
    negative = value & 0x80;
    zero = (value & reg_a) == 0;
    overflow = value & 0x40;
}

void CPU::BRK() {
    pc += 2;
    interrupt(true);
    pc = mem.readWord(0xFFFE);
    operation_cycles = 7;
}

void CPU::clearStatusBit(bool *status) {
    *status = false;
    operation_cycles = 2;
}

void CPU::compare(Byte value, Byte reg) {
    Byte result = reg - value;
    negative = result & 0x80;
    zero = result == 0;
    carry = reg >= value;
}

void CPU::DEC(Word address) {
    Byte value = mem.readByte(address);
    --value;
    negative = value & 0x80;
    zero = value == 0;
    mem.writeByte(address, value);
    if (is_absolute_index) {
        operation_cycles = 5;
    }
    operation_cycles += 2;
}

void CPU::decReg(Byte *reg) {
    --(*reg);
    negative = (*reg) & 0x80;
    zero = (*reg) == 0;
    operation_cycles = 2;
}

void CPU::EOR(Byte value) {
    reg_a ^= value;
    negative = reg_a & 0x80;
    zero = reg_a == 0;
}

void CPU::INC(Word address) {
    Byte value = mem.readByte(address);
    value++;
    negative = value & 0x80;
    zero = value == 0;
    mem.writeByte(address, value);
    if (is_absolute_index) {
        operation_cycles = 5;
    }
    operation_cycles += 2;
}

void CPU::incReg(Byte *reg) {
    (*reg)++;
    negative = (*reg) & 0x80;
    zero = (*reg) == 0;
    operation_cycles = 2;
}

void CPU::JMP(Word value) {
    pc = value;
    operation_cycles = 3;
}

void CPU::JMPI(Word address) {
    pc =
        (mem.readByte((address & 0xFF00) | ((address + 1) & 0x00FF)) << 8) |
        mem.readByte(address);
    operation_cycles = 5;
}

void CPU::JSR(Word address) {
    pushWord(pc);
    pc = address;
    operation_cycles = 6;
}

void CPU::load(Byte value, Byte *reg) {
    *reg = value;
    negative = value & 0x80;
    zero = value == 0;
}

void CPU::LSR(Word address, bool is_accumulator) {
    Byte value = is_accumulator ? reg_a : mem.readByte(address);
    carry = value & 0x01;
    value = (value >> 1) & ~0x80;
    negative = value & 0x80;
    zero = value == 0;
    if (is_accumulator) {
        reg_a = value;
    }
    else {
        mem.writeByte(address, value);
    }
    if (is_absolute_index) {
        operation_cycles = 5;
    }
    operation_cycles += 2;
}

void CPU::NOP() {
    operation_cycles = 2;
}

void CPU::ORA(Byte value) {
    reg_a |= value;
    negative = reg_a & 0x80;
    zero = reg_a == 0;
}

void CPU::PHA() {
    pushByte(reg_a);
    operation_cycles = 3;
}

void CPU::PHP() {
    pushByte(getStatusToByte(true));
    operation_cycles = 3;
}

void CPU::PLA() {
    reg_a = pullByte();
    negative = reg_a & 0x80;
    zero = reg_a == 0;
    operation_cycles = 4;
}

void CPU::PLP() {
    setByteToStatus(pullByte());
    operation_cycles = 4;
}

void CPU::ROL(Word address, bool is_accumulator) {
    Byte value = is_accumulator ? reg_a : mem.readByte(address);
    Byte mask = carry;
    carry = value & 0x80;
    value = (value << 1) | (Byte)mask;
    negative = value & 0x80;
    zero = value == 0;
    if (is_accumulator) {
        reg_a = value;
    }
    else {
        mem.writeByte(address, value);
    }
    if (is_absolute_index) {
        operation_cycles = 5;
    }
    operation_cycles += 2;
}

void CPU::ROR(Word address, bool is_accumulator) {
    Byte value = is_accumulator ? reg_a : mem.readByte(address);
    Byte mask = carry << 7;
    carry = value & 0x01;
    value = (value >> 1) | mask;
    negative = value & 0x80;
    zero = value == 0;
    if (is_accumulator) {
        reg_a = value;
    }
    else {
        mem.writeByte(address, value);
    }
    if (is_absolute_index) {
        operation_cycles = 5;
    }
    operation_cycles += 2;
}

void CPU::RTI() {
    setByteToStatus(pullByte());
    pc = pullWord();
    operation_cycles = 6;
}

void CPU::RTS() {
    pc = pullWord() + 1;
    operation_cycles = 6;
}

void CPU::SBC(Byte value) {
    ADC(~value);
}

void CPU::setStatusBit(bool *status) {
    *status = true;
    operation_cycles = 2;
}

void CPU::store(Word address, Byte reg) {
    mem.writeByte(address, reg);
}

void CPU::transfer(Byte src, Byte *dst) {
    *dst = src;
    if (dst != &sp) {
        negative = src & 0x80;
        zero = src == 0;
    }
    operation_cycles = 2;
}

void CPU::interrupt(bool is_instruction_interupt) {
    pushWord(pc);
    pushByte(getStatusToByte(is_instruction_interupt));
    interrupt_disable = true;
    operation_cycles = 7;
}

CPU::~CPU() {
}
