//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _CPU_H_
#define _CPU_H_

struct ArmCpu;

#include <stdbool.h>
#include <stdint.h>

#include "mem.h"
#include "pace_patch.h"

#ifdef __cplusplus

    #include <functional>

extern "C" {
#endif

#define ARM_SR_N 0x80000000UL
#define ARM_SR_Z 0x40000000UL
#define ARM_SR_C 0x20000000UL
#define ARM_SR_V 0x10000000UL
#define ARM_SR_Q 0x08000000UL
#define ARM_SR_I 0x00000080UL
#define ARM_SR_F 0x00000040UL
#define ARM_SR_T 0x00000020UL
#define ARM_SR_M 0x0000001FUL

#define ARM_SR_MODE_USR 0x00000010UL
#define ARM_SR_MODE_FIQ 0x00000011UL
#define ARM_SR_MODE_IRQ 0x00000012UL
#define ARM_SR_MODE_SVC 0x00000013UL
#define ARM_SR_MODE_ABT 0x00000017UL
#define ARM_SR_MODE_UND 0x0000001BUL
#define ARM_SR_MODE_SYS 0x0000001FUL

#define ARV_VECTOR_OFFT_RST 0x00000000UL
#define ARM_VECTOR_OFFT_UND 0x00000004UL
#define ARM_VECTOR_OFFT_SWI 0x00000008UL
#define ARM_VECTOR_OFFT_P_ABT 0x0000000CUL
#define ARM_VECTOR_OFFT_D_ABT 0x00000010UL
#define ARM_VECTOR_OFFT_UNUSED 0x00000014UL
#define ARM_VECTOR_OFFT_IRQ 0x00000018UL
#define ARM_VECTOR_OFFT_FIQ 0x0000001CUL

// the following are for cpuGetRegExternal() and are generally used for debugging purposes
#define ARM_REG_NUM_CPSR 16
#define ARM_REG_NUM_SPSR 17

#define SLOW_PATH_REASON_EVENTS 0x01
#define SLOW_PATH_REASON_SLEEP 0x02
#define SLOW_PATH_REASON_CP15 0x04
#define SLOW_PATH_REASON_PATCH_PENDING 0x08
#define SLOW_PATH_REASON_INSTRUCTION_SET_CHANGE 0x10
#define SLOW_PATH_REASON_RESCHEDULE 0x20
#define SLOW_PATH_REASON_PACE_SYSCALL_BREAK 0x40
#define SLOW_PATH_REASON_INJECTED_CALL_DONE 0x80

typedef bool (*ArmCoprocRegXferF)(struct ArmCpu *cpu, void *userData, bool two /* MCR2/MRC2 ? */,
                                  bool MRC, uint8_t op1, uint8_t Rx, uint8_t CRn, uint8_t CRm,
                                  uint8_t op2);
typedef bool (*ArmCoprocDatProcF)(struct ArmCpu *cpu, void *userData, bool two /* CDP2 ? */,
                                  uint8_t op1, uint8_t CRd, uint8_t CRn, uint8_t CRm, uint8_t op2);
typedef bool (*ArmCoprocMemAccsF)(
    struct ArmCpu *cpu, void *userData, bool two /* LDC2/STC2 ? */, bool N, bool store, uint8_t CRd,
    uint8_t addrReg, uint32_t addBefore, uint32_t addAfter,
    uint8_t
        *option /* NULL if none */);  /// addBefore/addAfter are UNSCALED. spec syas *4, but WMMX
                                      /// has other ideas. exercise caution. writeback is ON YOU!
typedef bool (*ArmCoprocTwoRegF)(struct ArmCpu *cpu, void *userData, bool MRRC, uint8_t op,
                                 uint8_t Rd, uint8_t Rn, uint8_t CRm);

struct ArmMmu;
struct ArmMem;
struct PatchDispatch;

struct ArmCoprocessor {
    ArmCoprocRegXferF regXfer;
    ArmCoprocDatProcF dataProcessing;
    ArmCoprocMemAccsF memAccess;
    ArmCoprocTwoRegF twoRegF;
    void *userData;
};

struct ArmCpu *cpuInit(uint32_t pc, struct ArmMem *mem, bool xscale, bool omap, int debugPort,
                       uint32_t cpuid, uint32_t cacheId, struct PatchDispatch *patchDispatch,
                       struct PacePatch *pacePatch);

uint32_t *cpuGetRegisters(struct ArmCpu *cpu);

void cpuStartInjectedSyscall(struct ArmCpu *cpu, uint32_t syscall);

void cpuStartInjectedSyscall68k(struct ArmCpu *cpu, uint16_t syscall);
void cpuFinishInjectedSyscall68k(struct ArmCpu *cpu);

void cpuReset(struct ArmCpu *cpu, uint32_t pc);

void cpuIrq(struct ArmCpu *cpu, bool fiq, bool raise);  // unraise when acknowledged

uint32_t cpuGetRegExternal(struct ArmCpu *cpu, uint_fast8_t reg);
void cpuSetReg(struct ArmCpu *cpu, uint_fast8_t reg, uint32_t val);
bool cpuMemOpExternal(struct ArmCpu *cpu, void *buf, uint32_t vaddr, uint_fast8_t size,
                      bool write);  // for external use

void cpuCoprocessorRegister(struct ArmCpu *cpu, uint8_t cpNum, struct ArmCoprocessor *coproc);

void cpuSetVectorAddr(struct ArmCpu *cpu, uint32_t adr);
void cpuSetPid(struct ArmCpu *cpu, uint32_t pid);
uint32_t cpuGetPid(struct ArmCpu *cpu);

uint16_t cpuGetCPAR(struct ArmCpu *cpu);
void cpuSetCPAR(struct ArmCpu *cpu, uint16_t cpar);

void cpuSetSleeping(struct ArmCpu *cpu);
void cpuWakeup(struct ArmCpu *cpu);

uint32_t cpuDecodeArm(uint32_t instr);
uint32_t cpuDecodeThumb(uint32_t instr);

void cpuSetSlowPath(struct ArmCpu *cpu, uint32_t reason);
void cpuClearSlowPath(struct ArmCpu *cpu, uint32_t reason);
uint32_t cpuGetSlowPathReason(struct ArmCpu *cpu);

void cpuSetBreakPaceSyscall(struct ArmCpu *cpu, uint16_t syscall);

struct ArmMmu *cpuGetMMU(struct ArmCpu *cpu);
struct ArmMem *cpuGetMem(struct ArmCpu *cpu);

#ifdef __cplusplus
}

template <bool monitorPC>
uint32_t cpuCycle(struct ArmCpu *cpu, uint32_t cycles);

uint32_t cpuCyclePure(struct ArmCpu *cpu);

using M68kTrapHandler = std::function<bool(struct ArmCpu *, uint32_t address)>;
void cpuAddM68kTrap0Handler(struct ArmCpu *cpu, uint32_t address, M68kTrapHandler handler);
void cpuRemoveM68kTrap0Handler(struct ArmCpu *cpu, uint32_t address);

bool cpuIsModePace(struct ArmCpu *cpu);
void cpuSetModePace(struct ArmCpu *cpu, bool modePace);

bool cpuIsThumb(struct ArmCpu *cpu);

uint32_t cpuGetCurInstrPC(struct ArmCpu *cpu);
void cpuSetCurInstrPC(struct ArmCpu *cpu, uint32_t curInstrPC);

template <typename T>
void cpuSave(struct ArmCpu *cpu, T &savestate);

template <typename T>
void cpuLoad(struct ArmCpu *cpu, T &loader);

template <typename T>
void cpuPrepareInjectedCall(struct ArmCpu *cpu, T &savestate);

template <typename T>
void cpuFinishInjectedCall(struct ArmCpu *cpu, T &loader);
#endif

#endif
