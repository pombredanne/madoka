// Copyright (c) 2014 dacci.org

#ifndef MADOKA_CONCURRENT_CONDITION_VARIABLE_H_
#define MADOKA_CONCURRENT_CONDITION_VARIABLE_H_

#include <stdint.h>

#include <madoka/common.h>
#include <madoka/concurrent/critical_section.h>

namespace madoka {
namespace concurrent {

struct VariableImpl;

class ConditionVariable {
 public:
  ConditionVariable();
  virtual ~ConditionVariable();

  bool Sleep(CriticalSection* lock);
  void Wake();
  void WakeAll();

 private:
  VariableImpl* variable_;

  DISALLOW_COPY_AND_ASSIGN(ConditionVariable);
};

}  // namespace concurrent
}  // namespace madoka

#endif  // MADOKA_CONCURRENT_CONDITION_VARIABLE_H_
