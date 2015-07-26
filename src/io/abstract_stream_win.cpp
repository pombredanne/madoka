// Copyright (c) 2015 dacci.org

#include "madoka/io/abstract_stream.h"

#include "madoka/concurrent/lock_guard.h"

#include "io/abstract_stream_impl.h"

namespace madoka {
namespace io {

AbstractStream::~AbstractStream() {
  Reset();
}

AbstractStream::AbstractStream() {
}

void AbstractStream::Reset() {
  madoka::concurrent::LockGuard guard(&lock_);

  while (!requests_.empty())
    empty_.Sleep(&lock_);
}

HRESULT AbstractStream::DispatchRequest(
    std::unique_ptr<AsyncContext>&& context) {  // NOLINT(build/c++11)
  madoka::concurrent::LockGuard guard(&lock_);

  if (!TrySubmitThreadpoolCallback(OnRequested, context.get(), nullptr))
    return HRESULT_FROM_LAST_ERROR();

  requests_.push_back(std::move(context));

  return S_OK;
}

void AbstractStream::EndRequest(AsyncContext* context) {
  madoka::concurrent::LockGuard guard(&lock_);

  for (auto i = requests_.begin(), l = requests_.end(); i != l; ++i) {
    if (i->get() == context) {
      requests_.erase(i);

      if (requests_.empty())
        empty_.WakeAll();

      break;
    }
  }
}

bool AbstractStream::IsValidRequest(AsyncContext* context) {
  if (context == nullptr || context->stream != this)
    return false;

  madoka::concurrent::LockGuard guard(&lock_);

  for (auto i = requests_.begin(), l = requests_.end(); i != l; ++i) {
    if (i->get() == context)
      return true;
  }

  return false;
}

void CALLBACK AbstractStream::OnRequested(PTP_CALLBACK_INSTANCE /*callback*/,
                                          void* request) {
  auto context = static_cast<AsyncContext*>(request);
  context->stream->OnRequested(context);
}

}  // namespace io
}  // namespace madoka
