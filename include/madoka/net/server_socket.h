// Copyright (c) 2013 dacci.org

#ifndef MADOKA_NET_SERVER_SOCKET_H_
#define MADOKA_NET_SERVER_SOCKET_H_

#include <madoka/net/abstract_socket.h>
#include <madoka/net/socket.h>

namespace madoka {
namespace net {

class ServerSocket : public AbstractSocket {
 public:
  ServerSocket() : bound_() {
  }

  virtual ~ServerSocket() {
    Close();
  }

  virtual void Close() {
    bound_ = false;
    AbstractSocket::Close();
  }

  bool Bind(const addrinfo* end_point) {
    if (bound_)
      return false;
    if (!Create(end_point))
      return false;

    return Bind(end_point->ai_addr, end_point->ai_addrlen);
  }

#ifdef _WIN32
#if ((NTDDI_VERSION >= NTDDI_WINXPSP2) || (_WIN32_WINNT >= 0x0502))

  bool Bind(const ADDRINFOW* end_point) {
    if (bound_)
      return false;
    if (!Create(end_point))
      return false;

    return Bind(end_point->ai_addr, end_point->ai_addrlen);
  }

#endif  // ((NTDDI_VERSION >= NTDDI_WINXPSP2) || (_WIN32_WINNT >= 0x0502))
#endif  // _WIN32

  bool Listen(int backlog) {
    if (!bound_)
      return false;

    return ::listen(descriptor_, backlog) == 0;
  }

  Socket* Accept() {
    if (!bound_)
      return NULL;

    sockaddr_storage address;
    int length = sizeof(address);

    SOCKET peer = ::accept(descriptor_, reinterpret_cast<sockaddr*>(&address),
                           &length);

    if (peer == INVALID_SOCKET)
      return NULL;

    return new Socket(peer);
  }

  bool bound() const {
    return bound_;
  }

 private:
  bool Bind(const sockaddr* address, size_t length) {
    if (bound_)
      return false;
    if (!IsValid())
      return false;

    bound_ = ::bind(descriptor_, address, static_cast<int>(length)) == 0;
    if (!bound_)
      Close();

    return bound_;
  }

  bool bound_;

  DISALLOW_COPY_AND_ASSIGN(ServerSocket);
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_SERVER_SOCKET_H_
