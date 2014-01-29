// Copyright (c) 2014 dacci.org

#include <madoka/concurrent/read_write_lock.h>

#include <windows.h>

#include "concurrent/internals.h"

namespace madoka {
namespace concurrent {

ReadWriteLock::ReadWriteLock() : lock_(new LockImpl) {
  ::InitializeSRWLock(lock_);
}

ReadWriteLock::~ReadWriteLock() {
  delete lock_;
}

void ReadWriteLock::AcquireReadLock() {
  ::AcquireSRWLockShared(lock_);
}

bool ReadWriteLock::TryAcquireReadLock() {
  return ::TryAcquireSRWLockShared(lock_) != 0;
}

void ReadWriteLock::ReleaseReadLock() {
  ::ReleaseSRWLockShared(lock_);
}

void ReadWriteLock::AcquireWriteLock() {
  ::AcquireSRWLockExclusive(lock_);
}

bool ReadWriteLock::TryAcquireWriteLock() {
  return ::TryAcquireSRWLockExclusive(lock_) != 0;
}

void ReadWriteLock::ReleaseWriteLock() {
  ::ReleaseSRWLockExclusive(lock_);
}

}  // namespace concurrent
}  // namespace madoka
