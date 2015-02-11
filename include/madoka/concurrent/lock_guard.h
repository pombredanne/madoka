// Copyright (c) 2013 dacci.org

#ifndef MADOKA_CONCURRENT_LOCK_GUARD_H_
#define MADOKA_CONCURRENT_LOCK_GUARD_H_

#include <madoka/common.h>
#include <madoka/concurrent/lockable.h>

namespace madoka {
namespace concurrent {

class ReadWriteLock;

class LockGuard {
 public:
  explicit LockGuard(Lockable* lock);
  LockGuard(ReadWriteLock* rw_lock, bool exclusive);

  ~LockGuard();

 private:
  enum Mode {
    Invalid,
    Generic,
    Shared,
    Exclusive,
  };

  Lockable* const lock_;
  ReadWriteLock* const rw_lock_;
  Mode mode_;

  MADOKA_DISALLOW_COPY_AND_ASSIGN(LockGuard);
};

}  // namespace concurrent
}  // namespace madoka

#endif  // MADOKA_CONCURRENT_LOCK_GUARD_H_
