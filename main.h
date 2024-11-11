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

#define nrp          u32*
#define nrg(addr)    (*((nrp)(addr)))

#define vrp          volatile u32*
#define vrg(addr)    (*((vrp)(addr)))

#define me_section_size (&__stop__me_section - &__start__me_section)
#define _meLoop      vrg((0xbfc00040 + me_section_size))

// me
// __attribute__((optimize("unroll-loops")))
inline void meDCacheWritebackInvalidAll() {
 for (int i = 0; i < 8192; i += 64) {
  asm("cache 0x14, 0(%0)" :: "r"(i));
  asm("cache 0x14, 0(%0)" :: "r"(i));
 }
 asm("sync");
}

// gu
#define BUF_WIDTH   512
#define SCR_WIDTH   480
#define SCR_HEIGHT  272

struct Vertex {
  u32 color;
  u16 x, y, z;
  u16 pad;
} __attribute__((aligned(4)));
