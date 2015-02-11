// Copyright (c) 2015 dacci.org

#ifndef IO_HANDLE_STREAM_IMPL_H_
#define IO_HANDLE_STREAM_IMPL_H_

#include "io/abstract_stream_impl.h"

namespace madoka {
namespace io {

struct HandleStream::AsyncContext : AbstractStream::AsyncContext, OVERLAPPED {
};

}  // namespace io
}  // namespace madoka

#endif  // IO_HANDLE_STREAM_IMPL_H_
