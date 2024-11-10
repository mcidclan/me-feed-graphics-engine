#include <cstring>
#include <psppower.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspsdk.h>
#include "kcall.h"

#define vrp          volatile u32*
#define vrg(addr)    (*((vrp)(addr)))
#define _meLoop      vrg(0xbfc00160)

PSP_MODULE_INFO("megu", 0, 1, 1);
PSP_HEAP_SIZE_KB(-1024);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_USER);

__attribute__((optimize("unroll-loops")))
inline void meDCacheWritebackInvalidAll() {
 for (int i = 0; i < 8192; i += 64) {
  asm("cache 0x14, 0(%0)" :: "r"(i));
  asm("cache 0x14, 0(%0)" :: "r"(i));
 }
 asm("sync");
}

static volatile bool _meExit = false;
static void meExit() {
  _meExit = true;
  meDCacheWritebackInvalidAll();
}

static volatile u32 counter = 0;
int meLoop() {
  do {
    meDCacheWritebackInvalidAll();
    counter++;
  } while(!_meExit);
  return _meExit;
}

extern char __start__me_section;
extern char __stop__me_section;
__attribute__((section("_me_section")))
void meHandler() {
  vrg(0xbc100050) = 0x04;          // 0b100; // enable AW RegB Bus
  vrg(0xbc100004) = 0xFFFFFFFF; // clear all interrupts, just usefull
  vrg(0xbc100040) = 0x02;       // allow 64MB ram, probably better (default is 16MB)
  asm("sync");
  ((FCall)_meLoop)();
}

static int meInit() {
  void* start = &__start__me_section;
  const u32 size = (u32)(&__stop__me_section - (u32)start);
  memcpy((void *)0xbfc00040, start, size);
  _meLoop = (u32)&meLoop;
  meDCacheWritebackInvalidAll();
  vrg(0xBC10004C) |= 0x04;        // 0b0100;  // reset enable, just the me
  asm("sync");
  vrg(0xBC10004C) = 0x0;          // disable reset to start the me
  asm("sync"); 
  return 0;
}

int main(int argc, char **argv) {
  scePowerSetClockFrequency(333, 333, 166);

  if (pspSdkLoadStartModule("ms0:/PSP/GAME/me/kcall.prx", PSP_MEMORY_PARTITION_KERNEL) < 0){
    sceKernelExitGame();
    return 0;
  }
  
  pspDebugScreenInit();
  kcall(&meInit);
  
  SceCtrlData ctl;
  do {
    meDCacheWritebackInvalidAll();
    pspDebugScreenSetXY(0, 1);
    pspDebugScreenPrintf("%u                                       ", counter);
    sceCtrlPeekBufferPositive(&ctl, 1);
    sceKernelDelayThread(10);
  } while(!(ctl.Buttons & PSP_CTRL_HOME));
  
  meExit();
  pspDebugScreenClear();
  pspDebugScreenSetXY(0, 1);
  pspDebugScreenPrintf("Exiting...");
  sceKernelDelayThread(500000);
  sceKernelExitGame();
  return 0;
}
