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
  virtual void OnReceived(AsyncSocket* socket, DWORD error, int length) = 0;
  virtual void OnSent(AsyncSocket* socket, DWORD error, int length) = 0;

  virtual void OnReceived(AsyncDatagramSocket* socket, DWORD error,
                          int length) = 0;
  virtual void OnReceivedFrom(AsyncDatagramSocket* socket, DWORD error,
                              int length, sockaddr* from, int from_length) = 0;
  virtual void OnSent(AsyncDatagramSocket* socket, DWORD error, int length) = 0;
  virtual void OnSentTo(AsyncDatagramSocket* socket, DWORD error, int length,
                        sockaddr* to, int to_length) = 0;
};

class SocketEventAdapter : public SocketEventListener {
 public:
  virtual ~SocketEventAdapter() {}

  virtual void OnAccepted(AsyncServerSocket* server, AsyncSocket* client,
                          DWORD error) {}

  virtual void OnConnected(AsyncSocket* socket, DWORD error) {}
  virtual void OnReceived(AsyncSocket* socket, DWORD error, int length) {}
  virtual void OnSent(AsyncSocket* socket, DWORD error, int length) {}

  virtual void OnReceived(AsyncDatagramSocket* socket, DWORD error,
                          int length) {}
  virtual void OnReceivedFrom(AsyncDatagramSocket* socket, DWORD error,
                              int length, sockaddr* from, int from_length) {}
  virtual void OnSent(AsyncDatagramSocket* socket, DWORD error, int length) {}
  virtual void OnSentTo(AsyncDatagramSocket* socket, DWORD error, int length,
                        sockaddr* to, int to_length) {}
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_SOCKET_EVENT_LISTENER_H_
