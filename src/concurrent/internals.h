// Copyright (c) 2014 dacci.org

#ifndef CONCURRENT_INTERNALS_H_
#define CONCURRENT_INTERNALS_H_

namespace madoka {
namespace concurrent {

#ifdef _WIN32

struct MutexImpl : CRITICAL_SECTION {
};

struct LockImpl : SRWLOCK {
};

struct VariableImpl : CONDITION_VARIABLE {
};

#else   // _WIN32

struct MutexImpl {
  pthread_mutex_t mutex;
};

struct LockImpl {
  pthread_rwlock_t lock;
};

struct VariableImpl {
  pthread_cond_t cond;
};

#endif  // _WIN32

}  // namespace concurrent
}  // namespace madoka

#endif  // CONCURRENT_INTERNALS_H_
