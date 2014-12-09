
EXTRA_DIST = \
  include \
  LICENSE \
  madoka.sln \
  madoka.vcxproj

AM_CPPFLAGS = -Iinclude -Isrc

noinst_LIBRARIES = libmadoka.a
libmadoka_a_SOURCES = \
  src/concurrent/internals.h \
  src/concurrent/lock_guard.cpp

if ENABLE_WIN32
libmadoka_a_SOURCES += \
  src/concurrent/condition_variable_win.cpp \
  src/concurrent/critical_section_win.cpp \
  src/concurrent/read_write_lock_win.cpp \
  src/net/async_datagram_socket_win.cpp \
  src/net/async_server_socket_win.cpp \
  src/net/async_socket_win.cpp
else
libmadoka_a_SOURCES += \
  src/concurrent/condition_variable_posix.cpp \
  src/concurrent/critical_section_posix.cpp \
  src/concurrent/read_write_lock_posix.cpp
endif
