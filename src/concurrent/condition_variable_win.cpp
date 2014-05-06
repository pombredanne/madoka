// Copyright (c) 2014 dacci.org

#include <madoka/concurrent/condition_variable.h>

#include <windows.h>

#include "concurrent/internals.h"

namespace madoka {
namespace concurrent {

ConditionVariable::ConditionVariable() : variable_(new VariableImpl) {
  ::InitializeConditionVariable(variable_);
}

ConditionVariable::~ConditionVariable() {
  delete variable_;
}

bool ConditionVariable::Sleep(CriticalSection* lock) {
  return ::SleepConditionVariableCS(variable_, lock->mutex_, INFINITE) != FALSE;
}

bool ConditionVariable::Sleep(ReadWriteLock* lock, bool shared) {
  return ::SleepConditionVariableSRW(
      variable_, lock->lock_, INFINITE,
      shared ? CONDITION_VARIABLE_LOCKMODE_SHARED : 0) != FALSE;
}

void ConditionVariable::Wake() {
  ::WakeConditionVariable(variable_);
}

void ConditionVariable::WakeAll() {
  ::WakeAllConditionVariable(variable_);
}

}  // namespace concurrent
}  // namespace madoka
