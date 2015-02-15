// Copyright (c) 2015 dacci.org

#ifndef MADOKA_NET_SERVER_SOCKET_H_
#define MADOKA_NET_SERVER_SOCKET_H_

#include <madoka/net/abstract_socket.h>

#include <memory>

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

  template<class T>
  std::unique_ptr<T> Accept() {
    SOCKET peer = Accept();
    if (peer == INVALID_SOCKET)
      return nullptr;

    auto client = new T(peer);
    if (client == nullptr) {
      closesocket(peer);
      return nullptr;
    }

    return std::unique_ptr<T>(client);
  }

 private:
  SOCKET Accept() {
    sockaddr_storage address;
    int length = sizeof(address);
    return accept(descriptor_, reinterpret_cast<sockaddr*>(&address), &length);
  }

  MADOKA_DISALLOW_COPY_AND_ASSIGN(ServerSocket);
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_SERVER_SOCKET_H_
