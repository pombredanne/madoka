// Copyright (c) 2015 dacci.org

#include "madoka/net/async_server_socket.h"

#include "madoka/concurrent/lock_guard.h"

#ifndef HRESULT_FROM_LAST_ERROR
#define HRESULT_FROM_LAST_ERROR() HRESULT_FROM_WIN32(GetLastError())
#endif  // HRESULT_FROM_LAST_ERROR

namespace madoka {
namespace net {

struct AsyncServerSocket::AsyncContext : OVERLAPPED {
  AsyncServerSocket* server;
  SOCKET socket;
  std::unique_ptr<char[]> buffer;
  Listener* listener;
};

AsyncServerSocket::AsyncServerSocket()
    : old_descriptor_(INVALID_SOCKET), io_(nullptr) {
}

AsyncServerSocket::AsyncServerSocket(int family, int type, int protocol)
    : old_descriptor_(INVALID_SOCKET), io_(nullptr) {
  Create(family, type, protocol);
}

AsyncServerSocket::~AsyncServerSocket() {
  Close();
  Reset();

  madoka::concurrent::LockGuard guard(&lock_);
  while (!requests_.empty())
    empty_.Sleep(&lock_);
}

void AsyncServerSocket::AcceptAsync(Listener* listener) {
  HRESULT result = S_OK;

  if (listener == nullptr)
    result = E_INVALIDARG;
  else if (!bound())
    result = __HRESULT_FROM_WIN32(WSAEINVAL);
  else if (!IsValid())
    result = __HRESULT_FROM_WIN32(WSAENOTSOCK);

  if (SUCCEEDED(result)) {
    auto context = CreateContext(listener, NULL);
    if (context != nullptr)
      result = DispatchRequest(std::move(context));
    else
      result = E_OUTOFMEMORY;
  }

  if (FAILED(result))
    listener->OnAccepted(this, result, nullptr);
}

AsyncServerSocket::AsyncContext* AsyncServerSocket::BeginAccept(HANDLE event) {
  if (event == NULL)
    return nullptr;
  if (!bound())
    return nullptr;
  if (!IsValid())
    return nullptr;

  auto context = CreateContext(nullptr, event);
  if (context == nullptr)
    return nullptr;

  ResetEvent(event);

  auto pointer = context.get();
  HRESULT result = DispatchRequest(std::move(context));
  if (FAILED(result))
    return nullptr;

  return pointer;
}

void AsyncServerSocket::Reset() {
  if (io_ != nullptr) {
    WaitForThreadpoolIoCallbacks(io_, FALSE);
    CloseThreadpoolIo(io_);
    io_ = nullptr;
  }

  old_descriptor_ = INVALID_SOCKET;
}

HRESULT AsyncServerSocket::EndAccept(AsyncContext* context, SOCKET* socket) {
  if (context == nullptr || socket == nullptr)
    return E_INVALIDARG;
  if (context->server != this)
    return E_HANDLE;

  HRESULT result = S_OK;

  do {
    if (context->hEvent &&
        WaitForSingleObject(context->hEvent, INFINITE) != WAIT_OBJECT_0) {
      result = HRESULT_FROM_LAST_ERROR();
      break;
    }

    DWORD length, flags;
    if (!WSAGetOverlappedResult(descriptor_, context, &length, TRUE, &flags)) {
      result = HRESULT_FROM_LAST_ERROR();
      break;
    }

    bool succeeded = setsockopt(context->socket,
                                SOL_SOCKET,
                                SO_UPDATE_ACCEPT_CONTEXT,
                                reinterpret_cast<char*>(&descriptor_),
                                sizeof(descriptor_)) == 0;
    if (!succeeded) {
      result = HRESULT_FROM_LAST_ERROR();
      break;
    }

    *socket = context->socket;
    context->socket = INVALID_SOCKET;
  } while (false);

  DeleteContext(context);

  return result;
}

std::unique_ptr<AsyncServerSocket::AsyncContext>
    AsyncServerSocket::CreateContext(Listener* listener, HANDLE event) {
  auto context = std::make_unique<AsyncContext>();
  if (context != nullptr) {
    memset(static_cast<OVERLAPPED*>(context.get()), 0, sizeof(OVERLAPPED));
    context->server = this;
    context->socket = INVALID_SOCKET;
    context->listener = listener;
    context->hEvent = event;
  }

  return context;
}

HRESULT AsyncServerSocket::DispatchRequest(
    std::unique_ptr<AsyncContext>&& context) {  // NOLINT(build/c++11)
  madoka::concurrent::LockGuard guard(&lock_);

  if (!TrySubmitThreadpoolCallback(OnRequested, context.get(), nullptr))
    return HRESULT_FROM_LAST_ERROR();

  requests_.push_back(std::move(context));

  return S_OK;
}

void AsyncServerSocket::DeleteContext(AsyncContext* context) {
  madoka::concurrent::LockGuard guard(&lock_);

  for (auto i = requests_.begin(), l = requests_.end(); i != l; ++i) {
    if (i->get() == context) {
      if (context->socket != INVALID_SOCKET)
        closesocket(context->socket);

      requests_.erase(i);

      if (requests_.empty())
        empty_.WakeAll();

      break;
    }
  }
}

void CALLBACK AsyncServerSocket::OnRequested(PTP_CALLBACK_INSTANCE instance,
                                             void* param) {
  auto context = static_cast<AsyncContext*>(param);
  context->server->OnRequested(context);
}

void AsyncServerSocket::OnRequested(AsyncContext* context) {
  HRESULT result = S_OK;

  do {
    madoka::concurrent::LockGuard guard(&lock_);

    if (descriptor_ == old_descriptor_)
      break;

    Reset();

    if (!GetOption(SOL_SOCKET, SO_PROTOCOL_INFO, &protocol_)) {
      result = HRESULT_FROM_LAST_ERROR();
      break;
    }

    io_ = CreateThreadpoolIo(reinterpret_cast<HANDLE>(descriptor_),
                             OnCompleted, this, nullptr);
    if (io_ == nullptr) {
      result = HRESULT_FROM_LAST_ERROR();
      break;
    }

    old_descriptor_ = descriptor_;
  } while (false);

  do {
    if (FAILED(result))
      break;

    context->socket = socket(protocol_.iAddressFamily, protocol_.iSocketType,
                             protocol_.iProtocol);
    if (context->socket == INVALID_SOCKET) {
      result = HRESULT_FROM_LAST_ERROR();
      break;
    }

    DWORD address_length = sizeof(sockaddr_storage) + 16;

    context->buffer = std::make_unique<char[]>(address_length * 2);
    if (context->buffer == nullptr) {
      result = E_OUTOFMEMORY;
      break;
    }

    StartThreadpoolIo(io_);

    BOOL succeeded = AcceptEx(descriptor_, context->socket,
                              context->buffer.get(), 0, address_length,
                              address_length, nullptr, context);
    DWORD error = GetLastError();
    if (!succeeded && error != ERROR_IO_PENDING) {
      result = __HRESULT_FROM_WIN32(error);
      CancelThreadpoolIo(io_);
      break;
    }
  } while (false);

  if (FAILED(result))
    OnCompleted(context, result, 0);
}

void CALLBACK AsyncServerSocket::OnCompleted(PTP_CALLBACK_INSTANCE instance,
                                             void* param, void* overlapped,
                                             ULONG error, ULONG_PTR length,
                                             PTP_IO io) {
  auto context =
      static_cast<AsyncContext*>(static_cast<OVERLAPPED*>(overlapped));
  context->server->OnCompleted(context, __HRESULT_FROM_WIN32(error), length);
}

void AsyncServerSocket::OnCompleted(AsyncContext* context, HRESULT result,
                                    ULONG_PTR length) {
  {
    madoka::concurrent::LockGuard guard(&lock_);

    bool found = false;
    for (auto& queued : requests_) {
      if (queued.get() == context) {
        found = true;
        break;
      }
    }
    if (!found || context->listener == nullptr)
      return;
  }

  context->listener->OnAccepted(this, result, context);

  DeleteContext(context);
}

}  // namespace net
}  // namespace madoka
