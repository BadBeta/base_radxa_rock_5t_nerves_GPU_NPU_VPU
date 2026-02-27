/*
 * Cog AI Extension — WebKit Web Process Extension with Native WebSocket Bridge
 *
 * Injects overlay.js into non-localhost pages and provides a native WebSocket
 * connection to the Phoenix backend via libsoup3. This bypasses browser-level
 * security restrictions (CSP, mixed content) by handling the WebSocket
 * connection in native code rather than from the page's JavaScript context.
 *
 * Bridge API exposed to JavaScript:
 *   window.__cogAiSend(jsonString)    — send message to backend
 *   window.__cogAiConnected           — boolean connection state
 *   window.__cogAiOnMessage(json)     — set by JS, called on incoming message
 *   window.__cogAiOnConnect()         — set by JS, called on connect
 *   window.__cogAiOnDisconnect()      — set by JS, called on disconnect
 */

#include <glib.h>
#include <wpe/webkit-web-process-extension.h>
#include <libsoup/soup.h>
#include <stdio.h>
#include <string.h>

/* ========================================================================
 * Debug Logging (writes to /tmp since rootfs is read-only)
 * ======================================================================== */

static void write_debug(const char *msg)
{
    FILE *f = fopen("/tmp/cog-ai-debug.log", "a");
    if (f) {
        fprintf(f, "%s\n", msg);
        fflush(f);
        fclose(f);
    }
}

/* ========================================================================
 * State
 * ======================================================================== */

static char *overlay_js = NULL;
static gsize overlay_js_len = 0;

static SoupSession *soup_session = NULL;
static SoupWebsocketConnection *ws_conn = NULL;
static gboolean ws_connected = FALSE;

/* Current page's JSC context — updated on each document-loaded */
static JSCContext *current_ctx = NULL;

/* ========================================================================
 * Forward declarations
 * ======================================================================== */

static void connect_websocket(void);

/* ========================================================================
 * JSC Bridge: C → JavaScript (deliver messages and state to page)
 * ======================================================================== */

static void notify_js_connection_state(void)
{
    if (!current_ctx) return;

    JSCValue *global = jsc_context_get_global_object(current_ctx);

    /* Update __cogAiConnected boolean */
    JSCValue *bool_val = jsc_value_new_boolean(current_ctx, ws_connected);
    jsc_value_object_set_property(global, "__cogAiConnected", bool_val);
    g_object_unref(bool_val);

    /* Call the appropriate handler */
    const char *name = ws_connected ? "__cogAiOnConnect" : "__cogAiOnDisconnect";
    JSCValue *handler = jsc_value_object_get_property(global, name);
    if (handler && jsc_value_is_function(handler)) {
        JSCValue *result = jsc_value_function_call(handler, G_TYPE_NONE);
        g_clear_object(&result);
    }
    g_clear_object(&handler);
    g_object_unref(global);
}

static void deliver_ws_message_to_js(const char *data, gsize len)
{
    if (!current_ctx || !data || len == 0) return;

    JSCValue *global = jsc_context_get_global_object(current_ctx);
    JSCValue *handler = jsc_value_object_get_property(global, "__cogAiOnMessage");

    if (handler && jsc_value_is_function(handler)) {
        JSCValue *json_val = jsc_value_new_string(current_ctx, data);
        JSCValue *result = jsc_value_function_call(handler,
            JSC_TYPE_VALUE, json_val, G_TYPE_NONE);
        g_clear_object(&result);
        g_object_unref(json_val);
    }
    g_clear_object(&handler);
    g_object_unref(global);
}

/* ========================================================================
 * JSC Bridge: JavaScript → C (receive commands from page)
 * ======================================================================== */

static void js_send_callback(const char *json_str)
{
    if (!ws_connected || !ws_conn || !json_str) return;
    soup_websocket_connection_send_text(ws_conn, json_str);
}

static void setup_jsc_bridge(JSCContext *ctx)
{
    JSCValue *global = jsc_context_get_global_object(ctx);

    /* Register __cogAiSend(jsonString) — sends message to backend */
    JSCValue *send_fn = jsc_value_new_function(ctx, "__cogAiSend",
        G_CALLBACK(js_send_callback), NULL, NULL,
        G_TYPE_NONE, 1, G_TYPE_STRING);
    jsc_value_object_set_property(global, "__cogAiSend", send_fn);
    g_object_unref(send_fn);

    /* Set initial connection state */
    JSCValue *bool_val = jsc_value_new_boolean(ctx, ws_connected);
    jsc_value_object_set_property(global, "__cogAiConnected", bool_val);
    g_object_unref(bool_val);

    g_object_unref(global);
}

/* ========================================================================
 * Native WebSocket Client (libsoup3)
 * ======================================================================== */

static void
on_ws_message(SoupWebsocketConnection *conn,
              gint                      type,
              GBytes                   *message,
              gpointer                  user_data)
{
    if (type != SOUP_WEBSOCKET_DATA_TEXT) return;

    gsize len;
    const char *data = g_bytes_get_data(message, &len);
    deliver_ws_message_to_js(data, len);
}

static gboolean reconnect_timeout(gpointer user_data)
{
    connect_websocket();
    return G_SOURCE_REMOVE;
}

static void
on_ws_closed(SoupWebsocketConnection *conn, gpointer user_data)
{
    g_message("cog-ai-extension: WebSocket closed");
    ws_connected = FALSE;
    g_clear_object(&ws_conn);
    notify_js_connection_state();
    g_timeout_add_seconds(3, reconnect_timeout, NULL);
}

static void
on_ws_error(SoupWebsocketConnection *conn,
            GError                  *error,
            gpointer                 user_data)
{
    g_warning("cog-ai-extension: WebSocket error: %s", error->message);
}

static void
on_ws_connect_finish(GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
    GError *error = NULL;
    ws_conn = soup_session_websocket_connect_finish(
        SOUP_SESSION(source), result, &error);

    if (error) {
        char buf[256];
        snprintf(buf, sizeof(buf), "WebSocket connect FAILED: %s", error->message);
        write_debug(buf);
        g_warning("cog-ai-extension: %s", buf);
        g_error_free(error);
        ws_connected = FALSE;
        g_timeout_add_seconds(3, reconnect_timeout, NULL);
        return;
    }

    ws_connected = TRUE;
    write_debug("WebSocket CONNECTED to backend!");
    g_message("cog-ai-extension: WebSocket connected to backend!");

    g_signal_connect(ws_conn, "message", G_CALLBACK(on_ws_message), NULL);
    g_signal_connect(ws_conn, "closed", G_CALLBACK(on_ws_closed), NULL);
    g_signal_connect(ws_conn, "error", G_CALLBACK(on_ws_error), NULL);

    notify_js_connection_state();
}

static void connect_websocket(void)
{
    if (ws_conn) return;

    if (!soup_session)
        soup_session = soup_session_new();

    /* Use http:// scheme — libsoup handles the WebSocket upgrade */
    SoupMessage *msg = soup_message_new("GET", "http://127.0.0.1:80/ws/ai");
    if (!msg) {
        g_warning("cog-ai-extension: Failed to create SoupMessage");
        return;
    }

    g_message("cog-ai-extension: Connecting WebSocket to 127.0.0.1:80...");
    soup_session_websocket_connect_async(
        soup_session, msg,
        NULL,                  /* origin */
        NULL,                  /* protocols */
        G_PRIORITY_DEFAULT,
        NULL,                  /* cancellable */
        on_ws_connect_finish,
        NULL);
    g_object_unref(msg);
}

/* ========================================================================
 * Page Injection
 * ======================================================================== */

static void
on_document_loaded(WebKitWebPage *page, gpointer user_data)
{
    const gchar *uri = webkit_web_page_get_uri(page);

    /* Skip localhost — that's the Phoenix control panel */
    if (uri && g_str_has_prefix(uri, "http://localhost"))
        return;
    if (uri && g_str_has_prefix(uri, "http://127.0.0.1"))
        return;

    if (!overlay_js || overlay_js_len == 0)
        return;

    {
        char buf[512];
        snprintf(buf, sizeof(buf), "injecting overlay into: %s", uri ? uri : "(null)");
        write_debug(buf);
    }
    g_message("cog-ai-extension: injecting overlay into %s", uri ? uri : "(null)");

    WebKitFrame *frame = webkit_web_page_get_main_frame(page);
    if (!frame) return;

    JSCContext *ctx = webkit_frame_get_js_context(frame);
    if (!ctx) return;

    /* Update current context for message delivery */
    g_clear_object(&current_ctx);
    current_ctx = g_object_ref(ctx);

    /* Set up native function bridge BEFORE injecting overlay.js */
    setup_jsc_bridge(ctx);

    /* Inject overlay.js */
    JSCValue *result = jsc_context_evaluate(ctx, overlay_js, overlay_js_len);
    g_clear_object(&result);
    g_object_unref(ctx);

    g_message("cog-ai-extension: overlay injected, ws_connected=%d", ws_connected);
}

static void
on_page_created(WebKitWebProcessExtension *extension,
                WebKitWebPage             *page,
                gpointer                   user_data)
{
    guint64 page_id = webkit_web_page_get_id(page);
    g_message("cog-ai-extension: page %" G_GUINT64_FORMAT " created", page_id);

    g_signal_connect(page, "document-loaded",
                     G_CALLBACK(on_document_loaded), NULL);
}

/* ========================================================================
 * Extension Entry Point
 * ======================================================================== */

G_MODULE_EXPORT void
webkit_web_process_extension_initialize(WebKitWebProcessExtension *extension)
{
    write_debug("=== cog-ai-extension initializing ===");
    g_message("cog-ai-extension: initializing (native WebSocket bridge v2)");

    /* Load overlay.js */
    const gchar *js_path = "/usr/lib/cog-extensions/overlay.js";
    GError *error = NULL;

    if (!g_file_get_contents(js_path, &overlay_js, &overlay_js_len, &error)) {
        char buf[256];
        snprintf(buf, sizeof(buf), "ERROR: failed to load %s: %s",
                 js_path, error->message);
        write_debug(buf);
        g_warning("cog-ai-extension: %s", buf);
        g_error_free(error);
        return;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "loaded overlay.js: %zu bytes", overlay_js_len);
    write_debug(buf);

    /* Start WebSocket connection — will reconnect automatically */
    write_debug("starting WebSocket connection...");
    connect_websocket();

    g_signal_connect(extension, "page-created",
                     G_CALLBACK(on_page_created), NULL);
    write_debug("extension init complete, waiting for pages");
}
