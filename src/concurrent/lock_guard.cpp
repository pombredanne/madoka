// Copyright (c) 2014 dacci.org

#include <madoka/concurrent/lock_guard.h>
#include <madoka/concurrent/read_write_lock.h>

namespace madoka {
namespace concurrent {

LockGuard::LockGuard(Lockable* lock)
    : lock_(lock), rw_lock_(nullptr), mode_(Invalid) {
  lock_->Lock();
  mode_ = Generic;
}

LockGuard::LockGuard(ReadWriteLock* rw_lock, bool exclusive)
    : lock_(nullptr), rw_lock_(rw_lock), mode_(Invalid) {
  if (exclusive) {
    rw_lock_->AcquireWriteLock();
    mode_ = Exclusive;
  } else {
    rw_lock_->AcquireReadLock();
    mode_ = Shared;
  }
}

LockGuard::~LockGuard() {
  switch (mode_) {
    case Generic:
      lock_->Unlock();
      break;

    case Shared:
      rw_lock_->ReleaseReadLock();
      break;

    case Exclusive:
      rw_lock_->ReleaseWriteLock();
      break;
  }
}

}  // namespace concurrent
}  // namespace madoka
