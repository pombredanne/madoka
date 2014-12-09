// Copyright (c) 2014 dacci.org

#ifndef MADOKA_CONCURRENT_READ_WRITE_LOCK_H_
#define MADOKA_CONCURRENT_READ_WRITE_LOCK_H_

#include <madoka/common.h>
#include <madoka/concurrent/lockable.h>

namespace madoka {
namespace concurrent {

struct LockImpl;

class ReadWriteLock : public Lockable {
 public:
  ReadWriteLock();
  ~ReadWriteLock();

  void AcquireReadLock();
  bool TryAcquireReadLock();
  void ReleaseReadLock();

  inline void Lock() MADOKA_OVERRIDE {
    AcquireReadLock();
  }

  inline void Unlock() MADOKA_OVERRIDE {
    ReleaseReadLock();
  }

  void AcquireWriteLock();
  bool TryAcquireWriteLock();
  void ReleaseWriteLock();

 private:
  friend class ConditionVariable;

  LockImpl* lock_;

  DISALLOW_COPY_AND_ASSIGN(ReadWriteLock);
};

class ReadLock : public Lockable {
 public:
  explicit ReadLock(ReadWriteLock* lock) : lock_(lock) {}

  void Lock() MADOKA_OVERRIDE {
    lock_->AcquireReadLock();
  }

  void Unlock() MADOKA_OVERRIDE {
    lock_->ReleaseReadLock();
  }

 private:
  ReadWriteLock* const lock_;

  DISALLOW_COPY_AND_ASSIGN(ReadLock);
};

class WriteLock : public Lockable {
 public:
  explicit WriteLock(ReadWriteLock* lock) : lock_(lock) {}

  void Lock() MADOKA_OVERRIDE {
    lock_->AcquireWriteLock();
  }

  void Unlock() MADOKA_OVERRIDE {
    lock_->ReleaseWriteLock();
  }

 private:
  ReadWriteLock* const lock_;

  DISALLOW_COPY_AND_ASSIGN(WriteLock);
};

}  // namespace concurrent
}  // namespace madoka

#endif  // MADOKA_CONCURRENT_READ_WRITE_LOCK_H_
