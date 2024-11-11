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
    
    if(_d++ > 100) {
      _r = randInRange(4);
      _g = randInRange(4);
      _b = randInRange(4);
      _d = 0;
    }
    _r >= 2 ? b < 0xfe ? b++ : b-- : b > 1 ? b-- : b++;
    _g >= 2 ? g < 0xfe ? g++ : g-- : g > 1 ? g-- : g++;
    _b >= 2 ? r < 0xfe ? r++ : r-- : r > 1 ? r-- : r++;
    meDCacheWritebackInvalidAll();

    asm("sync");
    vertices[1].color = 0xFF000000 | b << 16 | g << 8 | r;
    vertices[1].x = 480;
    vertices[1].y = 272;
    asm("sync");
    
    cmdCursor = 0;
    sendGeCommand(0xd4, (20 << 10) | 0);                                        // CMD_SCISSOR1
    sendGeCommand(0xd5, ((SCR_HEIGHT - 20) << 10) | SCR_WIDTH);                 // CMD_SCISSOR2
    sendGeCommand(0xd3, (0b101 << 8) | 0x01);                                   // CMD_CLEAR, clear before drawing just in case
    sendGeCommand(0x12, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D);     // CMD_VTYPE
    sendGeCommand(0x01, ((u32)vertices) & 0xffffff);                            // CMD_VADR, vertex data addr
    sendGeCommand(0x04, (GU_SPRITES << 16) | 2);                                // CMD_PRIM, draw
    sendGeCommand(0xd3, 0);                                                     // CMD_CLEAR
    sendGeCommand(0x0b, 0);                                                     // CMD_RET
    meListRefresh = true;
    asm("sync");
  }
}

// me
volatile u32 counter = 0;
static int meLoop() {
  do {
    writeMeList();
    counter++;
    meDCacheWritebackInvalidAll();
  } while(!_meExit);
  return _meExit;
}

extern char __start__me_section;
extern char __stop__me_section;
__attribute__((section("_me_section")))
void meHandler() {
  vrg(0xbc100050) |= 0x07;      // enable clocks: ME + AW bus RegA & RegB (bits 0-2)
  vrg(0xbc100004) = 0xFFFFFFFF; // clear all interrupts
  vrg(0xbc100040) = 0x01;       // allow 32MB ram
  asm("sync");
  ((FCall)_meLoop)();
}

static int meInit() {
  memcpy((void *)0xbfc00040, (void*)&__start__me_section, me_section_size);
  _meLoop = (u32)&meLoop;
  meDCacheWritebackInvalidAll();
  vrg(0xBC10004C) |= 0x04;        // reset enable, just the me
  asm("sync");
  vrg(0xBC10004C) = 0x0;          // disable reset to start the me
  asm("sync"); 
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
    pspDebugScreenPrintf("%u                                       ", counter);
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
  
  meExit();
  pspDebugScreenClear();
  pspDebugScreenSetXY(0, 1);
  pspDebugScreenPrintf("Exiting...");
  sceKernelDelayThread(500000);
  
  meListSetMemory(false);
  sceKernelExitGame();
  return 0;
}
