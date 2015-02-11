// Copyright (c) 2015 dacci.org

#include "madoka/io/pipe_stream.h"

#include <assert.h>
#include <stdio.h>

#include <memory>
#include <string>

#include "io/handle_stream_impl.h"

namespace {

enum PipeRequest {
  WaitForConnection = 1,
  Transact,
};

}  // namespace

namespace madoka {
namespace io {

struct PipeStream::AsyncContext : HandleStream::AsyncContext{
  void* buffer2;
  DWORD length2;
};

PipeStream::PipeStream() : connected_(false) {
}

PipeStream::~PipeStream() {
  Reset();
}

void PipeStream::Close() {
  if (!IsValid())
    return;

  Flush();

  HandleStream::Close();
}

HRESULT PipeStream::Create(const wchar_t* name, DWORD pipe_mode) {
  if (name == nullptr)
    return E_INVALIDARG;
  if (IsValid())
    return E_HANDLE;

  Reset();

  std::wstring pipe_name(L"\\\\.\\pipe\\");
  pipe_name.append(name);

  handle_ = CreateNamedPipeW(pipe_name.c_str(),
                             PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                             pipe_mode, PIPE_UNLIMITED_INSTANCES, 0, 0, 0,
                             nullptr);
  if (handle_ == INVALID_HANDLE_VALUE)
    return HRESULT_FROM_LAST_ERROR();

  return S_OK;
}

HRESULT PipeStream::Create(const char* name, DWORD pipe_mode) {
  if (name == nullptr)
    return E_INVALIDARG;

  int length = strlen(name);
  std::wstring wide_name;

  wide_name.resize(length);
  length = MultiByteToWideChar(CP_OEMCP, MB_COMPOSITE, name, length,
                               &wide_name[0], wide_name.size() + 1);
  if (length == 0)
    return HRESULT_FROM_LAST_ERROR();

  wide_name.resize(length);

  return Create(wide_name.c_str(), pipe_mode);
}

void PipeStream::WaitForConnectionAsync(Listener* listener) {
  HRESULT result = S_OK;

  if (listener == nullptr)
    result = E_INVALIDARG;
  else if (!IsValid())
    result = E_HANDLE;

  if (SUCCEEDED(result)) {
    auto context = CreateContext<AsyncContext>(PipeRequest::WaitForConnection,
                                               nullptr, 0, listener);
    if (context != nullptr)
      result = DispatchRequest(std::move(context));
    else
      result = E_OUTOFMEMORY;
  }

  if (FAILED(result))
    listener->OnConnected(this, result);
}

PipeStream::AsyncContext* PipeStream::BeginWaitForConnection(HANDLE event) {
  if (event == NULL)
    return nullptr;
  if (!IsValid())
    return nullptr;

  auto context = CreateContext<AsyncContext>(PipeRequest::WaitForConnection,
                                             nullptr, 0, nullptr);
  if (context == nullptr)
    return nullptr;

  context->hEvent = event;

  auto pointer = context.get();
  HRESULT result = DispatchRequest(std::move(context));
  if (FAILED(result))
    return nullptr;

  return pointer;
}

HRESULT PipeStream::EndWaitForConnection(AsyncContext* context) {
  if (context == nullptr)
    return E_INVALIDARG;
  if (context->stream != this ||
      context->type != PipeRequest::WaitForConnection)
    return E_HANDLE;

  HRESULT result = EndRequest(context, nullptr);
  if (SUCCEEDED(result))
    connected_ = true;

  return result;
}

HRESULT PipeStream::WaitForConnection() {
  if (!IsValid())
    return E_HANDLE;

  HANDLE event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
  if (event == NULL)
    return HRESULT_FROM_LAST_ERROR();

  HRESULT result = S_OK;

  do {
    auto context = BeginWaitForConnection(event);
    if (context == nullptr) {
      result = E_FAIL;
      break;
    }

    if (!WaitForSingleObject(event, INFINITE) != WAIT_OBJECT_0) {
      result = HRESULT_FROM_LAST_ERROR();
      EndWaitForConnection(context);
      break;
    }

    result = EndWaitForConnection(context);
  } while (false);

  CloseHandle(event);

  return result;
}

HRESULT PipeStream::Disconnect() {
  if (!IsValid())
    return E_HANDLE;
  if (!connected_)
    return E_ILLEGAL_METHOD_CALL;

  if (!DisconnectNamedPipe(handle_))
    return HRESULT_FROM_LAST_ERROR();

  connected_ = false;

  return S_OK;
}

HRESULT PipeStream::ImpersonateClient() {
  if (!IsValid())
    return E_HANDLE;

  if (!ImpersonateNamedPipeClient(handle_))
    return HRESULT_FROM_LAST_ERROR();

  return S_OK;
}

HRESULT PipeStream::GetClientComputerName(wchar_t* buffer, ULONG length) {
  if (buffer == nullptr)
    return E_INVALIDARG;
  if (!IsValid())
    return E_HANDLE;

  if (!GetNamedPipeClientComputerNameW(handle_, buffer, length))
    return HRESULT_FROM_LAST_ERROR();

  return S_OK;
}

HRESULT PipeStream::GetClientComputerName(char* buffer, ULONG length) {
  if (buffer == nullptr)
    return E_INVALIDARG;
  if (!IsValid())
    return E_HANDLE;

  if (!GetNamedPipeClientComputerNameA(handle_, buffer, length))
    return HRESULT_FROM_LAST_ERROR();

  return S_OK;
}

HRESULT PipeStream::GetClientComputerName(std::wstring* buffer) {
  if (buffer == nullptr)
    return E_INVALIDARG;

  buffer->resize(MAX_COMPUTERNAME_LENGTH);

  HRESULT result = GetClientComputerName(&buffer->front(),
                                         buffer->capacity() + 1);
  if (SUCCEEDED(result))
    buffer->resize(wcslen(buffer->c_str()));
  else
    buffer->clear();

  return result;
}

HRESULT PipeStream::GetClientComputerName(std::string* buffer) {
  if (buffer == nullptr)
    return E_INVALIDARG;

  buffer->resize(MAX_COMPUTERNAME_LENGTH);

  HRESULT result = GetClientComputerName(&buffer->front(),
                                         buffer->capacity() + 1);
  if (SUCCEEDED(result))
    buffer->resize(strlen(buffer->c_str()));
  else
    buffer->clear();

  return result;
}

HRESULT PipeStream::GetClientUserName(wchar_t* buffer, ULONG length) {
  if (buffer == nullptr)
    return E_INVALIDARG;
  if (!IsValid())
    return E_HANDLE;

  if (!GetNamedPipeHandleStateW(handle_, nullptr, nullptr, nullptr, nullptr,
                                buffer, length))
    return HRESULT_FROM_LAST_ERROR();

  return S_OK;
}

HRESULT PipeStream::GetClientUserName(char* buffer, ULONG length) {
  if (buffer == nullptr)
    return E_INVALIDARG;
  if (!IsValid())
    return E_HANDLE;

  if (!GetNamedPipeHandleStateA(handle_, nullptr, nullptr, nullptr, nullptr,
                                buffer, length))
    return HRESULT_FROM_LAST_ERROR();

  return S_OK;
}

HRESULT PipeStream::GetClientUserName(std::wstring* buffer) {
  if (buffer == nullptr)
    return E_INVALIDARG;

  buffer->resize(256);

  HRESULT result = GetClientUserName(&buffer->front(), buffer->capacity() + 1);
  if (SUCCEEDED(result))
    buffer->resize(wcslen(buffer->c_str()));
  else
    buffer->clear();

  return result;
}

HRESULT PipeStream::GetClientUserName(std::string* buffer) {
  if (buffer == nullptr)
    return E_INVALIDARG;

  buffer->resize(256);

  HRESULT result = GetClientUserName(&buffer->front(), buffer->capacity() + 1);
  if (SUCCEEDED(result))
    buffer->resize(strlen(buffer->c_str()));
  else
    buffer->clear();

  return result;
}

HRESULT PipeStream::GetClientProcessId(ULONG* process_id) {
  if (process_id == nullptr)
    return E_INVALIDARG;
  if (!IsValid())
    return E_HANDLE;

  if (!GetNamedPipeClientProcessId(handle_, process_id))
    return HRESULT_FROM_LAST_ERROR();

  return S_OK;
}

HRESULT PipeStream::GetClientSessionId(ULONG* session_id) {
  if (session_id == nullptr)
    return E_INVALIDARG;
  if (!IsValid())
    return E_HANDLE;

  if (!GetNamedPipeClientSessionId(handle_, session_id))
    return HRESULT_FROM_LAST_ERROR();

  return S_OK;
}

HRESULT PipeStream::Open(const wchar_t* name, DWORD timeout) {
  if (name == nullptr)
    return E_INVALIDARG;
  if (IsValid())
    return E_HANDLE;

  Reset();

  std::wstring pipe_name(L"\\\\.\\pipe\\");
  pipe_name.append(name);

  if (!WaitNamedPipeW(pipe_name.c_str(), timeout))
    return HRESULT_FROM_LAST_ERROR();

  handle_ = CreateFileW(pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                        OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
  if (handle_ == INVALID_HANDLE_VALUE)
    return HRESULT_FROM_LAST_ERROR();

  connected_ = true;

  return S_OK;
}

HRESULT PipeStream::Open(const char* name, DWORD timeout) {
  if (name == nullptr)
    return E_INVALIDARG;

  int length = strlen(name);
  std::wstring wide_name;

  wide_name.resize(length);
  length = MultiByteToWideChar(CP_OEMCP, MB_COMPOSITE, name, length,
                               &wide_name[0], wide_name.size() + 1);
  if (length == 0)
    return HRESULT_FROM_LAST_ERROR();

  wide_name.resize(length);

  return Open(wide_name.c_str(), timeout);
}

HRESULT PipeStream::GetState(DWORD* state, DWORD* instances,
                             DWORD* max_collections, DWORD* collect_timeout) {
  if (!IsValid())
    return E_HANDLE;

  if (!GetNamedPipeHandleState(handle_, state, instances, max_collections,
                               collect_timeout, nullptr, 0))
    return HRESULT_FROM_LAST_ERROR();

  return S_OK;
}

HRESULT PipeStream::SetState(DWORD* mode, DWORD* max_collections,
                             DWORD* collect_timeout) {
  if (!IsValid())
    return E_HANDLE;

  if (!SetNamedPipeHandleState(handle_, mode, max_collections, collect_timeout))
    return HRESULT_FROM_LAST_ERROR();

  return S_OK;
}

HRESULT PipeStream::PeekData(void* buffer, DWORD length, DWORD* bytes_read,
                             DWORD* bytes_avail, DWORD* bytes_left) {
  if (!IsValid())
    return E_HANDLE;

  if (!PeekNamedPipe(handle_, buffer, length, bytes_read, bytes_avail,
                     bytes_left))
    return HRESULT_FROM_LAST_ERROR();

  return S_OK;
}

void PipeStream::TransactDataAsync(void* write_buffer, DWORD write_length,
                                   void* read_buffer, DWORD read_length,
                                   Listener* listener) {
  HRESULT result = S_OK;

  if (listener == nullptr)
    result = E_INVALIDARG;
  else if (!IsValid())
    result = E_HANDLE;

  if (SUCCEEDED(result)) {
    auto context = CreateContext<AsyncContext>(
        PipeRequest::Transact, write_buffer, write_length, listener);
    if (context != nullptr) {
      context->buffer2 = read_buffer;
      context->length2 = read_length;
      result = DispatchRequest(std::move(context));
    } else {
      result = E_OUTOFMEMORY;
    }
  }

  if (FAILED(result))
    listener->OnTransacted(this, result, 0);
}

PipeStream::AsyncContext* PipeStream::BeginTransactData(void* write_buffer,
                                                        DWORD write_length,
                                                        void* read_buffer,
                                                        DWORD read_length,
                                                        HANDLE event) {
  if (event == NULL)
    return nullptr;
  if (!IsValid())
    return nullptr;

  auto context = CreateContext<AsyncContext>(
      PipeRequest::Transact, write_buffer, write_length, nullptr);
  if (context == nullptr)
    return nullptr;

  context->hEvent = event;
  context->buffer2 = read_buffer;
  context->length2 = read_length;

  auto pointer = context.get();
  HRESULT result = DispatchRequest(std::move(context));
  if (FAILED(result))
    return nullptr;

  return pointer;
}

HRESULT PipeStream::EndTransactData(AsyncContext* context, DWORD* read_length) {
  if (context == nullptr)
    return E_INVALIDARG;
  if (context->stream != this || context->type != PipeRequest::Transact)
    return E_HANDLE;

  return EndRequest(context, read_length);
}

HRESULT PipeStream::TransactData(void* write_buffer, DWORD write_length,
                                 void* read_buffer, DWORD* read_length) {
  HANDLE event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
  if (event == NULL)
    return HRESULT_FROM_LAST_ERROR();

  HRESULT result = S_OK;

  do {
    auto context = BeginTransactData(write_buffer, write_length, read_buffer,
                                     *read_length, event);
    if (context == nullptr) {
      result = E_FAIL;
      break;
    }

    if (WaitForSingleObject(event, INFINITE) != WAIT_OBJECT_0) {
      result = HRESULT_FROM_LAST_ERROR();
      EndTransactData(context, read_length);
      break;
    }

    result = EndTransactData(context, read_length);
  } while (false);

  CloseHandle(event);

  return result;
}

HRESULT PipeStream::OnCustomRequested(
    HandleStream::AsyncContext* handle_context) {
  auto context = static_cast<AsyncContext*>(handle_context);
  BOOL succeeded;

  switch (context->type) {
    case PipeRequest::WaitForConnection:
      succeeded = ConnectNamedPipe(handle_, context);
      break;

    case PipeRequest::Transact:
      succeeded = TransactNamedPipe(handle_, context->buffer, context->length,
                                    context->buffer2, context->length2, nullptr,
                                    context);
      break;

    default:
      assert(false);
      return E_NOTIMPL;
  }

  DWORD error = GetLastError();

  if (!succeeded && error != ERROR_IO_PENDING) {
    if (error == ERROR_PIPE_CONNECTED && context->hEvent != NULL)
      SetEvent(context->hEvent);

    return __HRESULT_FROM_WIN32(error);
  }

  return S_OK;
}

void PipeStream::OnCustomCompleted(HandleStream::AsyncContext* async_context,
                                   HRESULT result, ULONG_PTR length) {
  auto context = static_cast<AsyncContext*>(async_context);
  auto listener =
      static_cast<Listener*>(
          static_cast<HandleStream::Listener*>(context->listener));

  switch (context->type) {
    case PipeRequest::WaitForConnection:
      if (HRESULT_CODE(result) == ERROR_PIPE_CONNECTED)
        result = S_OK;

      listener->OnConnected(this, result);
      break;

    case PipeRequest::Transact:
      listener->OnTransacted(this, result, length);
      break;

    default:
      assert(false);
  }
}

}  // namespace io
}  // namespace madoka
