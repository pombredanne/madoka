// Copyright (c) 2015 dacci.org

#ifndef MADOKA_IO_ABSTRACT_STREAM_H_
#define MADOKA_IO_ABSTRACT_STREAM_H_

#include <windows.h>

#include <madoka/common.h>
#include <madoka/concurrent/condition_variable.h>
#include <madoka/concurrent/critical_section.h>
#include <madoka/io/stream.h>

#include <list>
#include <memory>

namespace madoka {
namespace io {

class AbstractStream : public Stream {
 public:
  virtual ~AbstractStream();

 protected:
  enum GeneralRequest;
  struct AsyncContext;

  AbstractStream();

  virtual void Reset();

  template<class T>
  std::unique_ptr<T> CreateContext(int type, void* buffer, uint64_t length,
                                   Listener* listener);

  HRESULT DispatchRequest(std::unique_ptr<AsyncContext>&& context);  // NOLINT
  void EndRequest(AsyncContext* context);
  bool IsValidRequest(AsyncContext* context);

  madoka::concurrent::CriticalSection lock_;

 private:
  static void CALLBACK OnRequested(PTP_CALLBACK_INSTANCE instance,
                                   void* request);
  virtual void OnRequested(AsyncContext* context) = 0;

  std::list<std::unique_ptr<AsyncContext>> requests_;
  madoka::concurrent::ConditionVariable empty_;

  MADOKA_DISALLOW_COPY_AND_ASSIGN(AbstractStream);
};

}  // namespace io
}  // namespace madoka

#endif  // MADOKA_IO_ABSTRACT_STREAM_H_
