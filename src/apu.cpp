#include "apu.h"
#include <cstdio>
APU::APU() {
    pulse_channel_1.is_pulse_1 = true;
    pulse_channel_2.is_pulse_1 = false;
    is_apu_cycle = false;
    //dmc.memRead = memRead;
}

APU::~APU() {
}

void APU::runCycles(int cpu_cycles) {
    prev_output = output[output_size - 1];
    output_size = cpu_cycles;
    for (int i = 0; i < cpu_cycles; i++, is_apu_cycle = !is_apu_cycle) {
        clockFrameCounter();
        triangle_channel.clockCycle();
        // clock triangle channel's timer 
        if (is_apu_cycle) {
            pulse_channel_1.clockCycle();
            pulse_channel_2.clockCycle();
            noise_channel.clockCycle();
            //dmc.clockCycle();
        }
        if (frame_interupt_flag || dmc.isInterupt()) {
            sendIRQ();
        }
        bool test = false;
        pulse1_output = test ? 0 : pulse_channel_1.getOutput();
        pulse2_output = test ? 0 :pulse_channel_2.getOutput();
        triangle_output = test ? 0 : triangle_channel.getOutput();
        noise_output = noise_channel.getOutput();
        dmc_output = 0;//dmc.getOutput();
        output[i] = mixer();//(pulse1_output * 0x7FFF / 15);
    }
}

void APU::setReadMem(Byte(*memRead)(Word)) {
    dmc.readMem = memRead;
}

void APU::clockFrameCounter() {
    // Sequencer mode: 0 selects 4-step sequence, 1 selects 5-step sequence
    switch (frame_counter_timer++) {
        case 7457:
        {
            clockQuarter();
            break;
        }
        case 14913:
        {
            clockHalf();
            break;
        }
        case 22371:
        {
            clockQuarter();
            break;
        }
        case 29828:
        {
            if (!(reg[Reg::FrameCounter] & 0b10000000)) {
                frame_interupt_flag = true;
            }
            break;
        }
        case 29829:
        {
            if (!(reg[Reg::FrameCounter] & 0b10000000)) {
                clockHalf();
                frame_interupt_flag = true;
            }
            break;
        }
        case 29830:
        {
            if (!(reg[Reg::FrameCounter] & 0b10000000)) {
                frame_interupt_flag = true;
            }
            frame_counter_timer = 0;
            break;
        }
        case 37281:
        {
            clockHalf();
            break;
        }
        case 37282:
        {
            frame_counter_timer = 0;
            break;
        }
    }
    if (reg[Reg::FrameCounter] & 0b01000000) {
        frame_interupt_flag = false;
    }
}

void APU::writeReg(int address, Byte data) {
    switch (address / 4) {
        case 0:
        {
            pulse_channel_1.writeReg(address % 4, data);
            break;
        }
        case 1:
        {
            pulse_channel_2.writeReg(address % 4, data);
            break;
        }
        case 2:
        {
            triangle_channel.writeReg(address % 4, data);
            break;
        }
        case 3:
        {
            noise_channel.writeReg(address % 4, data);
            break;
        }
        case 4:
        {
            dmc.writeReg(address % 4, data);
            break;
        }
        case 5:
        {
            if (address == 0x15) {
                pulse_channel_1.length_counter.setEnable(data & 0b0001);
                pulse_channel_2.length_counter.setEnable(data & 0b0010);
                triangle_channel.length_counter.setEnable(data & 0b0100);
                noise_channel.length_counter.setEnable(data & 0b1000);
                if ((data & 0b10000)) {
                    dmc.checkRestart();
                }
                else {
                    dmc.clearBytesRemaing();
                }
                dmc.clearInterupt();
                reg[Reg::Status] = data;
            }
            else if (address == 0x17) {
                // After 3 or 4 CPU clock cycles*, the timer is reset.
                frame_counter_timer = 0;
                // If the mode flag is set, then both "quarter frame" and "half frame" signals are also generated.
                if (data & 0b10000000) {
                    //clockQuarter();
                    clockHalf();
                }
                reg[Reg::FrameCounter] = data;
            }
            break;
        }
    }
}

APU::Byte APU::readReg(int address) {
    if (address == 0x15) {
        Byte value = 0;
        value |= !pulse_channel_1.length_counter.isZero();
        value |= (!pulse_channel_2.length_counter.isZero()) << 1;
        value |= (!triangle_channel.length_counter.isZero()) << 2;
        value |= (!noise_channel.length_counter.isZero()) << 3;
        value |= dmc.isRemaining() << 4;
        value |= frame_interupt_flag << 6;
        value |= dmc.isInterupt() << 7;
        frame_interupt_flag = false;
        return value;
    }
    else {
        return 0;
    }
}

APU::AudioFormat APU::getOutput(int i) {
    return output[i];
    //return i == -1 ? prev_output : output[i];
}

bool APU::isIRQ() {
    if (interupt_flag) {
        interupt_flag = false;
        return true;
    }
    return interupt_flag;
}

APU::AudioFormat APU::mixer() {
    float tnd_out = 0.00851f * triangle_output + 0.00494f * noise_output + 0.00335f * dmc_output;
    float pulse_out = 0.00752f * (pulse1_output + pulse2_output);
    return (AudioFormat)(0xFFFF * (pulse_out + tnd_out));
}

void APU::clockQuarter() {
    pulse_channel_1.clockQuarter();
    pulse_channel_2.clockQuarter();
    triangle_channel.clockQuarter();
    noise_channel.clockQuarter();
}

void APU::clockHalf() {
    pulse_channel_1.clockHalf();
    pulse_channel_2.clockHalf();
    triangle_channel.clockHalf();
    noise_channel.clockHalf();
}

void APU::sendIRQ() {
    interupt_flag = true;
}

bool APU::Counter::clock(Word reloaded_value) {
    if (counter) {
        counter--;
        return false;
    }
    else {
        counter = reloaded_value;
        return true;
    }
}

APU::Byte APU::EnvelopeGenerator::generate() {
    // parameter : DDLC VVVV Duty (D), envelope loop / length counter halt (L), constant volume (C), volume/envelope (V)
    if (start_flag) {
        start_flag = false;
        decay_level.counter = 15;
        divider.counter = (*parameter) & 0b1111;
    }
    else {
        if (divider.clock((*parameter) & 0b1111)) {
            if ((*parameter) & 0b00100000) {
                decay_level.clock(15);
            }
            else {
                decay_level.clock(0);
            }
        }
    }
    return ((*parameter) & 0b00010000) ? ((*parameter) & 0b1111) : decay_level.counter;
}

void APU::EnvelopeGenerator::setStart() {
    start_flag = true;
}

void APU::SweepUnit::calcTargetPeriod() {
    // calc differ_period
    if ((*parameter) & 0b1000) {
        if (is_pulse_1) {
            differ_period = ~((*timer) >> ((*parameter) & 0b111));
        }
        else {
            differ_period = ~((*timer) >> ((*parameter) & 0b111)) + 1;
        }
    }
    else {
        differ_period = (*timer) >> ((*parameter) & 0b111);
    }
    target_period = (*timer) + differ_period;
}

void APU::SweepUnit::clockHalf() {
    if (divider.counter == 0 && !isMute() && ((*parameter) & 0b10000000)) {
        if ((*parameter) & 0b111) {
            calcTargetPeriod();
            (*timer) = target_period;
        }
    }
    if (reload_flag || divider.counter == 0) {
        divider.counter = ((*parameter) >> 4) & 0b111;
        reload_flag = false;
    }
    else {
        divider.counter--;
    }
}

void APU::SweepUnit::setReload() {
    reload_flag = true;
}

bool APU::SweepUnit::isMute() {
    calcTargetPeriod();
    return (*timer) < 8 || target_period > 0x7FF;;
}

void APU::LengthCounter::count() {
    bool is_halt = is_triangle ? (*parameter) & 0b10000000 : (*parameter) & 0b00100000;
    if (is_enable) {
        if (counter != 0 && !is_halt) {
            counter--;
        }
    }
}

bool APU::LengthCounter::isZero() {
    return counter == 0;
}

void APU::LengthCounter::setCounter(Byte value) {
    if (is_enable) {
        counter = length_table[value];
    }
}

void APU::LengthCounter::setEnable(bool enable) {
    is_enable = enable;
    if (!enable) {
        counter = 0;
    }
}

APU::Byte APU::Channel::getOutput() {
    return output;
}

APU::PulseChannel::PulseChannel() {
    length_counter.parameter = &(reg[0]);
    length_counter.is_triangle = false;
    envelope_generator.parameter = &(reg[0]);
    sweep_unit.is_pulse_1 = is_pulse_1;
    sweep_unit.timer = &(timer.counter);
    sweep_unit.parameter = &(reg[1]);
}

void APU::PulseChannel::clockCycle() {
    Word timer_value = (reg[Reg::High] & 0b111) << 8 | reg[Reg::Low];
    if (timer.clock(timer_value)) {
        stepSequencer();
    }
    
    bool is_mute = length_counter.isZero() || !getSequencerNext() || sweep_unit.isMute();
    output = is_mute ? 0 : volume;
}

void APU::PulseChannel::clockQuarter() {
    volume = envelope_generator.generate();
}

void APU::PulseChannel::clockHalf() {
    volume = envelope_generator.generate();
    sweep_unit.clockHalf();
    length_counter.count();
}

void APU::PulseChannel::setDutyCycle(Byte duty) {
    sequencer = duty_table[duty];
}

void APU::PulseChannel::writeReg(int address, Byte data) {
    switch (address) {
        case Reg::Volume:
        {
            setDutyCycle((data >> 6) & 0b11);
            break;
        }
        case Reg::Sweep:
        {
            sweep_unit.setReload();
            sweep_unit.calcTargetPeriod();
            break;
        }
        case Reg::Low:
        {
            timer_value = (reg[Reg::High] & 0b111) << 8 | data;
            break;
        }
        case Reg::High:
        {
            sequencer_index = 0;
            envelope_generator.setStart();
            length_counter.setCounter(data >> 3);
            timer_value = (data & 0b111) << 8 | reg[Reg::Low];
            break;
        }
    }
    reg[address] = data;
}

void APU::PulseChannel::stepSequencer() {
    sequencer_index = sequencer_index ? sequencer_index - 1 : 7;
}

bool APU::PulseChannel::getSequencerNext() {
    return sequencer & (1 << sequencer_index);
}

APU::TriangleChannel::TriangleChannel() {
    length_counter.parameter = &(reg[0]);
    length_counter.is_triangle = true;
}

void APU::TriangleChannel::clockCycle() {
    if (!length_counter.isZero() && linear_counter.counter) {
        if (timer.clock(timer_value)) {
            stepSequencer();
        }
    }
    output = sequence[sequencer_index];
}

void APU::TriangleChannel::clockQuarter() {
    if (reload_flag) {
        linear_counter.counter = reload_value;
    }
    else {
        linear_counter.clock(0);
    }

    if (!control_flag) {
        reload_flag = false;
    }
}

void APU::TriangleChannel::clockHalf() {
    clockQuarter();
    length_counter.count();
}

void APU::TriangleChannel::writeReg(int address, Byte data) {
    switch (address) {
        case Reg::Linear:
        {
            reload_value = data & 0b01111111;
            control_flag = data & 0b10000000;
            break;
        }
        case Reg::Low:
        {
            timer_value = (reg[Reg::High] & 0b111) << 8 | data;
            break;
        }
        case Reg::High:
        {
            reload_flag = true;
            timer_value = (data & 0b111) << 8 | reg[Reg::Low];
            length_counter.setCounter(data >> 3);
            break;
        }
    }
    reg[address] = data;
}

void APU::TriangleChannel::stepSequencer() {
    sequencer_index = (sequencer_index + 1) % 32;
}

APU::NoiseChannel::NoiseChannel() {
    length_counter.parameter = &(reg[0]);
    length_counter.is_triangle = false;
    envelope_generator.parameter = &(reg[0]);
}

void APU::NoiseChannel::clockCycle() {
    if (timer.clock(rate)) {
        Word feedback = (shift_reg ^ (shift_reg >> (mode_flag ? 6 : 1))) & 0b1;
        shift_reg >>= 1;
        shift_reg |= feedback << 14;
    }
    bool is_mute = shift_reg & 0b1 || length_counter.isZero();
    output = is_mute ? 0 : volume;
}

void APU::NoiseChannel::clockQuarter() {
    volume = envelope_generator.generate();
}

void APU::NoiseChannel::clockHalf() {
    volume = envelope_generator.generate();
    length_counter.count();
}

void APU::NoiseChannel::writeReg(int address, Byte data) {
    switch(address) {
        case Reg::Low:
        {
            mode_flag = reg[Reg::Low] & 0b10000000;
            rate = rate_table[reg[Reg::Low] & 0b1111];
            break;
        }
        case Reg::High:
        {
            envelope_generator.setStart();
            length_counter.setCounter(data >> 3);
            break;
        }
    }
    reg[address] = data;
}

void APU::DMC::clockCycle() {
    fillBuffer();
    if (!is_mute && timer.clock(rate)) {
        if (shift_register & 0b1) {
            output += output < 126 ? 2 : 0;
        }
        else {
            output -= output > 1 ? 2 : 0;
        }
        shift_register >>= 1;
        if (bits_remaining_counter.clock(8)) {
            if (is_buffer_clear) {
                is_mute = true;
            }
            else {
                is_mute = false;
                shift_register = sample_buffer;
            }
        }
    }
}

void APU::DMC::fillBuffer() {
    if (is_buffer_clear && bytes_remaining > 0) {
        // TODO : The CPU is stalled for up to 4 CPU cycles
        sample_buffer = readMem(current_address);
        is_buffer_clear = false;
        current_address = current_address == 0xFFFF ? 0x8000 : current_address + 1;
        bytes_remaining--;
        checkRestart();
    }
}

void APU::DMC::checkRestart() {
    if (bytes_remaining) {
        if (loop_flag) {
            current_address = 0xC000 + (reg[Reg::Start] * 64);
            bytes_remaining = (reg[Reg::Len] * 16) + 1;
        }
        else if (is_irq_enable) {
            interupt_flag = true;
        }
    }
}

void APU::DMC::writeReg(int address, Byte data) {
    switch (address) {
        case Reg::Frequency:
        {
            is_irq_enable = data & 0b10000000;
            loop_flag = data & 0b01000000;
            rate = rate_table[data & 0b1111] / 2;
            if (!is_irq_enable) {
                interupt_flag = false;
            }
            break;
        }
        case Reg::Raw:
        {
            output = data & 0b1111111;
            break;
        }
        case Reg::Start:
        {
            break;
        }
        case Reg::Len:
        {
            
            break;
        }
    }
    reg[address] = data;
}

bool APU::DMC::isInterupt() {
    return interupt_flag;
}

bool APU::DMC::isRemaining() {
    return bytes_remaining > 0;
}

APU::Byte APU::DMC::getOutput() {
    return output;
}

void APU::DMC::clearInterupt() {
    interupt_flag = false;
}

void APU::DMC::clearBytesRemaing() {
    bytes_remaining = 0;
}
