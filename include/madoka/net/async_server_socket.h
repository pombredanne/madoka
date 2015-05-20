// Copyright (c) 2015 dacci.org

#ifndef MADOKA_NET_ASYNC_SERVER_SOCKET_H_
#define MADOKA_NET_ASYNC_SERVER_SOCKET_H_

#include <madoka/net/common.h>

#ifndef _WIN32
#  error The AsyncSocket supports Windows platforms only.
#endif  // _WIN32

#if _WIN32_WINNT < 0x0600
#  error The AsyncSocket requires the thread pool API.
#endif  // _WIN32_WINNT < 0x0600

#include <madoka/concurrent/critical_section.h>
#include <madoka/net/server_socket.h>

#include <list>
#include <memory>

namespace madoka {
namespace net {

class AsyncServerSocket final : public ServerSocket {
 public:
  struct Context;

  class Listener {
   public:
    virtual ~Listener() {}

    virtual void OnAccepted(AsyncServerSocket* server, HRESULT result,
                            Context* context) = 0;
  };

  AsyncServerSocket();
  AsyncServerSocket(int family, int type, int protocol);

  ~AsyncServerSocket();

  void Close() override;

  static PTP_CALLBACK_ENVIRON GetCallbackEnvironment();
  static void SetCallbackEnvironment(PTP_CALLBACK_ENVIRON environment);

  void AcceptAsync(Listener* listener);
  Context* BeginAccept(HANDLE event);

  template<class Impl>
  std::unique_ptr<Impl> EndAccept(Context* context, HRESULT* result) {
    struct Wrapper : Impl {
      explicit Wrapper(SOCKET descriptor) {
        descriptor_ = descriptor;
        bound_ = true;
        connected_ = true;
      }
    };

    if (context == nullptr || result == nullptr)
      return nullptr;

    SOCKET descriptor = RawEndAccept(context, result);
    if (descriptor == INVALID_SOCKET)
      return nullptr;

    auto accepted = std::make_unique<Wrapper>(descriptor);
    if (accepted == nullptr)
      closesocket(descriptor);

    return std::move(accepted);
  }

 private:
  SOCKET RawEndAccept(Context* pointer, HRESULT* result);

  static void CALLBACK OnRequested(PTP_CALLBACK_INSTANCE callback,
                                   void* instance, PTP_WORK work);
  void OnRequested(PTP_WORK work);

  static void CALLBACK OnCompleted(PTP_CALLBACK_INSTANCE callback,
                                   void* instance, void* overlapped,
                                   ULONG error, ULONG_PTR length, PTP_IO io);
  void OnCompleted(std::unique_ptr<Context>&& context, HRESULT result);

  static PTP_CALLBACK_ENVIRON environment_;

  madoka::concurrent::CriticalSection lock_;
  PTP_WORK work_;
  std::list<std::unique_ptr<Context>> requests_;
  WSAPROTOCOL_INFO protocol_;
  PTP_IO io_;

  MADOKA_DISALLOW_COPY_AND_ASSIGN(AsyncServerSocket);
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_ASYNC_SERVER_SOCKET_H_
