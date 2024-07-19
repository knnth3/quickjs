#include "qjslib.h"

#include <stdlib.h>
#include "cutils.h"
#include "quickjs-libc.h"

uint8_t* js_load_contents(JSContext* ctx, const char* code, size_t* pbuf_len)
{
    size_t buf_len = strlen(code);
    uint8_t* buf = js_malloc(ctx, buf_len + 1);
    memcpy(buf, code, buf_len);
    buf[buf_len] = '\0';
    *pbuf_len = buf_len;
    return buf;
}

static JSValue js_gc(JSContext* ctx, JSValue this_val,
    int argc, JSValue* argv)
{
    JS_RunGC(JS_GetRuntime(ctx));
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry navigator_obj[] = {
    JS_PROP_STRING_DEF("userAgent", "quickjs-ng", JS_PROP_ENUMERABLE),
};

static const JSCFunctionListEntry global_obj[] = {
    JS_CFUNC_DEF("gc", 0, js_gc),
    JS_OBJECT_DEF("navigator", navigator_obj, countof(navigator_obj), JS_PROP_C_W_E),
};

static JSValue eval_buf(JSContext* ctx, const void* buf, int buf_len, const char* filename, int eval_flags)
{
    JSValue val;
    if ((eval_flags & JS_EVAL_TYPE_MASK) == JS_EVAL_TYPE_MODULE) {
        /* for the modules, we compile then run to be able to set
           import.meta */
        val = JS_Eval(ctx, buf, buf_len, filename,
            eval_flags | JS_EVAL_FLAG_COMPILE_ONLY);
        if (!JS_IsException(val)) {
            js_module_set_import_meta(ctx, val, TRUE, TRUE);
            val = JS_EvalFunction(ctx, val);
        }
        val = js_std_await(ctx, val);
    }
    else {
        val = JS_Eval(ctx, buf, buf_len, filename, eval_flags);
    }

    return val;
}

static JSContext* JS_NewCustomContext(JSRuntime* rt)
{
    JSContext* ctx;
    ctx = JS_NewContext(rt);
    if (!ctx)
        return NULL;
    /* system modules */
    js_init_module_std(ctx, "std");
    js_init_module_os(ctx, "os");

    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyFunctionList(ctx, global, global_obj, countof(global_obj));
    JS_FreeValue(ctx, global);

    return ctx;
}

static JSModuleDef* js_module_loader_ext(JSContext* ctx, const char* module_name, void* opaque)
{
    JSModuleDef* m;

    size_t buf_len;
    uint8_t* buf;
    JSValue func_val;

    buf = js_load_file(ctx, &buf_len, module_name);

    JSLoadModuleFunc* loadFunc = (JSLoadModuleFunc*)opaque;
    if (!loadFunc) {
        JS_ThrowReferenceError(ctx, "could not load module filename '%s'",
            module_name);
        return NULL;
    }

    NativeBuffer buffer = { NULL, NULL };
    loadFunc(ctx, module_name, &buffer);
    if (!buffer.data) {
        JS_ThrowReferenceError(ctx, "could not load module filename '%s'",
            module_name);
        return NULL;
    }

    buf = js_load_contents(ctx, buffer.data, &buf_len);

    if (buffer.unload_func) {
        buffer.unload_func(buffer.data);
    }

    if (!buf) {
        JS_ThrowReferenceError(ctx, "could not load module filename '%s'",
            module_name);
        return NULL;
    }

    /* compile the module */
    func_val = JS_Eval(ctx, (char*)buf, buf_len, module_name,
        JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    js_free(ctx, buf);
    if (JS_IsException(func_val))
        return NULL;
    /* XXX: could propagate the exception */
    js_module_set_import_meta(ctx, func_val, TRUE, FALSE);
    /* the module is already referenced, so we must free it */
    m = JS_VALUE_GET_PTR(func_val);
    JS_FreeValue(ctx, func_val);
    return m;
}

JSRuntime* qjs_create_runtime()
{
    JSRuntime* rt = JS_NewRuntime();
    js_std_set_worker_new_context_func(JS_NewCustomContext);
    js_std_init_handlers(rt);
    return rt;
}

void qjs_release_runtime(JSRuntime* rt)
{
    js_std_free_handlers(rt);
    JS_FreeRuntime(rt);
}

void qjs_set_memory_limit(JSRuntime* rt, uint32_t limit)
{
    JS_SetMemoryLimit(rt, limit);
}

JSContext* qjs_create_context(JSRuntime* rt)
{
    JSContext* ctx = JS_NewCustomContext(rt);
    if (!ctx) {
        fprintf(stderr, "qjs: cannot allocate JS context\n");
        exit(2);
    }
    return ctx;
}

void qjs_release_context(JSContext* ctx)
{
    JS_FreeContext(ctx);
}

void qjs_get_exception(JSContext* ctx, void** message, void** stackTrace)
{
    JSValue exception_val = JS_GetException(ctx);
    BOOL is_error = JS_IsError(ctx, exception_val);
    (*message) = JS_ToCString(ctx, exception_val);
    if (is_error) {
        JSValue val = JS_GetPropertyStr(ctx, exception_val, "stack");
        if (!JS_IsUndefined(val)) {
            (*stackTrace) = JS_ToCString(ctx, val);
        }
        JS_FreeValue(ctx, val);
    }

    JS_FreeValue(ctx, exception_val);
}

JSValue qjs_create_func(JSContext* ctx, JSCFunction* func, const char* name)
{
    return JS_NewCFunction(ctx, func, name, 1);
}

JSValue qjs_create_object(JSContext* ctx)
{
    return JS_NewObject(ctx);
}

JSValue qjs_create_string(JSContext* ctx, const char* value)
{
    return JS_NewString(ctx, value);
}

JSValue qjs_create_int_32(JSContext* ctx, int32_t value)
{
    return JS_NewInt32(ctx, value);
}

JSValue qjs_create_int_64(JSContext* ctx, int64_t value)
{
    return JS_NewInt64(ctx, value);
}

JSValue qjs_create_double(JSContext* ctx, double value)
{
    return JS_NewFloat64(ctx, value);
}

void qjs_set_property(JSContext* ctx, JSValue obj, const char* name, JSValue value)
{
    JS_SetPropertyStr(ctx, obj, name, value);
}

void qjs_set_global(JSContext* ctx, const char* name, JSValue value)
{
    JSValue global_obj = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global_obj, name, value);
    JS_FreeValue(ctx, global_obj);
}

const char* qjs_as_string(JSContext* ctx, JSValue value, int32_t* size)
{
    size_t len = 0;
    const char* str = JS_ToCStringLen(ctx, &len, value);

    *size = len;
    return str;
}

void qjs_free_string(JSContext* ctx, const char* str)
{
    JS_FreeCString(ctx, str);
}

void qjs_free_value(JSContext* ctx, JSValue value)
{
    JS_FreeValue(ctx, value);
}

int32_t qjs_as_int_32(JSContext* ctx, JSValue value)
{
    int32_t result;
    JS_ToInt32(ctx, &result, value);
    return result;
}

int64_t qjs_as_int_64(JSContext* ctx, JSValue value)
{
    int64_t result;
    JS_ToInt64(ctx, &result, value);
    return result;
}

double qjs_as_double(JSContext* ctx, JSValue value)
{
    double result;
    JS_ToFloat64(ctx, &result, value);
    return result;
}

int32_t qjs_is_func(JSContext* ctx, JSValue value)
{
    return JS_IsFunction(ctx, value);
}

JSValue qjs_call_func(JSContext* ctx, JSValue value, JSValue this_obj, int argc, JSValue* argv)
{
    return JS_Call(ctx, value, this_obj, argc, argv);
}

void qjs_fetch_string(JSContext* ctx, JSValue value)
{
    return JS_ToCString(ctx, value);
}

int qjs_eval(JSContext* ctx, const char* filename, const char* code, uint32_t isModule)
{
    int ret, eval_flags;
    if (isModule)
        eval_flags = JS_EVAL_TYPE_MODULE;
    else
        eval_flags = JS_EVAL_TYPE_GLOBAL;

    JSValue val = eval_buf(ctx, code, strlen(code), filename, eval_flags);
    if (JS_IsException(val)) {
        ret = -1;
    }
    else {
        ret = 0;
    }
    JS_FreeValue(ctx, val);
    return ret;
}

JSValue qjs_eval_ex(JSContext* ctx, const char* filename, const char* code, uint32_t isModule)
{
    int ret, eval_flags;
    if (isModule)
        eval_flags = JS_EVAL_TYPE_MODULE;
    else
        eval_flags = JS_EVAL_TYPE_GLOBAL;

    return eval_buf(ctx, code, strlen(code), filename, eval_flags);
}

void qjs_tick(JSContext* ctx)
{
    js_std_loop(ctx);
}

void qjs_set_module_loader(JSRuntime* rt, JSLoadModuleFunc* func)
{
    JS_SetModuleLoaderFunc(rt, NULL, js_module_loader_ext, func);
}
