// Copyright (c) 2014 dacci.org

#include "madoka/net/async_socket.h"

#include <assert.h>

#include <memory>
#include <utility>

#include "madoka/concurrent/lock_guard.h"
#include "madoka/net/socket_event_listener.h"

namespace madoka {
namespace net {

struct AsyncSocket::AsyncContext : OVERLAPPED, WSABUF {
  explicit AsyncContext(AsyncSocket* socket)
      : OVERLAPPED(),
        WSABUF(),
        socket(socket),
        action(None),
        end_point(nullptr),
        flags(0),
        listener(nullptr),
        event(NULL) {
  }

  AsyncSocket* socket;
  Action action;
  const addrinfo* end_point;
  DWORD flags;
  SocketEventListener* listener;
  HANDLE event;
};

PTP_CALLBACK_ENVIRON AsyncSocket::environment_ = NULL;
LPFN_CONNECTEX AsyncSocket::ConnectEx = NULL;

AsyncSocket::AsyncSocket() : io_(), cancel_connect_(false) {
  ::InitOnceInitialize(&init_once_);
}

AsyncSocket::~AsyncSocket() {
  Close();
  CloseInternal();

  madoka::concurrent::LockGuard guard(&lock_);
  while (!requests_.empty())
    empty_.Sleep(&lock_);
}

void AsyncSocket::Close() {
  cancel_connect_ = true;

  Socket::Close();

  ::InitOnceInitialize(&init_once_);
}

PTP_CALLBACK_ENVIRON AsyncSocket::GetCallbackEnvironment() {
  return environment_;
}

void AsyncSocket::SetCallbackEnvironment(PTP_CALLBACK_ENVIRON environment) {
  environment_ = environment;
}

void AsyncSocket::ConnectAsync(const addrinfo* end_points,
                               SocketEventListener* listener) {
  if (listener == nullptr)
    listener->OnConnected(this, E_POINTER);
  else if (end_points == nullptr)
    listener->OnConnected(this, WSAEFAULT);
  else if (connected())
    listener->OnConnected(this, WSAEISCONN);
  else if (DispatchRequest(Connecting, end_points, nullptr, 0, 0, listener,
                           NULL) == nullptr)
    listener->OnConnected(this, GetLastError());
}

AsyncSocket::AsyncContext* AsyncSocket::BeginConnect(const addrinfo* end_points,
                                                     HANDLE event) {
  if (end_points == nullptr || event == NULL)
    return nullptr;
  if (connected())
    return nullptr;

  cancel_connect_ = false;

  return DispatchRequest(Connecting, end_points, nullptr, 0, 0, nullptr, event);
}

bool AsyncSocket::EndConnect(AsyncContext* context) {
  if (context == nullptr || context->socket != this)
    return false;
  if (context->action != Connecting)
    return false;

  int result = EndRequest(context);
  if (result == SOCKET_ERROR)
    return false;

  if (!SetOption(SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0))
    return false;

  set_connected(true);

  return true;
}

void AsyncSocket::ReceiveAsync(void* buffer, int size, int flags,
                               SocketEventListener* listener) {
  if (listener == nullptr)
    listener->OnReceived(this, E_POINTER, buffer, 0);
  if (!connected())
    listener->OnReceived(this, WSAENOTCONN, buffer, 0);
  else if (DispatchRequest(Receiving, nullptr, buffer, size, flags, listener,
                         NULL) == nullptr)
    listener->OnReceived(this, GetLastError(), buffer, 0);
}

AsyncSocket::AsyncContext* AsyncSocket::BeginReceive(void* buffer, int size,
                                                     int flags, HANDLE event) {
  if (event == NULL)
    return nullptr;
  if (!connected())
    return nullptr;

  return DispatchRequest(Receiving, nullptr, buffer, size, flags, nullptr,
                         event);
}

int AsyncSocket::EndReceive(AsyncContext* context) {
  if (context == nullptr || context->socket != this)
    return SOCKET_ERROR;
  if (context->action != Receiving)
    return SOCKET_ERROR;

  return EndRequest(context);
}

void AsyncSocket::SendAsync(const void* buffer, int size, int flags,
                            SocketEventListener* listener) {
  if (listener == nullptr)
    listener->OnSent(this, E_POINTER, const_cast<void*>(buffer), 0);
  if (!connected())
    listener->OnSent(this, WSAENOTCONN, const_cast<void*>(buffer), 0);
  else if (DispatchRequest(Sending, nullptr, const_cast<void*>(buffer), size,
                           flags, listener, NULL) == nullptr)
    listener->OnSent(this, GetLastError(), const_cast<void*>(buffer), 0);
}

AsyncSocket::AsyncContext* AsyncSocket::BeginSend(const void* buffer, int size,
                                                  int flags, HANDLE event) {
  if (event == NULL)
    return nullptr;
  if (!connected())
    return nullptr;

  return DispatchRequest(Sending, nullptr, const_cast<void*>(buffer), size,
                         flags, nullptr, event);
}

int AsyncSocket::EndSend(AsyncContext* context) {
  if (context == nullptr || context->socket != this)
    return SOCKET_ERROR;
  if (context->action != Sending)
    return SOCKET_ERROR;

  return EndRequest(context);
}

void AsyncSocket::CloseInternal() {
  if (io_ != NULL) {
    ::WaitForThreadpoolIoCallbacks(io_, FALSE);
    ::CloseThreadpoolIo(io_);
    io_ = NULL;
  }
}

AsyncSocket::AsyncContext* AsyncSocket::DispatchRequest(
    Action action,  const addrinfo* end_points, void* buffer, int size,
    int flags, SocketEventListener* listener, HANDLE event) {
  if (action != Connecting &&
      !::InitOnceExecuteOnce(&init_once_, OnInitialize, this, nullptr))
    return nullptr;

  auto  context = std::make_unique<AsyncContext>(this);
  if (context == nullptr) {
    SetLastError(E_OUTOFMEMORY);
    return nullptr;
  }

  if (action == Connecting)
    cancel_connect_ = false;

  context->len = size;
  context->buf = static_cast<char*>(buffer);
  context->action = action;
  context->end_point = end_points;
  context->flags = flags;
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

int AsyncSocket::EndRequest(AsyncContext* context) {
  if (context->event != NULL)
    ::WaitForSingleObject(context->event, INFINITE);

  DWORD bytes = 0;
  BOOL succeeded = ::GetOverlappedResult(reinterpret_cast<HANDLE>(descriptor_),
                                         context, &bytes, FALSE);

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

  if (!succeeded)
    return SOCKET_ERROR;

  return bytes;
}

int AsyncSocket::DoAsyncConnect(AsyncContext* context) {
  if (cancel_connect_)
    return SOCKET_ERROR;

  const addrinfo* end_point = context->end_point;
  if (!Create(end_point))
    return SOCKET_ERROR;

  cancel_connect_ = false;

  if (!::InitOnceExecuteOnce(&init_once_, OnInitialize, this, nullptr))
    return SOCKET_ERROR;

  if (ConnectEx == NULL) {
    GUID guid = WSAID_CONNECTEX;
    DWORD size = 0;
    ::WSAIoctl(descriptor_, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid,
               sizeof(guid), &ConnectEx, sizeof(ConnectEx), &size, NULL, NULL);
  }

  sockaddr_storage address = { end_point->ai_family };
  int result = ::bind(descriptor_, reinterpret_cast<sockaddr*>(&address),
                      end_point->ai_addrlen);
  if (result != 0)
    return SOCKET_ERROR;

  ::StartThreadpoolIo(io_);

  return ConnectEx(descriptor_, end_point->ai_addr, end_point->ai_addrlen, NULL,
                   0, NULL, context);
}

BOOL CALLBACK AsyncSocket::OnInitialize(INIT_ONCE* init_once, void* param,
                                        void** context) {
  return static_cast<AsyncSocket*>(param)->OnInitialize(context);
}

BOOL AsyncSocket::OnInitialize(void** context) {
  if (!IsValid())
    return FALSE;

  CloseInternal();
  assert(io_ == NULL);

  io_ = ::CreateThreadpoolIo(reinterpret_cast<HANDLE>(descriptor_),
                              OnCompleted, this, environment_);
  if (io_ == NULL)
    return FALSE;

  return TRUE;
}

void CALLBACK AsyncSocket::OnRequested(PTP_CALLBACK_INSTANCE instance,
                                       void* param) {
  AsyncContext* context = static_cast<AsyncContext*>(param);
  context->socket->OnRequested(context);
}

void AsyncSocket::OnRequested(AsyncContext* context) {
  DWORD bytes = 0;
  int result = 0;
  int error = ERROR_SUCCESS;

  switch (context->action) {
    case Connecting:
      result = DoAsyncConnect(context);
      break;

    case Receiving:
      ::StartThreadpoolIo(io_);
      result = ::WSARecv(descriptor_, context, 1, &bytes, &context->flags,
                         context, NULL);
      break;

    case Sending:
      ::StartThreadpoolIo(io_);
      result = ::WSASend(descriptor_, context, 1, &bytes, context->flags,
                         context, NULL);
      break;

    default:
      assert(false);
  }

  error = ::WSAGetLastError();
  if (result != 0 && error != WSA_IO_PENDING) {
    ::CancelThreadpoolIo(io_);
    OnCompleted(context, error, 0);
  }
}

void CALLBACK AsyncSocket::OnCompleted(PTP_CALLBACK_INSTANCE instance,
                                       void* context, void* overlapped,
                                       ULONG error, ULONG_PTR bytes,
                                       PTP_IO io) {
  static_cast<AsyncSocket*>(context)->OnCompleted(
      static_cast<AsyncContext*>(static_cast<OVERLAPPED*>(overlapped)),
      error, bytes);
}

void AsyncSocket::OnCompleted(AsyncContext* context, ULONG error,
                              ULONG_PTR bytes) {
  SocketEventListener* listener = context->listener;

  if (context->action == Connecting && error != 0) {
    if (!cancel_connect_ && context->end_point->ai_next != nullptr) {
      context->end_point = context->end_point->ai_next;
      if (::TrySubmitThreadpoolCallback(OnRequested, context, environment_))
        return;
    }
  }

  if (listener != NULL) {
    void* buffer = context->buf;

    switch (context->action) {
      case Connecting: {
        bool succeeded = EndConnect(context);
        if (!succeeded && error == 0)
          error = ::GetLastError();

        listener->OnConnected(this, error);
        break;
      }

      case Receiving: {
        int result = EndReceive(context);
        if (result == SOCKET_ERROR && error == 0)
          error = ::GetLastError();

        listener->OnReceived(this, error, buffer, bytes);
        break;
      }

      case Sending: {
        int result = EndSend(context);
        if (result == SOCKET_ERROR && error == 0)
          error = ::GetLastError();

        listener->OnSent(this, error, buffer, bytes);
        break;
      }

      default:
        assert(false);
    }
  } else if (context->event != NULL) {
    ::SetEvent(context->event);
  } else {
    assert(false);
  }
}

}  // namespace net
}  // namespace madoka
