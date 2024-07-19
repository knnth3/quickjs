#ifndef QJS_LIBRARY
#define QJS_LIBRARY

#include <stdio.h>
#include "qjslib_exports.h"
#include "stdint.h"
#include "quickjs.h"

typedef void JSBufferUnload(void* buffer);

typedef struct NativeBuffer
{
    const char* data;
    JSBufferUnload* unload_func;
} NativeBuffer;

typedef void JSLoadModuleFunc(JSContext* ctx, const char* module_name, NativeBuffer* output);

QJSLIB_EXPORT JSRuntime* qjs_create_runtime();
QJSLIB_EXPORT void qjs_release_runtime(JSRuntime* rt);
QJSLIB_EXPORT void qjs_set_memory_limit(JSRuntime* rt, uint32_t limit);

QJSLIB_EXPORT JSContext* qjs_create_context(JSRuntime* rt);
QJSLIB_EXPORT void qjs_release_context(JSContext* ctx);

QJSLIB_EXPORT void qjs_get_exception(JSContext* ctx, void** message, void** stackTrace);

QJSLIB_EXPORT JSValue qjs_create_func(JSContext* ctx, JSCFunction* func, const char* name);
QJSLIB_EXPORT JSValue qjs_create_object(JSContext* ctx);
QJSLIB_EXPORT JSValue qjs_create_string(JSContext* ctx, const char* value);
QJSLIB_EXPORT JSValue qjs_create_int_32(JSContext* ctx, int32_t value);
QJSLIB_EXPORT JSValue qjs_create_int_64(JSContext* ctx, int64_t value);
QJSLIB_EXPORT JSValue qjs_create_double(JSContext* ctx, double value);

QJSLIB_EXPORT void qjs_set_property(JSContext* ctx, JSValue obj, const char* name, JSValue value);
QJSLIB_EXPORT void qjs_set_global(JSContext* ctx, const char* name, JSValue value);

QJSLIB_EXPORT const char* qjs_as_string(JSContext* ctx, JSValue value, int32_t* size);
QJSLIB_EXPORT void qjs_free_string(JSContext* ctx, const char* str);
QJSLIB_EXPORT void qjs_free_value(JSContext* ctx, JSValue value);

QJSLIB_EXPORT int32_t qjs_as_int_32(JSContext* ctx, JSValue value);
QJSLIB_EXPORT int64_t qjs_as_int_64(JSContext* ctx, JSValue value);
QJSLIB_EXPORT double qjs_as_double(JSContext* ctx, JSValue value);

QJSLIB_EXPORT int32_t qjs_is_func(JSContext* ctx, JSValue value);
QJSLIB_EXPORT JSValue qjs_call_func(JSContext* ctx, JSValue value, JSValue this_obj, int argc, JSValue* argv);

QJSLIB_EXPORT int qjs_eval(JSContext* ctx, const char* filename, const char* code, uint32_t isModule);
QJSLIB_EXPORT JSValue qjs_eval_ex(JSContext* ctx, const char* filename, const char* code, uint32_t isModule);
QJSLIB_EXPORT void qjs_tick(JSContext* ctx);

// Modules
QJSLIB_EXPORT void qjs_set_module_loader(JSRuntime* rt, JSLoadModuleFunc* func);

#endif  // QJS_LIBRARY
