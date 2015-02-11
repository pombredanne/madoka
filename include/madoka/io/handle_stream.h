// Copyright (c) 2015 dacci.org

#ifndef MADOKA_IO_HANDLE_STREAM_H_
#define MADOKA_IO_HANDLE_STREAM_H_

#include <madoka/io/abstract_stream.h>

namespace madoka {
namespace io {

class HandleStream : public AbstractStream {
 public:
  struct AsyncContext;

  virtual ~HandleStream();

  void Close() override;

  HRESULT Flush();

  HRESULT Read(void* buffer, uint64_t* length) override;
  void ReadAsync(void* buffer, uint64_t length, Listener* listener) override;
  AsyncContext* BeginRead(void* buffer, DWORD length, HANDLE event);
  HRESULT EndRead(AsyncContext* context, DWORD* length);

  HRESULT Write(const void* buffer, uint64_t* length) override;
  void WriteAsync(const void* buffer, uint64_t length,
                  Listener* listener) override;
  AsyncContext* BeginWrite(const void* buffer, DWORD length, HANDLE event);
  HRESULT EndWrite(AsyncContext* context, DWORD* length);

  bool IsValid() const {
    return handle_ != INVALID_HANDLE_VALUE;
  }

  operator bool() const {
    return IsValid();
  }

 protected:
  HandleStream();

  void Reset() override;

  HRESULT EndRequest(AsyncContext* context, DWORD* length);

  void OnRequested(AbstractStream::AsyncContext* async_context) override;
  virtual HRESULT OnCustomRequested(AsyncContext* context) = 0;

  virtual void OnCustomCompleted(AsyncContext* context, HRESULT result,
                                 ULONG_PTR length) = 0;

  HANDLE handle_;

 private:
  static void CALLBACK OnCompleted(PTP_CALLBACK_INSTANCE instance,
                                   void* context, void* overlapped, ULONG error,
                                   ULONG_PTR length, PTP_IO io);
  void OnCompleted(AsyncContext* context, HRESULT result, ULONG_PTR length);

  PTP_IO io_;

  MADOKA_DISALLOW_COPY_AND_ASSIGN(HandleStream);
};

}  // namespace io
}  // namespace madoka

#endif  // MADOKA_IO_HANDLE_STREAM_H_
