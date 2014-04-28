// Copyright (c) 2014 dacci.org

#ifndef MADOKA_NET_SOCKET_EVENT_LISTENER_H_
#define MADOKA_NET_SOCKET_EVENT_LISTENER_H_

#include <madoka/net/common.h>

namespace madoka {
namespace net {

class AsyncServerSocket;
class AsyncSocket;
class AsyncDatagramSocket;

class SocketEventListener {
 public:
  virtual ~SocketEventListener() {}

  virtual void OnAccepted(AsyncServerSocket* server, AsyncSocket* client,
                          DWORD error) = 0;

  virtual void OnConnected(AsyncSocket* socket, DWORD error) = 0;
  virtual void OnReceived(AsyncSocket* socket, DWORD error, void* buffer,
                          int length) = 0;
  virtual void OnSent(AsyncSocket* socket, DWORD error, void* buffer,
                      int length) = 0;

  virtual void OnReceived(AsyncDatagramSocket* socket, DWORD error,
                          void* buffer, int length) = 0;
  virtual void OnReceivedFrom(AsyncDatagramSocket* socket, DWORD error,
                              void* buffer, int length, sockaddr* from,
                              int from_length) = 0;
  virtual void OnSent(AsyncDatagramSocket* socket, DWORD error, void* buffer,
                      int length) = 0;
  virtual void OnSentTo(AsyncDatagramSocket* socket, DWORD error, void* buffer,
                        int length, sockaddr* to, int to_length) = 0;
};

class SocketEventAdapter : public SocketEventListener {
 public:
  virtual ~SocketEventAdapter() {}

  virtual void OnAccepted(AsyncServerSocket* server, AsyncSocket* client,
                          DWORD error) MADOKA_OVERRIDE {}

  virtual void OnConnected(AsyncSocket* socket, DWORD error) MADOKA_OVERRIDE {}
  virtual void OnReceived(AsyncSocket* socket, DWORD error, void* buffer,
                          int length) MADOKA_OVERRIDE {}
  virtual void OnSent(AsyncSocket* socket, DWORD error, void* buffer,
                      int length) MADOKA_OVERRIDE {}

  virtual void OnReceived(AsyncDatagramSocket* socket, DWORD error,
                          void* buffer, int length) MADOKA_OVERRIDE {}
  virtual void OnReceivedFrom(AsyncDatagramSocket* socket, DWORD error,
                              void* buffer, int length, sockaddr* from,
                              int from_length) MADOKA_OVERRIDE {}
  virtual void OnSent(AsyncDatagramSocket* socket, DWORD error, void* buffer,
                      int length) MADOKA_OVERRIDE {}
  virtual void OnSentTo(AsyncDatagramSocket* socket, DWORD error, void* buffer,
                        int length, sockaddr* to, int to_length)
                        MADOKA_OVERRIDE {}
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_SOCKET_EVENT_LISTENER_H_
