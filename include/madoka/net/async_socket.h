// Copyright (c) 2014 dacci.org

#ifndef MADOKA_NET_ASYNC_SOCKET_H_
#define MADOKA_NET_ASYNC_SOCKET_H_

#include <madoka/net/common.h>

#ifndef _WIN32
  #error The AsyncSocket supports Windows platforms only.
#endif  // _WIN32

#if _WIN32_WINNT < 0x0600
  #error The AsyncSocket requires the thread pool API.
#endif  // _WIN32_WINNT < 0x0600

#include <madoka/net/socket.h>

namespace madoka {
namespace net {

class SocketEventListener;

class AsyncSocket : public Socket {
 public:
  struct AsyncContext;

  AsyncSocket();
  virtual ~AsyncSocket();

  virtual void Close();

  static PTP_CALLBACK_ENVIRON GetCallbackEnvironment();
  static void SetCallbackEnvironment(PTP_CALLBACK_ENVIRON environment);

  // Connects to a specified address asynchronously. The process will complete
  // when any connection attempt succeeds or all attempts fail.
  void ConnectAsync(const addrinfo* end_points, SocketEventListener* listener);
  AsyncContext* BeginConnect(const addrinfo* end_points, HANDLE event);
  bool EndConnect(AsyncContext* context);

  void ReceiveAsync(void* buffer, int size, int flags,
                    SocketEventListener* listener);
  AsyncContext* BeginReceive(void* buffer, int size, int flags, HANDLE event);
  int EndReceive(AsyncContext* context);

  void SendAsync(const void* buffer, int size, int flags,
                 SocketEventListener* listener);
  AsyncContext* BeginSend(const void* buffer, int size, int flags,
                          HANDLE event);
  int EndSend(AsyncContext* context);

 private:
  friend class AsyncServerSocket;

  enum Action {
    None, Connecting, Receiving, Sending
  };

  AsyncContext* DispatchRequest(Action action, const addrinfo* end_points,
                                void* buffer, int size, int flags,
                                SocketEventListener* listener, HANDLE event);
  int EndRequest(AsyncContext* context);

  int DoAsyncConnect(AsyncContext* context);

  static BOOL CALLBACK OnInitialize(INIT_ONCE* init_once, void* param,
                                    void** context);
  BOOL OnInitialize();

  static void CALLBACK OnRequested(PTP_CALLBACK_INSTANCE instance, void* param);
  void OnRequested(AsyncContext* context);

  static void CALLBACK OnCompleted(PTP_CALLBACK_INSTANCE instance,
                                   void* context, void* overlapped, ULONG error,
                                   ULONG_PTR bytes, PTP_IO io);
  void OnCompleted(AsyncContext* context, ULONG error, ULONG_PTR bytes);

  static PTP_CALLBACK_ENVIRON environment_;
  static LPFN_CONNECTEX ConnectEx;
  INIT_ONCE init_once_;
  PTP_IO io_;
  bool cancel_connect_;
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_ASYNC_SOCKET_H_
