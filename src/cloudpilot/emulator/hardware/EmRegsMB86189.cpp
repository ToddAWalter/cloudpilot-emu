#include "EmRegsMB86189.h"

#include "savestate/ChunkHelper.h"
#include "savestate/Savestate.h"
#include "savestate/SavestateLoader.h"
#include "savestate/SavestateProbe.h"

// #define TRACE_ACCESS

#define INSTALL_HANDLER(read, write, offset, size)                                           \
    SetHandler((ReadFunction) & EmRegsMB86189::read, (WriteFunction) & EmRegsMB86189::write, \
               baseAddress + offset, size)

namespace {
    constexpr uint32 SAVESTATE_VERSION = 1;

    constexpr uint32 REGISTER_FILE_SIZE = 0x14;

    constexpr uint32 OFFSET_MSCMD = 0x00;
    constexpr uint32 OFFSET_MSCS = 0x02;
    constexpr uint32 OFSSET_MSDATA = 0x04;
    constexpr uint32 OFFSET_MSICS = 0x06;
    constexpr uint32 OFFSET_MSPPCD = 0x08;

    constexpr uint16 MSCS_INT = 0x8000;
    constexpr uint16 MSCS_RST = 0x0080;
    constexpr uint16 MSCS_DRQ = 0x4000;
    constexpr uint16 MSCS_RBE = 0x0800;
    constexpr uint16 MSCS_RBF = 0x0400;

    constexpr uint8 IRQ_RDY = 0x80;
    constexpr uint8 IRQ_SIF = 0x40;
    constexpr uint8 IRQ_PIN = 0x10;

    constexpr uint16 MSICS_INTEN = 0x0080;

    constexpr uint16 DATA_SIZE_MASK = 0x03ff;
}  // namespace

EmRegsMB86189::EmRegsMB86189(emuptr baseAddress) : baseAddress(baseAddress) {
    memoryStick.irq.AddHandler([=]() { RaiseIrq(IRQ_SIF); });
}

void EmRegsMB86189::Initialize() {
    EmRegs::Initialize();
    memoryStick.Initialize();
}

void EmRegsMB86189::Reset(Bool hardwareReset) {
    reg.msics = 0x00;

    ResetHostController();
    memoryStick.Reset();
}

void EmRegsMB86189::Save(Savestate<ChunkType>& savestate) { DoSave(savestate); }

void EmRegsMB86189::Save(SavestateProbe<ChunkType>& savestateProbe) { DoSave(savestateProbe); }

void EmRegsMB86189::Load(SavestateLoader<ChunkType>& loader) {
    memoryStick.Load(loader);

    Chunk* chunk = loader.GetChunk(ChunkType::regsMB86189);
    if (!chunk) return;

    const uint32 version = chunk->Get32();
    if (version > SAVESTATE_VERSION) {
        logPrintf("unable to restore RegsMB86189: unsupported savestate version\n");
        loader.NotifyError();

        return;
    }

    LoadChunkHelper helper(*chunk);
    DoSaveLoad(helper);
}

void EmRegsMB86189::SetGpioReadHandler(function<uint8()> handler) { gpioReadHandler = handler; }

template <typename T>
void EmRegsMB86189::DoSave(T& savestate) {
    memoryStick.Save(savestate);

    typename T::chunkT* chunk = savestate.GetChunk(ChunkType::regsMB86189);
    if (!chunk) return;

    chunk->Put32(SAVESTATE_VERSION);

    SaveChunkHelper helper(*chunk);
    DoSaveLoad(helper);
}

template <typename T>
void EmRegsMB86189::DoSaveLoad(T& helper) {
    uint8 stateByte = static_cast<uint8>(state);

    helper.Do(typename T::Pack16() << reg.mscmd << reg.mscs)
        .Do(typename T::Pack16() << reg.msics << reg.msppcd)
        .Do(typename T::Pack8() << irqStat << stateByte)
        .Do32(writeBufferSize)
        .Do32(readBufferIndex)
        .DoBuffer(writeBuffer, sizeof(writeBuffer));

    fifo.DoSaveLoad(helper);

    state = static_cast<State>(stateByte);
}

uint8* EmRegsMB86189::GetRealAddress(emuptr address) {
    EmAssert(false);

    // We don't support raw access
    return nullptr;
}

emuptr EmRegsMB86189::GetAddressStart(void) { return baseAddress; }

uint32 EmRegsMB86189::GetAddressRange(void) { return REGISTER_FILE_SIZE; }

void EmRegsMB86189::SetSubBankHandlers(void) {
    INSTALL_HANDLER(mscmdRead, mscmdWrite, OFFSET_MSCMD, 2);
    INSTALL_HANDLER(mscsRead, mscsWrite, OFFSET_MSCS, 2);
    INSTALL_HANDLER(msdataRead, msdataWrite, OFSSET_MSDATA, 2);
    INSTALL_HANDLER(msicsRead, msicsWrite, OFFSET_MSICS, 2);
    INSTALL_HANDLER(msppcdRead, msppcdWrite, OFFSET_MSPPCD, 2);
    INSTALL_HANDLER(stubRead, stubWrite, OFFSET_MSPPCD + 2, REGISTER_FILE_SIZE - OFFSET_MSPPCD - 2);
}

bool EmRegsMB86189::SupportsImageInSlot(EmHAL::Slot slot, uint32 blocksTotal) {
    return slot == EmHAL::Slot::memorystick && MemoryStick::IsSizeRepresentable(blocksTotal);
}

void EmRegsMB86189::Mount(EmHAL::Slot slot, CardImage& cardImage) {
    if (this->GetNextHandler()) this->GetNextHandler()->Mount(slot, cardImage);
    if (slot != EmHAL::Slot::memorystick) return;

    memoryStick.Mount(&cardImage);
}

void EmRegsMB86189::Unmount(EmHAL::Slot slot) {
    if (this->GetNextHandler()) this->GetNextHandler()->Unmount(slot);
    if (slot != EmHAL::Slot::memorystick) return;

    memoryStick.Unmount();
}

void EmRegsMB86189::Remount(EmHAL::Slot slot, CardImage& cardImage) {
    if (slot != EmHAL::Slot::memorystick) return;

    memoryStick.Remount(&cardImage);
}

bool EmRegsMB86189::GetIrq() { return (reg.mscs & MSCS_INT) != 0; }

void EmRegsMB86189::NotifyGpioChanged() { RaiseIrq(IRQ_PIN); }

void EmRegsMB86189::ResetHostController() {
    reg.mscmd = 0;
    reg.mscs = 0x0a05;
    reg.msics &= MSICS_INTEN;
    reg.msppcd = 0;

    state = State::idle;

    irqStat = 0;
    memoryStick.Reset();

    fifo.Clear();

    writeBufferSize = 0;
    readBufferIndex = 0;
}

void EmRegsMB86189::RaiseIrq(uint8 bits) {
    irqStat |= bits;

    UpdateIrqLine();
    TransferIrqFlags(bits);
}

void EmRegsMB86189::NegateIrq(uint8 bits) {
    irqStat &= ~bits;

    UpdateIrqLine();
}

void EmRegsMB86189::ClearIrqFlags(uint8 bits) { reg.msics &= ~(bits << 8); }

void EmRegsMB86189::UpdateIrqLine() {
    if ((reg.msics & MSICS_INTEN) == 0) return;

    uint16 mcsOld = reg.mscs;

    if (irqStat != 0) {
        reg.mscs |= MSCS_INT;
        if ((mcsOld & MSCS_INT) == 0) irqChange.Dispatch(true);
    } else {
        reg.mscs &= ~MSCS_INT;
        if ((mcsOld & MSCS_INT) != 0) irqChange.Dispatch(false);
    }
}

void EmRegsMB86189::TransferIrqFlags(uint8 bits) { reg.msics |= (bits << 8); }

void EmRegsMB86189::SetState(State state) {
    if (state != this->state && state == State::idle) RaiseIrq(IRQ_RDY);

    this->state = state;
}

void EmRegsMB86189::BeginTpc() {
    const uint8 tpcId = reg.mscmd >> 12;

    SetState(MemoryStick::GetTpcType(tpcId) == MemoryStick::TpcType::read ? State::tpcRead
                                                                          : State::tpcWrite);

    writeBufferSize = 0;
    readBufferIndex = 0;
    if (state == State::tpcRead) memoryStick.ExecuteTpc(tpcId);

    if (state == State::tpcWrite) {
        if (GetTransferSize() == 0)
            FinishTpc();
        else
            while (fifo.Size() > 0 && state != State::idle) FifoWrite(fifo.Pop());
    }
}

void EmRegsMB86189::FinishTpc() {
    if (state == State::tpcWrite)
        memoryStick.ExecuteTpc(reg.mscmd >> 12, GetTransferSize(), writeBuffer);
    else
        memoryStick.FinishReadTpc();

    fifo.Clear();

    SetState(State::idle);
}

void EmRegsMB86189::FifoWrite(uint16 data) {
    if (state != State::idle) {
        writeBuffer[writeBufferSize++] = data >> 8;
        if (writeBufferSize >= GetTransferSize()) return FinishTpc();

        writeBuffer[writeBufferSize++] = data;
        if (writeBufferSize >= GetTransferSize()) FinishTpc();

        return;
    }

    if (fifo.Size() == 8) {
        cerr << "MSHCS: fifo overflow during write" << endl << flush;
        return;
    }

    fifo.Push(data);
}

uint32 EmRegsMB86189::GetTransferSize() {
    const uint32 transferSize = reg.mscmd & DATA_SIZE_MASK;
    if (transferSize > 512) {
        cerr << "capping unexpected transfer size to 512 bytes" << endl << flush;
        return 512;
    }

    return transferSize;
}

uint32 EmRegsMB86189::mscmdRead(emuptr address, int size) {
    if (size != 2) {
        cerr << "illegal read of size " << size << " from MSCMD (byte " << (address - baseAddress)
             << ")" << endl
             << flush;
        return 0;
    }

#ifdef TRACE_ACCESS
    cerr << "MSCMD -> 0x" << hex << reg.mscmd << dec << endl << flush;
#endif

    return reg.mscmd;
}

uint32 EmRegsMB86189::mscsRead(emuptr address, int size) {
    uint32 value = reg.mscs;
    value &= 0xf0ff;

    if (state == State::tpcRead && memoryStick.GetDataOutSize() - readBufferIndex >= 4)
        value |= MSCS_RBF;
    if (state != State::tpcRead || memoryStick.GetDataOutSize() - readBufferIndex == 0)
        value |= MSCS_RBE;
    if (state != State::idle) value |= MSCS_DRQ;

    value = compositeRegisterRead(baseAddress + OFFSET_MSCS, address, size, value);

#ifdef TRACE_ACCESS
    cerr << "MSCS_" << (address - baseAddress - OFFSET_MSCS) << " -> 0x" << hex << value << dec
         << endl
         << flush;
#endif

    return value;
}

uint32 EmRegsMB86189::msdataRead(emuptr address, int size) {
    if (size != 2) {
        cerr << "illegal read of size " << size << " from MSDATA" << endl << flush;
        return 0;
    }

    uint32 value = 0;

    if (state == State::tpcRead) {
        uint8* buffer = memoryStick.GetDataOut();
        uint32 bufferSize = memoryStick.GetDataOutSize();

        if (!buffer) {
            readBufferIndex += 2;
        } else {
            if (readBufferIndex < bufferSize) value = buffer[readBufferIndex++] << 8;
            if (readBufferIndex < bufferSize) value |= buffer[readBufferIndex++];
        }

        if (readBufferIndex >= min(GetTransferSize(), bufferSize)) FinishTpc();
    }

#ifdef TRACE_ACCESS
    cerr << "MSDATA -> 0x" << hex << value << dec << endl << flush;
#endif

    return value;
}

uint32 EmRegsMB86189::msicsRead(emuptr address, int size) {
    uint32 value = compositeRegisterRead(baseAddress + OFFSET_MSICS, address, size, reg.msics);

    NegateIrq(IRQ_SIF | IRQ_RDY | IRQ_PIN);

#ifdef TRACE_ACCESS
    cerr << "MSICS_" << (address - baseAddress - OFFSET_MSICS) << " -> 0x" << hex << value << dec
         << endl
         << flush;
#endif

    return value;
}

uint32 EmRegsMB86189::msppcdRead(emuptr address, int size) {
    uint32 value =
        compositeRegisterRead(baseAddress + OFFSET_MSPPCD, address, size,
                              ((reg.msppcd & ~0x3000) | ((gpioReadHandler() & 0x03) << 12)));

    ClearIrqFlags(IRQ_PIN);

#ifdef TRACE_ACCESS
    cerr << "MSPPCD_" << (address - baseAddress - OFFSET_MSPPCD) << " -> 0x" << hex << value << dec
         << endl
         << flush;
#endif

    return value;
}

void EmRegsMB86189::mscmdWrite(emuptr address, int size, uint32 value) {
    if (state != State::idle) {
        cerr << "ignoring MSCMD write during running transaction" << endl << flush;
        return;
    }

    if (size != 2) {
        cerr << "illegal write of size " << size << " (byte " << (address - baseAddress)
             << ") to MSCMD (0x" << hex << value << dec << ")" << endl
             << flush;

        return;
    }

#ifdef TRACE_ACCESS
    cerr << "MSCMD <- 0x" << hex << value << dec << endl << flush;
#endif

    ClearIrqFlags(IRQ_SIF | IRQ_RDY);

    reg.mscmd = value;
    BeginTpc();
}

void EmRegsMB86189::mscsWrite(emuptr address, int size, uint32 value) {
#ifdef TRACE_ACCESS
    cerr << "MSCS_" << (address - baseAddress - OFFSET_MSCS) << " <- 0x" << hex << value << dec
         << endl
         << flush;
#endif

    uint16 oldValue = reg.mscs;

    compositeRegisterWrite(baseAddress + OFFSET_MSCS, address, size, value, reg.mscs);

    reg.mscs &= 0x00ff;
    reg.mscs |= (oldValue & 0xff00);

    if (((oldValue ^ reg.mscs) & oldValue) & MSCS_RST) {
        ResetHostController();
        RaiseIrq(IRQ_RDY);
    }
}

void EmRegsMB86189::msdataWrite(emuptr address, int size, uint32 value) {
    if (size != 2) {
        cerr << "illegal write of size " << size << " (0x" << hex << value << dec << ") to MSDATA"
             << endl
             << flush;

        return;
    }

#ifdef TRACE_ACCESS
    cerr << "MSDATA <- 0x" << hex << value << dec << endl << flush;
#endif

    FifoWrite(value);
}

void EmRegsMB86189::msicsWrite(emuptr address, int size, uint32 value) {
    value &= 0x00ff;

#ifdef TRACE_ACCESS
    cerr << "MSICS_" << (address - baseAddress - OFFSET_MSICS) << " <- 0x" << hex << value << dec
         << endl
         << flush;
#endif

    compositeRegisterWrite(baseAddress + OFFSET_MSICS, address, size, value, reg.msics);
}

void EmRegsMB86189::msppcdWrite(emuptr address, int size, uint32 value) {
#ifdef TRACE_ACCESS
    cerr << "MSCPPCD_" << (address - baseAddress - OFFSET_MSPPCD) << " <- 0x" << hex << value << dec
         << endl
         << flush;
#endif

    compositeRegisterWrite(baseAddress + OFFSET_MSPPCD, address, size, value, reg.msppcd);
}

uint32 EmRegsMB86189::stubRead(emuptr address, int size) {
    cerr << "unimplemented MSHC read of size " << size << " from 0x" << hex << address << dec
         << endl
         << flush;

    return 0;
}

void EmRegsMB86189::stubWrite(emuptr address, int size, uint32 value) {
    cerr << "unimplemented MSHC write of size " << size << " to 0x" << hex << address << " (0x"
         << value << dec << ")" << endl
         << flush;
}

uint32 EmRegsMB86189::compositeRegisterRead(emuptr base, emuptr address, int size, uint16 target) {
    switch (size) {
        case 1:
            return base == address ? (target >> 8) : (target & 0xff);

        case 2:
            return target;

        case 4:
            cerr << "invalid 32 bit read from MB86189 0x" << hex << address << dec << endl << flush;
            return 0;

        default:
            EmAssert(false);

            return 0;
    }
}

inline void EmRegsMB86189::compositeRegisterWrite(emuptr base, emuptr address, int size,
                                                  uint32 value, uint16& target) {
    switch (size) {
        case 1:
            if (base == address)
                target = (target & 0x00ff) | (value << 8);
            else
                target = (target & 0xff00) | (value & 0xff);

            return;

        case 2:
            target = value;
            return;

        case 4:
            cerr << "invalid 32 bit read from MB86189 0x" << hex << address << " <- 0x" << value
                 << dec << endl
                 << flush;

            return;

        default:
            EmAssert(false);
    }
}
