// Copyright (c) 2013 dacci.org

#ifndef MADOKA_CONCURRENT_CRITICAL_SECTION_H_
#define MADOKA_CONCURRENT_CRITICAL_SECTION_H_

#include <madoka/common.h>
#include <madoka/concurrent/lockable.h>

namespace madoka {
namespace concurrent {

struct MutexImpl;

class CriticalSection : public Lockable {
 public:
  CriticalSection();
  virtual ~CriticalSection();

  void Lock() MADOKA_OVERRIDE;
  bool TryLock();
  void Unlock() MADOKA_OVERRIDE;

 private:
  friend class ConditionVariable;

  MutexImpl* mutex_;

  MADOKA_DISALLOW_COPY_AND_ASSIGN(CriticalSection);
};

}  // namespace concurrent
}  // namespace madoka

#endif  // MADOKA_CONCURRENT_CRITICAL_SECTION_H_
