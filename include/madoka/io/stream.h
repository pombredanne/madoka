// Copyright (c) 2015 dacci.org

#ifndef MADOKA_IO_STREAM_H_
#define MADOKA_IO_STREAM_H_

#include <stdint.h>

#include <winerror.h>  // for HRESULT

namespace madoka {
namespace io {

class Stream {
 public:
  class Listener {
   public:
    virtual ~Listener() {}

    virtual void OnRead(Stream* stream, HRESULT result, void* buffer,
                        uint64_t length) = 0;
    virtual void OnWritten(Stream* stream, HRESULT result, void* buffer,
                           uint64_t length) = 0;
  };

  virtual ~Stream() {}

  virtual void Close() = 0;

  virtual HRESULT Read(void* buffer, uint64_t* length) = 0;
  virtual void ReadAsync(void* buffer, uint64_t length, Listener* listener) = 0;

  virtual HRESULT Write(const void* buffer, uint64_t* length) = 0;
  virtual void WriteAsync(const void* buffer, uint64_t length,
                          Listener* listener) = 0;
};

}  // namespace io
}  // namespace madoka

#endif  // MADOKA_IO_STREAM_H_
