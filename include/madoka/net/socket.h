// Copyright (c) 2013 dacci.org

#ifndef MADOKA_NET_SOCKET_H_
#define MADOKA_NET_SOCKET_H_

#include <stdio.h>

#include <madoka/net/abstract_socket.h>

namespace madoka {
namespace net {

class Socket : public AbstractSocket {
 public:
  Socket() : connected_() {
  }

  virtual ~Socket() {
    Close();
  }

  virtual void Close() {
    connected_ = false;
    AbstractSocket::Close();
  }

  bool Connect(const addrinfo* end_point) {
    if (connected_)
      return false;
    if (end_point->ai_socktype != SOCK_STREAM)
      return false;
    if (!Create(end_point))
      return false;

    return Connect(end_point->ai_addr, end_point->ai_addrlen);
  }

#ifdef _WIN32
#if ((NTDDI_VERSION >= NTDDI_WINXPSP2) || (_WIN32_WINNT >= 0x0502))

  bool Connect(const ADDRINFOW* end_point) {
    if (connected_)
      return false;
    if (end_point->ai_socktype != SOCK_STREAM)
      return false;
    if (!Create(end_point))
      return false;

    return Connect(end_point->ai_addr, end_point->ai_addrlen);
  }

#endif  // ((NTDDI_VERSION >= NTDDI_WINXPSP2) || (_WIN32_WINNT >= 0x0502))
#endif  // _WIN32

  bool Shutdown(int how) {
    if (!IsValid())
      return false;

    connected_ = false;
    return ::shutdown(descriptor_, how) == 0;
  }

  int Receive(void* buffer, int length, int flags) {
    if (!IsValid() || !connected_ || buffer == NULL || length < 0)
      return SOCKET_ERROR;
    if (length == 0)
      return 0;

    int result = ::recv(descriptor_, static_cast<char*>(buffer), length, flags);
    if (result == 0)
      connected_ = false;

    return result;
  }

  int Send(const void* buffer, int length, int flags) {
    if (!IsValid() || !connected_ || buffer == NULL || length < 0)
      return SOCKET_ERROR;
    if (length == 0)
      return 0;

    return ::send(descriptor_, static_cast<const char*>(buffer), length, flags);
  }

  bool GetRemoteEndPoint(sockaddr* end_point, int* length) {
    if (!IsValid())
      return false;

    return ::getpeername(descriptor_, end_point, length) == 0;
  }

  bool connected() const {
    return connected_;
  }

 protected:
   void set_connected(bool connected) {
     connected_ = connected;
   }

 private:
  friend class ServerSocket;

  explicit Socket(SOCKET descriptor) : AbstractSocket(descriptor) {
    connected_ = true;
  }

  bool Connect(const sockaddr* address, size_t length) {
    if (connected_)
      return false;
    if (!IsValid())
      return false;

    connected_ = ::connect(descriptor_, address, static_cast<int>(length)) == 0;
    if (!connected_)
      Close();

    return connected_;
  }

  bool connected_;

  DISALLOW_COPY_AND_ASSIGN(Socket);
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_SOCKET_H_
