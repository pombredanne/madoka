// Copyright (c) 2015 dacci.org

#include "madoka/net/async_server_socket.h"

#include <assert.h>

#include "madoka/concurrent/lock_guard.h"

namespace madoka {
namespace net {

namespace {
LPFN_ACCEPTEX AcceptEx = nullptr;
}  // namespace

struct AsyncServerSocket::Context : OVERLAPPED {
  Context()
      : OVERLAPPED(),
        result(S_OK),
        listener(nullptr),
        event(NULL),
        socket(INVALID_SOCKET),
        buffer(new char[(sizeof(sockaddr_storage) + 16) * 2]) {
  }

  ~Context() {
    if (socket != INVALID_SOCKET) {
      closesocket(socket);
      socket = INVALID_SOCKET;
    }
  }

  HRESULT result;
  Listener* listener;
  HANDLE event;
  SOCKET socket;
  std::unique_ptr<char[]> buffer;
};

PTP_CALLBACK_ENVIRON AsyncServerSocket::environment_ = NULL;

AsyncServerSocket::AsyncServerSocket()
    : work_(CreateThreadpoolWork(OnRequested, this, environment_)),
      protocol_(),
      io_(nullptr) {
}

AsyncServerSocket::AsyncServerSocket(int family, int type, int protocol)
    : AsyncServerSocket() {
  Create(family, type, protocol);
}

AsyncServerSocket::~AsyncServerSocket() {
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

void AsyncServerSocket::Close() {
  madoka::concurrent::LockGuard guard(&lock_);

  ServerSocket::Close();

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

PTP_CALLBACK_ENVIRON AsyncServerSocket::GetCallbackEnvironment() {
  return environment_;
}

void AsyncServerSocket::SetCallbackEnvironment(
    PTP_CALLBACK_ENVIRON environment) {
  environment_ = environment;
}

void AsyncServerSocket::AcceptAsync(Listener* listener) {
  HRESULT result = S_OK;

  do {
    if (listener == nullptr) {
      result = E_INVALIDARG;
      break;
    }

    auto context = std::make_unique<Context>();
    if (context == nullptr || context->buffer == nullptr) {
      result = E_POINTER;
      break;
    }

    context->listener = listener;

    madoka::concurrent::LockGuard guard(&lock_);

    if (work_ == nullptr) {
      result = E_HANDLE;
      break;
    }

    requests_.push_back(std::move(context));
    if (requests_.size() == 1)
      SubmitThreadpoolWork(work_);

    return;
  } while (false);

  assert(FAILED(result));
  listener->OnAccepted(this, result, nullptr);
}

AsyncServerSocket::Context* AsyncServerSocket::BeginAccept(HANDLE event) {
  if (event == nullptr)
    return nullptr;

  auto context = std::make_unique<Context>();
  if (context == nullptr || context->buffer == nullptr)
    return nullptr;

  context->event = event;

  madoka::concurrent::LockGuard guard(&lock_);

  if (work_ == nullptr)
    return nullptr;

  ResetEvent(event);

  auto pointer = context.get();
  requests_.push_back(std::move(context));
  if (requests_.size() == 1)
    SubmitThreadpoolWork(work_);

  return pointer;
}

SOCKET AsyncServerSocket::RawEndAccept(Context* context, HRESULT* result) {
  SOCKET descriptor = INVALID_SOCKET;

  HRESULT local_result = S_OK;

  do {
    if (context->event != NULL &&
        WaitForSingleObject(context->event, INFINITE) != WAIT_OBJECT_0) {
      local_result = HRESULT_FROM_WIN32(GetLastError());
      break;
    }

    if (FAILED(context->result)) {
      local_result = context->result;
      break;
    }

    descriptor = context->socket;
    context->socket = INVALID_SOCKET;
  } while (false);

  if (context->listener == nullptr)
    delete context;

  if (result != nullptr)
    *result = local_result;

  return descriptor;
}

void CALLBACK AsyncServerSocket::OnRequested(PTP_CALLBACK_INSTANCE callback,
                                             void* instance, PTP_WORK work) {
  static_cast<AsyncServerSocket*>(instance)->OnRequested(work);
}

void AsyncServerSocket::OnRequested(PTP_WORK work) {
  HRESULT result = S_OK;

  lock_.Lock();

  auto context = std::move(requests_.front());
  requests_.pop_front();
  if (!requests_.empty())
    SubmitThreadpoolWork(work);

  do {
    if (!IsValid()) {
      result = __HRESULT_FROM_WIN32(WSAENOTSOCK);
      break;
    }

    if (!bound_) {
      result = __HRESULT_FROM_WIN32(WSAEINVAL);
      break;
    }

    if (AcceptEx == nullptr) {
      GUID guid = WSAID_ACCEPTEX;
      DWORD length;
      if (WSAIoctl(descriptor_, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid,
                   sizeof(guid), &AcceptEx, sizeof(AcceptEx), &length, nullptr,
                   nullptr) != 0) {
        result = HRESULT_FROM_WIN32(WSAGetLastError());
        break;
      }
    }

    if (protocol_.iAddressFamily == AF_UNSPEC) {
      if (!GetOption(SOL_SOCKET, SO_PROTOCOL_INFO, &protocol_)) {
        result = HRESULT_FROM_WIN32(WSAGetLastError());
        break;
      }
    }

    if (io_ == nullptr) {
      io_ = CreateThreadpoolIo(reinterpret_cast<HANDLE>(descriptor_),
                               OnCompleted, this, environment_);
      if (io_ == nullptr) {
        result = HRESULT_FROM_WIN32(GetLastError());
        break;
      }
    }

    context->socket = socket(protocol_.iAddressFamily,
                             protocol_.iSocketType,
                             protocol_.iProtocol);
    if (context->socket == INVALID_SOCKET) {
      result = HRESULT_FROM_WIN32(WSAGetLastError());
      break;
    }

    StartThreadpoolIo(io_);

    BOOL succeeded = AcceptEx(descriptor_,
                              context->socket,
                              context->buffer.get(),
                              0,        // buffer to receive
                              sizeof(sockaddr_storage) + 16,
                              sizeof(sockaddr_storage) + 16,
                              nullptr,  // bytes received
                              context.get());
    int error = WSAGetLastError();
    if (!succeeded && error != ERROR_IO_PENDING) {
      CancelThreadpoolIo(io_);
      result = __HRESULT_FROM_WIN32(error);
      break;
    }

    context.release();
  } while (false);

  lock_.Unlock();

  if (FAILED(result))
    OnCompleted(std::move(context), result);
}

void CALLBACK AsyncServerSocket::OnCompleted(PTP_CALLBACK_INSTANCE callback,
                                             void* instance, void* overlapped,
                                             ULONG error, ULONG_PTR length,
                                             PTP_IO io) {
  static_cast<AsyncServerSocket*>(instance)->OnCompleted(
      std::unique_ptr<Context>(
          static_cast<Context*>(
              static_cast<OVERLAPPED*>(overlapped))),
      __HRESULT_FROM_WIN32(error));
}

void AsyncServerSocket::OnCompleted(std::unique_ptr<Context>&& context,
                                    HRESULT result) {
  if (SUCCEEDED(result)) {
    if (setsockopt(context->socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                   reinterpret_cast<char*>(&descriptor_),
                   sizeof(descriptor_)) != 0)
      result = HRESULT_FROM_WIN32(WSAGetLastError());
  }

  context->result = result;

  if (context->listener != nullptr) {
    context->listener->OnAccepted(this, result, context.get());
  } else if (context->event != NULL) {
    SetEvent(context->event);
    context.release();
  } else {
    assert(false);
  }
}

}  // namespace net
}  // namespace madoka
