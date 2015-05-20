// Copyright (c) 2014 dacci.org

#include "madoka/net/async_socket.h"

#include <assert.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "madoka/concurrent/lock_guard.h"

#undef min

namespace madoka {
namespace net {

namespace {
enum Request {
  Invalid, Connect, Receive, ReceiveFrom, Send, SendTo
};

LPFN_CONNECTEX ConnectEx = nullptr;
}  // namespace

struct AsyncSocket::Context : OVERLAPPED, WSABUF {
  Context()
      : OVERLAPPED(),
        WSABUF(),
        request(Request::Invalid),
        result(S_OK),
        end_point(nullptr),
        flags(0),
        address(),
        address_length(sizeof(address)),
        listener(nullptr),
        event(NULL) {
  }

  Request request;
  HRESULT result;
  const addrinfo* end_point;
  DWORD flags;
  sockaddr_storage address;
  int address_length;
  Listener* listener;
  HANDLE event;
};

PTP_CALLBACK_ENVIRON AsyncSocket::environment_ = nullptr;

AsyncSocket::AsyncSocket()
    : work_(CreateThreadpoolWork(OnRequested, this, environment_)),
      cancel_connect_(false),
      io_(nullptr) {
}

AsyncSocket::AsyncSocket(int family, int type, int protocol) : AsyncSocket() {
  Create(family, type, protocol);
}

AsyncSocket::~AsyncSocket() {
  Close();

  madoka::concurrent::LockGuard guard(&lock_);

  if (work_ != nullptr) {
    PTP_WORK work = work_;
    work_ = nullptr;

    lock_.Unlock();
    WaitForThreadpoolWorkCallbacks(work, FALSE);
    lock_.Lock();

    CloseThreadpoolWork(work);
  }
}

void AsyncSocket::Close() {
  madoka::concurrent::LockGuard guard(&lock_);

  cancel_connect_ = true;
  Socket::Close();

  if (io_ != nullptr) {
    lock_.Unlock();
    WaitForThreadpoolIoCallbacks(io_, FALSE);
    lock_.Lock();

    if (io_ != nullptr) {
      CloseThreadpoolIo(io_);
      io_ = nullptr;
    }
  }
}

PTP_CALLBACK_ENVIRON AsyncSocket::GetCallbackEnvironment() {
  return environment_;
}

void AsyncSocket::SetCallbackEnvironment(PTP_CALLBACK_ENVIRON environment) {
  environment_ = environment;
}

void AsyncSocket::ConnectAsync(const addrinfo* end_point, Listener* listener) {
  HRESULT result = S_OK;

  do {
    if (end_point == nullptr || listener == nullptr) {
      result = E_INVALIDARG;
      break;
    }

    auto context = CreateContext(Request::Connect, end_point, nullptr, 0, 0,
                                 nullptr, 0, listener, NULL);
    if (context == nullptr) {
      result = E_OUTOFMEMORY;
      break;
    }

    result = RequestAsync(std::move(context));
  } while (false);

  if (FAILED(result))
    listener->OnConnected(this, result, end_point);
}

AsyncSocket::Context* AsyncSocket::BeginConnect(const addrinfo* end_point,
                                                HANDLE event) {
  if (end_point == nullptr || event == NULL)
    return nullptr;

  auto context = CreateContext(Request::Connect, end_point, nullptr, 0, 0,
                               nullptr, 0, nullptr, event);
  if (context == nullptr)
    return nullptr;

  return BeginRequest(std::move(context));
}

HRESULT AsyncSocket::EndConnect(Context* context) {
  if (context == nullptr || context->request != Request::Connect)
    return E_INVALIDARG;

  HRESULT result = S_OK;
  if (context->event != NULL &&
      WaitForSingleObject(context->event, INFINITE) != WAIT_OBJECT_0)
    result = HRESULT_FROM_WIN32(GetLastError());
  else
    result = context->result;

  if (context->listener == nullptr)
    delete context;

  return result;
}

void AsyncSocket::ReceiveAsync(void* buffer, int length, int flags,
                               Listener* listener) {
  HRESULT result = S_OK;

  do {
    if (buffer == nullptr && length != 0 || listener == nullptr) {
      result = E_INVALIDARG;
      break;
    }

    auto context = CreateContext(Request::Receive, nullptr,
                                 static_cast<char*>(buffer), length, flags,
                                 nullptr, 0, listener, NULL);
    if (context == nullptr) {
      result = E_OUTOFMEMORY;
      break;
    }

    result = RequestAsync(std::move(context));
  } while (false);

  if (FAILED(result))
    listener->OnReceived(this, result, buffer, 0, 0);
}

AsyncSocket::Context* AsyncSocket::BeginReceive(void* buffer, int length,
                                                int flags, HANDLE event) {
  if (buffer == nullptr && length != 0 || event == NULL)
    return nullptr;

  auto context = CreateContext(Request::Receive, nullptr,
                               static_cast<char*>(buffer), length, flags,
                               nullptr, 0, nullptr, event);
  if (context == nullptr)
    return nullptr;

  return BeginRequest(std::move(context));
}

int AsyncSocket::EndReceive(Context* context, HRESULT* result) {
  if (context == nullptr || context->request != Request::Receive) {
    *result = E_INVALIDARG;
    return -1;
  }

  if (context->event != NULL &&
      WaitForSingleObject(context->event, INFINITE) != WAIT_OBJECT_0)
    *result = HRESULT_FROM_WIN32(GetLastError());
  else
    *result = context->result;

  int length = context->len;

  if (context->listener == nullptr)
    delete context;

  return length;
}

void AsyncSocket::ReceiveFromAsync(void* buffer, int length, int flags,
                                   Listener* listener) {
  HRESULT result = S_OK;

  do {
    if (buffer == nullptr && length != 0 || listener == nullptr) {
      result = E_INVALIDARG;
      break;
    }

    auto context = CreateContext(Request::ReceiveFrom, nullptr,
                                 static_cast<char*>(buffer), length, flags,
                                 nullptr, 0, listener, NULL);
    if (context == nullptr) {
      result = E_OUTOFMEMORY;
      break;
    }

    result = RequestAsync(std::move(context));
  } while (false);

  if (FAILED(result))
    listener->OnReceivedFrom(this, result, buffer, 0, 0, nullptr, 0);
}

AsyncSocket::Context* AsyncSocket::BeginReceiveFrom(void* buffer, int length,
                                                    int flags, HANDLE event) {
  if (buffer == nullptr && length != 0 || event == NULL)
    return nullptr;

  auto context = CreateContext(Request::ReceiveFrom, nullptr,
                               static_cast<char*>(buffer), length, flags,
                               nullptr, 0, nullptr, event);
  if (context == nullptr)
    return nullptr;

  return BeginRequest(std::move(context));
}

int AsyncSocket::EndReceiveFrom(Context* context, void* address, int* length) {
  if (context == nullptr || context->request != Request::ReceiveFrom)
    return -1;

  HRESULT result = S_OK;
  if (context->event != NULL &&
      WaitForSingleObject(context->event, INFINITE) != WAIT_OBJECT_0)
    result = HRESULT_FROM_WIN32(GetLastError());

  int received = context->len;
  if (FAILED(result))
    received = -1;

  if (address != nullptr && length != nullptr) {
    *length = std::min(context->address_length, *length);
    memmove(address, &context->address, *length);
  }

  if (context->listener == nullptr)
    delete context;

  return received;
}

void AsyncSocket::SendAsync(const void* buffer, int length, int flags,
                            Listener* listener) {
  HRESULT result = S_OK;

  do {
    if (buffer == nullptr && length != 0 || listener == nullptr) {
      result = E_INVALIDARG;
      break;
    }

    auto context = CreateContext(Request::Send, nullptr,
                                 const_cast<void*>(buffer), length, flags,
                                 nullptr, 0, listener, NULL);
    if (context == nullptr) {
      result = E_OUTOFMEMORY;
      break;
    }

    result = RequestAsync(std::move(context));
  } while (false);

  if (FAILED(result))
    listener->OnSent(this, result, const_cast<void*>(buffer), 0);
}

AsyncSocket::Context* AsyncSocket::BeginSend(const void* buffer, int length,
                                             int flags, HANDLE event) {
  if (buffer == nullptr && length != 0 || event == NULL)
    return nullptr;

  auto context = CreateContext(Request::Send, nullptr,
                               const_cast<void*>(buffer), length, flags,
                               nullptr, 0, nullptr, event);
  if (context == nullptr)
    return nullptr;

  return BeginRequest(std::move(context));
}

int AsyncSocket::EndSend(Context* context, HRESULT* result) {
  if (context == nullptr || context->request != Request::Send)
    return -1;

  if (context->event != NULL &&
      WaitForSingleObject(context->event, INFINITE) != WAIT_OBJECT_0)
    *result = HRESULT_FROM_WIN32(GetLastError());
  else
    *result = context->result;

  int length = context->len;

  if (context->listener == nullptr)
    delete context;

  return length;
}

void AsyncSocket::SendToAsync(const void* buffer, int length, int flags,
                              const void* address, int address_length,
                              Listener* listener) {
  HRESULT result = S_OK;

  do {
    if (buffer == nullptr && length != 0 || address == nullptr ||
        address_length == 0 || listener == nullptr) {
      result = E_INVALIDARG;
      break;
    }

    auto context = CreateContext(Request::SendTo, nullptr,
                                 const_cast<void*>(buffer), length, flags,
                                 address, address_length, listener, NULL);
    if (context == nullptr) {
      result = E_OUTOFMEMORY;
      break;
    }

    result = RequestAsync(std::move(context));
  } while (false);

  if (FAILED(result))
    listener->OnSentTo(this, result, const_cast<void*>(buffer), 0,
                       static_cast<const sockaddr*>(address), address_length);
}

AsyncSocket::Context* AsyncSocket::BeginSendTo(
    const void* buffer, int length, int flags, const void* address,
    int address_length, HANDLE event) {
  if (buffer == nullptr && length != 0 || address == nullptr ||
      address_length == 0 || event == NULL)
    return nullptr;

  auto context = CreateContext(Request::SendTo, nullptr,
                               const_cast<void*>(buffer), length, flags,
                               address, address_length, nullptr, event);
  if (context == nullptr)
    return nullptr;

  return BeginRequest(std::move(context));
}

int AsyncSocket::EndSendTo(Context* context, HRESULT* result) {
  if (context == nullptr || context->request != Request::SendTo)
    return -1;

  if (context->event != NULL &&
      WaitForSingleObject(context->event, INFINITE) == WAIT_OBJECT_0)
    *result = HRESULT_FROM_WIN32(GetLastError());
  else
    *result = context->result;

  int length = context->len;

  if (context->listener == nullptr)
    delete context;

  return length;
}

std::unique_ptr<AsyncSocket::Context> AsyncSocket::CreateContext(
    int request, const addrinfo* end_point, void* buffer, int length,
    DWORD flags, const void* address, int address_length, Listener* listener,
    HANDLE event) {
  auto context = std::make_unique<Context>();
  if (context != nullptr) {
    context->request = static_cast<Request>(request);
    context->end_point = end_point;
    context->buf = static_cast<char*>(buffer);
    context->len = length;
    context->flags = flags;

    if (address != nullptr && address_length > 0) {
      memmove_s(&context->address, context->address_length, address,
                address_length);
      context->address_length = address_length;
    }

    context->listener = listener;
    context->event = event;
  }

  return std::move(context);
}

HRESULT AsyncSocket::RequestAsync(std::unique_ptr<Context>&& context) {
  if (context == nullptr)
    return E_INVALIDARG;

  madoka::concurrent::LockGuard guard(&lock_);

  if (work_ == nullptr)
    return E_HANDLE;

  requests_.push_back(std::move(context));
  if (requests_.size() == 1)
    SubmitThreadpoolWork(work_);

  return S_OK;
}

AsyncSocket::Context* AsyncSocket::BeginRequest(
    std::unique_ptr<Context>&& context) {
  if (context == nullptr || context->event == NULL)
    return nullptr;

  madoka::concurrent::LockGuard guard(&lock_);

  if (work_ == nullptr)
    return nullptr;

  ResetEvent(context->event);

  auto pointer = context.get();
  requests_.push_back(std::move(context));
  if (requests_.size() == 1)
    SubmitThreadpoolWork(work_);

  return pointer;
}

void CALLBACK AsyncSocket::OnRequested(PTP_CALLBACK_INSTANCE callback,
                                       void* instance, PTP_WORK work) {
  static_cast<AsyncSocket*>(instance)->OnRequested(work);
}

void AsyncSocket::OnRequested(PTP_WORK work) {
  HRESULT result = S_OK;

  lock_.Lock();

  auto context = std::move(requests_.front());
  requests_.pop_front();
  if (!requests_.empty())
    SubmitThreadpoolWork(work);

  do {
    if (context->request == Request::Connect) {
      if (connected_) {
        result = __HRESULT_FROM_WIN32(WSAEISCONN);
        break;
      }

      if (cancel_connect_) {
        result = E_ABORT;
        break;
      }

      if (!Create(context->end_point->ai_family,
                  context->end_point->ai_socktype,
                  context->end_point->ai_protocol)) {
        result = HRESULT_FROM_WIN32(WSAGetLastError());
        break;
      }

      cancel_connect_ = false;

      if (ConnectEx == nullptr) {
        GUID guid = WSAID_CONNECTEX;
        DWORD length;
        if (WSAIoctl(descriptor_, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid,
                     sizeof(guid), &ConnectEx, sizeof(ConnectEx), &length,
                     nullptr, nullptr) != 0) {
          result = HRESULT_FROM_WIN32(WSAGetLastError());
          break;
        }
      }

      sockaddr_storage address = { context->end_point->ai_family };
      if (!Bind(&address, context->end_point->ai_addrlen)) {
        result = HRESULT_FROM_WIN32(WSAGetLastError());
        break;
      }
    } else if ((context->request == Request::Receive ||
                context->request == Request::Send) &&
               !connected_) {
      result = __HRESULT_FROM_WIN32(WSAENOTCONN);
      break;
    } else if (context->request == Request::ReceiveFrom && !bound_) {
      result = __HRESULT_FROM_WIN32(WSAEINVAL);
      break;
    } else if (!IsValid()) {
      result = __HRESULT_FROM_WIN32(WSAENOTSOCK);
      break;
    }

    if (io_ == nullptr) {
      io_ = CreateThreadpoolIo(reinterpret_cast<HANDLE>(descriptor_),
                               OnCompleted, this, environment_);
      if (io_ == nullptr) {
        result = HRESULT_FROM_WIN32(GetLastError());
        break;
      }
    }

    StartThreadpoolIo(io_);
    BOOL succeeded = FALSE;

    switch (context->request) {
      case Request::Connect:
        succeeded = ConnectEx(descriptor_,
                              context->end_point->ai_addr,
                              context->end_point->ai_addrlen,
                              nullptr,  // data to send
                              0,        // bytes to send
                              nullptr,  // bytes sent
                              context.get());
        break;

      case Request::Receive:
        succeeded = WSARecv(descriptor_, context.get(), 1, nullptr,
                            &context->flags, context.get(), nullptr) == 0;
        break;

      case Request::ReceiveFrom:
        succeeded = WSARecvFrom(descriptor_, context.get(), 1, nullptr,
                                &context->flags,
                                reinterpret_cast<sockaddr*>(&context->address),
                                &context->address_length, context.get(),
                                nullptr) == 0;
        break;

      case Request::Send:
        succeeded = WSASend(descriptor_, context.get(), 1, nullptr,
                            context->flags, context.get(), nullptr) == 0;
        break;

      case Request::SendTo:
        succeeded = WSASendTo(descriptor_, context.get(), 1, nullptr,
                              context->flags,
                              reinterpret_cast<sockaddr*>(&context->address),
                              context->address_length, context.get(),
                              nullptr) == 0;
        break;

      default:
        assert(false);
    }

    int error = WSAGetLastError();
    if (!succeeded && error != WSA_IO_PENDING) {
      CancelThreadpoolIo(io_);
      result = __HRESULT_FROM_WIN32(error);
      break;
    }

    context.release();
  } while (false);

  lock_.Unlock();

  if (FAILED(result))
    OnCompleted(std::move(context), result, 0);
}

void CALLBACK AsyncSocket::OnCompleted(PTP_CALLBACK_INSTANCE callback,
                                       void* instance, void* overlapped,
                                       ULONG error, ULONG_PTR bytes,
                                       PTP_IO io) {
  static_cast<AsyncSocket*>(instance)->OnCompleted(
      std::unique_ptr<Context>(
          static_cast<Context*>(
              static_cast<OVERLAPPED*>(overlapped))),
      __HRESULT_FROM_WIN32(error), bytes);
}

void AsyncSocket::OnCompleted(std::unique_ptr<Context>&& context,
                              HRESULT result, int length) {
  if (SUCCEEDED(result)) {
    if (context->request == Request::Connect) {
      if (SetOption(SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0))
        connected_ = true;
      else
        result = HRESULT_FROM_WIN32(WSAGetLastError());
    } else if (context->request == Request::Receive ||
               context->request == Request::ReceiveFrom) {
      DWORD bytes;
      if (!WSAGetOverlappedResult(descriptor_, context.get(), &bytes, FALSE,
                                  &context->flags))
        result = HRESULT_FROM_WIN32(WSAGetLastError());
    }

    if (FAILED(result))
      length = -1;
  }

  context->result = result;
  context->len = length;

  if (context->listener != nullptr) {
    switch (context->request) {
      case Request::Connect:
        context->listener->OnConnected(this, result, context->end_point);
        break;

      case Request::Receive:
        context->listener->OnReceived(this, result, context->buf, length,
                                      context->flags);
        break;

      case Request::ReceiveFrom:
        context->listener->OnReceivedFrom(
            this, result, context->buf, length, context->flags,
            reinterpret_cast<sockaddr*>(&context->address),
            context->address_length);
        break;

      case Request::Send:
        context->listener->OnSent(this, result, context->buf, length);
        break;

      case Request::SendTo:
        context->listener->OnSentTo(
            this, result, context->buf, length,
            reinterpret_cast<sockaddr*>(&context->address),
            context->address_length);
        break;

      default:
        assert(false);
    }
  } else if (context->event != NULL) {
    SetEvent(context->event);
    context.release();
  } else {
    assert(false);
  }
}

}  // namespace net
}  // namespace madoka
