//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _PXA_PWM_H_
#define _PXA_PWM_H_

#include "mem.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PXA_PWM0_BASE 0x40B00000UL
#define PXA_PWM1_BASE 0x40C00000UL
// PXA27x only
#define PXA_PWM2_BASE 0x40B00010UL
#define PXA_PWM3_BASE 0x40C00010UL

struct PxaPwm;

struct PxaPwm* pxaPwmInit(struct ArmMem* physMem, uint32_t base);

#ifdef __cplusplus
}

template <typename T>
void pxaPwmSave(struct PxaPwm* pwm, T& savestate, uint32_t index);

template <typename T>
void pxaPwmLoad(struct PxaPwm* pwm, T& loader, uint32_t index);

#endif

#endif
