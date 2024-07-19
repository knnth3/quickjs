#include <stdio.h>
#include <stdlib.h>
#include <qjslib.h>

static char* read_file(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        // handle error, e.g., file not found
        return NULL;
    }

    // Seek to the end of the file to determine its size
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);

    // Allocate a buffer large enough to hold the file's contents
    char* buffer = malloc(size + 1);
    if (buffer == NULL) {
        // handle error, e.g., out of memory
        fclose(file);
        return NULL;
    }

    // Seek back to the beginning of the file and read its contents
    fseek(file, 0, SEEK_SET);
    fread(buffer, 1, size, file);

    // Null-terminate the buffer
    buffer[size] = '\0';

    fclose(file);
    return buffer;
}

static JSValue js_log(JSContext* ctx, JSValue this_val, int argc, JSValue* argv)
{
    int i;
    const char* str;
    size_t len;

    for (i = 0; i < argc; i++) {
        if (i != 0)
            putchar(' ');
        str = JS_ToCStringLen(ctx, &len, argv[i]);
        if (!str)
            return JS_EXCEPTION;
        fwrite(str, 1, len, stdout);
        JS_FreeCString(ctx, str);
    }
    putchar('\n');
    fflush(stdout);

    JSValue result = JS_UNDEFINED;
    return result;
}

static void module_loader(JSContext* ctx, const char* name, NativeBuffer* output)
{
    printf("Loading module: %s\n", name);
    if (strcmp(name, "examples/fib_module.js") == 0)
    {
        const char* contents = read_file("./examples/fib_module.js");
        output->data = contents;
        return;
    }
}

int main(int argc, char** argv)
{
    JSRuntime* rt = qjs_create_runtime();
    JSContext* ctx = qjs_create_context(rt);
    qjs_set_module_loader(rt, module_loader);

    JSValue console = qjs_create_object(ctx);
    JSValue log = qjs_create_func(ctx, js_log, "log_ver_0");
    qjs_set_property(ctx, console, "log", log);
    qjs_set_global(ctx, "console", console);

    const char* os = "import * as std from 'std';\n"
        "import * as os from 'os';\n"
        "globalThis.std = std;\n"
        "globalThis.os = os;\n"
        "globalThis.setTimeout = os.setTimeout;\n";
    qjs_eval(ctx, "<os>", os, 1);

    const char* script = ""
        "const ENV = require('environment');"
        "console.log('Starting Timeout...');";
    int result = qjs_eval(ctx, "<main>", script, 0);

    if (result != 0)
    {
        const char* message = 0;
        const char* stackTrace = 0;
        qjs_get_exception(ctx, &message, &stackTrace);
        printf("extracted exception:\n%s\n%s", message, stackTrace);
        qjs_free_string(ctx, message);
        qjs_free_string(ctx, stackTrace);
    }

    qjs_tick(ctx);
    qjs_release_context(ctx);
    qjs_release_runtime(rt);
    return 0;
}
