// Copyright (c) 2013 dacci.org

#ifndef MADOKA_NET_SERVER_SOCKET_H_
#define MADOKA_NET_SERVER_SOCKET_H_

#include <madoka/net/abstract_socket.h>
#include <madoka/net/resolver.h>
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
    if (end_point->ai_socktype != SOCK_STREAM)
      return false;
    if (!Create(end_point))
      return false;

    return Bind(end_point->ai_addr, end_point->ai_addrlen);
  }

  bool Bind(const char* node_name, const char* service) {
    if (bound_)
      return false;

    Resolver resolver;
    resolver.SetFlags(AI_PASSIVE);
    resolver.SetType(SOCK_STREAM);
    if (!resolver.Resolve(node_name, service))
      return false;

    for (auto i = resolver.begin(), l = resolver.end(); i != l; ++i) {
      if (Bind(*i))
        break;

      Close();
    }

    return bound_;
  }

  bool Bind(const char* node_name, int port) {
    if (!IsValidPort(port))
      return false;

    char service[8];
#ifdef _MSC_VER
    ::sprintf_s(service, "%d", port);
#else
    ::snprintf(service, sizeof(service), "%d", port);
#endif  // _MSC_VER

    return Bind(node_name, service);
  }

#if defined WIN32 && \
    ((NTDDI_VERSION >= NTDDI_WINXPSP2) || (_WIN32_WINNT >= 0x0502))
  bool Bind(const ADDRINFOW* end_point) {
    if (bound_)
      return false;
    if (end_point->ai_socktype != SOCK_STREAM)
      return false;
    if (!Create(end_point))
      return false;

    return Bind(end_point->ai_addr, end_point->ai_addrlen);
  }

  bool Bind(const wchar_t* node_name, const wchar_t* service) {
    if (bound_)
      return false;

    ResolverW resolver;
    resolver.SetFlags(AI_PASSIVE);
    resolver.SetType(SOCK_STREAM);
    if (!resolver.Resolve(node_name, service))
      return false;

    for (auto i = resolver.begin(), l = resolver.end(); i != l; ++i) {
      if (Bind(*i))
        break;

      Close();
    }

    return bound_;
  }

  bool Bind(const wchar_t* node_name, int port) {
    if (!IsValidPort(port))
      return false;

    wchar_t service[8];
#ifdef _MSC_VER
    ::swprintf_s(service, L"%d", port);
#else
    ::swprintf(service, _countof(service), L"%d", port);
#endif  // _MSC_VER

    return Bind(node_name, service);
  }
#endif  // defined WIN32 &&
        // ((NTDDI_VERSION >= NTDDI_WINXPSP2) || (_WIN32_WINNT >= 0x0502))

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

 protected:
  bool bound_;

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

  DISALLOW_COPY_AND_ASSIGN(ServerSocket);
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_SERVER_SOCKET_H_
