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

#define hwp          volatile u32*
#define hw(addr)     (*((hwp)(addr)))

// me
#define UNCACHED_USER_MASK    0x40000000
#define CACHED_KERNEL_MASK    0x80000000
#define UNCACHED_KERNEL_MASK  0xA0000000

#define ME_HANDLER_BASE       0xbfc00000

// __attribute__((optimize("unroll-loops")))
static inline void meDCacheWritebackInvalidAll() {
 asm("sync");
 for (int i = 0; i < 8192; i += 64) {
  asm("cache 0x14, 0(%0)" :: "r"(i));
  asm("cache 0x14, 0(%0)" :: "r"(i));
 }
 asm("sync");
}

inline void meHalt() {
  asm volatile(".word 0x70000000");
}

inline void meGetUncached32(volatile u32** const mem, const u32 size) {
  static void* _base = nullptr;
  if (!_base) {
    _base = memalign(16, size*4);
    memset(_base, 0, size);
    *mem = (u32*)(UNCACHED_USER_MASK | (u32)_base);
    sceKernelDcacheWritebackInvalidateAll();
    return;
  } else if (!size) {
    free(_base);
  }
  *mem = nullptr;
  sceKernelDcacheWritebackInvalidateAll();
  return;
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
static volatile u32* mem = nullptr;
#define meCounter         (mem[0])
#define meExit            (mem[1])
#define meListBase        (mem[2])
#define meList            (mem[3])
#define cmdCursor         (mem[4])
#define meListRefresh     (mem[5])

static inline void sendGeCommand(const u32 cmd, const u32 value) {
  asm("sync");
  ((u32*)meList)[cmdCursor++] = (cmd << 24) | (value & 0x00FFFFFF);
  asm("sync");
}

static inline void meListSetMemory(const bool clean = false) {
  static u32* _base = nullptr;
  if (!_base) {
    _base = (u32*)memalign(16, MAX_LIST_MEMORY);
    memset(_base, 0, MAX_LIST_MEMORY);
    sceKernelDcacheWritebackInvalidateAll();
  }
  if (!clean) {
    meListBase = (UNCACHED_USER_MASK | (u32)_base);
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
  return (void*)(cursor + meListBase);
}

static inline u32* meListSwitch() {
  const u32 _meList = meList;
  // a lock/try-lock could have been used here, see me-sample-lock.
  // e.g., tryLock; const bool refresh = meListRefresh;
  // if (!refresh) { ... meListRefresh = true; }
  // unlock;
  if (meListRefresh) {
    static u32 offset = 0;
    offset ^= (((u32)MAX_LIST_MEMORY) >> 1);
    asm("sync");
    meList = meListBase + offset;
    meListRefresh = 0;
    asm("sync");
  }
  return (u32*)_meList;
}

static inline void meListWaitRefresh() {
  while (!meListRefresh) {
    sceKernelDcacheWritebackInvalidateAll();
    sceKernelDelayThread(1);
  }
}
