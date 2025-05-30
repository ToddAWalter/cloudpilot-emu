/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
        Copyright (c) 1998-2001 Palm, Inc. or its subsidiaries.
        All rights reserved.

        This file is part of the Palm OS Emulator.

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2 of the License, or
        (at your option) any later version.
\* ===================================================================== */

#ifndef EmCPU_h
#define EmCPU_h

#include "EmCommon.h"
#include "savestate/ChunkType.h"

template <typename ChunkType>
class SavestateProbe;

template <typename ChunkType>
class Savestate;

template <typename ChunkType>
class SavestateLoader;

class EmSession;

class EmCPU;
extern EmCPU* gCPU;

class EmCPU {
   public:
    // -----------------------------------------------------------------------------
    // constructor / destructor
    // -----------------------------------------------------------------------------

    EmCPU(EmSession*);
    virtual ~EmCPU(void);

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    // Standard sub-system methods:
    //		Reset:	Resets the state.  Called on hardware resets or on
    //				calls to SysReset.  Also called from constructor.
    //		Save:	Saves the state to the given file.
    //		Load:	Loads the state from the given file.  Can assume that
    //				Reset has been called first.

    virtual void Reset(Bool hardwareReset);
    virtual void Save(Savestate<ChunkType>&);
    virtual void Save(SavestateProbe<ChunkType>&);
    virtual void Load(SavestateLoader<ChunkType>&);

    // Execute the main CPU loop until asked to stop.

    virtual uint32 Execute(uint32 maxCycles) = 0;
    virtual void CheckAfterCycle(void) = 0;

    // Low-level access to CPU state.

    virtual emuptr GetPC(void) = 0;
    virtual emuptr GetSP(void) = 0;
    virtual uint32 GetRegister(int) = 0;

    virtual void SetPC(emuptr) = 0;
    virtual void SetSP(emuptr) = 0;
    virtual void SetRegister(int, uint32) = 0;

    virtual Bool Stopped(void) = 0;

   protected:
    EmSession* fSession;
};

#endif /* EmCPU_h */
