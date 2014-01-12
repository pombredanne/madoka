// Copyright (c) 2014 dacci.org

#ifndef MADOKA_CONCURRENT_READ_WRITE_LOCK_H_
#define MADOKA_CONCURRENT_READ_WRITE_LOCK_H_

#include <madoka/common.h>
#include <madoka/concurrent/lockable.h>

namespace madoka {
namespace concurrent {

class ReadWriteLock {
 public:
  ReadWriteLock();
  ~ReadWriteLock();

  void AcquireReadLock();
  bool TryAcquireReadLock();
  void ReleaseReadLock();

  void AcquireWriteLock();
  bool TryAcquireWriteLock();
  void ReleaseWriteLock();

 private:
  struct LockImpl;

  LockImpl* lock_;

  DISALLOW_COPY_AND_ASSIGN(ReadWriteLock);
};

class ReadLock : public Lockable {
 public:
  ReadLock(ReadWriteLock* lock) : lock_(lock) {}

  virtual void Lock() {
    lock_->AcquireReadLock();
  }

  virtual void Unlock() {
    lock_->ReleaseReadLock();
  }

 private:
  ReadWriteLock* const lock_;

  DISALLOW_COPY_AND_ASSIGN(ReadLock);
};

class WriteLock : public Lockable {
 public:
  WriteLock(ReadWriteLock* lock) : lock_(lock) {}

  virtual void Lock() {
    lock_->AcquireWriteLock();
  }

  virtual void Unlock() {
    lock_->ReleaseWriteLock();
  }

 private:
  ReadWriteLock* const lock_;

  DISALLOW_COPY_AND_ASSIGN(WriteLock);
};

}  // namespace concurrent
}  // namespace madoka

#endif  // MADOKA_CONCURRENT_READ_WRITE_LOCK_H_
