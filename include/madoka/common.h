// Copyright (c) 2013 dacci.org

#ifndef MADOKA_COMMON_H_
#define MADOKA_COMMON_H_

#ifndef DISALLOW_COPY_AND_ASSIGN
// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)
#endif  // DISALLOW_COPY_AND_ASSIGN

#ifndef MADOKA_DEPRECATED
  #if defined(_MSC_VER)
    #define MADOKA_DEPRECATED __declspec(deprecated)
  #elif defined(__GNUC__)
    #define MADOKA_DEPRECATED __attribute__((deprecated))
  #else
    #define MADOKA_DEPRECATED
  #endif
#endif  // MADOKA_DEPRECATED

#endif  // MADOKA_COMMON_H_
