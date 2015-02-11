// Copyright (c) 2015 dacci.org

#ifndef IO_ABSTRACT_STREAM_IMPL_H_
#define IO_ABSTRACT_STREAM_IMPL_H_

#ifndef HRESULT_FROM_LAST_ERROR
#define HRESULT_FROM_LAST_ERROR() HRESULT_FROM_WIN32(GetLastError())
#endif  // HRESULT_FROM_LAST_ERROR

namespace madoka {
namespace io {

enum AbstractStream::GeneralRequest {
  Invalid = 0,
  Read = -1,
  Write = -2,
};

struct AbstractStream::AsyncContext {
  AbstractStream* stream;
  int type;
  void* buffer;
  uint64_t length;
  void* listener;
};

template<class T>
std::unique_ptr<T> AbstractStream::CreateContext(
    int type, void* buffer, uint64_t length, Listener* listener) {
  auto context = std::make_unique<T>();
  if (context != nullptr) {
    context->stream = this;
    context->type = type;
    context->buffer = buffer;
    context->length = length;
    context->listener = listener;
  }

  return context;
}

}  // namespace io
}  // namespace madoka

#endif  // IO_ABSTRACT_STREAM_IMPL_H_
