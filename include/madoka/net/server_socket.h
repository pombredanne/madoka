// Copyright (c) 2015 dacci.org

#ifndef MADOKA_NET_SERVER_SOCKET_H_
#define MADOKA_NET_SERVER_SOCKET_H_

#include <madoka/net/abstract_socket.h>
#include <madoka/net/socket.h>

namespace madoka {
namespace net {

class ServerSocket : public AbstractSocket {
 public:
  ServerSocket() {
  }

  ServerSocket(int family, int type, int protocol) {
    Create(family, type, protocol);
  }

  bool Listen(int backlog) {
    return listen(descriptor_, backlog) == 0;
  }

  Socket* Accept() {
    sockaddr_storage address;
    int length = sizeof(address);

    SOCKET peer = accept(descriptor_, reinterpret_cast<sockaddr*>(&address),
                         &length);
    if (peer == INVALID_SOCKET)
      return nullptr;

    return new Socket(peer);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServerSocket);
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_SERVER_SOCKET_H_
