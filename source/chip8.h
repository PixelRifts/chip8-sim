/* date = May 14th 2023 11:49 am */

#ifndef CHIP8_H
#define CHIP8_H

#include "defines.h"
#include "base/base.h"

#include "chip8.h"
#include "al/alc.h"
#include "al/al.h"

typedef struct Chip_Exec_Context {
  
  // 4096 bytes of memory
  u8 memory[Kilobytes(4)];
  
  // 16 8 bit registers
  u8 V[16];
  
  // Few special registers
  u16 I;         // Kindof like an address pointer
  u16 PC;        // Program counter
  u8  SP;        // Stack pointer
  u8  delay_reg; // Delay timer
  u8  sound_reg; // Sound timer
  
  u16 stack[16];
  
  // Display Framebuffer Top-Left to Bottom-Right
  b8 framebuffer[64 * 32];
  
  // Metadata
  i8  waiting_key;
  f32 time_accumulator;
  f32 dec_time_accumulator;
  f32 target_time;
  f32 dec_target_time;
  b8  jumped;
  
  ALCdevice* al_device;
  ALCcontext* al_context;
  ALuint al_source;
  ALuint beepbuffer;
  
} Chip_Exec_Context;


void Chip_Initialize(Chip_Exec_Context* ctx);
void Chip_Step(Chip_Exec_Context* ctx);
void Chip_Tick(Chip_Exec_Context* ctx, f32 dt);
void Chip_Free(Chip_Exec_Context* ctx);

#endif //CHIP8_H
