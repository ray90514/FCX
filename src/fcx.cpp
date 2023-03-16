#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#include "fcx.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "cartridge.h"
typedef uint8_t Byte;
typedef uint16_t Word;
uint8_t* LoadFile() {
    FILE *fileptr;
    int filelen;
    uint8_t *buffer;
    fileptr = fopen("test.nes", "rb");
    fseek(fileptr, 0, SEEK_END);
    filelen = ftell(fileptr);
    rewind(fileptr);
    buffer = new uint8_t[filelen];
    fread(buffer, filelen, 1, fileptr);
    fclose(fileptr);
    return buffer;
}
void LoadRom(Byte **prg_rom, Byte **chr_rom) {
    uint8_t *buffer = LoadFile();
    if (buffer[0] != 'N' || buffer[1] != 'E' || buffer[2] != 'S' || buffer[3] != 0x1A) return;
    *prg_rom = new uint8_t[buffer[4] * 0x4000];
    *chr_rom = new uint8_t[buffer[5] * 0x2000];
    int i;
    for (i = 0; i < buffer[4] * 0x4000; i++) (*prg_rom)[i] = buffer[16 + i];
    for (int j = 0; j < buffer[5] * 0x2000; j++) (*chr_rom)[j] = buffer[16 + i + j];
    std::cout << (buffer[6] & 1) << std::endl;
    delete[] buffer;
    
}

void loadPal() {
    std::fstream file;
    file.open("test.pal", std::ios::binary | std::ios::in);
    uint32_t v;
    std::cout << '{';
    while (!file.fail()) {
        v = 0;
        file.read((char*)&v, 3);
        v = (v & 0xFF0000) >> 16 | (v & 0x00FF00) | (v & 0x0000FF) << 16;
    }
    std::cout << '}';
}

int main(int argc, char* argv[]) {
    void *buffer;
    uint32_t delay_buffer[256 * 240] = { 0 };
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_Window *window = SDL_CreateWindow("Hello World", 100, 100, 768, 720, 0);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, 256, 240);
    int pitch;
    //emu
    Cartridge cartridge = argc > 1 ? Cartridge(argv[1]) : Cartridge();
    Controller controller;
    PPU ppu(cartridge);
    APU apu;
    Bus bus(ppu, apu, cartridge, controller);
    CPU cpu(bus);
    //sdl
    uint8_t* keyboard = (uint8_t*)SDL_GetKeyboardState(NULL);
    int num_keys;
    SDL_Event e;
    bool close = false;
    bool hasInput = false;
    bool toggle = false;
    int cycles;
    int frame = 0;
    int delay = 1;
    float cpu_time = 0;
    float ppu_time = 0;
    float run_time = 0;
    uint64_t start_counter;
    uint64_t dtime1;
    //audio
    SDL_AudioSpec audio_spec;
    audio_spec.freq = 44100;
    audio_spec.format = AUDIO_U16;
    audio_spec.channels = 1;
    audio_spec.silence = 0;
    audio_spec.samples = 735;
    audio_spec.callback = NULL;
    SDL_AudioDeviceID device_id;
    device_id = SDL_OpenAudioDevice(NULL, 0, &audio_spec, NULL, 0);
    SDL_PauseAudioDevice(device_id, 0);
    int sample_index = 0;
    uint32_t sample_size = 0;
    uint16_t audio_buffer[735];
    uint32_t current_sample = 0;
    uint32_t prev_sample = 0;
    while(!close) {
        start_counter = SDL_GetPerformanceCounter();
        cpu_time = 0;
        ppu_time = 0;
        run_time = 0;

        hasInput = false;
        controller.clearInput();
        if (keyboard[SDL_SCANCODE_Z] || keyboard[SDL_SCANCODE_J]) {
            controller.setInput(Controller::Button::A);
        }
        if (keyboard[SDL_SCANCODE_X] || keyboard[SDL_SCANCODE_K]) {
            controller.setInput(Controller::Button::B);
        }
        if (keyboard[SDL_SCANCODE_BACKSPACE]) {
            controller.setInput(Controller::Button::Select);
        }
        if (keyboard[SDL_SCANCODE_RETURN]) {
            controller.setInput(Controller::Button::Start);
        }
        if (keyboard[SDL_SCANCODE_UP] || keyboard[SDL_SCANCODE_W]) {
            controller.setInput(Controller::Button::Up);
        }
        if (keyboard[SDL_SCANCODE_DOWN] || keyboard[SDL_SCANCODE_S]) {
            controller.setInput(Controller::Button::Down);
        }
        if (keyboard[SDL_SCANCODE_LEFT] || keyboard[SDL_SCANCODE_A]) {
            controller.setInput(Controller::Button::Left);
        }
        if (keyboard[SDL_SCANCODE_RIGHT] || keyboard[SDL_SCANCODE_D]) {
            controller.setInput(Controller::Button::Right);
        }
        if (keyboard[SDL_SCANCODE_ESCAPE]) {
            cpu.reset();
        }

        while(!ppu.checkFrameReady()) {
            cycles = cpu.run();
            ppu.runCycles(3 * cycles);
            apu.runCycles(cycles);
            for (int i = 0; i < cycles && sample_size < 735; i++) {
                //current_sample += apu.getOutput(i);
                current_sample = prev_sample / 4 + apu.getOutput(i) * 3 / 4;
                prev_sample = current_sample;
                sample_index++;
                if (sample_index == (toggle ? 41 : 40)) {
                    sample_index = 0;
                    audio_buffer[sample_size] = current_sample;
                    audio_buffer[sample_size] = apu.getOutput(i);
                    //current_sample = 0;
                    sample_size++;
                    toggle = !toggle;
                }
            }
            dtime1 = SDL_GetPerformanceCounter();
            //SDL_PumpEvents();
            //run_time += (SDL_GetPerformanceCounter() - dtime1) / (float)SDL_GetPerformanceFrequency();
        }
        SDL_QueueAudio(device_id, audio_buffer, sample_size * sizeof(audio_buffer[0]));
        sample_size = 0;
        frame = (frame + 1) % 60;
        SDL_LockTexture(texture, NULL, &buffer, &pitch);
        /*for (int i = 0; i < 240; i++) {
            for (int j = 0; j < 256; j++) {
                ((uint32_t*)buffer)[256 * i + j] = delay_buffer[256 * i + j];
            }
        }*/
        ppu.renderFromBuffer((uint32_t*)buffer);
        SDL_UnlockTexture(texture);
        //ppu.renderFromBuffer(delay_buffer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                close = true;
            }
        }
        uint32_t dtime = 1000 * (SDL_GetPerformanceCounter() - start_counter) / (float)SDL_GetPerformanceFrequency();
        if (16 > dtime) {
            SDL_Delay(16 - dtime);
        }
    }
    ppu.test();
    return 0;
}