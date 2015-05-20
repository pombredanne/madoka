// Copyright (c) 2015 dacci.org

#ifndef MADOKA_NET_SERVER_SOCKET_H_
#define MADOKA_NET_SERVER_SOCKET_H_

#include <madoka/net/abstract_socket.h>

#include <memory>

namespace madoka {
namespace net {

class ServerSocket : public AbstractSocket {
 public:
  ServerSocket() : listening_(false) {
  }

  ServerSocket(int family, int type, int protocol) : ServerSocket() {
    Create(family, type, protocol);
  }

  bool Listen(int backlog) {
    bool succeeded = listen(descriptor_, backlog) == 0;
    if (succeeded)
      listening_ = true;

    return succeeded;
  }

  template<class Impl>
  std::unique_ptr<Impl> Accept() {
    struct Wrapper : Impl {
      explicit Wrapper(SOCKET descriptor) {
        descriptor_ = descriptor;
        bound_ = true;
        connected_ = true;
      }
    };

    sockaddr_storage address;
    int length = sizeof(address);
    SOCKET descriptor = accept(descriptor_,
                               reinterpret_cast<sockaddr*>(&address), &length);
    if (descriptor == INVALID_SOCKET)
      return nullptr;

    auto accepted = std::make_unique<Impl>(descriptor);
    if (accepted == nullptr)
      closesocket(descriptor);

    return std::move(accepted);
  }

 protected:
  bool listening_;

 private:
  MADOKA_DISALLOW_COPY_AND_ASSIGN(ServerSocket);
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_SERVER_SOCKET_H_
