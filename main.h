#pragma once
#include <cstring>
#include <psppower.h>
#include <pspgu.h>
#include <pspdisplay.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include <cstring>
#include <malloc.h>
#include "kcall.h"
#include "random.h"

#define u8  unsigned char
#define u16 unsigned short int
#define u32 unsigned int

#define nrp          u32*
#define nrg(addr)    (*((nrp)(addr)))

#define vrp          volatile u32*
#define vrg(addr)    (*((vrp)(addr)))

#define me_section_size (&__stop__me_section - &__start__me_section)
#define _meLoop      vrg((0xbfc00040 + me_section_size))

// me
// __attribute__((optimize("unroll-loops")))
static inline void meDCacheWritebackInvalidAll() {
 asm("sync");
 for (int i = 0; i < 8192; i += 64) {
  asm("cache 0x14, 0(%0)" :: "r"(i));
  asm("cache 0x14, 0(%0)" :: "r"(i));
 }
 asm("sync");
}

static volatile bool _meExit = false;
static inline void meExit() {
  _meExit = true;
  meDCacheWritebackInvalidAll();
}

// gu
#define BUF_WIDTH   512
#define SCR_WIDTH   480
#define SCR_HEIGHT  272

struct Vertex {
  u32 color;
  u16 x, y, z;
} __attribute__((aligned(4)));

// me list & ge
#define MAX_LIST_MEMORY 2048
static volatile u32* meListBase = nullptr;
static volatile u32* meList = nullptr;

static volatile u32 cmdCursor = 0;
static inline void sendGeCommand(const u32 cmd, const u32 value) {
  asm("sync");
  meList[cmdCursor++] = (cmd << 24) | (value & 0x00FFFFFF);
  asm("sync");
}

static inline void meListSetMemory(const bool clean = false) {
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

static inline void* meListGetMemory(const u32 size, const bool reset) { //
 static u32 cursor = MAX_LIST_MEMORY;
 if (reset) {
  cursor = MAX_LIST_MEMORY;
 }
 cursor -= size;
 return (void*)(cursor + (u32)meListBase);
}

static volatile bool meListRefresh = false;
static inline u32* meListSwitch() {
  u32* const _meList = (u32*)meList;
  // a lock/try-lock could have been used here, see me-sample-lock.
  // e.g., tryLock; const bool refresh = meListRefresh;
  // if (!refresh) { ... meListRefresh = true; }
  // unlock;
  if (meListRefresh) {
    static u32 offset = 0;
    offset ^= (((u32)MAX_LIST_MEMORY) >> 1);
    asm("sync");
    meList = (u32*)(((u32)meListBase) + offset);
    meListRefresh = false;
    asm("sync");
  }
  return _meList;
}

static inline void meListWaitRefresh() {
  while(!meListRefresh) {
    meDCacheWritebackInvalidAll();
    sceKernelDelayThread(1);
  }
}
