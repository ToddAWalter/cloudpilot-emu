/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
        Copyright (c) 2000-2001 Palm, Inc. or its subsidiaries.
        All rights reserved.

        This file is part of the Palm OS Emulator.

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2 of the License, or
        (at your option) any later version.
\* ===================================================================== */

#ifndef EmRegsSED1376_h
#define EmRegsSED1376_h

#include "EmHAL.h"  // EmHALHandler
#include "EmPalmStructs.h"
#include "EmRegs.h"
#include "EmStructs.h"  // RGBList

struct Frame;
class EmRegsFrameBuffer;

#define sed1376RegsAddr (0x1FF80000)
#define sed1376VideoMemStart (0x1FFA0000)
#define sed1376VideoMemSize (80 * 1024L)

class EmRegsSED1376 : public EmRegs, public EmHALHandler {
   public:
    static constexpr uint32 FRAMEBUFFER_SIZE = 80 * 1024;

   public:
    EmRegsSED1376(emuptr baseRegsAddr, emuptr baseVideoAddr, EmRegsFrameBuffer& framebuffer);
    virtual ~EmRegsSED1376(void);

    // EmRegs overrides
    virtual void Initialize(void);
    virtual void Reset(Bool hardwareReset);
    virtual void Save(Savestate<ChunkType>&);
    virtual void Save(SavestateProbe<ChunkType>&);
    virtual void Load(SavestateLoader<ChunkType>&);
    virtual void Dispose(void);

    virtual void SetSubBankHandlers(void);
    virtual uint8* GetRealAddress(emuptr address);
    virtual emuptr GetAddressStart(void);
    virtual uint32 GetAddressRange(void);

    // EmHAL overrides
    virtual Bool GetLCDScreenOn(void);
    virtual Bool GetLCDBacklightOn(void);
    virtual Bool GetLCDHasFrame(void);

   private:
    uint32 powerSaveConfigurationRead(emuptr address, int size);

    void invalidateWrite(emuptr address, int size, uint32 value);
    void lutWriteAddressWrite(emuptr address, int size, uint32 value);
    void lutReadAddressWrite(emuptr address, int size, uint32 value);

    template <typename T>
    void DoSave(T& savestate);

    template <typename T>
    void DoSaveLoad(T& helper, uint32 version);

   protected:
    uint32* GetLUT(bool mono);

   protected:
    emuptr fBaseRegsAddr;
    emuptr fBaseVideoAddr;
    EmProxySED1376RegsType fRegs;
    EmRegsFrameBuffer& framebuffer;

    uint32 fClutData[256];
    uint32 fClutDataMono[256];
    Bool lutDirty{true};
};

#if 0

class EmRegsSED1376VisorPrism : public EmRegsSED1376 {
   public:
    EmRegsSED1376VisorPrism(emuptr baseRegsAddr, emuptr baseVideoAddr);
    virtual ~EmRegsSED1376VisorPrism(void);

    virtual void SetSubBankHandlers(void);

    virtual void GetLCDScanlines(EmScreenUpdateInfo& info);

   private:
    void reservedWrite(emuptr address, int size, uint32 value);

   private:
    int32 PrvGetLCDDepth(void);
};

#endif

class EmRegsSED1376PalmGeneric : public EmRegsSED1376 {
   public:
    EmRegsSED1376PalmGeneric(emuptr baseRegsAddr, emuptr baseVideoAddr,
                             EmRegsFrameBuffer& framebuffer);
    virtual ~EmRegsSED1376PalmGeneric(void);

    virtual bool CopyLCDFrame(Frame& frame, bool fullRefresh);
    virtual uint16 GetLCD2bitMapping();
};

#endif /* EmRegsSED1376_h */
