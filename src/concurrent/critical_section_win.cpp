// Copyright (c) 2013 dacci.org

#include <madoka/concurrent/critical_section.h>

#include <windows.h>

#include "concurrent/internals.h"

namespace madoka {
namespace concurrent {

CriticalSection::CriticalSection() : mutex_(new MutexImpl) {
  ::InitializeCriticalSection(mutex_);
}

CriticalSection::~CriticalSection() {
  ::EnterCriticalSection(mutex_);
  ::DeleteCriticalSection(mutex_);
  delete mutex_;
}

void CriticalSection::Lock() {
  ::EnterCriticalSection(mutex_);
}

bool CriticalSection::TryLock() {
  return ::TryEnterCriticalSection(mutex_) != FALSE;
}

void CriticalSection::Unlock() {
  ::LeaveCriticalSection(mutex_);
}

}  // namespace concurrent
}  // namespace madoka
