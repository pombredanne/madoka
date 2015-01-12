// Copyright (c) 2014 dacci.org

#ifndef MADOKA_CONCURRENT_CONDITION_VARIABLE_H_
#define MADOKA_CONCURRENT_CONDITION_VARIABLE_H_

#include <stdint.h>

#include <madoka/common.h>
#include <madoka/concurrent/critical_section.h>
#include <madoka/concurrent/read_write_lock.h>

namespace madoka {
namespace concurrent {

struct VariableImpl;

class ConditionVariable {
 public:
  ConditionVariable();
  virtual ~ConditionVariable();

  bool Sleep(CriticalSection* lock);
  bool Sleep(ReadWriteLock* lock, bool exclusive);
  bool Sleep(ReadWriteLock* lock) {
    return Sleep(lock, false);
  }

  void Wake();
  void WakeAll();

 private:
  VariableImpl* variable_;

  DISALLOW_COPY_AND_ASSIGN(ConditionVariable);
};

}  // namespace concurrent
}  // namespace madoka

#endif  // MADOKA_CONCURRENT_CONDITION_VARIABLE_H_
