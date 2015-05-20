// Copyright (c) 2015 dacci.org

#include "madoka/net/socket_stream.h"

#include <assert.h>

#include <vector>

#include "madoka/concurrent/lock_guard.h"

#include "io/abstract_stream_impl.h"

namespace {

enum SocketRequest {
  Connect = 1,
  Receive,
  ReceiveFrom,
  Send,
  SendTo,
};

std::vector<WSABUF> CreateBuffers(void* start, uint64_t length) {
  const uint64_t kDivisor = UINT32_MAX + 1ULL;
  DWORD count = length / kDivisor;
  if (count == 0 || count % kDivisor != 0)
    ++count;

  std::vector<WSABUF> buffers(count);
  char* pointer = static_cast<char*>(start);
  uint64_t remaining = length;

  for (auto& buffer : buffers) {
    if (remaining > UINT32_MAX)
      buffer.len = UINT32_MAX;
    else
      buffer.len = remaining;

    buffer.buf = pointer;

    remaining -= buffer.len;
    pointer += buffer.len;
  }

  return buffers;
}

LPFN_CONNECTEX ConnectEx = nullptr;

}  // namespace

namespace madoka {
namespace net {

struct SocketStream::AsyncContext : AbstractStream::AsyncContext, OVERLAPPED {
  const addrinfo* end_point;
  const ADDRINFOW* end_pointw;
  sockaddr_storage address;
  int address_length;
  DWORD flags;
};

SocketStream::SocketStream() : io_(nullptr) {
}

SocketStream::~SocketStream() {
  Reset();
}

void SocketStream::Close() {
  Shutdown(SD_BOTH);
  AbstractSocket::Close();
}

void SocketStream::ConnectAsync(const addrinfo* end_point, Listener* listener) {
  ConnectAsync(end_point, nullptr, listener);
}

SocketStream::AsyncContext* SocketStream::BeginConnect(
    const addrinfo* end_point, HANDLE event) {
  return BeginConnect(end_point, nullptr, event);
}

HRESULT SocketStream::EndConnect(AsyncContext* context,
                                 const addrinfo** end_point) {
  if (context == nullptr || end_point == nullptr)
    return E_INVALIDARG;
  if (context->stream != this || context->type != SocketRequest::Connect ||
      context->end_point == nullptr)
    return E_HANDLE;

  HRESULT result = S_OK;
  DWORD length, flags;

  if (WaitForSingleObject(context->hEvent, INFINITE) == WAIT_OBJECT_0 &&
      WSAGetOverlappedResult(descriptor_, context, &length, TRUE, &flags)) {
    if (SetOption(SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0))
      connected_ = true;
    else
      result = HRESULT_FROM_LAST_ERROR();

    *end_point = context->end_point;
  } else {
    result = HRESULT_FROM_LAST_ERROR();
  }

  EndRequest(context);

  if (FAILED(result))
    Close();

  return result;
}

void SocketStream::ConnectAsync(const ADDRINFOW* end_point,
                                Listener* listener) {
  ConnectAsync(nullptr, end_point, listener);
}

SocketStream::AsyncContext* SocketStream::BeginConnect(
    const ADDRINFOW* end_point, HANDLE event) {
  return BeginConnect(nullptr, end_point, event);
}

HRESULT SocketStream::EndConnect(AsyncContext* context,
                                 const ADDRINFOW** end_point) {
  if (context == nullptr || end_point == nullptr)
    return E_INVALIDARG;
  if (context->stream != this || context->type != SocketRequest::Connect ||
      context->end_pointw == nullptr)
    return E_HANDLE;

  HRESULT result = S_OK;
  DWORD length, flags;

  if (WaitForSingleObject(context->hEvent, INFINITE) == WAIT_OBJECT_0 &&
      WSAGetOverlappedResult(descriptor_, context, &length, TRUE, &flags)) {
    *end_point = context->end_pointw;
    connected_ = true;
  } else {
    result = HRESULT_FROM_LAST_ERROR();
  }

  EndRequest(context);

  return result;
}

HRESULT SocketStream::EndConnect(AsyncContext* context) {
  if (context->end_point != nullptr) {
    const addrinfo* end_point = nullptr;
    return EndConnect(context, &end_point);
  } else {
    const ADDRINFOW* end_point = nullptr;
    return EndConnect(context, &end_point);
  }
}

void SocketStream::ReceiveAsync(void* buffer, uint64_t length, int flags,
                                Listener* listener) {
  CommunicateAsync(SocketRequest::Receive, buffer, length, flags, nullptr, 0,
                   nullptr, listener);
}

SocketStream::AsyncContext* SocketStream::BeginReceive(
    void* buffer, DWORD length, int flags, HANDLE event) {
  return BeginCommunicate(SocketRequest::Receive, buffer, length, flags,
                          nullptr, 0, event);
}

HRESULT SocketStream::EndReceive(AsyncContext* context, DWORD* length,
                                 int* flags) {
  if (context == nullptr || length == nullptr)
    return E_INVALIDARG;
  if (context->stream != this || context->type != SocketRequest::Receive)
    return E_HANDLE;

  *length = 0;

  HRESULT result = S_OK;

  if (WaitForSingleObject(context->hEvent, INFINITE) == WAIT_OBJECT_0 &&
      WSAGetOverlappedResult(descriptor_, context, length, TRUE,
                             &context->flags)) {
    if (flags != nullptr)
      *flags = context->flags;
  } else {
    result = HRESULT_FROM_LAST_ERROR();
  }

  EndRequest(context);

  return S_OK;
}

void SocketStream::ReceiveFromAsync(void* buffer, uint64_t length, int flags,
                                    Listener* listener) {
  CommunicateAsync(SocketRequest::ReceiveFrom, buffer, length, flags, nullptr,
                   0, nullptr, listener);
}

void SocketStream::SendAsync(const void* buffer, uint64_t length, int flags,
                             Listener* listener) {
  CommunicateAsync(SocketRequest::Send, const_cast<void*>(buffer), length,
                   flags, nullptr, 0, nullptr, listener);
}

SocketStream::AsyncContext* SocketStream::BeginReceiveFrom(
    void* buffer, DWORD length, int flags, HANDLE event) {
  return BeginCommunicate(SocketRequest::ReceiveFrom, buffer, length, flags,
                          nullptr, 0, event);
}

HRESULT SocketStream::EndReceiveFrom(AsyncContext* context, DWORD* length,
                                     int* flags, void* address,
                                     int* address_length) {
  if (context == nullptr || length == nullptr)
    return E_INVALIDARG;
  if (context->stream != this || context->type != SocketRequest::ReceiveFrom)
    return E_HANDLE;

  *length = 0;

  HRESULT result = S_OK;

  if (WaitForSingleObject(context->hEvent, INFINITE) == WAIT_OBJECT_0 &&
      WSAGetOverlappedResult(descriptor_, context, length, TRUE,
                             &context->flags)) {
    if (flags != nullptr)
      *flags = context->flags;

    if (address != nullptr && address_length != nullptr) {
      memmove_s(address, *address_length, &context->address,
                context->address_length);
      *address_length = context->address_length;
    }
  } else {
    result = HRESULT_FROM_LAST_ERROR();
  }

  EndRequest(context);

  return result;
}

SocketStream::AsyncContext* SocketStream::BeginSend(
    const void* buffer, DWORD length, int flags, HANDLE event) {
  return BeginCommunicate(SocketRequest::Send, const_cast<void*>(buffer),
                          length, flags, nullptr, 0, event);
}

HRESULT SocketStream::EndSend(AsyncContext* context, DWORD* length) {
  if (context == nullptr || length == nullptr)
    return E_INVALIDARG;
  if (context->stream != this || context->type != SocketRequest::Send)
    return E_HANDLE;

  *length = 0;

  HRESULT result = S_OK;

  if (WaitForSingleObject(context->hEvent, INFINITE) != WAIT_OBJECT_0 ||
      !WSAGetOverlappedResult(descriptor_, context, length, TRUE,
                              &context->flags))
    result = HRESULT_FROM_LAST_ERROR();

  EndRequest(context);

  return result;
}

void SocketStream::SendToAsync(const void* buffer, uint64_t length, int flags,
                               void* to, int to_length, Listener* listener) {
  if (to != nullptr)
    CommunicateAsync(SocketRequest::SendTo, const_cast<void*>(buffer), length,
                     flags, to, to_length, nullptr, listener);
  else
    listener->OnSentTo(this, E_INVALIDARG, const_cast<void*>(buffer), 0,
                       static_cast<sockaddr*>(to), to_length);
}

SocketStream::AsyncContext* SocketStream::BeginSendTo(
    const void* buffer, DWORD length, int flags, void* to, int to_length,
    HANDLE event) {
  if (to == nullptr)
    return nullptr;

  return BeginCommunicate(SocketRequest::SendTo, const_cast<void*>(buffer),
                          length, flags, to, to_length, event);
}

HRESULT SocketStream::EndSendTo(AsyncContext* context, DWORD* length) {
  if (context == nullptr || length == nullptr)
    return E_INVALIDARG;
  if (context->stream != this || context->type != SocketRequest::SendTo)
    return E_HANDLE;

  *length = 0;

  HRESULT result = S_OK;

  if (WaitForSingleObject(context->hEvent, INFINITE) != WAIT_OBJECT_0 ||
      !WSAGetOverlappedResult(descriptor_, context, length, TRUE,
                              &context->flags))
    result = HRESULT_FROM_LAST_ERROR();

  EndRequest(context);

  return result;
}

HRESULT SocketStream::Read(void* buffer, uint64_t* length) {
  if (length == nullptr)
    return E_INVALIDARG;
  if (!IsValid())
    return __HRESULT_FROM_WIN32(WSAENOTSOCK);

  int length32;
  if (*length > INT32_MAX)
    length32 = INT32_MAX;
  else
    length32 = *length;

  *length = 0;

  length32 = Receive(buffer, length32, 0);
  if (length32 == SOCKET_ERROR)
    return HRESULT_FROM_LAST_ERROR();

  *length = length32;

  return S_OK;
}

void SocketStream::ReadAsync(void* buffer, uint64_t length,
                             AbstractStream::Listener* listener) {
  CommunicateAsync(GeneralRequest::Read, buffer, length, 0, nullptr, 0,
                   listener, nullptr);
}

HRESULT SocketStream::Write(const void* buffer, uint64_t* length) {
  if (length == nullptr)
    return E_INVALIDARG;
  if (!IsValid())
    return __HRESULT_FROM_WIN32(WSAENOTSOCK);

  int length32;
  if (*length > INT32_MAX)
    length32 = INT32_MAX;
  else
    length32 = *length;

  *length = 0;

  length32 = Send(buffer, length32, 0);
  if (length32 == SOCKET_ERROR)
    return HRESULT_FROM_LAST_ERROR();

  *length = length32;

  return S_OK;
}

void SocketStream::WriteAsync(const void* buffer, uint64_t length,
                              AbstractStream::Listener* listener) {
  CommunicateAsync(GeneralRequest::Write, const_cast<void*>(buffer), length, 0,
                   nullptr, 0, listener, nullptr);
}

void SocketStream::Reset() {
  Close();

  madoka::concurrent::LockGuard guard(&lock_);

  if (io_ == nullptr)
    return;

  lock_.Unlock();
  WaitForThreadpoolIoCallbacks(io_, FALSE);
  lock_.Lock();

  CloseThreadpoolIo(io_);
  io_ = nullptr;
}

void SocketStream::ConnectAsync(const addrinfo* end_point,
                                const ADDRINFOW* end_pointw,
                                Listener* listener) {
  HRESULT result = S_OK;

  if (end_point == nullptr && end_pointw == nullptr || listener == nullptr)
    result = E_INVALIDARG;
  else if (connected())
    result = __HRESULT_FROM_WIN32(WSAEISCONN);

  if (SUCCEEDED(result)) {
    auto context = CreateContext<AsyncContext>(SocketRequest::Connect, nullptr,
                                               0, nullptr);
    if (context != nullptr) {
      context->listener = listener;
      context->end_point = end_point;
      context->end_pointw = end_pointw;
      result = DispatchRequest(std::move(context));
    } else {
      result = E_OUTOFMEMORY;
    }
  }

  if (FAILED(result)) {
    if (end_point != nullptr)
      listener->OnConnected(this, result, end_point);
    else
      listener->OnConnected(this, result, end_pointw);
  }
}

SocketStream::AsyncContext* SocketStream::BeginConnect(
    const addrinfo* end_point, const ADDRINFOW* end_pointw, HANDLE event) {
  if (end_point == nullptr && end_pointw == nullptr || event == NULL)
    return nullptr;
  if (connected())
    return nullptr;

  if (!ResetEvent(event))
    return nullptr;

  auto context = CreateContext<AsyncContext>(SocketRequest::Connect, nullptr, 0,
                                             nullptr);
  if (context == nullptr)
    return nullptr;

  context->hEvent = event;
  context->end_point = end_point;
  context->end_pointw = end_pointw;

  auto pointer = context.get();
  HRESULT result = DispatchRequest(std::move(context));
  if (FAILED(result))
    return nullptr;

  return pointer;
}

BOOL SocketStream::ConnectAsync(AsyncContext* context) {
  sockaddr* address;
  int address_length;
  if (context->end_point != nullptr) {
    address = context->end_point->ai_addr;
    address_length = context->end_point->ai_addrlen;
  } else {
    address = context->end_pointw->ai_addr;
    address_length = context->end_pointw->ai_addrlen;
  }

  Reset();

  bool succeeded;
  if (context->end_point != nullptr)
    succeeded = Create(context->end_point);
  else
    succeeded = Create(context->end_pointw);
  if (!succeeded)
    return FALSE;

  if (ConnectEx == nullptr) {
    GUID guid = WSAID_CONNECTEX;
    DWORD size = 0;
    int result = WSAIoctl(descriptor_, SIO_GET_EXTENSION_FUNCTION_POINTER,
                          &guid, sizeof(guid), &ConnectEx, sizeof(ConnectEx),
                          &size, nullptr, nullptr);
    if (result != 0)
      return FALSE;
  }

  sockaddr_storage bind_address;
  memset(&bind_address, 0, sizeof(bind_address));
  bind_address.ss_family = address->sa_family;
  if (!Bind(&bind_address, address_length))
    return FALSE;

  io_ = CreateThreadpoolIo(reinterpret_cast<HANDLE>(descriptor_),
                           OnCompleted, this, nullptr);
  if (io_ == nullptr)
    return FALSE;

  StartThreadpoolIo(io_);

  return ConnectEx(descriptor_, address, address_length, nullptr, 0, nullptr,
                   context);
}

void SocketStream::CommunicateAsync(
    int type, void* buffer, uint64_t length, int flags, void* address,
    int address_length, Stream::Listener* listener, Listener* socket_listener) {
  HRESULT result = S_OK;

  if (address != nullptr &&
      (address_length <= 0 || address_length > _SS_MAXSIZE) ||
      listener == nullptr && socket_listener == nullptr)
    result = E_INVALIDARG;
  else if (!IsValid())
    result = __HRESULT_FROM_WIN32(WSAENOTSOCK);

  if (SUCCEEDED(result)) {
    switch (type) {
      case SocketRequest::Receive:
      case SocketRequest::Send:
      case GeneralRequest::Read:
      case GeneralRequest::Write:
        if (!connected())
          result = __HRESULT_FROM_WIN32(WSAENOTCONN);
        break;

      case SocketRequest::ReceiveFrom:
      case SocketRequest::SendTo:
        if (!bound())
          result = __HRESULT_FROM_WIN32(WSAEINVAL);
        else if (connected())
          result = __HRESULT_FROM_WIN32(WSAEISCONN);
        break;
    }
  }

  if (SUCCEEDED(result)) {
    auto context = CreateContext<AsyncContext>(type, buffer, length, nullptr);
    if (context != nullptr) {
      if (socket_listener != nullptr)
        context->listener = socket_listener;
      else
        context->listener = listener;

      context->flags = flags;

      if (address != nullptr) {
        memmove_s(&context->address, sizeof(context->address), address,
                  address_length);
        context->address_length = address_length;
      } else {
        context->address_length = sizeof(context->address);
      }

      result = DispatchRequest(std::move(context));
    } else {
      result = E_OUTOFMEMORY;
    }
  }

  if (SUCCEEDED(result))
    return;

  switch (type) {
    case SocketRequest::Receive:
      socket_listener->OnReceived(this, result, buffer, 0, 0);
      break;

    case SocketRequest::ReceiveFrom:
      socket_listener->OnReceivedFrom(this, result, buffer, 0, 0, nullptr, 0);
      break;

    case SocketRequest::Send:
      socket_listener->OnSent(this, result, buffer, 0);
      break;

    case SocketRequest::SendTo:
      socket_listener->OnSentTo(
          this, result, buffer, 0, static_cast<sockaddr*>(address),
          address_length);
      break;

    case GeneralRequest::Read:
      listener->OnRead(this, result, buffer, 0);
      break;

    case GeneralRequest::Write:
      listener->OnWritten(this, result, const_cast<void*>(buffer), 0);
      break;

    default:
      assert(false);
  }
}

SocketStream::AsyncContext* SocketStream::BeginCommunicate(
    int type, void* buffer, DWORD length, int flags, void* address,
    int address_length, HANDLE event) {
  if (address != nullptr &&
      (address_length <= 0 || address_length > _SS_MAXSIZE) ||
      event == NULL)
    return nullptr;
  if (!connected())
    return nullptr;
  if (!IsValid())
    return nullptr;

  if (!ResetEvent(event))
    return nullptr;

  auto context = CreateContext<AsyncContext>(type, buffer, length, nullptr);
  if (context == nullptr)
    return nullptr;

  context->hEvent = event;
  context->flags = flags;

  if (address != nullptr) {
    memmove_s(&context->address, sizeof(context->address), address,
              address_length);
    context->address_length = address_length;
  }

  auto pointer = context.get();
  HRESULT result = DispatchRequest(std::move(context));
  if (FAILED(result))
    return nullptr;

  return pointer;
}

void SocketStream::OnRequested(AbstractStream::AsyncContext* abstract_context) {
  auto context = static_cast<AsyncContext*>(abstract_context);
  HRESULT result = S_OK;

  if (context->type != SocketRequest::Connect) {
    madoka::concurrent::LockGuard guard(&lock_);

    if (io_ == nullptr) {
      io_ = CreateThreadpoolIo(reinterpret_cast<HANDLE>(descriptor_),
                               OnCompleted, this, nullptr);
      if (io_ == nullptr)
        result = HRESULT_FROM_LAST_ERROR();
    }
  }

  if (SUCCEEDED(result)) {
    BOOL succeeded;

    if (context->type != SocketRequest::Connect)
      StartThreadpoolIo(io_);

    switch (context->type) {
      case SocketRequest::Connect:
        succeeded = ConnectAsync(context);
        break;

      case SocketRequest::Receive:
      case GeneralRequest::Read: {
        auto buffers = CreateBuffers(context->buffer, context->length);
        succeeded = WSARecv(descriptor_, &buffers[0], buffers.size(), nullptr,
                            &context->flags, context, nullptr) == 0;
        break;
      }

      case SocketRequest::ReceiveFrom: {
        auto buffers = CreateBuffers(context->buffer, context->length);
        succeeded = WSARecvFrom(descriptor_,
                                &buffers[0],
                                buffers.size(),
                                nullptr,
                                &context->flags,
                                reinterpret_cast<sockaddr*>(&context->address),
                                &context->address_length,
                                context,
                                nullptr) == 0;
        break;
      }

      case SocketRequest::Send:
      case GeneralRequest::Write: {
        auto buffers = CreateBuffers(context->buffer, context->length);
        succeeded = WSASend(descriptor_, &buffers[0], buffers.size(), nullptr,
                            context->flags, context, nullptr) == 0;
        break;
      }

      case SocketRequest::SendTo: {
        auto buffers = CreateBuffers(context->buffer, context->length);
        succeeded = WSASendTo(descriptor_,
                              &buffers[0],
                              buffers.size(),
                              nullptr,
                              context->flags,
                              reinterpret_cast<sockaddr*>(&context->address),
                              context->address_length,
                              context,
                              nullptr) == 0;
        break;
      }

      default:
        assert(false);
        succeeded = FALSE;
        result = E_NOTIMPL;
    }

    int error = WSAGetLastError();
    if (!succeeded && error != WSA_IO_PENDING) {
      if (io_ != nullptr)
        CancelThreadpoolIo(io_);

      if (context->type == SocketRequest::Connect)
        Reset();

      result = __HRESULT_FROM_WIN32(error);
    }
  }

  if (FAILED(result))
    OnCompleted(context, result, 0);
}

void CALLBACK SocketStream::OnCompleted(PTP_CALLBACK_INSTANCE instance,
                                        void* context, void* overlapped,
                                        ULONG error, ULONG_PTR length,
                                        PTP_IO io) {
  auto stream = static_cast<SocketStream*>(context);
  auto async_context =
      static_cast<AsyncContext*>(static_cast<OVERLAPPED*>(overlapped));
  stream->OnCompleted(async_context, __HRESULT_FROM_WIN32(error), length);
}

void SocketStream::OnCompleted(AsyncContext* context, HRESULT result,
                               ULONG_PTR length) {
  {
    madoka::concurrent::LockGuard guard(&lock_);

    if (!IsValidRequest(context) || context->listener == nullptr)
      return;
  }

  auto listener = static_cast<Listener*>(context->listener);

  switch (context->type) {
    case SocketRequest::Connect:
      if (SUCCEEDED(result)) {
        if (SetOption(SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0))
          connected_ = true;
        else
          result = HRESULT_FROM_LAST_ERROR();
      }

      if (!connected())
        Close();

      if (context->end_point != nullptr)
        listener->OnConnected(this, result, context->end_point);
      else
        listener->OnConnected(this, result, context->end_pointw);
      break;

    case SocketRequest::Receive:
      listener->OnReceived(this, result, context->buffer,
                           length, context->flags);
      break;

    case SocketRequest::ReceiveFrom:
      listener->OnReceivedFrom(
          this, result, context->buffer, length, context->flags,
          reinterpret_cast<sockaddr*>(&context->address),
          context->address_length);
      break;

    case SocketRequest::Send:
      listener->OnSent(this, result, context->buffer, length);
      break;

    case SocketRequest::SendTo:
      listener->OnSentTo(this, result, context->buffer, length,
                         reinterpret_cast<sockaddr*>(&context->address),
                         context->address_length);
      break;

    case GeneralRequest::Read:
      static_cast<Stream::Listener*>(context->listener)->OnRead(
          this, result, context->buffer, length);
      break;

    case GeneralRequest::Write:
      static_cast<Stream::Listener*>(context->listener)->OnWritten(
          this, result, context->buffer, length);
      break;

    default:
      assert(false);
  }

  EndRequest(context);
}

}  // namespace net
}  // namespace madoka
