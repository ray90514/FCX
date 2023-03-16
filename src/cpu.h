#ifndef _CPU_H
#define _CPU_H
#include <cstdint>
#include "bus.h"
#include "ppu.h"
#include "cartridge.h"
class CPU {
public:
    using Byte = uint8_t;
    using Word = uint16_t;
    CPU(Bus &bus);
    ~CPU();
    int run();
    void decode(Byte opcode);
    void powerup();
    void reset();
    void test();
private:
    Bus &bus;
    Bus &mem;
    long long total_cycles;
    int operation_cycles;
    bool irq;
    bool is_pc_changed;
    bool is_absolute_index;
    bool is_indirect_y;
    //register
    Byte reg_a; 
    Byte reg_x; 
    Byte reg_y;
    Byte sp; 
    Word pc;
    //status register
    bool carry;
    bool zero;
    bool interrupt_disable;
    bool decimal;
    bool overflow;
    bool negative;
    //stack operation
    void pushByte(Byte value);
    Byte pullByte();
    void pushWord(Word value);
    Word pullWord();
    void setByteToStatus(Byte value);
    Byte getStatusToByte(bool isInstruction);
    //addressing
    Word getZeroPage(Byte *reg);
    Word getAbsolute(Byte *reg);
    Word getXIndirect();
    Word getIndirectY();
    Byte getImmediate();
    //operatoin
    void ADC(Byte value);
    void AND(Byte value);
    void ASL(Word address, bool is_accumulator);
    void branch(Word value, bool condition);
    void BIT(Word address);
    void BRK();
    void clearStatusBit(bool *status);
    void compare(Byte value, Byte reg);
    void DEC(Word address);
    void decReg(Byte *reg);
    void EOR(Byte value);
    void INC(Word address);
    void incReg(Byte *reg);
    void JMP(Word value);
    void JMPI(Word address);
    void JSR(Word address);
    void load(Byte value, Byte *reg);
    void LSR(Word address, bool is_accumulator);
    void NOP();
    void ORA(Byte value);
    void PHA();
    void PHP();
    void PLA();
    void PLP();
    void ROL(Word address, bool is_accumulator);
    void ROR(Word address, bool is_accumulator);
    void RTI();
    void RTS();
    void SBC(Byte value);
    void setStatusBit(bool *status);
    void store(Word address, Byte reg);
    void transfer(Byte src, Byte *dst);
    void interrupt(bool is_instruction_interupt);
};
#endif