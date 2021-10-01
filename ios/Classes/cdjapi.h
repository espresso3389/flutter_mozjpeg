#ifndef _cdjapi_h_
#define _cdjapi_h_

#include <stddef.h>

#if defined(BUILD_FOR_ANDROID)
#include "dart_api_dl.h"
#elif !defined(RUNTIME_INCLUDE_DART_API_DL_H_)
// For bridging header generation purpose only.
typedef long long cdjapi_int64_t;
typedef cdjapi_int64_t Dart_Port_DL;
#endif

#if defined(__cplusplus)
extern "C"
{
#endif

    void set_dart_port(Dart_Port_DL port);
    void debug_print(const char *message);
    void debug_printf(const char *format, ...);
    void notify_progress(void *context, int pass, int totalPass, size_t percentage);
    void notify_progress_v(void *context, int pass, int totalPass, void *address);
    void jt_exit(int code);

#if defined(__cplusplus)
}
#endif

#endif /* _cdjapi_h_ */
