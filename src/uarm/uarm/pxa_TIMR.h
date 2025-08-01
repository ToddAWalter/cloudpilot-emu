//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _PXA_TIMR_H_
#define _PXA_TIMR_H_

#include <stdint.h>

#include "CPU.h"
#include "mem.h"
#include "soc_IC.h"

#ifdef __cplusplus
extern "C" {
#endif

struct PxaTimr;

struct PxaTimr* pxaTimrInit(struct ArmMem* physMem, struct SocIc* ic);

void pxaTimrTick(struct PxaTimr* timr, uint32_t batchedClocks);

uint32_t pxaTimrTicksToNextInterrupt(struct PxaTimr* timr);

void pxaTimrSuspendInterrupts(struct PxaTimr* timr, bool suspendInterrupts);

#ifdef __cplusplus
}

template <typename T>
void pxaTimrSave(struct PxaTimr* timr, T& savestate);

template <typename T>
void pxaTimrLoad(struct PxaTimr* timr, T& loader);
#endif

#endif
