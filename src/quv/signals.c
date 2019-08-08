/*
 * QuickJS libuv bindings
 * 
 * Copyright (c) 2019-present Saúl Ibarra Corretgé <s@saghul.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "../cutils.h"
#include "../list.h"
#include "signals.h"
#include "utils.h"


typedef struct {
    struct list_head link;
    uv_signal_t handle;
    int sig_num;
    JSValue func;
    JSContext *ctx;
} JSUVSignalHandler;

static JSUVSignalHandler *find_sh(JSContext *ctx, int sig_num)
{
    JSUVSignalHandler *sh;
    quv_state_t *quv_state;
    struct list_head *el;
    quv_state = JS_GetContextOpaque(ctx);
    list_for_each(el, &quv_state->signal_handlers) {
        sh = list_entry(el, JSUVSignalHandler, link);
        if (sh->sig_num == sig_num)
            return sh;
    }
    return NULL;
}

static void uv__signal_close(uv_handle_t *handle) {
    JSUVSignalHandler *sh = handle->data;
    if (sh) {
        JSContext *ctx = sh->ctx;
        JS_FreeValue(ctx, sh->func);
        js_free(ctx, sh);
    }
}

static void uv__signal_cb(uv_signal_t *handle, int sig_num) {
    JSUVSignalHandler *sh = handle->data;
    if (sh) {
        JSContext *ctx = sh->ctx;
        js_uv_call_handler(ctx, sh->func);
    }
}

static JSValue js_uv_signal(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv)
{
    JSUVSignalHandler *sh;
    uint32_t sig_num;
    JSValueConst func;
    quv_state_t *quv_state;

    quv_state = JS_GetContextOpaque(ctx);
    if (!quv_state) {
        return JS_ThrowInternalError(ctx, "couldn't find uv state");
    }

    if (JS_ToUint32(ctx, &sig_num, argv[0]))
        return JS_EXCEPTION;
    if (sig_num >= 64)
        return JS_ThrowRangeError(ctx, "invalid signal number");
    func = argv[1];
    if (JS_IsNull(func) || JS_IsUndefined(func)) {
        sh = find_sh(ctx, sig_num);
        if (sh) {
            list_del(&sh->link);
            uv_close((uv_handle_t*)&sh->handle, uv__signal_close);
        }
    } else {
        if (!JS_IsFunction(ctx, func))
            return JS_ThrowTypeError(ctx, "not a function");
        sh = find_sh(ctx, sig_num);
        if (!sh) {
            sh = js_mallocz(ctx, sizeof(*sh));
            if (!sh)
                return JS_EXCEPTION;
            sh->ctx = ctx;
            uv_signal_init(&quv_state->uvloop, &sh->handle);
            sh->handle.data = sh;
            sh->sig_num = sig_num;
            list_add_tail(&sh->link, &quv_state->signal_handlers);
        }
        JS_FreeValue(ctx, sh->func);
        sh->func = JS_DupValue(ctx, func);
        uv_signal_start(&sh->handle, uv__signal_cb, sig_num);
        uv_unref((uv_handle_t*)&sh->handle);
    }
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_uv_signal_funcs[] = {
    JS_CFUNC_DEF("signal", 2, js_uv_signal ),
};

void js_uv_mod_signals_init(JSContext *ctx, JSModuleDef *m) {
    JS_SetModuleExportList(ctx, m, js_uv_signal_funcs, countof(js_uv_signal_funcs));
}

void js_uv_mod_signals_export(JSContext *ctx, JSModuleDef *m) {
    JS_AddModuleExportList(ctx, m, js_uv_signal_funcs, countof(js_uv_signal_funcs));
}
