// Copyright (c) 2015 dacci.org

#include "madoka/io/handle_stream.h"

#include <assert.h>

#include "madoka/concurrent/lock_guard.h"

#include "io/handle_stream_impl.h"

namespace madoka {
namespace io {

HandleStream::~HandleStream() {
  Reset();
}

void HandleStream::Close() {
  madoka::concurrent::LockGuard guard(&lock_);

  if (!IsValid())
    return;

  CloseHandle(handle_);
  handle_ = INVALID_HANDLE_VALUE;
}

HRESULT HandleStream::Flush() {
  if (!IsValid())
    return E_HANDLE;

  if (!FlushFileBuffers(handle_))
    return HRESULT_FROM_LAST_ERROR();

  return S_OK;
}

HRESULT HandleStream::Read(void* buffer, uint64_t* length) {
  if (length == nullptr)
    return E_INVALIDARG;
  if (!IsValid())
    return E_HANDLE;

  DWORD length32;
  if (*length > UINT32_MAX)
    length32 = UINT32_MAX;
  else
    length32 = *length;

  *length = 0;

  if (!ReadFile(handle_, buffer, length32, &length32, nullptr))
    return HRESULT_FROM_LAST_ERROR();

  *length = length32;

  return S_OK;
}

void HandleStream::ReadAsync(void* buffer, uint64_t length,
                             Listener* listener) {
  HRESULT result = S_OK;

  if (buffer == nullptr || listener == nullptr)
    result = E_INVALIDARG;
  else if (!IsValid())
    result = E_HANDLE;

  if (SUCCEEDED(result)) {
    if (length > UINT32_MAX)
      length = UINT32_MAX;

    auto context = CreateContext<AsyncContext>(GeneralRequest::Read, buffer,
                                               length, listener);
    if (context != nullptr)
      result = DispatchRequest(std::move(context));
    else
      result = E_OUTOFMEMORY;
  }

  if (FAILED(result))
    listener->OnRead(this, result, buffer, 0);
}

HandleStream::AsyncContext* HandleStream::BeginRead(void* buffer, DWORD length,
                                                    HANDLE event) {
  if (buffer == nullptr || length == 0)
    return nullptr;
  if (!IsValid())
    return nullptr;

  auto context = CreateContext<AsyncContext>(GeneralRequest::Read, buffer,
                                             length, nullptr);
  if (context == nullptr)
    return nullptr;

  context->hEvent = event;

  auto pointer = context.get();
  HRESULT result = DispatchRequest(std::move(context));
  if (FAILED(result))
    return nullptr;

  return pointer;
}

HRESULT HandleStream::EndRead(AsyncContext* context, DWORD* length) {
  if (context == nullptr)
    return E_INVALIDARG;
  if (context->stream != this || context->type != GeneralRequest::Read)
    return E_HANDLE;

  return EndRequest(context, length);
}

HRESULT HandleStream::Write(const void* buffer, uint64_t* length) {
  if (length == nullptr)
    return E_INVALIDARG;
  if (!IsValid())
    return E_HANDLE;

  DWORD length32;
  if (*length > UINT32_MAX)
    length32 = UINT32_MAX;
  else
    length32 = *length;

  *length = 0;

  if (!WriteFile(handle_, buffer, length32, &length32, nullptr))
    return HRESULT_FROM_LAST_ERROR();

  *length = length32;

  return S_OK;
}

void HandleStream::WriteAsync(const void* buffer, uint64_t length,
                              Listener* listener) {
  HRESULT result = S_OK;

  if (buffer == nullptr || listener == nullptr)
    result = E_INVALIDARG;
  else if (!IsValid())
    result = E_HANDLE;

  if (SUCCEEDED(result)) {
    if (length > UINT32_MAX)
      length = UINT32_MAX;

    auto context = CreateContext<AsyncContext>(GeneralRequest::Write,
                                               const_cast<void*>(buffer),
                                               length, listener);
    if (context != nullptr)
      result = DispatchRequest(std::move(context));
    else
      result = E_OUTOFMEMORY;
  }

  if (FAILED(result))
    listener->OnWritten(this, result, const_cast<void*>(buffer), 0);
}

HandleStream::AsyncContext* HandleStream::BeginWrite(const void* buffer,
                                                     DWORD length,
                                                     HANDLE event) {
  if (buffer == nullptr || length == 0)
    return nullptr;
  if (!IsValid())
    return nullptr;

  auto context = CreateContext<AsyncContext>(
      GeneralRequest::Write, const_cast<void*>(buffer), length, nullptr);
  if (context == nullptr)
    return nullptr;

  context->hEvent = event;

  auto pointer = context.get();
  HRESULT result = DispatchRequest(std::move(context));
  if (FAILED(result))
    return nullptr;

  return pointer;
}

HRESULT HandleStream::EndWrite(AsyncContext* context, DWORD* length) {
  if (context == nullptr)
    return E_INVALIDARG;
  if (context->stream != this || context->type != GeneralRequest::Write)
    return E_HANDLE;

  return EndRequest(context, length);
}

HandleStream::HandleStream() : handle_(INVALID_HANDLE_VALUE) {
}

void HandleStream::Reset() {
  madoka::concurrent::LockGuard guard(&lock_);

  if (io_ == nullptr)
    return;

  Close();
  AbstractStream::Reset();

  lock_.Unlock();
  WaitForThreadpoolIoCallbacks(io_, FALSE);
  lock_.Lock();

  CloseThreadpoolIo(io_);
  io_ = nullptr;
}

HRESULT HandleStream::EndRequest(AsyncContext* context, DWORD* length) {
  HRESULT result = S_OK;

  DWORD bytes = 0;
  if (WaitForSingleObject(context->hEvent, INFINITE) == WAIT_OBJECT_0 &&
      GetOverlappedResult(handle_, context, &bytes, TRUE)) {
    if (length != nullptr)
      *length = bytes;
  } else {
    result = HRESULT_FROM_LAST_ERROR();
  }

  AbstractStream::EndRequest(context);

  return result;
}

void HandleStream::OnRequested(AbstractStream::AsyncContext* async_context) {
  auto context = static_cast<AsyncContext*>(async_context);
  HRESULT result = S_OK;

  {
    madoka::concurrent::LockGuard guard(&lock_);

    if (io_ == nullptr) {
      io_ = CreateThreadpoolIo(handle_, OnCompleted, this, nullptr);
      if (io_ == nullptr)
        result = HRESULT_FROM_LAST_ERROR();
    }
  }

  StartThreadpoolIo(io_);

  if (SUCCEEDED(result)) {
    BOOL succeeded;
    switch (context->type) {
      case GeneralRequest::Read:
        succeeded = ReadFile(handle_, context->buffer, context->length, nullptr,
                             context);
        break;

      case GeneralRequest::Write:
        succeeded = WriteFile(handle_, context->buffer, context->length,
                              nullptr, context);
        break;

      default:
        succeeded = TRUE;
        result = OnCustomRequested(context);
    }

    DWORD error = GetLastError();
    if (!succeeded && error != ERROR_IO_PENDING)
      result = __HRESULT_FROM_WIN32(error);
  }

  if (FAILED(result)) {
    CancelThreadpoolIo(io_);
    OnCompleted(context, result, 0);
  }
}

void CALLBACK HandleStream::OnCompleted(PTP_CALLBACK_INSTANCE instance,
                                        void* context, void* overlapped,
                                        ULONG error, ULONG_PTR length,
                                        PTP_IO io) {
  auto stream = static_cast<HandleStream*>(context);
  auto async_context =
      static_cast<AsyncContext*>(static_cast<OVERLAPPED*>(overlapped));
  stream->OnCompleted(async_context, __HRESULT_FROM_WIN32(error), length);
}

void HandleStream::OnCompleted(AsyncContext* context, HRESULT result,
                               ULONG_PTR length) {
  {
    madoka::concurrent::LockGuard guard(&lock_);

    if (!IsValidRequest(context) || context->listener == nullptr)
      return;
  }

  auto listener = static_cast<Listener*>(context->listener);

  switch (context->type) {
    case GeneralRequest::Read:
      listener->OnRead(this, result, context->buffer, length);
      break;

    case GeneralRequest::Write:
      listener->OnWritten(this, result, context->buffer, length);
      break;

    default:
      OnCustomCompleted(context, result, length);
      break;
  }

  AbstractStream::EndRequest(context);
}

}  // namespace io
}  // namespace madoka
