// Copyright (c) 2015 dacci.org

#ifndef MADOKA_IO_PIPE_STREAM_H_
#define MADOKA_IO_PIPE_STREAM_H_

#include <madoka/io/handle_stream.h>

#include <string>

namespace madoka {
namespace io {

class PipeStream : public HandleStream {
 public:
  struct AsyncContext;

  class Listener : public HandleStream::Listener {
   public:
    virtual ~Listener() {}

    virtual void OnConnected(PipeStream* stream, HRESULT result) = 0;
    virtual void OnTransacted(PipeStream* stream, HRESULT result,
                              uint64_t length) = 0;
  };

  PipeStream();
  virtual ~PipeStream();

  void Close() override;

  // Server operations

  HRESULT Create(const wchar_t* name, DWORD pipe_mode);
  HRESULT Create(const char* name, DWORD pipe_mode);

  HRESULT Create(const std::wstring& name, DWORD pipe_mode) {
    return Create(name.c_str(), pipe_mode);
  }

  HRESULT Create(const std::string& name, DWORD pipe_mode) {
    return Create(name.c_str(), pipe_mode);
  }

  void WaitForConnectionAsync(Listener* listener);
  AsyncContext* BeginWaitForConnection(HANDLE event);
  HRESULT EndWaitForConnection(AsyncContext* context);
  // It's an inefficient pseudo synchronous function.
  // It creates an event object each time it is called.
  HRESULT WaitForConnection();

  HRESULT Disconnect();

  HRESULT ImpersonateClient();

  HRESULT GetClientComputerName(wchar_t* buffer, ULONG length);
  HRESULT GetClientComputerName(char* buffer, ULONG length);
  HRESULT GetClientComputerName(std::wstring* buffer);
  HRESULT GetClientComputerName(std::string* buffer);

  HRESULT GetClientUserName(wchar_t* buffer, ULONG length);
  HRESULT GetClientUserName(char* buffer, ULONG length);
  HRESULT GetClientUserName(std::wstring* buffer);
  HRESULT GetClientUserName(std::string* buffer);

  HRESULT GetClientProcessId(ULONG* process_id);
  HRESULT GetClientSessionId(ULONG* session_id);

  // Client operations

  HRESULT Open(const wchar_t* name, DWORD timeout);
  HRESULT Open(const char* name, DWORD timeout);

  HRESULT Open(const std::wstring& name, DWORD timeout) {
    return Open(name.c_str(), timeout);
  }

  HRESULT Open(const std::string& name, DWORD timeout) {
    return Open(name.c_str(), timeout);
  }

  // Common operations

  HRESULT GetState(DWORD* state, DWORD* instances, DWORD* max_collections,
                   DWORD* collect_timeout);
  HRESULT SetState(DWORD* mode, DWORD* max_collections, DWORD* collect_timeout);

  // I/O operations

  HRESULT PeekData(void* buffer, DWORD length, DWORD* bytes_read,
                   DWORD* bytes_avail, DWORD* bytes_left);

  void TransactDataAsync(void* write_buffer, DWORD write_length,
                         void* read_buffer, DWORD read_length,
                         Listener* listener);
  AsyncContext* BeginTransactData(void* write_buffer, DWORD write_length,
                                  void* read_buffer, DWORD read_length,
                                  HANDLE event);
  HRESULT EndTransactData(AsyncContext* context, DWORD* read_length);
  // It's an inefficient pseudo synchronous function.
  // It creates an event object each time it is called.
  HRESULT TransactData(void* write_buffer, DWORD write_length,
                       void* read_buffer, DWORD* read_length);

 private:
  HRESULT OnCustomRequested(
      HandleStream::AsyncContext* handle_context) override;
  void OnCustomCompleted(HandleStream::AsyncContext* async_context,
                         HRESULT result, ULONG_PTR length) override;

  bool connected_;

  MADOKA_DISALLOW_COPY_AND_ASSIGN(PipeStream);
};

}  // namespace io
}  // namespace madoka

#endif  // MADOKA_IO_PIPE_STREAM_H_
