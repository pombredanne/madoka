// Copyright (c) 2013 dacci.org

#ifndef MADOKA_NET_DATAGRAM_SOCKET_H_
#define MADOKA_NET_DATAGRAM_SOCKET_H_

#include <stdio.h>

#include <madoka/net/abstract_socket.h>

namespace madoka {
namespace net {

class DatagramSocket : public AbstractSocket {
 public:
  DatagramSocket() : bound_(), connected_() {
  }

  DatagramSocket(int family, int protocol) : bound_(), connected_() {
    Create(family, SOCK_DGRAM, protocol);
  }

  virtual ~DatagramSocket() {
    Close();
  }

  virtual void Close() {
    bound_ = false;
    connected_ = false;
    AbstractSocket::Close();
  }

  bool Bind(const sockaddr* address, size_t length) {
    if (bound_)
      return false;
    if (!IsValid())
      return false;

    bound_ = ::bind(descriptor_, address, static_cast<int>(length)) == 0;
    return bound_;
  }

  bool Bind(const addrinfo* end_point) {
    if (end_point->ai_socktype != SOCK_DGRAM)
      return false;
    if (!Create(end_point))
      return false;

    return Bind(end_point->ai_addr, end_point->ai_addrlen);
  }

  bool Connect(const sockaddr* address, size_t length) {
    if (connected_)
      return false;
    if (!IsValid())
      return false;

    connected_ = ::connect(descriptor_, address, static_cast<int>(length)) == 0;
    return connected_;
  }

  bool Connect(const addrinfo* end_point) {
    if (end_point->ai_socktype != SOCK_DGRAM)
      return false;
    if (!Create(end_point))
      return false;

    return Connect(end_point->ai_addr, end_point->ai_addrlen);
  }

#ifdef _WIN32
#if (NTDDI_VERSION >= NTDDI_WINXPSP2) || (_WIN32_WINNT >= 0x0502)

  bool Bind(const ADDRINFOW* end_point) {
    if (end_point->ai_socktype != SOCK_DGRAM)
      return false;
    if (!Create(end_point))
      return false;

    return Bind(end_point->ai_addr, end_point->ai_addrlen);
  }

  bool Connect(const ADDRINFOW* end_point) {
    if (end_point->ai_socktype != SOCK_DGRAM)
      return false;
    if (!Create(end_point))
      return false;

    return Connect(end_point->ai_addr, end_point->ai_addrlen);
  }

#endif  // (NTDDI_VERSION >= NTDDI_WINXPSP2) || (_WIN32_WINNT >= 0x0502)
#endif  // _WIN32

  virtual int Receive(void* buffer, int length, int flags) {
    if (!IsValid() || !(bound_ || connected_) || buffer == NULL || length < 0)
      return SOCKET_ERROR;
    if (length == 0)
      return 0;

    return ::recv(descriptor_, static_cast<char*>(buffer), length, flags);
  }

  virtual int Send(const void* buffer, int length, int flags) {
    if (!IsValid() || !connected_ || buffer == NULL || length < 0)
      return SOCKET_ERROR;
    if (length == 0)
      return 0;

    return ::send(descriptor_, static_cast<const char*>(buffer), length,
                  flags);
  }

  int ReceiveFrom(void* buffer, int length, int flags, sockaddr* from,
                  int* fromlen) {
    if (!IsValid() || buffer == NULL || length < 0)
      return SOCKET_ERROR;

    return ::recvfrom(descriptor_, static_cast<char*>(buffer), length, flags,
                      from, fromlen);
  }

  int SendTo(const void* buffer, int length, int flags, const sockaddr* to,
             int tolen) {
    if (!IsValid() || buffer == NULL || length < 0)
      return SOCKET_ERROR;

    return ::sendto(descriptor_, static_cast<const char*>(buffer), length,
                    flags, to, tolen);
  }

  bool bound() const {
    return bound_;
  }

  bool connected() const {
    return connected_;
  }

 protected:
  bool set_bound(bool bound) {
    bound_ = bound;
  }

  bool set_connected(bool connected) {
    connected_ = connected;
  }

 private:
  bool bound_;
  bool connected_;

  DISALLOW_COPY_AND_ASSIGN(DatagramSocket);
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_DATAGRAM_SOCKET_H_
