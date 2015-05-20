// Copyright (c) 2015 dacci.org

#ifndef MADOKA_NET_ABSTRACT_SOCKET_H_
#define MADOKA_NET_ABSTRACT_SOCKET_H_

#include <madoka/net/common.h>

namespace madoka {
namespace net {

class AbstractSocket {
 public:
  virtual ~AbstractSocket() {
    Close();
  }

  virtual void Close() {
    closesocket(descriptor_);
    descriptor_ = INVALID_SOCKET;
    bound_ = false;
  }

  bool Bind(const void* address, int address_length) {
    bool succeeded = bind(descriptor_, static_cast<const sockaddr*>(address),
                          address_length) == 0;
    if (succeeded)
      bound_ = true;

    return succeeded;
  }

  bool Bind(const addrinfo* end_point) {
    if (bound_) {
#ifdef _WIN32
      WSASetLastError(WSAEINVAL);
#else
      errno = EINVAL;
#endif
      return false;
    }

    if (!Create(end_point))
      return false;

    return Bind(end_point->ai_addr, end_point->ai_addrlen);
  }

#ifdef _WIN32
  bool Bind(const ADDRINFOW* end_point) {
    if (bound_) {
      WSASetLastError(WSAEINVAL);
      return false;
    }

    if (!Create(end_point))
      return false;

    return Bind(end_point->ai_addr, end_point->ai_addrlen);
  }
#endif  // _WIN32

  bool IOControl(int command, void* param) {
    return ioctlsocket(descriptor_, command, static_cast<u_long*>(param)) == 0;
  }

  bool GetLocalEndPoint(void* address, int* length) {
    return getsockname(descriptor_, static_cast<sockaddr*>(address),
                       length) == 0;
  }

  bool GetOption(int level, int option, void* value, int* length) {
    return getsockopt(descriptor_, level, option, static_cast<char*>(value),
                      length) == 0;
  }

  template<typename T>
  bool GetOption(int level, int option, T* value) {
    int size = sizeof(*value);
    return GetOption(level, option, value, &size);
  }

  bool SetOption(int level, int option, const void* value, int length) {
    return setsockopt(descriptor_, level, option,
                      static_cast<const char*>(value), length) == 0;
  }

  template<typename T>
  bool SetOption(int level, int option, const T& value) {
    return SetOption(level, option, &value, sizeof(value));
  }

  bool bound() const {
    return bound_;
  }

  bool IsValid() const {
    return descriptor_ != INVALID_SOCKET;
  }

  explicit operator bool() const {
    return IsValid();
  }

 protected:
  AbstractSocket() : descriptor_(INVALID_SOCKET), bound_(false) {
  }

  bool Create(int family, int type, int protocol) {
    Close();

    descriptor_ = socket(family, type, protocol);
    return IsValid();
  }

  bool Create(const addrinfo* end_point) {
    return Create(end_point->ai_family, end_point->ai_socktype,
                  end_point->ai_protocol);
  }

#ifdef _WIN32
  bool Create(const ADDRINFOW* end_point) {
    return Create(end_point->ai_family, end_point->ai_socktype,
                  end_point->ai_protocol);
  }
#endif  // _WIN32

  SOCKET descriptor_;
  bool bound_;

 private:
  MADOKA_DISALLOW_COPY_AND_ASSIGN(AbstractSocket);
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_ABSTRACT_SOCKET_H_
