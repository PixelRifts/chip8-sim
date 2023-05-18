#include "defines.h"
#include "os/os.h"
#include "os/window.h"
#include "os/input.h"
#include "base/tctx.h"
#include "core/backend.h"
#include "core/resources.h"
#include "opt/render_2d.h"

#include "chip8.h"

void MyResizeCallback(OS_Window* window, i32 w, i32 h) {
  // TODO(voxel): @awkward Add a "first resize" to Win32Window so that This if isn't required
  if (window->user_data) {
    R_Viewport(0, 0, w, h);
  }
}

int main(int argc, char **argv) {
  OS_Init();
  ThreadContext context = {0};
  tctx_init(&context);
  M_Arena global_arena;
  arena_init(&global_arena);
  U_FrameArenaInit();
  OS_Window* window = OS_WindowCreate(64 * 20, 32 * 20, str_lit("This should work"));
  window->resize_callback = MyResizeCallback;
  B_BackendInit(window);
  OS_WindowShow(window);
  
  R2D_Renderer renderer = {0};
  R2D_Init(window, &renderer);
  
  srand((u32) OS_TimeMicrosecondsNow());
  
  Chip_Exec_Context* ctx = arena_alloc(&global_arena, sizeof(Chip_Exec_Context));
  Chip_Initialize(ctx);
  /*for (u32 j = 0; j < 32; j ++) {
    for (u32 i = 0; i < 64; i ++) {
      ctx->framebuffer[j * 64 + i] = (i + j) % 2;
    }
  }*/
  
  string fp = str_make(argv[1]);
  if (!OS_FileExists(fp)) LogFatal("File %.*s not found", str_expand(fp));
  string rom = OS_FileRead(&global_arena, fp);
  memmove(&ctx->memory[0x200], rom.str, rom.size);
  
  f32 start = 0.f; f32 end = 0.016f;
  f32 delta = 0.016f;
  b8 step_mode = true;
  
  while (OS_WindowIsOpen(window)) {
    delta = end - start;
    start = OS_TimeMicrosecondsNow();
    delta /= 1e6;
    
    U_ResetFrameArena();
    OS_PollEvents();
    
    if (OS_InputKeyPressed(' ')) {
      step_mode = !step_mode;
      printf("Step Mode = %u\n", step_mode);
      flush;
    }
    
    R_Clear(BufferMask_Color);
    if (step_mode) {
      if (OS_InputButtonPressed(Input_MouseButton_Left)) {
        Chip_Step(ctx);
      }
    } else {
      Chip_Tick(ctx, delta);
    }
    
    R2D_BeginDraw(&renderer);
    for (u32 i = 0; i < 64; i++) {
      for (u32 j = 0; j < 32; j++) {
        if (ctx->framebuffer[j * 64 + i]) {
          R2D_DrawQuadC(&renderer, rct(i * 20, j * 20, 20, 20), Color_Green);
        }
      }
    }
    R2D_EndDraw(&renderer);
    
    B_BackendSwapchainNext(window);
    
    end = OS_TimeMicrosecondsNow();
  }
  
  
  Chip_Free(ctx);
  R2D_Free(&renderer);
  B_BackendFree(window);
  OS_WindowClose(window);
  U_FrameArenaFree();
  arena_free(&global_arena);
  tctx_free(&context);
}
