#include "main.h"

PSP_MODULE_INFO("megu", 0, 1, 1);
PSP_HEAP_SIZE_KB(-1024);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_USER);

#define MAX_LIST_MEMORY 2048

volatile u32* meListBase = nullptr;
volatile u32* meList = nullptr;
void meListSetMemory(const bool clean = false) {
  static u32* _base = nullptr;
  if (!_base) {
    _base = (u32*)memalign(16, MAX_LIST_MEMORY);
    memset(_base, 0, MAX_LIST_MEMORY); 
  }
  if (!clean) {
    meListBase = (u32*)(0x40000000 | (u32)_base);
    meList = meListBase;
    return;
  }
  free(_base);
}

inline void* getListMemory(const u32 size, const bool reset) {
 static volatile u32 cursor = MAX_LIST_MEMORY;
 if (reset) {
  cursor = MAX_LIST_MEMORY;
 }
 cursor -= size;
 return (void*)(cursor + (u32)meListBase);
}

static u32 cmdCursor = 0;
inline void sendGeCommand(const u32 cmd, const u32 value) {
  meList[cmdCursor++] = (cmd << 24) | (value & 0x00FFFFFF);
}

static bool refreshMeList = false;
u32* meListSwitch() {
  u32* const _meList = (u32* const)meList;
  if (refreshMeList) {
    static u32 offset = 0;
    offset ^= (((u32)MAX_LIST_MEMORY) >> 1);
    meList = (u32*)(((u32)meList) + offset);
    refreshMeList = false;
  }
  return _meList;
}

static void meListWaitRefresh() {
  while(!refreshMeList) {
    meDCacheWritebackInvalidAll();
    sceKernelDelayThread(1);
  }
}

void writeMeList() {
  static u8 r = 0x7f;
  static u8 g = 0x7f;
  static u8 b = 0x7f;
  if (!refreshMeList) {
    Vertex* vertices = (Vertex*)getListMemory(sizeof(Vertex) * 2, true);
    
    randInRange(0xff) > 0x7f ? b < 0xfe ? b++ : b-- : b > 1 ? b-- : b++;
    randInRange(0xff) > 0x7f ? g < 0xfe ? g++ : g-- : g > 1 ? g-- : g++;
    randInRange(0xff) > 0x7f ? r < 0xfe ? r++ : r-- : r > 1 ? r-- : r++;
    meDCacheWritebackInvalidAll();
    asm("sync");
    
    vertices[1].color = 0xFF000000 | b << 16 | g << 8 | r;
    vertices[1].x = 480;
    vertices[1].y = 272;
    
    cmdCursor = 0;
    asm("sync");
    
    sendGeCommand(0xd4, (16 << 10) | 0);                                          // CMD_SCISSOR1
    sendGeCommand(0xd5, ((SCR_HEIGHT - 16) << 10) | SCR_WIDTH);                   // CMD_SCISSOR2
    sendGeCommand(0xd3, (0b101 << 8) | 0x01);                                     // CMD_CLEAR
    sendGeCommand(0x12, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D);
    sendGeCommand(0x10, (((u32)vertices) >> 8) & 0xf0000);
    sendGeCommand(0x01, ((u32)vertices) & 0xffffff);
    sendGeCommand(0x04, (GU_SPRITES << 16) | 2);
    sendGeCommand(0xd3, 0);                                                       // CMD_CLEAR
    sendGeCommand(0x0b, 0);                                                       // CMD_RET
    refreshMeList = true;
    asm("sync");
  }
}

// me
volatile bool _meExit = false;
static void meExit() {
  _meExit = true;
  meDCacheWritebackInvalidAll();
}

volatile u32 counter = 0;
int meLoop() {
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
  vrg(0xbc100050) |= 0x7;       // transfer bus control bits
  vrg(0xbc100004) = 0xFFFFFFFF; // clear all interrupts
  vrg(0xbc100040) = 0x01;       // allow 32MB ram
  asm("sync");
  vrp ptr = (vrp)0xBC000000;    // disable memory protections
  while (ptr <= (vrp)0xBC00000C) {
    *(ptr++) = 0xFFFFFFFF;
  }
  asm("sync");
  ((FCall)_meLoop)();
}

static int meInit() {
  memcpy((void *)0xbfc00040, (void*)&__start__me_section, me_section_size);
  _meLoop = (u32)&meLoop;
  meDCacheWritebackInvalidAll();
  vrg(0xBC10004C) |= 0x04;        // 0b0100;  // reset enable, just the me
  asm("sync");
  vrg(0xBC10004C) = 0x0;          // disable reset to start the me
  asm("sync"); 
  return 0;
}

// gu
u32* mclist = nullptr;

void guInit() {
  sceGuInit();
  sceGuStart(GU_DIRECT, mclist);
  sceGuDrawBuffer(GU_PSM_8888, (void*)0x0, BUF_WIDTH);
  sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, (void*)0x88000, BUF_WIDTH);
  sceGuDepthBuffer((void*)0x110000, BUF_WIDTH);
  sceGuDisable(GU_DEPTH_TEST);
  sceGuDisable(GU_SCISSOR_TEST);
  sceGuDisplay(GU_TRUE);
  sceGuFinish();
  sceGuSync(0,0);
}

inline void start() {
  sceGuStart(GU_DIRECT, mclist);
  sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
}

inline void end() {
  sceGuFinish();
  sceGuSync(0,0);
  sceDisplayWaitVblankStart();
  sceGuSwapBuffers();
}

// main
int main(int argc, char **argv) {
  scePowerSetClockFrequency(333, 333, 166);

  meListSetMemory();
  mclist = (u32*)memalign(16, MAX_LIST_MEMORY);
  
  if (pspSdkLoadStartModule("ms0:/PSP/GAME/me/kcall.prx", PSP_MEMORY_PARTITION_KERNEL) < 0){
    sceKernelExitGame();
    return 0;
  }
  
  guInit();
  pspDebugScreenInitEx(0x0, PSP_DISPLAY_PIXEL_FORMAT_8888, 0);
  pspDebugScreenSetOffset(0);
  kcall(&meInit);
  meListWaitRefresh();
  
  SceCtrlData ctl;
  do {
    sceKernelDcacheWritebackInvalidateAll();
    
    start();
    pspDebugScreenSetXY(0, 1);
    pspDebugScreenPrintf("%u                                       ", counter);
    sceCtrlPeekBufferPositive(&ctl, 1);
    const u32* const list = meListSwitch();
    sceGuCallList(list);
    end();
    
  } while(!(ctl.Buttons & PSP_CTRL_HOME));
  
  meExit();
  pspDebugScreenClear();
  pspDebugScreenSetXY(0, 1);
  pspDebugScreenPrintf("Exiting...");
  sceKernelDelayThread(500000);
  meListSetMemory(false);
  // free(mclist);
  // free((void*)meList);
  
  sceKernelExitGame();
  return 0;
}
