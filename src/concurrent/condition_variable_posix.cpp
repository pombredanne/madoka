// Copyright (c) 2014 dacci.org

#include <madoka/concurrent/condition_variable.h>

#include <pthread.h>

#include "concurrent/internals.h"

namespace madoka {
namespace concurrent {

ConditionVariable::ConditionVariable() : variable_(new VariableImpl) {
  variable_->cond = PTHREAD_COND_INITIALIZER;
}

ConditionVariable::~ConditionVariable() {
  ::pthread_cond_destroy(&variable_->cond);
  delete variable_;
}

bool ConditionVariable::Sleep(CriticalSection* lock) {
  ::pthread_cond_wait(&variable_->cond, &lock->mutex_->mutex);
  return true;
}

bool ConditionVariable::Sleep(ReadWriteLock* lock, bool shared) {
  // not supported
  return false;
}

void ConditionVariable::Wake() {
  ::pthread_cond_signal(&variable_->cond);
}

void ConditionVariable::WakeAll() {
  ::pthread_cond_broadcast(&variable_->cond);
}

}  // namespace concurrent
}  // namespace madoka
