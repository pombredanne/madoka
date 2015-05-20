// Copyright (c) 2015 dacci.org

#ifndef MADOKA_NET_SOCKET_H_
#define MADOKA_NET_SOCKET_H_

#include <madoka/net/abstract_socket.h>

namespace madoka {
namespace net {

class Socket : public AbstractSocket {
 public:
  Socket() : connected_(false) {
  }

  Socket(int family, int type, int protocol) : Socket() {
    Create(family, type, protocol);
  }

  virtual ~Socket() {
    Close();
  }

  void Close() override {
    AbstractSocket::Close();
    connected_ = false;
  }

  bool Connect(const void* address, int length) {
    bool succeeded = connect(descriptor_, static_cast<const sockaddr*>(address),
                             length) == 0;
    if (succeeded)
      connected_ = true;

    return succeeded;
  }

  bool Connect(const addrinfo* end_point) {
    if (connected_) {
#ifdef _WIN32
      WSASetLastError(WSAEISCONN);
#else
      errno = EISCONN;
#endif
      return false;
    }

    if (!Create(end_point))
      return false;

    return Connect(end_point->ai_addr, end_point->ai_addrlen);
  }

#ifdef _WIN32
  bool Connect(const ADDRINFOW* end_point) {
    if (connected_) {
      WSASetLastError(WSAEISCONN);
      return false;
    }

    if (!Create(end_point))
      return false;

    return Connect(end_point->ai_addr, end_point->ai_addrlen);
  }
#endif  // _WIN32

  void Shutdown(int how) {
    shutdown(descriptor_, how);
    connected_ = false;
  }

  int Receive(void* buffer, int length, int flags) {
    return recv(descriptor_, static_cast<char*>(buffer), length, flags);
  }

  int ReceiveFrom(void* buffer, int length, int flags, void* address,
                  int* address_length) {
    return recvfrom(descriptor_, static_cast<char*>(buffer), length, flags,
                    static_cast<sockaddr*>(address), address_length);
  }

  int Send(const void* buffer, int length, int flags) {
    return send(descriptor_, static_cast<const char*>(buffer), length, flags);
  }

  int SendTo(const void* buffer, int length, int flags, const void* address,
             int address_length) {
    return sendto(descriptor_, static_cast<const char*>(buffer), length, flags,
                  static_cast<const sockaddr*>(address), address_length);
  }

  bool GetRemoteEndPoint(void* address, int* length) {
    return getpeername(descriptor_, static_cast<sockaddr*>(address),
                       length) == 0;
  }

  bool connected() const {
    return connected_;
  }

 protected:
  bool connected_;

 private:
  MADOKA_DISALLOW_COPY_AND_ASSIGN(Socket);
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_SOCKET_H_
