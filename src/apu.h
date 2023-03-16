#ifndef _APU_H
#define _APU_H
#include <cstdint>
class APU {
public:
    enum Reg {
        Status,
        FrameCounter
    };
    using Byte = uint8_t;
    using Word = uint16_t;
    using AudioFormat = uint16_t;
    APU();
    ~APU();
    void runCycles(int cycles);
    void setReadMem(Byte(*readMem)(Word));
    void writeReg(int address, Byte data);
    Byte readReg(int address);
    AudioFormat getOutput(int i);
    bool isIRQ();
private:
    struct Counter {
        Counter(){}
        Counter(Word counter) : counter(counter){}
        bool clock(Word reloaded_value);
        Word counter;
    };

    class EnvelopeGenerator {
    public:
        EnvelopeGenerator(){}
        Byte* parameter;
        Byte generate();
        void setStart();
    private:
        bool start_flag;
        Counter divider;
        Counter decay_level;
    };

    class SweepUnit {
    public:
        SweepUnit(){}
        Word* timer;
        Byte* parameter;
        bool is_pulse_1;
        void calcTargetPeriod();
        void clockHalf();
        void setReload();
        bool isMute();
    private:
        bool reload_flag;
        Word differ_period;
        Word target_period;
        Counter divider;
    };
    class LengthCounter {
    public:
        LengthCounter(){}
        bool is_triangle;
        Byte* parameter;
        void count();
        void setCounter(Byte value);
        void setEnable(bool enable);
        bool isZero();
    private:
        const Word length_table[32] = { 10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
                                 12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30 };
        bool is_enable;
        Word counter;
    };

    class Channel {
    public:
        LengthCounter length_counter;
        Byte getOutput();
    protected:
        Byte reg[4];
        Byte sequencer;
        Byte sequencer_index;
        Byte volume;
        Byte output;
        Counter timer;
        EnvelopeGenerator envelope_generator;
        SweepUnit sweep_unit;
    };

    class PulseChannel : public Channel {
    public:
        enum Reg {
            Volume,
            Sweep,
            Low,
            High,
        };
        PulseChannel();
        bool is_pulse_1;
        void clockCycle();
        void clockQuarter();
        void clockHalf();
        void setDutyCycle(Byte duty);
        void writeReg(int address, Byte data);
    private:
        const Byte duty_table[4] = { 0b10000000, 0b11000000, 0b11110000, 0b00111111 };
        Word timer_value;
        void stepSequencer();
        bool getSequencerNext();
    } pulse_channel_1, pulse_channel_2;

    class TriangleChannel : public Channel {
    public:
        enum Reg {
            Linear,
            Dummy,
            Low,
            High,
        };
        TriangleChannel();
        void clockCycle();
        void clockQuarter();
        void clockHalf();
        void writeReg(int address, Byte data);
    private:
        const Byte sequence[32] = { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
                              0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
        bool reload_flag;
        bool control_flag;
        Word reload_value;
        Word timer_value;
        Counter linear_counter;
        void stepSequencer();
    } triangle_channel;

    class NoiseChannel : public Channel {
    public:
        enum Reg {
            Volume,
            Dummy,
            Low,
            High,
        };
        NoiseChannel();
        void clockCycle();
        void clockQuarter();
        void clockHalf();
        void writeReg(int address, Byte data);
    private:
        const Word rate_table[16] = { 4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068 };
        Word shift_reg = 1;
        Word rate;
        bool mode_flag;
    } noise_channel;

    class DMC {
    public:
        enum Reg {
            Frequency,
            Raw,
            Start,
            Len,
        };
        void clockCycle();
        void clearInterupt();
        void clearBytesRemaing();
        void checkRestart();
        void writeReg(int address, Byte data);
        bool isInterupt();
        bool isRemaining();
        Byte getOutput();
        Byte (*readMem)(Word address);
    private:
        const Word rate_table[16] = { 428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54 };
        Word rate;
        Word current_address;
        Word bytes_remaining;
        Byte reg[4];
        Byte shift_register;
        Byte sample_buffer;
        Byte output;
        bool is_mute;
        bool is_buffer_clear;
        bool is_irq_enable;
        bool loop_flag;
        bool interupt_flag;
        Counter timer;
        Counter bits_remaining_counter;
        void fillBuffer();
    } dmc;

    AudioFormat output[16];
    AudioFormat prev_output;
    int output_size;
    Byte pulse1_output;
    Byte pulse2_output;
    Byte triangle_output;
    Byte noise_output;
    Byte dmc_output;
    Byte reg[2];
    Word frame_counter_timer;
    bool frame_interupt_flag;
    bool interupt_flag;
    bool is_apu_cycle;
    AudioFormat mixer();
    void clockFrameCounter();
    void clockQuarter();
    void clockHalf();
    void sendIRQ();
};
#endif
