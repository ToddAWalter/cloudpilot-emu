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

#include "EmRegsFrameBuffer.h"

#include "Byteswapping.h"  // ByteswapWords
#include "EmCommon.h"
#include "EmMemory.h"  // EmMemDoGet32
#include "EmSystemState.h"
#include "MemoryRegion.h"
#include "Miscellaneous.h"  // StWordSwapper
#include "Platform.h"       // Platform::AllocateMemoryClear
#include "savestate/Savestate.h"
#include "savestate/SavestateLoader.h"
#include "savestate/SavestateProbe.h"

namespace {
    constexpr int SAVESTATE_VERSION = 1;

    uint32 framebufferSize;
    uint8* framebuffer;
    uint8* dirtyPages;

    inline void markDirty(emuptr offset) {
        dirtyPages[offset >> 13] |= (1 << ((offset >> 10) & 0x07));
    }
}  // namespace

// ---------------------------------------------------------------------------
//		� EmRegsFrameBuffer::EmRegsFrameBuffer
// ---------------------------------------------------------------------------

EmRegsFrameBuffer::EmRegsFrameBuffer(emuptr baseAddr) : fBaseAddr(baseAddr) {}

// ---------------------------------------------------------------------------
//		� EmRegsFrameBuffer::~EmRegsFrameBuffer
// ---------------------------------------------------------------------------

EmRegsFrameBuffer::~EmRegsFrameBuffer(void) {}

// ---------------------------------------------------------------------------
//		� EmRegsFrameBuffer::Initialize
// ---------------------------------------------------------------------------

void EmRegsFrameBuffer::Initialize(void) {
    framebufferSize = EmMemory::GetRegionSize(MemoryRegion::framebuffer);
    framebuffer = EmMemory::GetForRegion(MemoryRegion::framebuffer);
    dirtyPages = EmMemory::GetDirtyPagesForRegion(MemoryRegion::framebuffer);

    EmAssert(framebufferSize > 0);
    EmAssert(framebuffer);
    EmAssert(dirtyPages);

    EmRegs::Initialize();
    memset(framebuffer, 0, framebufferSize);
}

// ---------------------------------------------------------------------------
//		� EmRegsFrameBuffer::Reset
// ---------------------------------------------------------------------------

void EmRegsFrameBuffer::Reset(Bool hardwareReset) { EmRegs::Reset(hardwareReset); }

void EmRegsFrameBuffer::Load(SavestateLoader<ChunkType>& loader) {
    if (!loader.HasChunk(ChunkType::regsFrameBuffer)) return;

    Chunk* chunk = loader.GetChunk(ChunkType::regsFrameBuffer);
    if (!chunk) return;

    if (chunk->Get32() > SAVESTATE_VERSION) {
        logPrintf("unable to restore regsFrameBuffer: unsupported savestate version\n");
        loader.NotifyError();

        return;
    }

    // We used to save the framebuffer in the savestate, but we have since moved
    // to appending it to the memory and saving it page by page. Savestate contains
    // framebuffer? -> load it and make sure the corrersponding pages will all be
    // saved.
    //
    // NOT ENDIANESS SAFE
    chunk->GetBuffer(framebuffer, framebufferSize);
    memset(dirtyPages, 0xff, framebufferSize / 1024 / 8);
}

// ---------------------------------------------------------------------------
//		� EmRegsFrameBuffer::GetLong
// ---------------------------------------------------------------------------

uint32 EmRegsFrameBuffer::GetLong(emuptr address) {
    uint32 offset = address - fBaseAddr;
    return EmMemDoGet32(framebuffer + offset);
}

// ---------------------------------------------------------------------------
//		� EmRegsFrameBuffer::GetWord
// ---------------------------------------------------------------------------

uint32 EmRegsFrameBuffer::GetWord(emuptr address) {
    uint32 offset = address - fBaseAddr;
    return EmMemDoGet16((framebuffer) + offset);
}

// ---------------------------------------------------------------------------
//		� EmRegsFrameBuffer::GetByte
// ---------------------------------------------------------------------------

uint32 EmRegsFrameBuffer::GetByte(emuptr address) {
    uint32 offset = address - fBaseAddr;
    return EmMemDoGet8((framebuffer) + offset);
}

// ---------------------------------------------------------------------------
//		� EmRegsFrameBuffer::SetLong
// ---------------------------------------------------------------------------

void EmRegsFrameBuffer::SetLong(emuptr address, uint32 value) {
    uint32 offset = address - fBaseAddr;
    EmMemDoPut32((framebuffer) + offset, value);

    gSystemState.MarkScreenDirty(address, address + 4);

    markDirty(offset);
    markDirty(offset + 2);
}

// ---------------------------------------------------------------------------
//		� EmRegsFrameBuffer::SetWord
// ---------------------------------------------------------------------------

void EmRegsFrameBuffer::SetWord(emuptr address, uint32 value) {
    uint32 offset = address - fBaseAddr;
    EmMemDoPut16((framebuffer) + offset, value);

    gSystemState.MarkScreenDirty(address, address + 2);

    markDirty(offset);
}

// ---------------------------------------------------------------------------
//		� EmRegsFrameBuffer::SetByte
// ---------------------------------------------------------------------------

void EmRegsFrameBuffer::SetByte(emuptr address, uint32 value) {
    uint32 offset = address - fBaseAddr;
    EmMemDoPut8((framebuffer) + offset, value);

    gSystemState.MarkScreenDirty(address, address);

    markDirty(offset);
}

// ---------------------------------------------------------------------------
//		� EmRegsFrameBuffer::ValidAddress
// ---------------------------------------------------------------------------

int EmRegsFrameBuffer::ValidAddress(emuptr address, uint32 size) {
    UNUSED_PARAM(address);
    UNUSED_PARAM(size);

    return true;
}

// ---------------------------------------------------------------------------
//		� EmRegsFrameBuffer::SetSubBankHandlers
// ---------------------------------------------------------------------------

void EmRegsFrameBuffer::SetSubBankHandlers(void) {
    // We don't have handlers for each byte of memory, so there's nothing
    // to install here.
}

// ---------------------------------------------------------------------------
//		� EmRegsFrameBuffer::GetRealAddress
// ---------------------------------------------------------------------------

uint8* EmRegsFrameBuffer::GetRealAddress(emuptr address) {
    uint32 offset = address - fBaseAddr;
    return framebuffer + offset;
}

// ---------------------------------------------------------------------------
//		� EmRegsFrameBuffer::GetAddressStart
// ---------------------------------------------------------------------------

emuptr EmRegsFrameBuffer::GetAddressStart(void) { return fBaseAddr; }

// ---------------------------------------------------------------------------
//		� EmRegsFrameBuffer::GetAddressRange
// ---------------------------------------------------------------------------

uint32 EmRegsFrameBuffer::GetAddressRange(void) { return framebufferSize; }
