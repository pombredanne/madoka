// Copyright (c) 2014 dacci.org

#include "madoka/net/async_server_socket.h"

#include <assert.h>

#include <memory>
#include <utility>

#include "madoka/concurrent/lock_guard.h"
#include "madoka/net/async_socket.h"
#include "madoka/net/socket_event_listener.h"

namespace madoka {
namespace net {

struct AsyncServerSocket::AsyncContext : OVERLAPPED {
  explicit AsyncContext(AsyncServerSocket* server)
      : OVERLAPPED(), server(server), bytes(0), listener(nullptr), event(NULL) {
  }

  AsyncServerSocket* server;
  std::unique_ptr<AsyncSocket> client;
  std::unique_ptr<char[]> buffer;
  DWORD bytes;
  SocketEventListener* listener;
  HANDLE event;
};

PTP_CALLBACK_ENVIRON AsyncServerSocket::environment_ = NULL;

AsyncServerSocket::AsyncServerSocket() : io_() {
  ::InitOnceInitialize(&init_once_);
}

AsyncServerSocket::~AsyncServerSocket() {
  Close();
  CloseInternal();

  madoka::concurrent::LockGuard guard(&lock_);
  while (!requests_.empty())
    empty_.Sleep(&lock_);
}

void AsyncServerSocket::Close() {
  ServerSocket::Close();

  ::InitOnceInitialize(&init_once_);
}

PTP_CALLBACK_ENVIRON AsyncServerSocket::GetCallbackEnvironment() {
  return environment_;
}

void AsyncServerSocket::SetCallbackEnvironment(
    PTP_CALLBACK_ENVIRON environment) {
  environment_ = environment;
}

void AsyncServerSocket::AcceptAsync(SocketEventListener* listener) {
  if (listener == nullptr)
    listener->OnAccepted(this, nullptr, E_POINTER);
  if (!IsValid())
    listener->OnAccepted(this, nullptr, WSAENOTSOCK);
  else if (!bound())
    listener->OnAccepted(this, nullptr, WSAEINVAL);
  else if (DispatchRequest(listener, NULL) == nullptr)
    listener->OnAccepted(this, nullptr, GetLastError());
}

AsyncServerSocket::AsyncContext* AsyncServerSocket::BeginAccept(HANDLE event) {
  if (event == NULL)
    return nullptr;
  if (!IsValid() || !bound())
    return nullptr;

  return DispatchRequest(nullptr, event);
}

AsyncSocket* AsyncServerSocket::EndAccept(AsyncContext* context) {
  if (context == nullptr || context->server != this)
    return nullptr;

  if (context->event != NULL)
    ::WaitForSingleObject(context->event, INFINITE);

  BOOL succeeded = ::GetOverlappedResult(
      reinterpret_cast<HANDLE>(context->client->descriptor_), context,
      &context->bytes, FALSE);
  AsyncSocket* client = nullptr;
  if (succeeded &&
      context->client->SetOption(SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                                 descriptor_)) {
    client = context->client.release();
    client->set_connected(true);
  }

  lock_.Lock();
  for (auto i = requests_.begin(), l = requests_.end(); i != l; ++i) {
    if (i->get() == context) {
      requests_.erase(i);

      if (requests_.empty())
        empty_.WakeAll();

      break;
    }
  }
  lock_.Unlock();

  return client;
}

void AsyncServerSocket::CloseInternal() {
  if (io_ != NULL) {
    ::WaitForThreadpoolIoCallbacks(io_, FALSE);
    ::CloseThreadpoolIo(io_);
    io_ = NULL;
  }
}

AsyncServerSocket::AsyncContext* AsyncServerSocket::DispatchRequest(
    SocketEventListener* listener, HANDLE event) {
  if (!::InitOnceExecuteOnce(&init_once_, OnInitialize, this, nullptr))
    return nullptr;

  auto  context = std::make_unique<AsyncContext>(this);
  if (context == nullptr) {
    SetLastError(E_OUTOFMEMORY);
    return nullptr;
  }

  context->client.reset(new AsyncSocket());
  if (context->client == nullptr) {
    SetLastError(E_OUTOFMEMORY);
    return nullptr;
  }

  context->buffer.reset(new char[(sizeof(sockaddr_storage) + 16) * 2]);
  if (context->buffer == nullptr) {
    SetLastError(E_OUTOFMEMORY);
    return nullptr;
  }

  if (!context->client->Create(protocol_info_.iAddressFamily,
                               protocol_info_.iSocketType,
                               protocol_info_.iProtocol))
    return nullptr;

  context->listener = listener;
  context->event = event;

  if (event != NULL && !::ResetEvent(event))
    return nullptr;

  madoka::concurrent::LockGuard guard(&lock_);
  auto pointer = context.get();

  if (!::TrySubmitThreadpoolCallback(OnRequested, pointer, environment_))
    return nullptr;

  requests_.push_back(std::move(context));

  return pointer;
}

BOOL CALLBACK AsyncServerSocket::OnInitialize(INIT_ONCE* init_once, void* param,
                                              void** context) {
  return static_cast<AsyncServerSocket*>(param)->OnInitialize(context);
}

BOOL AsyncServerSocket::OnInitialize(void** context) {
  if (!IsValid())
    return FALSE;

  CloseInternal();
  assert(io_ == NULL);

  io_ = ::CreateThreadpoolIo(reinterpret_cast<HANDLE>(descriptor_),
                              OnCompleted, this, environment_);
  if (io_ == NULL)
    return FALSE;

  if (!GetOption(SOL_SOCKET, SO_PROTOCOL_INFO, &protocol_info_))
    return FALSE;

  return TRUE;
}

void CALLBACK AsyncServerSocket::OnRequested(PTP_CALLBACK_INSTANCE instance,
                                             void* param) {
  AsyncContext* context = static_cast<AsyncContext*>(param);
  context->server->OnRequested(context);
}

void AsyncServerSocket::OnRequested(AsyncContext* context) {
  ::StartThreadpoolIo(io_);
  BOOL succeeded = ::AcceptEx(descriptor_,
                              context->client->descriptor_,
                              context->buffer.get(),
                              0,
                              sizeof(sockaddr_storage) + 16,
                              sizeof(sockaddr_storage) + 16,
                              &context->bytes,
                              context);
  int error = ::WSAGetLastError();

  if (!succeeded && error != ERROR_IO_PENDING) {
    ::CancelThreadpoolIo(io_);
    OnCompleted(context, error);
  }
}

void CALLBACK AsyncServerSocket::OnCompleted(PTP_CALLBACK_INSTANCE instance,
                                             void* context, void* overlapped,
                                             ULONG error, ULONG_PTR bytes,
                                             PTP_IO io) {
  static_cast<AsyncServerSocket*>(context)->OnCompleted(
      static_cast<AsyncContext*>(static_cast<OVERLAPPED*>(overlapped)), error);
}

void AsyncServerSocket::OnCompleted(AsyncContext* context, ULONG error) {
  if (context->listener != nullptr) {
    SocketEventListener* listener = context->listener;
    AsyncSocket* client = EndAccept(context);
    if (client == nullptr && error == 0)
      error = ::GetLastError();

    listener->OnAccepted(this, client, error);
  } else if (context->event != NULL) {
    ::SetEvent(context->event);
  } else {
    assert(false);
  }
}

}  // namespace net
}  // namespace madoka
