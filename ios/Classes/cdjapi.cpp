#include "cdjapi.h"
#include "dart_api_dl.h"

#include <stdio.h>
#include <stdarg.h>

static int64_t dart_port = 0;
void set_dart_port(int64_t port)
{
    dart_port = port;
}

void debug_print(const char *message)
{
    if (!dart_port)
        return;
    Dart_CObject msg;
    msg.type = Dart_CObject_kString;
    msg.value.as_string = (char *)message;
    Dart_PostCObject_DL(dart_port, &msg);
}

void debug_printf(const char *format, ...)
{
    if (!dart_port)
        return;
    va_list ap;
    va_start(ap, format);
    char *buf = NULL;
    vasprintf(&buf, format, ap);
    va_end(ap);
    if (buf)
    {
        debug_print(buf);
        free(buf);
    }
}
void notify_progress(void *context, int pass, int totalPass, int percentage)
{
    notify_progress_v(context, pass, totalPass, (void *)(size_t)percentage);
}

void notify_progress_v(void *context, int pass, int totalPass, void *address)
{
    if (!dart_port)
        return;
    Dart_CObject prog[4];
    prog[0].type = Dart_CObject_kInt64;
    prog[0].value.as_int64 = (int64_t)context;
    prog[1].type = Dart_CObject_kInt32;
    prog[1].value.as_int32 = pass;
    prog[2].type = Dart_CObject_kInt32;
    prog[2].value.as_int32 = totalPass;
    prog[3].type = Dart_CObject_kInt64;
    prog[3].value.as_int64 = (size_t)address;

    Dart_CObject *objs[] = {&prog[0], &prog[1], &prog[2], &prog[3]};
    Dart_CObject arr;
    arr.type = Dart_CObject_kArray;
    arr.value.as_array.length = 4;
    arr.value.as_array.values = objs;
    Dart_PostCObject_DL(dart_port, &arr);
}

void jt_exit(int code)
{
    throw code;
}
