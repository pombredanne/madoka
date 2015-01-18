// Copyright (c) 2014 dacci.org

#ifndef MADOKA_NET_ASYNC_DATAGRAM_SOCKET_H_
#define MADOKA_NET_ASYNC_DATAGRAM_SOCKET_H_

#include <madoka/net/common.h>

#ifndef _WIN32
  #error The AsyncDatagramSocket supports Windows platforms only.
#endif  // _WIN32

#if _WIN32_WINNT < 0x0600
  #error The AsyncDatagramSocket requires the thread pool API.
#endif  // _WIN32_WINNT < 0x0600

#include <madoka/net/datagram_socket.h>

namespace madoka {
namespace net {

class SocketEventListener;

class AsyncDatagramSocket : public DatagramSocket {
 public:
  struct AsyncContext;

  AsyncDatagramSocket();
  AsyncDatagramSocket(int family, int protocol);
  virtual ~AsyncDatagramSocket();

  void Close() MADOKA_OVERRIDE;

  static PTP_CALLBACK_ENVIRON GetCallbackEnvironment();
  static void SetCallbackEnvironment(PTP_CALLBACK_ENVIRON environment);

  void ReceiveAsync(void* buffer, int size, int flags,
                    SocketEventListener* listener);
  AsyncContext* BeginReceive(void* buffer, int size, int flags, HANDLE event);
  int EndReceive(AsyncContext* context);

  void SendAsync(const void* buffer, int size, int flags,
                 SocketEventListener* listener);
  AsyncContext* BeginSend(const void* buffer, int size, int flags,
                          HANDLE event);
  int EndSend(AsyncContext* context);

  void ReceiveFromAsync(void* buffer, int size, int flags,
                        SocketEventListener* listener);
  AsyncContext* BeginReceiveFrom(void* buffer, int size, int flags,
                                 HANDLE event);
  int EndReceiveFrom(AsyncContext* context, sockaddr* address, int* length);

  void SendToAsync(const void* buffer, int size, int flags,
                   const sockaddr* address, int length,
                   SocketEventListener* listener);
  AsyncContext* BeginSendTo(const void* buffer, int size, int flags,
                            const sockaddr* address, int length, HANDLE event);
  int EndSendTo(AsyncContext* context);

 private:
  enum Action {
    None, Receiving, Sending, ReceivingFrom, SendingTo
  };

  void CloseInternal();

  AsyncContext* DispatchRequest(Action action, void* buffer, int size,
                                int flags, const sockaddr* address, int length,
                                SocketEventListener* listener, HANDLE event);
  int EndRequest(AsyncContext* context, sockaddr* address, int* length);

  static BOOL CALLBACK OnInitialize(INIT_ONCE* init_once, void* param,
                                    void** context);
  BOOL OnInitialize(void** context);

  static void CALLBACK OnRequested(PTP_CALLBACK_INSTANCE instance, void* param);
  void OnRequested(AsyncContext* context);

  static void CALLBACK OnCompleted(PTP_CALLBACK_INSTANCE instance,
                                   void* context, void* overlapped, ULONG error,
                                   ULONG_PTR bytes, PTP_IO io);
  void OnCompleted(AsyncContext* context, ULONG error, ULONG_PTR bytes);

  static PTP_CALLBACK_ENVIRON environment_;
  INIT_ONCE init_once_;
  PTP_IO io_;

  DISALLOW_COPY_AND_ASSIGN(AsyncDatagramSocket);
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_ASYNC_DATAGRAM_SOCKET_H_
