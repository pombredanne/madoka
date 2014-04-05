// Copyright (c) 2014 dacci.org

#include "madoka/net/async_socket.h"

#include <assert.h>

#include <memory>

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

AsyncSocket::AsyncSocket() : init_once_(), io_(), cancel_connect_(false) {
}

AsyncSocket::~AsyncSocket() {
  Close();
}

void AsyncSocket::Close() {
  Socket::Close();

  if (io_ != NULL) {
    ::WaitForThreadpoolIoCallbacks(io_, FALSE);
    ::CloseThreadpoolIo(io_);
    io_ = NULL;
  }

  ::InitOnceInitialize(&init_once_);
}

PTP_CALLBACK_ENVIRON AsyncSocket::GetCallbackEnvironment() {
  return environment_;
}

void AsyncSocket::SetCallbackEnvironment(PTP_CALLBACK_ENVIRON environment) {
  environment_ = environment;
}

bool AsyncSocket::ConnectAsync(const addrinfo* end_points,
                               SocketEventListener* listener) {
  if (end_points == nullptr || listener == nullptr)
    return false;
  if (connected())
    return false;

  cancel_connect_ = false;

  return DispatchRequest(Connecting, end_points, nullptr, 0, 0, listener,
                         NULL) != nullptr;
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

void AsyncSocket::CancelAsyncConnect() {
  if (!connected()) {
    cancel_connect_ = true;
    Close();
  }
}

bool AsyncSocket::ReceiveAsync(void* buffer, int size, int flags,
                               SocketEventListener* listener) {
  if (buffer == nullptr || size == 0 || listener == nullptr)
    return false;
  if (!connected())
    return false;

  return DispatchRequest(Receiving, nullptr, buffer, size, flags, listener,
                         NULL) != nullptr;
}

AsyncSocket::AsyncContext* AsyncSocket::BeginReceive(void* buffer, int size,
                                                     int flags, HANDLE event) {
  if (buffer == nullptr || size == 0 || event == NULL)
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

bool AsyncSocket::SendAsync(const void* buffer, int size, int flags,
                            SocketEventListener* listener) {
  if (buffer == nullptr || size == 0 || listener == nullptr)
    return false;
  if (!connected())
    return false;

  return DispatchRequest(Sending, nullptr, const_cast<void*>(buffer), size,
                         flags, listener, NULL) != nullptr;
}

AsyncSocket::AsyncContext* AsyncSocket::BeginSend(const void* buffer, int size,
                                                  int flags, HANDLE event) {
  if (buffer == nullptr || size == 0 || event == NULL)
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

AsyncSocket::AsyncContext* AsyncSocket::DispatchRequest(
    Action action,  const addrinfo* end_points, void* buffer, int size,
    int flags, SocketEventListener* listener, HANDLE event) {
  if (IsValid() &&
      !::InitOnceExecuteOnce(&init_once_, OnInitialize, this, nullptr))
    return nullptr;

  std::unique_ptr<AsyncContext> context(new AsyncContext(this));
  if (context == nullptr)
    return nullptr;

  context->len = size;
  context->buf = static_cast<char*>(buffer);
  context->action = action;
  context->end_point = end_points;
  context->flags = flags;
  context->listener = listener;
  context->event = event;

  if (event != NULL && !::ResetEvent(event))
    return nullptr;

  if (!::TrySubmitThreadpoolCallback(OnRequested, context.get(), environment_))
    return nullptr;

  return context.release();
}

int AsyncSocket::EndRequest(AsyncContext* context) {
  if (context->event != NULL)
    ::WaitForSingleObject(context->event, INFINITE);

  DWORD bytes = 0;
  BOOL succeeded = ::GetOverlappedResult(reinterpret_cast<HANDLE>(descriptor_),
                                         context, &bytes, FALSE);

  delete context;

  if (!succeeded)
    return SOCKET_ERROR;

  return bytes;
}

int AsyncSocket::DoAsyncConnect(AsyncContext* context) {
  const addrinfo* end_point = context->end_point;

  if (!Create(end_point))
    return SOCKET_ERROR;

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
  return static_cast<AsyncSocket*>(param)->OnInitialize();
}

BOOL AsyncSocket::OnInitialize() {
  if (!IsValid())
    return TRUE;  // for use with DoAsyncConnect

  if (io_ == NULL) {
    io_ = ::CreateThreadpoolIo(reinterpret_cast<HANDLE>(descriptor_),
                               OnCompleted, this, environment_);
    if (io_ == NULL)
      return FALSE;
  }

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
      error = ::WSAGetLastError();
      break;

    case Receiving:
      ::StartThreadpoolIo(io_);
      result = ::WSARecv(descriptor_, context, 1, &bytes, &context->flags,
                         context, NULL);
      error = ::WSAGetLastError();
      break;

    case Sending:
      ::StartThreadpoolIo(io_);
      result = ::WSASend(descriptor_, context, 1, &bytes, context->flags,
                         context, NULL);
      error = ::WSAGetLastError();
      break;

    default:
      assert(false);
  }

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

        listener->OnReceived(this, error, bytes);
        break;
      }

      case Sending: {
        int result = EndSend(context);
        if (result == SOCKET_ERROR && error == 0)
          error = ::GetLastError();

        listener->OnSent(this, error, bytes);
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
