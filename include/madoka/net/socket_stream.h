// Copyright (c) 2015 dacci.org

#ifndef MADOKA_NET_SOCKET_STREAM_H_
#define MADOKA_NET_SOCKET_STREAM_H_

#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_  // prevent winsock.h to be included
#endif  // _WINSOCKAPI_

#include <madoka/io/abstract_stream.h>
#include <madoka/net/socket.h>

namespace madoka {
namespace net {

class SocketStream : public Socket, public madoka::io::AbstractStream {
 public:
  struct AsyncContext;

  class Listener {
   public:
    virtual ~Listener() {}

    virtual void OnConnected(SocketStream* stream, HRESULT result,
                             const addrinfo* end_point) = 0;
    virtual void OnConnected(SocketStream* stream, HRESULT result,
                             const ADDRINFOW* end_point) = 0;

    virtual void OnReceived(SocketStream* stream, HRESULT result, void* buffer,
                            uint64_t length, int flags) = 0;
    virtual void OnReceivedFrom(SocketStream* stream, HRESULT result,
                                void* buffer, uint64_t length, int flags,
                                sockaddr* from, int from_length) = 0;

    virtual void OnSent(SocketStream* stream, HRESULT result, void* buffer,
                        uint64_t length) = 0;
    virtual void OnSentTo(SocketStream* stream, HRESULT result, void* buffer,
                          uint64_t length, sockaddr* to, int to_length) = 0;
  };

  SocketStream();
  ~SocketStream();

  void Close() override;

  void ConnectAsync(const addrinfo* end_point, Listener* listener);
  AsyncContext* BeginConnect(const addrinfo* end_point, HANDLE event);
  HRESULT EndConnect(AsyncContext* context, const addrinfo** end_point);

  void ConnectAsync(const ADDRINFOW* end_point, Listener* listener);
  AsyncContext* BeginConnect(const ADDRINFOW* end_point, HANDLE event);
  HRESULT EndConnect(AsyncContext* context, const ADDRINFOW** end_point);

  HRESULT EndConnect(AsyncContext* context);

  void ReceiveAsync(void* buffer, uint64_t length, int flags,
                    Listener* listener);
  AsyncContext* BeginReceive(void* buffer, DWORD length, int flags,
                             HANDLE event);
  HRESULT EndReceive(AsyncContext* context, DWORD* length, int* flags);

  void ReceiveFromAsync(void* buffer, uint64_t length, int flags,
                        Listener* listener);
  AsyncContext* BeginReceiveFrom(void* buffer, DWORD length, int flags,
                                 HANDLE event);
  HRESULT EndReceiveFrom(AsyncContext* context, DWORD* length, int* flags,
                         void* address, int* address_length);

  void SendAsync(const void* buffer, uint64_t length, int flags,
                 Listener* listener);
  AsyncContext* BeginSend(const void* buffer, DWORD length, int flags,
                          HANDLE event);
  HRESULT EndSend(AsyncContext* context, DWORD* length);

  void SendToAsync(const void* buffer, uint64_t length, int flags, void* to,
                   int to_length, Listener* listener);
  AsyncContext* BeginSendTo(const void* buffer, DWORD length, int flags,
                            void* to, int to_length, HANDLE event);
  HRESULT EndSendTo(AsyncContext* context, DWORD* length);

  HRESULT Read(void* buffer, uint64_t* length) override;
  void ReadAsync(void* buffer, uint64_t length,
                 AbstractStream::Listener* listener) override;

  HRESULT Write(const void* buffer, uint64_t* length) override;
  void WriteAsync(const void* buffer, uint64_t length,
                  AbstractStream::Listener* listener) override;

 private:
  void Reset();

  void ConnectAsync(const addrinfo* end_point, const ADDRINFOW* end_pointw,
                    Listener* listener);
  AsyncContext* BeginConnect(const addrinfo* end_point,
                             const ADDRINFOW* end_pointw, HANDLE event);
  BOOL ConnectAsync(AsyncContext* context);

  void CommunicateAsync(int type, void* buffer, uint64_t length, int flags,
                        void* address, int address_length,
                        Stream::Listener* listener, Listener* socket_listener);
  AsyncContext* BeginCommunicate(int type, void* buffer, DWORD length,
                                 int flags, void* address, int address_length,
                                 HANDLE event);

  void OnRequested(AbstractStream::AsyncContext* abstract_context) override;

  static void CALLBACK OnCompleted(PTP_CALLBACK_INSTANCE instance,
                                   void* context, void* overlapped, ULONG error,
                                   ULONG_PTR length, PTP_IO io);
  void OnCompleted(AsyncContext* context, HRESULT result, ULONG_PTR length);

  PTP_IO io_;

  MADOKA_DISALLOW_COPY_AND_ASSIGN(SocketStream);
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_SOCKET_STREAM_H_
