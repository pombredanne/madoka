// Copyright (c) 2014 dacci.org

#ifndef MADOKA_NET_ASYNC_SERVER_SOCKET_H_
#define MADOKA_NET_ASYNC_SERVER_SOCKET_H_

#include <madoka/net/common.h>

#ifndef _WIN32
  #error The AsyncServerSocket supports Windows platforms only.
#endif  // _WIN32

#if _WIN32_WINNT < 0x0600
  #error The AsyncServerSocket requires the thread pool API.
#endif  // _WIN32_WINNT < 0x0600

#include <madoka/net/async_socket.h>
#include <madoka/net/server_socket.h>

namespace madoka {
namespace net {

class SocketEventListener;

class AsyncServerSocket : public ServerSocket {
 public:
  struct AsyncContext;

  AsyncServerSocket();
  virtual ~AsyncServerSocket();

  void Close() MADOKA_OVERRIDE;

  static PTP_CALLBACK_ENVIRON GetCallbackEnvironment();
  static void SetCallbackEnvironment(PTP_CALLBACK_ENVIRON environment);

  void AcceptAsync(SocketEventListener* listener);
  AsyncContext* BeginAccept(HANDLE event);
  AsyncSocket* EndAccept(AsyncContext* context);

 private:
  AsyncContext* DispatchRequest(SocketEventListener* listener, HANDLE event);

  static BOOL CALLBACK OnInitialize(INIT_ONCE* init_once, void* param,
                                    void** context);
  BOOL OnInitialize();

  static void CALLBACK OnRequested(PTP_CALLBACK_INSTANCE instance, void* param);
  void OnRequested(AsyncContext* context);

  static void CALLBACK OnCompleted(PTP_CALLBACK_INSTANCE instance,
                                   void* context, void* overlapped, ULONG error,
                                   ULONG_PTR bytes, PTP_IO io);
  void OnCompleted(AsyncContext* context, ULONG error);

  static PTP_CALLBACK_ENVIRON environment_;
  INIT_ONCE init_once_;
  PTP_IO io_;
  WSAPROTOCOL_INFO protocol_info_;

  DISALLOW_COPY_AND_ASSIGN(AsyncServerSocket);
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_ASYNC_SERVER_SOCKET_H_
