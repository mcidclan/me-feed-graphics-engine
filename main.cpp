#include "main.h"

PSP_MODULE_INFO("megu", 0, 1, 1);
PSP_HEAP_SIZE_KB(-1024);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_USER);

static void writeMeList() {
  static u8 r = 0x7f;
  static u8 g = 0x7f;
  static u8 b = 0x7f;
  static u16 _r = 0;
  static u16 _g = 0;
  static u16 _b = 0;
  static u16 _d = 0;
  
  if (!meListRefresh) {
    Vertex* vertices = (Vertex*)meListGetMemory(sizeof(Vertex) * 2, true);
    
    if (_d++ > 100) {
      _r = randInRange(4);
      _g = randInRange(4);
      _b = randInRange(4);
      _d = 0;
    }
    _r >= 2 ? b < 0xfe ? b++ : b-- : b > 1 ? b-- : b++;
    _g >= 2 ? g < 0xfe ? g++ : g-- : g > 1 ? g-- : g++;
    _b >= 2 ? r < 0xfe ? r++ : r-- : r > 1 ? r-- : r++;
    
    vertices[1].color = 0xFF000000 | b << 16 | g << 8 | r;
    vertices[1].x = 480;
    vertices[1].y = 272;
    
    cmdCursor = 0;
    sendGeCommand(0xd4, (20 << 10) | 0);                                        // CMD_SCISSOR1
    sendGeCommand(0xd5, ((SCR_HEIGHT - 20) << 10) | SCR_WIDTH);                 // CMD_SCISSOR2
    sendGeCommand(0xd3, (0b101 << 8) | 0x01);                                   // CMD_CLEAR, clear before drawing just in case
    sendGeCommand(0x12, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D);     // CMD_VTYPE
    sendGeCommand(0x01, ((u32)vertices) & 0xffffff);                            // CMD_VADR, vertex data addr
    sendGeCommand(0x04, (GU_SPRITES << 16) | 2);                                // CMD_PRIM, draw
    sendGeCommand(0xd3, 0);                                                     // CMD_CLEAR
    sendGeCommand(0x0b, 0);                                                     // CMD_RET
    meListRefresh = 1;
  }
}

// me
static void meLoop() {
  do {
    meDCacheWritebackInvalidAll();
  } while(!mem);
  
  do {
    writeMeList();
    meCounter++;
  } while(meExit == 0);
  meExit = 2;
  meHalt();
}

extern char __start__me_section;
extern char __stop__me_section;
__attribute__((section("_me_section")))
void meHandler() {
  hw(0xbc100040) = 0x02;        // allow 64MB ram
  hw(0xbc100050) = 0x06;        // enable clocks: AW bus RegA & RegB (bits 0-2)
  hw(0xbc100004) = 0xffffffff;  // Enable NMIs
  asm("sync");
  
  asm volatile(
    "li          $k0, 0x30000000\n"
    "mtc0        $k0, $12\n"
    "sync\n"
    "la          $k0, %0\n"
    "li          $k1, 0x80000000\n"
    "or          $k0, $k0, $k1\n"
    "jr          $k0\n"
    "nop\n"
    :
    : "i" (meLoop)
    : "k0"
  );
}

static int meInit() {
  #define me_section_size (&__stop__me_section - &__start__me_section)
  memcpy((void *)ME_HANDLER_BASE, (void*)&__start__me_section, me_section_size);
  sceKernelDcacheWritebackInvalidateAll();
  hw(0xbc10004c) = 0x04;        // reset enable, just the me
  hw(0xbc10004c) = 0x0;         // disable reset to start the me
  asm volatile("sync");
  return 0;
}

// gu
static unsigned int __attribute__((aligned(16))) mcList[MAX_LIST_MEMORY] = {0};

void guInit() {
  sceGuInit();
  sceGuStart(GU_DIRECT, mcList);
  sceGuDrawBuffer(GU_PSM_8888, (void*)0x0, BUF_WIDTH);
  sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, (void*)0x88000, BUF_WIDTH);
  sceGuDepthBuffer((void*)0x110000, BUF_WIDTH);
  sceGuDisable(GU_DEPTH_TEST);
  sceGuDisable(GU_SCISSOR_TEST);
  sceGuDisplay(GU_TRUE);
  sceGuFinish();
  sceGuSync(0,0);
}

// main
int main() {
  scePowerSetClockFrequency(333, 333, 166);
  if (pspSdkLoadStartModule("ms0:/PSP/GAME/me/kcall.prx", PSP_MEMORY_PARTITION_KERNEL) < 0){
    sceKernelExitGame();
    return 0;
  }
  
  guInit();
  pspDebugScreenInitEx(0x0, PSP_DISPLAY_PIXEL_FORMAT_8888, 0);
  pspDebugScreenSetOffset(0);

  meGetUncached32(&mem, 6);
  sceKernelDcacheWritebackInvalidateAll();

  meListSetMemory();
  kcall(&meInit);
  meListWaitRefresh();
  
  int dir = 1;
  int move = 0;
  SceCtrlData ctl;
  do {
    sceGuStart(GU_DIRECT, mcList);
    sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
    pspDebugScreenSetXY(0, 1);
    pspDebugScreenPrintf("%u                                       ", meCounter);
    sceCtrlPeekBufferPositive(&ctl, 1);
    const u32* const list = meListSwitch();
    sceGuCallList(list);
    {
      Vertex* const vertices = (Vertex*)sceGuGetMemory(sizeof(Vertex) * 2);
      move += dir;
      if(move > 64) {
        dir = -1;
      } else if(move < -64) {
        dir = 1;
      }
      vertices[0].color = 0;
      vertices[0].x = 176 + move;
      vertices[0].y = 72;
      vertices[0].z = 0;
      vertices[1].color = 0xFF0000FF;
      vertices[1].x = 128 + 176 + move;
      vertices[1].y = 128 + 72;
      vertices[1].z = 0;
      sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, nullptr, vertices);
    }
    sceGuFinish();
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
    sceKernelDcacheWritebackInvalidateAll();
  } while(!(ctl.Buttons & PSP_CTRL_HOME));
  
  meExit = 1;
  do {
    asm volatile("sync");
  } while(meExit < 2);
  
  pspDebugScreenClear();
  pspDebugScreenSetXY(0, 1);
  pspDebugScreenPrintf("Exiting...");
  sceKernelDelayThread(500000);
  
  meListSetMemory(false);
  meGetUncached32(&mem, 0);
  sceKernelExitGame();
  return 0;
}
