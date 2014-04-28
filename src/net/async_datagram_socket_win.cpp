// Copyright (c) 2014 dacci.org

#include "madoka/net/async_datagram_socket.h"

#include <assert.h>

#include <memory>

#include "madoka/net/socket_event_listener.h"

namespace madoka {
namespace net {

struct AsyncDatagramSocket::AsyncContext : OVERLAPPED, WSABUF {
  explicit AsyncContext(AsyncDatagramSocket* socket)
      : OVERLAPPED(),
        WSABUF(),
        socket(socket),
        action(None),
        flags(0),
        address(),
        address_length(sizeof(address)),
        listener(nullptr),
        event(NULL) {
  }

  AsyncDatagramSocket* socket;
  Action action;
  DWORD flags;
  sockaddr_storage address;
  int address_length;
  SocketEventListener* listener;
  HANDLE event;
};

PTP_CALLBACK_ENVIRON AsyncDatagramSocket::environment_ = NULL;

AsyncDatagramSocket::AsyncDatagramSocket() : init_once_(), io_() {
}

AsyncDatagramSocket::AsyncDatagramSocket(int family, int protocol)
    : DatagramSocket(family, protocol), init_once_(), io_() {
}

AsyncDatagramSocket::~AsyncDatagramSocket() {
  Close();
}

void AsyncDatagramSocket::Close() {
  DatagramSocket::Close();

  if (io_ != NULL) {
    ::WaitForThreadpoolIoCallbacks(io_, FALSE);
    ::CloseThreadpoolIo(io_);
    io_ = NULL;
  }

  ::InitOnceInitialize(&init_once_);
}

PTP_CALLBACK_ENVIRON AsyncDatagramSocket::GetCallbackEnvironment() {
  return environment_;
}

void AsyncDatagramSocket::SetCallbackEnvironment(
    PTP_CALLBACK_ENVIRON environment) {
  environment_ = environment;
}

bool AsyncDatagramSocket::ReceiveAsync(void* buffer, int size, int flags,
                                       SocketEventListener* listener) {
  if (buffer == nullptr || size == 0 || listener == nullptr)
    return false;
  if (!connected())
    return false;

  return DispatchRequest(Receiving, buffer, size, flags, nullptr, 0, listener,
                         NULL) != nullptr;
}

AsyncDatagramSocket::AsyncContext* AsyncDatagramSocket::BeginReceive(
    void* buffer, int size, int flags, HANDLE event) {
  if (buffer == nullptr || size == 0 || event == NULL)
    return nullptr;
  if (!connected())
    return nullptr;

  return DispatchRequest(Receiving, buffer, size, flags, nullptr, 0, nullptr,
                         event);
}

int AsyncDatagramSocket::EndReceive(AsyncContext* context) {
  if (context == nullptr || context->socket != this)
    return SOCKET_ERROR;
  if (context->action != Receiving)
    return SOCKET_ERROR;

  return EndRequest(context, nullptr, nullptr);
}

bool AsyncDatagramSocket::SendAsync(const void* buffer, int size, int flags,
                                    SocketEventListener* listener) {
  if (buffer == nullptr || size == 0 || listener == nullptr)
    return false;
  if (!connected())
    return false;

  return DispatchRequest(Sending, const_cast<void*>(buffer), size, flags,
                         nullptr, 0, listener, NULL) != nullptr;
}

AsyncDatagramSocket::AsyncContext* AsyncDatagramSocket::BeginSend(
    const void* buffer, int size, int flags, HANDLE event) {
  if (buffer == nullptr || size == 0 || event == NULL)
    return nullptr;
  if (!connected())
    return nullptr;

  return DispatchRequest(Sending, const_cast<void*>(buffer), size, flags,
                         nullptr, 0, nullptr, event);
}

int AsyncDatagramSocket::EndSend(AsyncContext* context) {
  if (context == nullptr || context->socket != this)
    return SOCKET_ERROR;
  if (context->action != Sending)
    return SOCKET_ERROR;

  return EndRequest(context, nullptr, nullptr);
}

bool AsyncDatagramSocket::ReceiveFromAsync(void* buffer, int size, int flags,
                                           SocketEventListener* listener) {
  if (buffer == nullptr || size == 0 || listener == nullptr)
    return false;
  if (!bound())
    return false;

  return DispatchRequest(ReceivingFrom, buffer, size, flags, nullptr, 0,
                         listener, NULL) != nullptr;
}

AsyncDatagramSocket::AsyncContext* AsyncDatagramSocket::BeginReceiveFrom(
    void* buffer, int size, int flags, HANDLE event) {
  if (buffer == nullptr || size == 0 || event == NULL)
    return nullptr;
  if (!bound())
    return nullptr;

  return DispatchRequest(ReceivingFrom, buffer, size, flags, nullptr, 0,
                         nullptr, event);
}

int AsyncDatagramSocket::EndReceiveFrom(AsyncContext* context,
                                        sockaddr* address, int* length) {
  if (context == nullptr || context->socket != this)
    return SOCKET_ERROR;
  if (context->action != ReceivingFrom)
    return SOCKET_ERROR;

  return EndRequest(context, address, length);
}

bool AsyncDatagramSocket::SendToAsync(const void* buffer, int size, int flags,
                                      const sockaddr* address, int length,
                                      SocketEventListener* listener) {
  if (buffer == nullptr || size == 0 || listener == nullptr)
    return false;
  if (!IsValid())
    return false;

  return DispatchRequest(SendingTo, const_cast<void*>(buffer), size, flags,
                         address, length, listener, NULL) != nullptr;
}

AsyncDatagramSocket::AsyncContext* AsyncDatagramSocket::BeginSendTo(
    const void* buffer, int size, int flags, const sockaddr* address,
    int length, HANDLE event) {
  if (buffer == nullptr || size == 0 || event == NULL)
    return nullptr;
  if (!IsValid())
    return nullptr;

  return DispatchRequest(SendingTo, const_cast<void*>(buffer), size, flags,
                         address, length, nullptr, event);
}

int AsyncDatagramSocket::EndSendTo(AsyncContext* context) {
  if (context == nullptr || context->socket != this)
    return SOCKET_ERROR;
  if (context->action != SendingTo)
    return SOCKET_ERROR;

  return EndRequest(context, nullptr, nullptr);
}

AsyncDatagramSocket::AsyncContext* AsyncDatagramSocket::DispatchRequest(
    Action action, void* buffer, int size, int flags, const sockaddr* address,
    int length, SocketEventListener* listener, HANDLE event) {
  if (!::InitOnceExecuteOnce(&init_once_, OnInitialize, this, nullptr))
    return nullptr;

  std::unique_ptr<AsyncContext> context(new AsyncContext(this));
  if (context == nullptr)
    return nullptr;

  context->len = size;
  context->buf = static_cast<char*>(buffer);
  context->action = action;
  context->flags = flags;
  context->listener = listener;
  context->event = event;

  if (address != nullptr && length > 0) {
    ::memmove(&context->address, address, length);
    context->address_length = length;
  }

  if (event != NULL && !::ResetEvent(event))
    return nullptr;

  if (!::TrySubmitThreadpoolCallback(OnRequested, context.get(), environment_))
    return nullptr;

  return context.release();
}

int AsyncDatagramSocket::EndRequest(AsyncContext* context, sockaddr* address,
                                    int* length) {
  if (context->event != NULL)
    ::WaitForSingleObject(context->event, INFINITE);

  DWORD bytes = 0;
  BOOL succeeded = ::GetOverlappedResult(reinterpret_cast<HANDLE>(descriptor_),
                                         context, &bytes, FALSE);
  if (succeeded && address != nullptr && length != nullptr) {
    if (*length >= context->address_length)
      ::memmove(address, &context->address, context->address_length);
    *length = context->address_length;
  }

  delete context;

  if (!succeeded)
    return SOCKET_ERROR;

  return bytes;
}

BOOL CALLBACK AsyncDatagramSocket::OnInitialize(INIT_ONCE* init_once,
                                                void* param, void** context) {
  return static_cast<AsyncDatagramSocket*>(param)->OnInitialize();
}

BOOL AsyncDatagramSocket::OnInitialize() {
  if (io_ == NULL) {
    io_ = ::CreateThreadpoolIo(reinterpret_cast<HANDLE>(descriptor_),
                               OnCompleted, this, environment_);
    if (io_ == NULL)
      return FALSE;
  }

  return TRUE;
}

void CALLBACK AsyncDatagramSocket::OnRequested(PTP_CALLBACK_INSTANCE instance,
                                               void* param) {
  AsyncContext* context = static_cast<AsyncContext*>(param);
  context->socket->OnRequested(context);
}

void AsyncDatagramSocket::OnRequested(AsyncContext* context) {
  DWORD bytes = 0;
  int result = 0;
  int error = ERROR_SUCCESS;

  ::StartThreadpoolIo(io_);

  switch (context->action) {
    case Receiving:
      result = ::WSARecv(descriptor_, context, 1, &bytes, &context->flags,
                         context, NULL);
      break;

    case Sending:
      result = ::WSASend(descriptor_, context, 1, &bytes, context->flags,
                         context, NULL);
      break;

    case ReceivingFrom:
      result = ::WSARecvFrom(descriptor_, context, 1, &bytes, &context->flags,
                             reinterpret_cast<sockaddr*>(&context->address),
                             &context->address_length, context, NULL);
      break;

    case SendingTo:
      result = ::WSASendTo(descriptor_, context, 1, &bytes, context->flags,
                           reinterpret_cast<sockaddr*>(&context->address),
                           context->address_length, context, NULL);
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

void CALLBACK AsyncDatagramSocket::OnCompleted(PTP_CALLBACK_INSTANCE instance,
                                               void* context, void* overlapped,
                                               ULONG error, ULONG_PTR bytes,
                                               PTP_IO io) {
  static_cast<AsyncDatagramSocket*>(context)->OnCompleted(
      static_cast<AsyncContext*>(static_cast<OVERLAPPED*>(overlapped)),
      error, bytes);
}

void AsyncDatagramSocket::OnCompleted(AsyncContext* context, ULONG error,
                                      ULONG_PTR bytes) {
  SocketEventListener* listener = context->listener;

  if (listener != NULL) {
    void* buffer = context->buf;

    switch (context->action) {
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

      case ReceivingFrom: {
        sockaddr_storage from = context->address;
        int from_length = context->address_length;
        int result = EndReceiveFrom(context, nullptr, nullptr);
        if (result == SOCKET_ERROR && error == 0)
          error = ::GetLastError();

        listener->OnReceivedFrom(this, error, buffer, bytes,
                                 reinterpret_cast<sockaddr*>(&from),
                                 from_length);
        break;
      }

      case SendingTo: {
        sockaddr_storage to = context->address;
        int to_length = context->address_length;
        int result = EndSendTo(context);
        if (result == SOCKET_ERROR && error == 0)
          error = ::GetLastError();

        listener->OnSentTo(this, error, buffer, bytes,
                           reinterpret_cast<sockaddr*>(&to), to_length);
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
