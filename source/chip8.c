#include "chip8.h"

#include "os/os.h"
#include "os/input.h"

//- Audio 

static void Audio_Init(Chip_Exec_Context* ctx) {
  ctx->al_device = alcOpenDevice(nullptr);
  if (!ctx->al_device)
    LogFatal("Failed to create audio device");
  
  ctx->al_context = alcCreateContext(ctx->al_device, nullptr);
  alcMakeContextCurrent(ctx->al_context);
  
  ALenum error;
  
  alGenBuffers(1, &ctx->beepbuffer);
  if ((error = alGetError()) != AL_NO_ERROR) {
    LogFatal("Failed to create buffer: %d", error);
  }
  
  float freq = 480.f;
  
  int seconds = 1;
  unsigned sample_rate = 44100;
  double my_pi = 3.14159;
  size_t buf_size = seconds * sample_rate;
  
  // allocate PCM audio buffer
  short* samples = malloc(sizeof(short) * buf_size);
  for(int i=0; i<buf_size; ++i) {
    samples[i] = 32760 * sin( (2.f * my_pi * freq)/sample_rate * i );
  }
  
  alBufferData(ctx->beepbuffer, AL_FORMAT_MONO16, samples, buf_size, sample_rate);
  free(samples);
  
  alGenSources(1, &ctx->al_source);
  alSourcef(ctx->al_source, AL_GAIN, 0.05);
  alSourcef(ctx->al_source, AL_PITCH, 1);
  alSource3f(ctx->al_source, AL_POSITION, 0, 0, 0);
  alSource3f(ctx->al_source, AL_VELOCITY, 0, 0, 0);
  alSourcei(ctx->al_source, AL_BUFFER, ctx->beepbuffer);
  alSourcei(ctx->al_source, AL_LOOPING, 1);
  
  alListener3f(AL_POSITION, 0, 0, 0);
  alListener3f(AL_VELOCITY, 0, 0, 0);
}

static void Audio_Free(Chip_Exec_Context* ctx) {
  alDeleteBuffers(1, &ctx->beepbuffer);
  alDeleteSources(1, &ctx->al_source);
  alcMakeContextCurrent(NULL);
  alcDestroyContext(ctx->al_context);
  alcCloseDevice(ctx->al_device);
}

//- Font 

static u8 font[80] = {
  0xF0, 0x90, 0x90, 0x90, 0xF0, /* 0 */
  0x20, 0x60, 0x20, 0x20, 0x70, /* 1 */
  0xF0, 0x10, 0xF0, 0x80, 0xF0, /* 2 */
  0xF0, 0x10, 0xF0, 0x10, 0xF0, /* 3 */
  0x90, 0x90, 0xF0, 0x10, 0x10, /* 4 */
  0xF0, 0x80, 0xF0, 0x10, 0xF0, /* 5 */
  0xF0, 0x80, 0xF0, 0x90, 0xF0, /* 6 */
  0xF0, 0x10, 0x20, 0x40, 0x40, /* 7 */
  0xF0, 0x90, 0xF0, 0x90, 0xF0, /* 8 */
  0xF0, 0x90, 0xF0, 0x10, 0xF0, /* 9 */
  0xF0, 0x90, 0xF0, 0x90, 0x90, /* A */
  0xE0, 0x90, 0xE0, 0x90, 0xE0, /* B */
  0xF0, 0x80, 0x80, 0x80, 0xF0, /* C */
  0xE0, 0x90, 0x90, 0x90, 0xE0, /* D */
  0xF0, 0x80, 0xF0, 0x80, 0xF0, /* E */
  0xF0, 0x80, 0xF0, 0x80, 0x80, /* F */
};

//~ Internals

#if 0
# define disassembly(fmt, ...) Statement(\
printf("%4x  ", instruction);\
printf(fmt, ##__VA_ARGS__);\
fflush(stdout);\
)
#else
# define disassembly(fmt, ...)
#endif

static u32 keymap[] = {
  [0x1] = '1', [0x2] = '2', [0x3] = '3', [0xC] = '4',
  [0x4] = 'Q', [0x5] = 'W', [0x6] = 'E', [0xD] = 'R',
  [0x7] = 'A', [0x8] = 'S', [0x9] = 'D', [0xE] = 'F',
  [0xA] = 'Z', [0x0] = 'X', [0xB] = 'C', [0xF] = 'V',
};

#define first(instr)  ((instr & 0xF000) >> 12)
#define second(instr) ((instr & 0x0F00) >> 8)
#define third(instr)  ((instr & 0x00F0) >> 4)
#define fourth(instr) ((instr & 0x000F) >> 0)
#define nnn(instr)    ((instr & 0x0FFF) >> 0)
#define kk(instr)     ((instr & 0x00FF) >> 0)
static void Chip_Execute(Chip_Exec_Context* ctx, u16 instruction) {
  
  switch (first(instruction)) {
    
    case 0: {
      if (second(instruction)) {
        // 0nnn:  SYS addr
        // Should apparently be ignored. Nop
        disassembly("0nnn (SYS addr): Does nothing\n");
      } else {
        
        if (fourth(instruction) == 0xE) {
          // 00EE:  RET
          
          if (ctx->PC == 0) {
            LogFatal("Never entered a subroutine, but called RET");
          }
          
          ctx->SP -= 1;
          ctx->PC = ctx->stack[ctx->SP];
          
          disassembly("00EE (RET): Jumped back to %X ; SP = %X\n", ctx->stack[ctx->SP], ctx->SP);
        } else {
          // 00E0:  CLS
          MemoryZero(ctx->framebuffer, 64 * 32 * sizeof(u8));
          
          disassembly("00E0 (CLS): Screen Clear\n");
        }
        
      }
    } break;
    
    case 1: {
      // 1nnn:  JMP addr
      ctx->PC = nnn(instruction);
      ctx->jumped = true;
      
      disassembly("1nnn (JMP addr): Jumped to %X ; PC: %X\n", nnn(instruction), ctx->PC);
    } break;
    
    case 2: {
      // 2nnn:  CALL addr
      ctx->stack[ctx->SP] = ctx->PC;
      ctx->SP += 1;
      if (ctx->SP > 0xF) LogFatal("Stack grew bigger than 16 spaces: %X", ctx->SP);
      ctx->PC = nnn(instruction);
      ctx->jumped = true;
      
      disassembly("2nnn (CALL addr): Called subroutine at %X ; SP = %X\n", nnn(instruction), ctx->SP);
    } break;
    
    case 3: {
      // 3xkk:  SE Vx, byte
      if (ctx->V[second(instruction)] == kk(instruction)) {
        ctx->PC += 2;
      }
      
      disassembly("3xkk (SE Vx, byte): V%X %s %u\n", second(instruction),
                  ctx->V[second(instruction)] == kk(instruction) ? "==" : "!=", kk(instruction));
    } break;
    
    case 4: {
      // 4xkk:  SNE Vx, byte
      if (ctx->V[second(instruction)] != kk(instruction)) {
        ctx->PC += 2;
      }
      
      disassembly("4xkk (SNE Vx, byte): V%X %s %u\n", second(instruction),
                  ctx->V[second(instruction)] == kk(instruction) ? "==" : "!=", kk(instruction));
    } break;
    
    case 5: {
      // 5xy0:  SE Vx, Vy
      if (ctx->V[second(instruction)] == ctx->V[third(instruction)]) {
        ctx->PC += 2;
      }
      
      disassembly("5xy0 (SE Vx, Vy): V%X %s V%X\n", second(instruction),
                  ctx->V[second(instruction)] == kk(instruction) ? "==" : "!=", third(instruction));
    } break;
    
    case 6: {
      // 6xkk:  LD Vx, byte
      ctx->V[second(instruction)] = kk(instruction);
      
      disassembly("6xkk (LD Vx, byte): V%X = %u\n", second(instruction), kk(instruction));
    } break;
    
    case 7: {
      // 7xkk:  ADD Vx, byte
      ctx->V[second(instruction)] += kk(instruction);
      
      disassembly("7xkk (ADD Vx, byte): V%X += %u ; V%X = %u\n", second(instruction), kk(instruction),
                  second(instruction), ctx->V[second(instruction)]);
    } break;
    
    case 8: {
      
      switch (fourth(instruction)) {
        // 8xy0:  LD Vx, Vy
        case 0: {
          ctx->V[second(instruction)]  = ctx->V[third(instruction)];
          disassembly("8xy0 (LD Vx, Vy): V%X = V%X ; V%X = %u\n", second(instruction),
                      third(instruction), second(instruction), ctx->V[second(instruction)]);
        } break;
        
        // 8xy1:  OR Vx, Vy
        case 1: {
          ctx->V[second(instruction)] |= ctx->V[third(instruction)];
          ctx->V[0xF] = 0;
          disassembly("8xy1 (OR Vx, Vy): V%X |= V%X ; V%X = %u\n", second(instruction),
                      third(instruction), second(instruction), ctx->V[second(instruction)]);
        } break;
        
        // 8xy2:  AND Vx, Vy
        case 2: {
          ctx->V[second(instruction)] &= ctx->V[third(instruction)];
          ctx->V[0xF] = 0;
          disassembly("8xy2 (AND Vx, Vy): V%X &= V%X ; V%X = %u\n", second(instruction),
                      third(instruction), second(instruction), ctx->V[second(instruction)]);
        } break;
        
        // 8xy3:  XOR Vx, Vy
        case 3: {
          ctx->V[second(instruction)] ^= ctx->V[third(instruction)];
          ctx->V[0xF] = 0;
          disassembly("8xy3 (XOR Vx, Vy): V%X ^= V%X ; V%X = %u\n", second(instruction),
                      third(instruction), second(instruction), ctx->V[second(instruction)]);
        } break;
        
        // 8xy4:  ADD Vx, Vy
        case 4: {
          u32 inbtwn = ctx->V[second(instruction)] + ctx->V[third(instruction)];
          ctx->V[second(instruction)] = (u8)(inbtwn & 0xFF);
          ctx->V[0xF] = inbtwn > 255;
          
          disassembly("8xy4 (ADD Vx, Vy): V%X += V%X ; V%X = %u ; VF = %u\n", second(instruction),
                      third(instruction), second(instruction), ctx->V[second(instruction)], ctx->V[0xF]);
        } break;
        
        // 8xy5:  SUB Vx, Vy
        case 5: {
          u8 old_second = ctx->V[second(instruction)];
          ctx->V[second(instruction)] -= ctx->V[third(instruction)];
          ctx->V[0xF] = old_second > ctx->V[third(instruction)];
          disassembly("8xy5 (SUB Vx, Vy): V%X -= V%X ; V%X = %u ; VF = %u\n", second(instruction),
                      third(instruction), second(instruction), ctx->V[second(instruction)], ctx->V[0xF]);
        } break;
        
        // 8xy6:  SHR Vx {, Vy}
        case 6: {
          u8 old_value = ctx->V[second(instruction)];
          ctx->V[second(instruction)] >>= 1;
          ctx->V[0xF] = old_value & 0x1;
          disassembly("8xy6 (SHR Vx {, Vy}): V%X >>= 1 ; V%X = %u ; VF = %u\n", second(instruction),
                      second(instruction), ctx->V[second(instruction)], ctx->V[0xF]);
        } break;
        
        // 8xy7:  SUBN Vx, Vy
        case 7: {
          ctx->V[second(instruction)] = ctx->V[third(instruction)] - ctx->V[second(instruction)];
          ctx->V[0xF] = ctx->V[second(instruction)] < ctx->V[third(instruction)];
          disassembly("8xy7 (SUBN Vx, Vy): V%X = V%X - V%X ; V%X = %u ; VF = %u\n",
                      second(instruction), third(instruction), second(instruction),
                      second(instruction), ctx->V[second(instruction)], ctx->V[0xF]);
        } break;
        
        // 8xyE:  SHL Vx {, Vy}
        case 0xE: {
          u8 old_value = ctx->V[second(instruction)];
          ctx->V[second(instruction)] <<= 1;
          ctx->V[0xF] = (old_value >> 7) & 0x1;
          disassembly("8xyE (SHL Vx {, Vy}): V%X <<= 1 ; V%X = %u ; VF = %u\n", second(instruction),
                      second(instruction), ctx->V[second(instruction)], ctx->V[0xF]);
        } break;
        
        default: {
          unreachable;
        } break;
      }
      
    } break;
    
    case 9: {
      // 9xy0:  SNE Vx, Vy
      if (ctx->V[second(instruction)] != ctx->V[third(instruction)]) {
        ctx->PC += 2;
      }
      
      disassembly("9xy0 (SNE Vx, Vy): V%X %s V%X\n", second(instruction),
                  ctx->V[second(instruction)] == kk(instruction) ? "==" : "!=", third(instruction));
    } break;
    
    case 0xA: {
      // Annn:  LD I, addr
      ctx->I = nnn(instruction);
      
      disassembly("Annn (LD I, addr): I = %X\n", nnn(instruction));
    } break;
    
    case 0xB: {
      // Bnnn:  JP V0, addr
      ctx->PC = nnn(instruction) + ctx->V[0];
      ctx->jumped = true;
      
      disassembly("Bnnn (JP I, addr): PC = %X + %X ; PC = %X\n", ctx->I, nnn(instruction), ctx->PC);
    } break;
    
    case 0xC: {
      // Cxkk:  RND Vx, byte
      ctx->V[second(instruction)] = rand() % kk(instruction);
      disassembly("Cxkk (RND Vx, byte): V%X = rand() %% %u ; V%X = %X\n", second(instruction), kk(instruction), second(instruction), ctx->V[second(instruction)]);
    } break;
    
    case 0xD: {
      // Dxyn:  DRW Vx, Vy, nibble
      b8 collision = false;
      
      u8 xoff = ctx->V[second(instruction)];
      u8 yoff = ctx->V[third(instruction)];
      u8 height = fourth(instruction);
      
      for (u32 line = 0; line < height; line++) {
        u8 line_data = ctx->memory[ctx->I + line];
        for (u32 x = 0; x < 8; x++) {
          
          if ((yoff + line >= 32) || (xoff + x >= 64)) continue;
          
          if (line_data & (0x80 >> x) && ctx->framebuffer[((yoff + line) * 64) + xoff + x])
            collision= true;
          
          ctx->framebuffer[((yoff + line) * 64) + xoff + x] ^= line_data & (0x80 >> x);
          
        }
      }
      
      ctx->V[0xF] = collision;
      
      disassembly("Dxyn (DRW Vx, Vy, nibble): Drew sprite of height %u at %u, %u\n", fourth(instruction), ctx->V[second(instruction)], ctx->V[third(instruction)]);
    } break;
    
    case 0xE: {
      if (kk(instruction) == 0x9E) {
        // Ex9E:  SKP Vx
        b8 press = OS_InputKey(keymap[ctx->V[second(instruction)]]);
        if (press)
          ctx->PC += 2;
        
        disassembly("Ex9E (SKP Vx): key: %X was %u ; PC = %X\n", ctx->V[second(instruction)],
                    press, ctx->PC);
      } else if (kk(instruction) == 0xA1) {
        // ExA1:  SKNP Vx
        b8 press = OS_InputKey(keymap[ctx->V[second(instruction)]]);
        if (!press)
          ctx->PC += 2;
        
        disassembly("ExA1 (SKNP Vx): key: %X was %u ; PC = %X\n", second(instruction),
                    press, ctx->PC);
      }
    } break;
    
    case 0xF: {
      if (kk(instruction) == 0x07) {
        // Fx07:  LD Vx, DT
        ctx->V[second(instruction)] = ctx->delay_reg;
        disassembly("Fx07 (LD Vx, DT): V%X = %u\n", second(instruction),
                    ctx->delay_reg);
        
      } else if (kk(instruction) == 0x0A) {
        // Fx0A:  LD Vx, K
        ctx->waiting_key = second(instruction);
        disassembly("Fx0A (LD Vx, K): Waiting for key to be put in V%X\n", second(instruction));
        
      } else if (kk(instruction) == 0x15) {
        // Fx15:  LD DT, Vx
        ctx->delay_reg = ctx->V[second(instruction)];
        disassembly("Fx07 (LD DT, Vx): DT = V%X ; DT = %u\n", second(instruction), ctx->delay_reg);
        
      } else if (kk(instruction) == 0x18) {
        // Fx18:  LD ST, Vx
        ctx->sound_reg = ctx->V[second(instruction)];
        if (ctx->sound_reg) alSourcePlay(ctx->al_source);
        disassembly("Fx07 (LD ST, Vx): ST = V%X ; ST = %u\n", second(instruction), ctx->sound_reg);
        
      } else if (kk(instruction) == 0x1E) {
        // Fx1E:  ADD I, Vx
        ctx->I += ctx->V[second(instruction)];
        disassembly("Fx1E (ADD I, Vx): I += V%X ; I = %X\n", second(instruction), ctx->I);
        
      } else if (kk(instruction) == 0x29) {
        // Fx29:  LD F, Vx
        ctx->I = 5 * ctx->V[second(instruction)];
        disassembly("Fx29 (LD F, Vx): Digit: %X ; I = %X\n", ctx->V[second(instruction)],
                    ctx->I);
        
      } else if (kk(instruction) == 0x33) {
        // Fx33:  LD B, Vx
        u8 num = ctx->V[second(instruction)];
        ctx->memory[ctx->I + 0] = num / 100;
        ctx->memory[ctx->I + 1] = (num / 10) % 10;
        ctx->memory[ctx->I + 2] = num % 10;
        disassembly("Fx33 (LD B, Vx): [%X] = %u , [%X] = %u , [%X] = %u\n",
                    ctx->I + 0, ctx->memory[ctx->I + 0],
                    ctx->I + 1, ctx->memory[ctx->I + 1],
                    ctx->I + 2, ctx->memory[ctx->I + 2]);
        
      } else if (kk(instruction) == 0x55) {
        // Fx55:  LD [I], Vx
        for (u32 i = 0; i <= second(instruction); i++) {
          ctx->memory[ctx->I++] = ctx->V[i];
        }
        
        disassembly("Fx55 (LD [I], Vx): Saved Registers V0 - V%X to %X - %X\n",
                    second(instruction), ctx->I, ctx->I + second(instruction));
        
      } else if (kk(instruction) == 0x65) {
        // Fx65:  LD Vx, [I]
        for (u32 i = 0; i <= second(instruction); i++) {
          ctx->V[i] = ctx->memory[ctx->I++];
        }
        //ctx->I += second(instruction);
        
        disassembly("Fx65 (LD Vx, [I]): Loaded Registers V0 - V%X from %X - %X\n",
                    second(instruction), ctx->I, ctx->I + second(instruction));
      }
      
    } break;
    
    default: {
      unreachable;
    } break;
  }
}
#undef first
#undef second
#undef third
#undef fourth
#undef nnn
#undef kk

//~ API

void Chip_Initialize(Chip_Exec_Context* ctx) {
  MemoryZeroStruct(ctx, Chip_Exec_Context);
  
  // Loaded roms go to 0x200
  ctx->PC = 0x200;
  
  ctx->waiting_key = -1;
  ctx->target_time = 1 / 750.f;
  ctx->dec_target_time = 1 / 60.f;
  
  memmove(&ctx->memory[0], font, sizeof(font));
  
  //Audio_Init(ctx);
}

void Chip_Step(Chip_Exec_Context* ctx) {
  if (ctx->waiting_key != -1) {
    for (u32 i = 0; i < 0xF; i++) {
      if (OS_InputKeyReleased(keymap[i])) {
        ctx->V[(u32)ctx->waiting_key] = (u8) i;
        ctx->waiting_key = -1;
        //break;
      }
    }
    if (ctx->waiting_key != -1) return;
  }
  
  u16 instruction = ctx->memory[ctx->PC] << 8 | ctx->memory[ctx->PC + 1];
  //printf("%u    ", ctx->PC);
  Chip_Execute(ctx, instruction);
  
  printf("PC: %x, %4x     ", ctx->PC, instruction);
  for (u32 i = 0; i <= 0xF; i++)
    printf("V%x: %3u   ", i, ctx->V[i]);
  printf("\n\n");
  flush;
  
  // Go to next instruction
  if (!ctx->jumped) ctx->PC += 2;
  ctx->jumped = false;
  
  if (ctx->delay_reg) ctx->delay_reg --;
  if (ctx->sound_reg) {
    ctx->sound_reg --;
    if (!ctx->sound_reg) alSourceStop(ctx->al_source);
  }
  
}

void Chip_Tick(Chip_Exec_Context* ctx, f32 dt) {
  
  if (ctx->waiting_key != -1) {
    for (u32 i = 0; i < 0xF; i++) {
      if (OS_InputKeyReleased(keymap[i])) {
        ctx->V[(u32)ctx->waiting_key] = (u8) i;
        ctx->waiting_key = -1;
        break;
      }
    }
    if (ctx->waiting_key != -1) return;
  }
  
  ctx->time_accumulator += dt;
  ctx->dec_time_accumulator += dt;
  
  while (ctx->time_accumulator >= ctx->target_time) {
    // All instructions are 2 bytes long
    u16 instruction = ctx->memory[ctx->PC] << 8 | ctx->memory[ctx->PC + 1];
    Chip_Execute(ctx, instruction);
    
    // Go to next instruction
    if (!ctx->jumped) ctx->PC += 2;
    ctx->jumped = false;
    
    ctx->time_accumulator -= ctx->target_time;
  }
  
  while (ctx->dec_time_accumulator >= ctx->dec_target_time) {
    if (ctx->delay_reg) ctx->delay_reg --;
    if (ctx->sound_reg) {
      ctx->sound_reg --;
      if (!ctx->sound_reg) alSourceStop(ctx->al_source);
    }
    
    ctx->dec_time_accumulator -= ctx->dec_target_time;
  }
  
}

void Chip_Free(Chip_Exec_Context* ctx) {
  // Nop
  //Audio_Free(ctx);
}
