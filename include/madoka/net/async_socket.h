// Copyright (c) 2014 dacci.org

#ifndef MADOKA_NET_ASYNC_SOCKET_H_
#define MADOKA_NET_ASYNC_SOCKET_H_

#include <madoka/net/common.h>

#ifndef _WIN32
#  error The AsyncSocket supports Windows platforms only.
#endif  // _WIN32

#if _WIN32_WINNT < 0x0600
#  error The AsyncSocket requires the thread pool API.
#endif  // _WIN32_WINNT < 0x0600

#include <madoka/concurrent/critical_section.h>
#include <madoka/net/socket.h>

#include <list>
#include <memory>

namespace madoka {
namespace net {

class AsyncSocket : public Socket {
 public:
  struct Context;

  class Listener {
   public:
    virtual ~Listener() {}

    virtual void OnConnected(AsyncSocket* socket, HRESULT result,
                             const addrinfo* end_point) = 0;
    virtual void OnReceived(AsyncSocket* socket, HRESULT result, void* buffer,
                            int length, int flags) = 0;
    virtual void OnReceivedFrom(AsyncSocket* socket, HRESULT result,
                                void* buffer, int length, int flags,
                                const sockaddr* address,
                                int address_length) = 0;
    virtual void OnSent(AsyncSocket* socket, HRESULT result, void* buffer,
                        int length) = 0;
    virtual void OnSentTo(AsyncSocket* socket, HRESULT result, void* buffer,
                          int length, const sockaddr* address,
                          int address_length) = 0;
  };

  AsyncSocket();
  AsyncSocket(int family, int type, int protocol);

  ~AsyncSocket();

  void Close() override;

  static PTP_CALLBACK_ENVIRON GetCallbackEnvironment();
  static void SetCallbackEnvironment(PTP_CALLBACK_ENVIRON environment);

  void ConnectAsync(const addrinfo* end_point, Listener* listener);
  Context* BeginConnect(const addrinfo* end_point, HANDLE event);
  HRESULT EndConnect(Context* context);

  void ReceiveAsync(void* buffer, int length, int flags, Listener* listener);
  Context* BeginReceive(void* buffer, int length, int flags, HANDLE event);
  int EndReceive(Context* context, HRESULT* result);
  int EndReceive(Context* context) {
    HRESULT result;
    return EndReceive(context, &result);
  }

  void ReceiveFromAsync(void* buffer, int length, int flags,
                        Listener* listener);
  Context* BeginReceiveFrom(void* buffer, int length, int flags, HANDLE event);
  int EndReceiveFrom(Context* context, void* address, int* length);

  void SendAsync(const void* buffer, int length, int flags, Listener* listener);
  Context* BeginSend(const void* buffer, int length, int flags, HANDLE event);
  int EndSend(Context* context, HRESULT* result);

  int EndSend(Context* context) {
    HRESULT result;
    return EndSend(context, &result);
  }

  void SendToAsync(const void* buffer, int length, int flags,
                   const void* address, int address_length, Listener* listener);
  Context* BeginSendTo(const void* buffer, int length, int flags,
                       const void* address, int address_length, HANDLE event);
  int EndSendTo(Context* context, HRESULT* result);

  int EndSendTo(Context* context) {
    HRESULT result;
    return EndSendTo(context, &result);
  }

 private:
  static std::unique_ptr<Context> CreateContext(
      int request, const addrinfo* end_point, void* buffer, int length,
      DWORD flags, const void* address, int address_length,
      Listener* listener, HANDLE event);
  HRESULT RequestAsync(std::unique_ptr<Context>&& context);
  Context* BeginRequest(std::unique_ptr<Context>&& context);

  static void CALLBACK OnRequested(PTP_CALLBACK_INSTANCE callback,
                                   void* instance, PTP_WORK work);
  void OnRequested(PTP_WORK work);

  static void CALLBACK OnCompleted(PTP_CALLBACK_INSTANCE callback,
                                   void* instance, void* overlapped,
                                   ULONG error, ULONG_PTR bytes, PTP_IO io);
  void OnCompleted(std::unique_ptr<Context>&& context, HRESULT result,
                   int length);

  static PTP_CALLBACK_ENVIRON environment_;

  PTP_WORK work_;
  madoka::concurrent::CriticalSection lock_;
  std::list<std::unique_ptr<Context>> requests_;
  bool cancel_connect_;
  PTP_IO io_;

  MADOKA_DISALLOW_COPY_AND_ASSIGN(AsyncSocket);
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_ASYNC_SOCKET_H_
