#pragma once

#if defined(_WIN32)
#  if defined(OpenDIS6_EXPORTS)
#    define OPENDIS6_EXPORT __declspec(dllexport)
#  else
#    define OPENDIS6_EXPORT __declspec(dllimport)
#  endif
#else
#  define OPENDIS6_EXPORT
#endif