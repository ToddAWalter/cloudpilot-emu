#ifndef _EM_REGS_MB86189_
#define _EM_REGS_MB86189_

#include <functional>

#include "EmCommon.h"
#include "EmEvent.h"
#include "EmHAL.h"
#include "EmRegs.h"
#include "Fifo.h"
#include "MemoryStick.h"

class EmRegsMB86189 : public EmRegs, public EmHALHandler {
   public:
    EmRegsMB86189(emuptr baseAddress);

    void Initialize() override;
    void Reset(Bool hardwareReset) override;

    void Save(Savestate<ChunkType>&) override;
    void Save(SavestateProbe<ChunkType>&) override;
    void Load(SavestateLoader<ChunkType>&) override;

    void SetGpioReadHandler(function<uint8()> handler);

    uint8* GetRealAddress(emuptr address) override;
    emuptr GetAddressStart(void) override;
    uint32 GetAddressRange(void) override;
    void SetSubBankHandlers(void) override;

    bool SupportsImageInSlot(EmHAL::Slot slot, uint32 blocksTotal) override;
    void Mount(EmHAL::Slot slot, CardImage& cardImage) override;
    void Unmount(EmHAL::Slot slot) override;
    void Remount(EmHAL::Slot slot, CardImage& cardImage) override;

    bool GetIrq();
    void NotifyGpioChanged();

   private:
    struct Registers {
        uint16 mscmd;
        uint16 mscs;
        uint16 msics;
        uint16 msppcd;
    };

    enum class State : uint8 { idle = 0, tpcWrite = 1, tpcRead = 2 };

   private:
    template <typename T>
    void DoSave(T& savestate);

    template <typename T>
    void DoSaveLoad(T& helper);

    void ResetHostController();

    void RaiseIrq(uint8 bits);
    void NegateIrq(uint8 bits);
    void ClearIrqFlags(uint8 bits);
    void UpdateIrqLine();
    void TransferIrqFlags(uint8 bits);

    void SetState(State state);

    void BeginTpc();
    void FinishTpc();

    void FifoWrite(uint16 data);

    uint32 GetTransferSize();

    uint32 mscmdRead(emuptr address, int size);
    uint32 mscsRead(emuptr address, int size);
    uint32 msdataRead(emuptr address, int size);
    uint32 msicsRead(emuptr address, int size);
    uint32 msppcdRead(emuptr address, int size);

    void mscmdWrite(emuptr address, int size, uint32 value);
    void mscsWrite(emuptr address, int size, uint32 value);
    void msdataWrite(emuptr address, int size, uint32 value);
    void msicsWrite(emuptr address, int size, uint32 value);
    void msppcdWrite(emuptr address, int size, uint32 value);

    uint32 stubRead(emuptr address, int size);
    void stubWrite(emuptr address, int size, uint32 value);

    inline uint32 compositeRegisterRead(emuptr base, emuptr address, int size, uint16 target);
    inline void compositeRegisterWrite(emuptr base, emuptr address, int size, uint32 value,
                                       uint16& target);

   public:
    EmEvent<bool> irqChange;

   private:
    emuptr baseAddress;
    Registers reg;

    uint8 irqStat{0};
    MemoryStick memoryStick;

    State state{State::idle};

    Fifo<uint16> fifo{4};

    uint8 writeBuffer[512];
    uint32 writeBufferSize{0};
    uint32 readBufferIndex{0};

    function<uint8()> gpioReadHandler{[]() { return 0; }};

   private:
    EmRegsMB86189(const EmRegsMB86189&) = delete;
    EmRegsMB86189(EmRegsMB86189&&) = delete;
    EmRegsMB86189& operator=(const EmRegsMB86189&) = delete;
    EmRegsMB86189& operator=(EmRegsMB86189&&) = delete;
};

#endif  // _EM_REGS_MB86189_
