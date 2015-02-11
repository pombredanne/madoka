// Copyright (c) 2013 dacci.org

#ifndef MADOKA_COMMON_H_
#define MADOKA_COMMON_H_

#ifdef _MSC_VER
#include <yvals.h>
#endif  // _MSC_VER

#ifndef MADOKA_DEPRECATED
#  if defined(_MSC_VER)
#    define MADOKA_DEPRECATED __declspec(deprecated)
#  elif defined(__GNUC__)
#    define MADOKA_DEPRECATED __attribute__((deprecated))
#  else
#    define MADOKA_DEPRECATED
#  endif
#endif  // MADOKA_DEPRECATED

#ifndef MADOKA_OVERRIDE
#  if __cplusplus >= 201103L
#    define MADOKA_OVERRIDE override
#  elif defined(_MSC_VER)
#    if defined(_HAS_CPP0X) && _HAS_CPP0X
#      define MADOKA_OVERRIDE override
#    else
#      define MADOKA_OVERRIDE
#    endif
#  else
#    define MADOKA_OVERRIDE
#  endif
#endif  // MADOKA_OVERRIDE

#ifndef MADOKA_DELETED
#  if __cplusplus >= 201103L
#    define MADOKA_DELETED = delete
#  elif defined(_MSC_VER)
#    if defined(_HAS_CPP0X) && _HAS_CPP0X
#      define MADOKA_DELETED = delete
#    else
#      define MADOKA_DELETED
#    endif
#  else
#    define MADOKA_DELETED
#  endif
#endif  // MADOKA_DELETED

#ifndef MADOKA_DISALLOW_COPY_AND_ASSIGN
// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define MADOKA_DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) MADOKA_DELETED;       \
  void operator=(const TypeName&) MADOKA_DELETED
#endif  // MADOKA_DISALLOW_COPY_AND_ASSIGN

#endif  // MADOKA_COMMON_H_
