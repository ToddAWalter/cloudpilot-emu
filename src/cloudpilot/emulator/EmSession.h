#ifndef _EM_SESSION_H_
#define _EM_SESSION_H_

#include <cstddef>
#include <memory>
#include <utility>

#include "ButtonEvent.h"
#include "EmCPU.h"
#include "EmCommon.h"
#include "EmDevice.h"
#include "EmEvent.h"
#include "EmHAL.h"
#include "EmThreadSafeQueue.h"
#include "EmTransportSerial.h"
#include "EmTransportSerialNull.h"
#include "KeyboardEvent.h"
#include "PenEvent.h"
#include "savestate/ChunkType.h"
#include "savestate/Savestate.h"

template <typename ChunkType>
class SavestateLoader;

class SessionImage;

class EmSession {
   public:
    enum class ResetType : uint8 { sys = 0x01, soft = 0x02, noext = 0x03, hard = 0x04 };

   public:
    bool Initialize(EmDevice* device, const uint8* romImage, size_t romLength);

    bool SaveImage(SessionImage& image);
    bool LoadImage(SessionImage& image);

    template <typename T>
    void Save(T& savestate);
    void Load(SavestateLoader<ChunkType>& loader);

    bool Save();
    bool Load(size_t size, uint8* buffer);

    void Reset(ResetType);

    Savestate<ChunkType>& GetSavestate();
    pair<size_t, uint8*> GetRomImage();

    uint32 RunEmulation(uint32 maxCycles = 10000);

    bool IsPowerOn();
    bool IsCpuStopped();

    EmDevice& GetDevice();
    uint32 GetRandomSeed() const;

    uint32 GetMemorySize() const;
    uint8* GetMemoryPtr() const;
    uint8* GetDirtyPagesPtr() const;

    void QueuePenEvent(PenEvent evt);
    void QueueKeyboardEvent(KeyboardEvent evt);
    void QueueButtonEvent(ButtonEvent evt);

    void SetHotsyncUserName(string hotsyncUserName);

    void SetClockFactor(double clockFactor);
    uint32 GetClocksPerSecond() const { return clocksPerSecond; }
    uint64 GetSystemCycles() const { return systemCycles; }

    bool LaunchAppByName(const string& name);

    void SetTransportSerial(EmUARTDeviceType type, EmTransportSerial* transport);

    ///////////////////////////////////////////////////////////////////////////
    // Internal stuff
    ///////////////////////////////////////////////////////////////////////////

    void Deinitialize();

    inline bool IsNested() const;

    void ReleaseBootKeys();

    void ExecuteSpecial();
    bool CheckForBreak() const;
    void ScheduleResetBanks();
    void ScheduleReset(ResetType resetType);

    void ExecuteSubroutine();
    void ScheduleSubroutineReturn();

    void YieldMemoryMgr();

    void HandleInstructionBreak();

    bool HasButtonEvent();
    ButtonEvent NextButtonEvent();

    void TriggerDeadMansSwitch();

    EmTransportSerial* GetTransportSerial(EmUARTDeviceType);

   private:
    template <typename T>
    void DoSaveLoad(T& helper, uint32 version);

    bool PromoteKeyboardEvent();
    bool PromotePenEvent();

    void RecalculateClocksPerSecond();

    void CheckDayForRollover();

    void UpdateUARTModeSync();

   private:
    bool bankResetScheduled{false};
    bool resetScheduled{false};
    ResetType resetType;

    int nestLevel{0};
    bool subroutineReturn{false};

    bool isInitialized{false};
    shared_ptr<EmDevice> device{nullptr};
    unique_ptr<EmCPU> cpu{nullptr};
    typename EmEvent<>::HandleT onSystemClockChangeHandle;

    EmThreadSafeQueue<ButtonEvent> buttonEventQueue{20};
    uint64 lastButtonEventReadAt{0};

    uint64 systemCycles{0};
    uint32 extraCycles{0};
    double clockFactor{1};

    uint64 dateCheckedAt{0};
    uint32 lastDate{0};

    uint32 clocksPerSecond;

    bool holdingBootKeys;
    ResetType bootKeysType;

    unique_ptr<uint8[]> romImage;
    size_t romSize{0};
    Savestate<ChunkType> savestate;

    bool deadMansSwitch{false};

    EmTransportSerialNull defaultTransportIR;
    EmTransportSerialNull defaultTransportSerial;

    unique_ptr<EmTransportSerial> transportIR;
    unique_ptr<EmTransportSerial> transportSerial;
    int transportIrRequiresSyncChangedHandle{-1};
    int transportSerialRequiresSyncChangedHandle{-1};
};

extern EmSession* gSession;

///////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION
///////////////////////////////////////////////////////////////////////////////

bool EmSession::IsNested() const {
    EmAssert(nestLevel >= 0);
    return nestLevel > 0;
}

#endif  // _EM_SESSION_H
