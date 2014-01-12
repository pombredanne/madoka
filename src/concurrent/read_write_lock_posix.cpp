// Copyright (c) 2014 dacci.org

#include <madoka/concurrent/read_write_lock.h>

#include <pthread.h>

namespace madoka {
namespace concurrent {

struct ReadWriteLock::LockImpl {
  pthread_rwlock_t lock;
};

ReadWriteLock::ReadWriteLock() : lock_(new LockImpl) {
  lock_->lock = PTHREAD_RWLOCK_INITIALIZER;
}

ReadWriteLock::~ReadWriteLock() {
  ::pthread_rwlock_destroy(&lock_->lock);
  delete lock_;
}

void ReadWriteLock::AcquireReadLock() {
  ::pthread_rwlock_rdlock(&lock_->lock);
}

bool ReadWriteLock::TryAcquireReadLock() {
  return ::pthread_rwlock_tryrdlock(&lock_->lock) == 0;
}

void ReadWriteLock::ReleaseReadLock() {
  ::pthread_rwlock_unlock(&lock_->lock);
}

void ReadWriteLock::AcquireWriteLock() {
  ::pthread_rwlock_wrlock(&lock_->lock);
}

bool ReadWriteLock::TryAcquireWriteLock() {
  return ::pthread_rwlock_trywrlock(&lock_->lock) == 0;
}

void ReadWriteLock::ReleaseWriteLock() {
  ::pthread_rwlock_unlock(&lock_->lock);
}

}  // namespace concurrent
}  // namespace madoka
