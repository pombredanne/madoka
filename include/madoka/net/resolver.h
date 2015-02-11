// Copyright (c) 2015 dacci.org

#ifndef MADOKA_NET_RESOLVER_H_
#define MADOKA_NET_RESOLVER_H_

#include <madoka/net/common.h>

#include <assert.h>

#include <iterator>
#include <string>

namespace madoka {
namespace net {

template<typename Info, typename String, typename Char = String::value_type>
class ResolverBase {
 public:
  typedef typename Info InfoType;
  typedef typename String StringType;
  typedef typename Char CharType;

  class Iterator
      : public std::iterator<std::input_iterator_tag, const InfoType*> {
   public:
    bool operator!=(const Iterator& other) const {
      assert(other.entries_ == nullptr || other.entries_ == entries_);
      return current_ != other.current_;
    }

    Iterator& operator++() {
      assert(current_ != nullptr);
      current_ = current_->ai_next;
      return *this;
    }

    const value_type& operator*() const {
      return current_;
    }

   private:
    friend class ResolverBase;

    explicit Iterator(const value_type& entries)
        : entries_(entries), current_(entries) {
    }

    const value_type entries_;
    value_type current_;
  };

  ResolverBase() : hints_(), entries_(nullptr) {
  }

  virtual ~ResolverBase() {
  }

  bool Resolve(const CharType* node_name, const CharType* service) {
    FreeInfo();
    entries_ = nullptr;

    error_ = GetInfo(node_name, service);
    return error_ == 0;
  }

  bool Resolve(const StringType& node_name, const StringType& service) {
    return Resolve(GetChars(node_name), GetChars(service));
  }

  bool Resolve(const CharType* node_name, int port) {
    if (port < 0 || 65535 < port) {
#ifdef _WIN32
      error_ = WSAEINVAL;
#else  // _WIN32
      error_ = EAI_SYSTEM;
      errno = EINVAL;
#endif
      return false;
    }

    return Resolve(node_name, GetChars(ToString(port)));
  }

  bool Resolve(const StringType& node_name, int port) {
    return Resolve(GetChars(node_name), port);
  }

  Iterator begin() const {
    return Iterator(entries_);
  }

  Iterator end() const {
    return Iterator(nullptr);
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
  InfoType hints_;
  InfoType* entries_;

 private:
  virtual int GetInfo(const CharType* node_name, const CharType* service) = 0;
  virtual void FreeInfo() = 0;
  virtual StringType ToString(int value) = 0;
  virtual const CharType* GetChars(const StringType& value) = 0;

  int error_;

  MADOKA_DISALLOW_COPY_AND_ASSIGN(ResolverBase);
};

class Resolver : public ResolverBase<addrinfo, std::string> {
 public:
  Resolver() {
  }

  ~Resolver() {
    FreeInfo();
  }

 private:
  int GetInfo(const CharType* node_name, const CharType* service) override {
    return getaddrinfo(node_name, service, &hints_, &entries_);
  }

  void FreeInfo() override {
    freeaddrinfo(entries_);
  }

  StringType ToString(int value) override {
    return std::to_string(value);
  }

  const CharType* GetChars(const StringType& value) override {
    return value.c_str();
  }

  MADOKA_DISALLOW_COPY_AND_ASSIGN(Resolver);
};

#ifdef _WIN32

class ResolverW : public ResolverBase<ADDRINFOW, std::wstring> {
 public:
  ResolverW() {
  }

  ~ResolverW() {
    FreeInfo();
  }

 private:
  int GetInfo(const CharType* node_name, const CharType* service) override {
    return GetAddrInfoW(node_name, service, &hints_, &entries_);
  }

  void FreeInfo() override {
    FreeAddrInfoW(entries_);
  }

  StringType ToString(int value) override {
    return std::to_wstring(value);
  }

  const CharType* GetChars(const StringType& value) override {
    return value.c_str();
  }

  MADOKA_DISALLOW_COPY_AND_ASSIGN(ResolverW);
};

#endif  // _WIN32

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_RESOLVER_H_
