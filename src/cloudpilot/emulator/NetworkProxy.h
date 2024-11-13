#ifndef _NETWORK_PROXY_H_
#define _NETWORK_PROXY_H_

#include <functional>

#include "EmCommon.h"
#include "EmEvent.h"
#include "SuspendContextNetworkRpc.h"
#include "networking.pb.h"

struct BufferDecodeContext;

class NetworkProxy {
   public:
    NetworkProxy() = default;

    void Reset();

    void Open();

    void Close();

    int OpenCount();

    void SocketOpen(uint8 domain, uint8 type, uint16 protocol);

    void SocketBind(int16 handle, NetSocketAddrType* sockAddrP, Int32 timeout);

    void SocketAddr(int16 handle, NetSocketAddrType* locAddrP, Int16* locAddrLenP,
                    NetSocketAddrType* remAddrP, Int16* remAddrLen, int32 timeout);

    void SocketSend(int16 handle, uint8* data, size_t count, uint16 flags,
                    NetSocketAddrType* toAddrP, int32 toLen, int32 timeout);

    void SocketSendPB(int16 handle, NetIOParamType* pbP, uint16 flags, int32 timeout);

    void SocketReceive(int16 handle, uint16 flags, uint16 bufLen, int32 timeout,
                       NetSocketAddrType* fromAddrP);

    void SocketReceivePB(int16 handle, NetIOParamType* pbP, uint16 flags, int32 timeout);

    void SocketDmReceive(int16 handle, uint16 flags, uint16 rcvlen, int32 timeout,
                         NetSocketAddrType* fromAddrP);

    void SocketClose(int16 handle, int32 timeout);

    void GetHostByName(const string name);

    void GetServByName(const string name, const string proto);

    void SocketConnect(int16 handle, NetSocketAddrType* address, int16 addrLen, int32 timeout);

    void Select(UInt16 width, NetFDSetType readFDs, NetFDSetType writeFDs, NetFDSetType exceptFDs,
                int32 timeout);

    bool SettingGet(UInt16 setting);

    void SocketOptionSet(int16 handle, uint16 level, uint16 option, emuptr valueP, size_t len,
                         int32 timeout);

    void SocketOptionGet(int16 handle, uint16 level, uint16 option, int32 timeout);

    void SocketListen(int16 handle, int32 timeout);

    void SocketAccept(int16 handle, int32 timeout);

    void SocketShutdown(int16 handle, int16 direction, int32 timeout);

   public:
    EmEvent<> onDisconnect;

   private:
    void ConnectSuccess();
    void ConnectAbort();

    void CloseDone(Err err);

    void SocketOpenSuccess(void* responseData, size_t size);
    void SocketOpenFail(Err err = netErrInternal);

    void SocketBindSuccess(void* responseData, size_t size);
    void SocketBindFail(Err err = netErrInternal);

    void SocketAddrSuccess(void* responseData, size_t size);
    void SocketAddrFail(Err err = netErrInternal);

    void SocketSendSuccess(void* responseData, size_t size);
    void SocketSendFail(Err err = netErrInternal);

    void SocketSendPBSuccess(void* responseData, size_t size);
    void SocketSendPBFail(Err err = netErrInternal);

    void SocketReceiveSuccess(void* responseData, size_t size);
    void SocketReceiveFail(Err err = netErrInternal);

    void SocketReceivePBSuccess(void* responseData, size_t size);
    void SocketReceivePBFail(Err err = netErrInternal);

    void SocketDmReceiveSuccess(void* responseData, size_t size);
    void SocketDmReceiveFail(Err err = netErrInternal);

    void SocketCloseSuccess(void* responseData, size_t size);
    void SocketCloseFail(Err err = netErrInternal);

    void GetHostByNameSuccess(void* responseData, size_t size);
    void GetHostByNameFail(Err err = netErrInternal);

    void GetServByNameSuccess(void* responseData, size_t size);
    void GetServByNameFail(Err err = netErrInternal);

    void SocketConnectSuccess(void* responseData, size_t size);
    void SocketConnectFail(Err err = netErrInternal);

    void SelectSuccess(void* responseData, size_t size);
    void SelectFail(Err err = netErrInternal);

    void SettingGetSuccess(void* responseData, size_t size);
    void SettingGetFail(Err err = netErrInternal);

    void SocketOptionSetSuccess(void* responseData, size_t size);
    void SocketOptionSetFail(Err err = netErrInternal);

    void SocketOptionGetSuccess(void* responseData, size_t size);
    void SocketOptionGetFail(Err err = netErrInternal);

    void SocketListenSuccess(void* responseData, size_t size);
    void SocketListenFail(Err err = netErrInternal);

    void SocketAcceptSuccess(void* responseData, size_t size);
    void SocketAcceptFail(Err err = netErrInternal);

    void SocketShutdownSuccess(void* responseData, size_t size);
    void SocketShutdownFail(Err err = netErrInternal);

    MsgRequest NewRequest(pb_size_t payloadTag);
    bool DecodeResponse(void* responseData, size_t size, MsgResponse& response,
                        pb_size_t payloadTag, BufferDecodeContext* bufferrDecodeContext = nullptr);

    void SendAndSuspend(MsgRequest& request, size_t bufferSize,
                        SuspendContextNetworkRpc::successCallbackT cbSuccess,
                        function<void(Err)> cbFail);

   private:
    uint32 openCount{0};
    uint32 currentId{0xffffffff};

   private:
    NetworkProxy(const NetworkProxy&) = delete;
    NetworkProxy(NetworkProxy&&) = delete;
    NetworkProxy& operator=(const NetworkProxy&) = delete;
    NetworkProxy& operator=(NetworkProxy&&) = delete;
};

extern NetworkProxy& gNetworkProxy;

#endif  // _NETWORK_PROXY_H_
