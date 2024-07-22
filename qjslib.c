#include "qjslib.h"

#include <stdlib.h>
#include "cutils.h"
#include "quickjs-libc.h"

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

static void* resolve_module_internal(JSContext* ctx, const char* module_base_name, const char* module_name, void* opaque)
{
    return qjs_create_native_string(ctx, module_name);
}

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

JSRuntime* qjs_create_runtime()
{
    JSRuntime* rt = JS_NewRuntime();
    js_std_init_handlers(rt);
    return rt;
}

void qjs_release_runtime(JSRuntime* rt)
{
    js_std_free_handlers(rt);
    JS_FreeRuntime(rt);
}

void qjs_set_memory_limit(JSRuntime* rt, uint64_t limit)
{
    JS_SetMemoryLimit(rt, limit);
}

void qjs_set_stack_size_limit(JSRuntime* rt, uint64_t limit)
{
    JS_SetMaxStackSize(rt, limit);
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

void* qjs_create_native_string(JSContext* ctx, const char* value)
{
    size_t buffer_len = strlen(value) + 1;
    void* clone = js_malloc(ctx, buffer_len);
    memcpy(clone, value, buffer_len);
    return clone;
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

JSModuleDef* qjs_create_module(JSContext* ctx, const char* name, const char* code)
{
    JSValue func_val = JS_Eval(ctx, code, strlen(code), name,
        JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);

    if (JS_IsException(func_val))
        return NULL;

    /* XXX: could propagate the exception */
    js_module_set_import_meta(ctx, func_val, TRUE, FALSE);

    /* the module is already referenced, so we must free it */
    JSModuleDef* m = JS_VALUE_GET_PTR(func_val);
    JS_FreeValue(ctx, func_val);
    return m;
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

int qjs_tick(JSContext* ctx)
{
    return js_std_tick(ctx);
}

void qjs_set_module_loader(JSRuntime* rt, JSModuleNormalizeFunc* nameResolver, JSModuleLoaderFunc* moduleLoader)
{
    if (!nameResolver) {
        nameResolver = resolve_module_internal;
    }
    JS_SetModuleLoaderFunc(rt, nameResolver, moduleLoader, NULL);
}
