#ifndef _cdjapi_h_
#define _cdjapi_h_

#ifndef RUNTIME_INCLUDE_DART_API_DL_H_
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
    void notify_progress(void *context, int pass, int totalPass, int percentage);
    void notify_progress_v(void *context, int pass, int totalPass, void *address);
    void jt_exit(int code);

#if defined(__cplusplus)
}
#endif

#endif /* _cdjapi_h_ */
