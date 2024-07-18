
#ifndef QJSLIB_EXPORT_H
#define QJSLIB_EXPORT_H

#ifdef QJSLIB_STATIC_DEFINE
#  define QJSLIB_EXPORT
#  define QJSLIB_NO_EXPORT
#else
#  ifndef QJSLIB_EXPORT
#    ifdef qjslib_EXPORTS
        /* We are building this library */
#      define QJSLIB_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define QJSLIB_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef QJSLIB_NO_EXPORT
#    define QJSLIB_NO_EXPORT 
#  endif
#endif

#ifndef QJSLIB_DEPRECATED
#  define QJSLIB_DEPRECATED __declspec(deprecated)
#endif

#ifndef QJSLIB_DEPRECATED_EXPORT
#  define QJSLIB_DEPRECATED_EXPORT QJSLIB_EXPORT QJSLIB_DEPRECATED
#endif

#ifndef QJSLIB_DEPRECATED_NO_EXPORT
#  define QJSLIB_DEPRECATED_NO_EXPORT QJSLIB_NO_EXPORT QJSLIB_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef QJSLIB_NO_DEPRECATED
#    define QJSLIB_NO_DEPRECATED
#  endif
#endif

#endif /* QJSLIB_EXPORT_H */
