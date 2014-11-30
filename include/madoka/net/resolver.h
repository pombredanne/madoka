// Copyright (c) 2014 dacci.org

#ifndef MADOKA_NET_RESOLVER_H_
#define MADOKA_NET_RESOLVER_H_

#include <madoka/net/common.h>

#include <assert.h>
#include <stdio.h>

#include <iterator>

namespace madoka {
namespace net {

template<typename InfoType, typename CharType>
class ResolverBase {
 public:
  class iterator
      : public std::iterator<std::input_iterator_tag, const InfoType*> {
   public:
    explicit iterator(const InfoType* entries)
        : entries_(entries), current_(entries) {
    }

    iterator& operator++() {
      current_ = current_->ai_next;
      return *this;
    }

    bool operator==(const iterator& other) const {
      assert(other.entries_ == NULL || entries_ == other.entries_);
      return current_ == other.current_;
    }

    bool operator!=(const iterator& other) const {
      assert(other.entries_ == NULL || entries_ == other.entries_);
      return current_ != other.current_;
    }

    const InfoType* operator*() const {
      return current_;
    }

   private:
    const InfoType* entries_;
    const InfoType* current_;
  };

  ResolverBase() : hints_(), entries_(), error_() {
  }

  virtual bool Resolve(const CharType* node_name, const CharType* service) = 0;
  virtual bool Resolve(const CharType* node_name, int port) = 0;

  iterator begin() const {
    return iterator(entries_);
  }

  iterator end() const {
    return iterator(nullptr);
  }

  int GetFlags() const {
    return hints_.ai_flags;
  }

  void SetFlags(int flags) {
    hints_.ai_flags = flags;
  }

  int GetFamily() const {
    return hints_.ai_family;
  }

  void SetFamily(int family) {
    hints_.ai_family = family;
  }

  int GetType() const {
    return hints_.ai_socktype;
  }

  void SetType(int type) {
    hints_.ai_socktype = type;
  }

  int GetProtocol() const {
    return hints_.ai_protocol;
  }

  void SetProtocol(int protocol) {
    hints_.ai_protocol = protocol;
  }

  int error() const {
    return error_;
  }

 protected:
  virtual ~ResolverBase() {
    assert(entries_ == nullptr);
  }

  virtual void Free() = 0;

  InfoType hints_;
  InfoType* entries_;
  int error_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResolverBase);
};

class Resolver : public ResolverBase<addrinfo, char> {
 public:
  Resolver() {
  }

  virtual ~Resolver() {
    Free();
  }

  bool Resolve(const char* node_name, const char* service) MADOKA_OVERRIDE {
    Free();
    error_ = ::getaddrinfo(node_name, service, &hints_, &entries_);
    return error_ == 0;
  }

  bool Resolve(const char* node_name, int port) MADOKA_OVERRIDE {
    Free();

    if (port < 0 || 65535 < port) {
#ifdef _WIN32
      error_ = WSAEINVAL;
#else  // _WIN32
      error_ = EAI_SYSTEM;
      errno = EINVAL;
#endif
      return false;
    }

    char service[8];
#ifdef _MSC_VER
    ::sprintf_s(service, "%d", port);
#else  // _MSC_VER
    ::snprintf(service, sizeof(service), "%d", port);
#endif  // _MSC_VER

    return Resolve(node_name, service);
  }

 protected:
  virtual void Free() {
    if (entries_) {
      ::freeaddrinfo(entries_);
      entries_ = nullptr;
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Resolver);
};

#ifdef _WIN32
#if (NTDDI_VERSION >= NTDDI_WINXPSP2) || (_WIN32_WINNT >= 0x0502)

class ResolverW : public ResolverBase<ADDRINFOW, wchar_t> {
 public:
  ResolverW() {
  }

  virtual ~ResolverW() {
    Free();
  }

  bool Resolve(const wchar_t* node_name,
               const wchar_t* service) MADOKA_OVERRIDE {
    Free();
    error_ = ::GetAddrInfoW(node_name, service, &hints_, &entries_);
    return error_ == 0;
  }

  bool Resolve(const wchar_t* node_name, int port) MADOKA_OVERRIDE {
    Free();

    if (port < 0 || 65535 < port) {
#ifdef _WIN32
      error_ = WSAEINVAL;
#else  // _WIN32
      error_ = EAI_SYSTEM;
      errno = EINVAL;
#endif
      return false;
    }

    wchar_t service[8];
#ifdef _MSC_VER
    ::swprintf_s(service, L"%d", port);
#else  // _MSC_VER
    ::swprintf(service, sizeof(service) / sizeof(*service), L"%d", port);
#endif  // _MSC_VER

    return Resolve(node_name, service);
  }

 protected:
  virtual void Free() {
    if (entries_) {
      ::FreeAddrInfoW(entries_);
      entries_ = nullptr;
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ResolverW);
};

#ifdef UNICODE
typedef ResolverW ResolverT;
#else  // UNICODE
typedef Resolver ResolverT;
#endif  // UNICODE

#endif  // (NTDDI_VERSION >= NTDDI_WINXPSP2) || (_WIN32_WINNT >= 0x0502)
#endif  // _WIN32

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_RESOLVER_H_
