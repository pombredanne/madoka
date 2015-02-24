// Copyright (c) 2015 dacci.org

#ifndef MADOKA_NET_ASYNC_SERVER_SOCKET_H_
#define MADOKA_NET_ASYNC_SERVER_SOCKET_H_

#include <madoka/concurrent/condition_variable.h>
#include <madoka/concurrent/critical_section.h>
#include <madoka/net/server_socket.h>

#include <list>
#include <memory>

namespace madoka {
namespace net {

class AsyncServerSocket : public ServerSocket {
 public:
  struct AsyncContext;

  class Listener {
   public:
    virtual ~Listener() {}

    virtual void OnAccepted(AsyncServerSocket* server, HRESULT result,
                            AsyncContext* context) = 0;
  };

  AsyncServerSocket();
  AsyncServerSocket(int family, int type, int protocol);

  ~AsyncServerSocket();

  void AcceptAsync(Listener* listener);
  AsyncContext* BeginAccept(HANDLE event);

  template<class T>
  HRESULT EndAccept(AsyncContext* context, std::unique_ptr<T>* socket) {
    SOCKET peer = INVALID_SOCKET;
    HRESULT result = EndAccept(context, &peer);
    if (FAILED(result))
      return result;

    T* client = new T(peer);
    if (client == nullptr) {
      closesocket(peer);
      return E_OUTOFMEMORY;
    }

    socket->reset(client);

    return S_OK;
  }

 private:
  void Reset();

  HRESULT EndAccept(AsyncContext* context, SOCKET* socket);

  std::unique_ptr<AsyncContext> CreateContext(Listener* listener, HANDLE event);
  HRESULT DispatchRequest(
      std::unique_ptr<AsyncContext>&& context);  // NOLINT(build/c++11)
  void DeleteContext(AsyncContext* context);

  static void CALLBACK OnRequested(PTP_CALLBACK_INSTANCE instance, void* param);
  void OnRequested(AsyncContext* context);

  static void CALLBACK OnCompleted(PTP_CALLBACK_INSTANCE instance, void* param,
                                   void* overlapped, ULONG error,
                                   ULONG_PTR length, PTP_IO io);
  void OnCompleted(AsyncContext* context, HRESULT result, ULONG_PTR length);

  std::list<std::unique_ptr<AsyncContext>> requests_;
  madoka::concurrent::ConditionVariable empty_;
  madoka::concurrent::CriticalSection lock_;
  WSAPROTOCOL_INFO protocol_;
  SOCKET old_descriptor_;
  PTP_IO io_;

  MADOKA_DISALLOW_COPY_AND_ASSIGN(AsyncServerSocket);
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_ASYNC_SERVER_SOCKET_H_
