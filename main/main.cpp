#include <cstring>
#include <cstdio>
#include <cstdlib>

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <cairo.h>

#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <dlfcn.h>

#include <sstream>
#include <map>
#include <string>

#include "plugin.h"

using namespace std;

extern char **environ;

#define NATIVE_OBJECT_NAME "sys"

static char *alloc_cstring_from_jsstring(JSStringRef js_str);
static void destroy_cb(GtkWidget *widget, gpointer data);
static gboolean input_channel_in(GIOChannel *, GIOCondition, gpointer);
static int register_native_method(const char *method_name, webds_js_callback_f func);

GtkWidget     *main_window = NULL;
WebKitWebView *web_view = NULL;
GtkWidget     *container = NULL;
JSGlobalContextRef jsNativeContext;
JSObjectRef        jsNativeObject;
GIOChannel        *input_channel;

int page_loaded = 0;

webds_host_interface_s host_interface = { register_native_method };
map<string, void *> plugin_handle_map;
map<string, webds_plugin_interface_t> plugin_interface_map;

static char *
alloc_cstring_from_jsstring(JSStringRef js_str)
{
    size_t size = JSStringGetMaximumUTF8CStringSize(js_str);
    char *buf = (char *)malloc(size);
    if (buf)
        JSStringGetUTF8CString(js_str, buf, size);
    return buf;
}

static int
load_plugin(const char *plugin_name, const char *plugin_file_name)
{
    webds_plugin_init_f _init_f;
    webds_plugin_interface_t interface;
    void *handle;

    fprintf(stderr, "Load plugin so file: %s\n", plugin_file_name);

    if (plugin_handle_map.find(plugin_name) != plugin_handle_map.end())
    {
        fprintf(stderr, "Failed, already loaded\n");
        goto failed;
    }
        
    dlerror();
    handle = dlopen(plugin_file_name, RTLD_LAZY);
    if (handle == NULL)
    {
        fprintf(stderr, "Failed, %s\n", dlerror());
        goto failed;
    }

    _init_f = (webds_plugin_init_f)dlsym(handle, "init");
    if (_init_f == NULL)
    {
        fprintf(stderr, "Failed, cannot find init func\n");
        dlclose(handle);
        goto failed;
    }

    interface = _init_f(&host_interface);
    if (interface == NULL)
    {
        fprintf(stderr, "Failed, returned NULL interface\n");
        dlclose(handle);
        goto failed;
    }
        
    plugin_handle_map[plugin_name] = handle;
    plugin_interface_map[plugin_name] = interface;

    fprintf(stderr, "Successed\n");

    return 0;

  failed:
    return -1;
}

static void destroy_cb(GtkWidget *widget, gpointer data) { main_window = NULL; gtk_main_quit(); }

static gboolean debug_cb(GObject *obj, gpointer data) { fprintf(stderr, "!!!\n"); return FALSE; }

static void main_window_focus_cb(GObject *obj, gpointer data) {
    if (gtk_window_has_toplevel_focus(GTK_WINDOW(main_window)))
    {
        gtk_widget_grab_focus(GTK_WIDGET(web_view));
    }
}

static void
load_status_cb(WebKitWebFrame *frame,
               gboolean arg1,
               gpointer data)
{
    if (webkit_web_frame_get_load_status(frame) == WEBKIT_LOAD_COMMITTED)
    {
        JSStringRef name;
        name = JSStringCreateWithUTF8CString(NATIVE_OBJECT_NAME);
        JSGlobalContextRef js_global_context =
            webkit_web_frame_get_global_context(frame);
        
        JSObjectSetProperty(js_global_context,
                            JSContextGetGlobalObject(js_global_context),
                            name,
                            *(JSObjectRef *)data,
                            0, NULL);
        JSStringRelease(name);
    }
    else if (webkit_web_frame_get_load_status(frame) == WEBKIT_LOAD_FINISHED)
    {
        /* Load finished */
        if (page_loaded == 0)
        {
            page_loaded = 1;
            // g_io_add_watch(input_channel, G_IO_IN, input_channel_in, NULL);
        }
    }
}

static gboolean
navigation_cb(WebKitWebView             *web_view,
              WebKitWebFrame            *frame,
              WebKitNetworkRequest      *request,
              WebKitWebNavigationAction *navigation_action,
              WebKitWebPolicyDecision   *policy_decision,
              gpointer                   user_data)
{
    if (webkit_web_frame_get_parent(frame) == NULL)
    {
        fprintf(stderr, "request to %s\n", webkit_network_request_get_uri(request));
        if (page_loaded)
            /* always ignore */
            webkit_web_policy_decision_ignore(policy_decision);
        else webkit_web_policy_decision_use(policy_decision);
        return TRUE;
    }
    else return FALSE;
}

gboolean console_message_cb(WebKitWebView *web_view,
                            gchar         *message,
                            gint           line,
                            gchar         *source_id,
                            gpointer       user_data)
{
    fprintf(stderr, "WebKit JS Console[%s:%d]:\n  %s\n", source_id, line, message);
    return TRUE;
}

gboolean context_menu_cb(WebKitWebView       *web_view,
                         GtkWidget           *default_menu,
                         WebKitHitTestResult *hit_test_result,
                         gboolean             triggered_with_keyboard,
                         gpointer             user_data)
{ return TRUE; }

static int
register_native_method(const char *method_name, webds_js_callback_f func)
{
    JSStringRef name = JSStringCreateWithUTF8CString(method_name);
    JSObjectSetProperty(jsNativeContext,
                        jsNativeObject,
                        name,
                        JSObjectMakeFunctionWithCallback(
                            jsNativeContext, NULL,
                            func),
                        0, NULL);
    JSStringRelease(name);
}


static JSValueRef
js_cb_debug_print(JSContextRef context,
                  JSObjectRef function,
                  JSObjectRef self,
                  size_t argc,
                  const JSValueRef argv[],
                  JSValueRef* exception)
{
    if (argc == 1 && JSValueIsString(context, argv[0]))
    {
        JSStringRef string = JSValueToStringCopy(context, argv[0], NULL);
        
        static const int MSG_BUF_SIZE = 4096;
        char msg_buf[MSG_BUF_SIZE];
        
        JSStringGetUTF8CString(string, msg_buf, MSG_BUF_SIZE);
        fprintf(stderr, "[DEBUG]:%s\n", msg_buf);
        
        JSStringRelease(string);
    }
    return JSValueMakeNull(context);
}

static JSValueRef
js_cb_get_env(JSContextRef context,
              JSObjectRef function,
              JSObjectRef self,
              size_t argc,
              const JSValueRef argv[],
              JSValueRef* exception)
{
    if (argc == 1 && JSValueIsString(context, argv[0]))
    {
        JSStringRef string = JSValueToStringCopy(context, argv[0], NULL);
        
        static const int NAME_BUF_SIZE = 4096;
        char name_buf[NAME_BUF_SIZE];
        
        JSStringGetUTF8CString(string, name_buf, NAME_BUF_SIZE);
        char *env = getenv(name_buf);
        JSStringRelease(string);
        
        JSStringRef str = JSStringCreateWithUTF8CString(env);
        JSValueRef ret = JSValueMakeString(context, str);
        JSStringRelease(str);

        return ret;
    } else return JSValueMakeNull(context);
}


static JSValueRef
js_cb_resize_window(JSContextRef context,
                    JSObjectRef function,
                    JSObjectRef self,
                    size_t argc,
                    const JSValueRef argv[],
                    JSValueRef* exception)
{
    if (argc == 2 && JSValueIsNumber(context, argv[0]) && JSValueIsNumber(context, argv[1]))
    {
        double widthF = JSValueToNumber(context, argv[0], NULL);
        double heightF = JSValueToNumber(context, argv[1], NULL);
        int width = (int)widthF;
        int height = (int)heightF;
        fprintf(stderr, "SET SIZE %d %d\n", width, height);
        gtk_widget_set_size_request(GTK_WIDGET(container), width, height);
    }
    return JSValueMakeNull(context);
}

// (plugin_name, plugin_file_name)
static JSValueRef
js_cb_load_plugin(JSContextRef context,
                  JSObjectRef function,
                  JSObjectRef self,
                  size_t argc,
                  const JSValueRef argv[],
                  JSValueRef* exception)
{
    if (argc == 2 && JSValueIsString(context, argv[0]) && JSValueIsString(context, argv[1]))
    {
        JSStringRef string;
        string = JSValueToStringCopy(context, argv[0], NULL);
        char *plugin_name = alloc_cstring_from_jsstring(string);
        JSStringRelease(string);

        string = JSValueToStringCopy(context, argv[1], NULL);
        char *plugin_file_name = alloc_cstring_from_jsstring(string);
        JSStringRelease(string);
        
        // Try to load the plugin
        bool r = load_plugin(plugin_name, plugin_file_name) == 0;

        free(plugin_name);
        free(plugin_file_name);
        return JSValueMakeBoolean(context, r);
    }
    return JSValueMakeBoolean(context, false);
}

static JSValueRef
js_cb_get_desktop_focus(JSContextRef context,
                        JSObjectRef function,
                        JSObjectRef self,
                        size_t argc,
                        const JSValueRef argv[],
                        JSValueRef* exception)
{
    gtk_window_present(GTK_WINDOW(main_window));
    return JSValueMakeNull(context);
}

static JSValueRef
js_cb_toggle_dock(JSContextRef context,
                  JSObjectRef function,
                  JSObjectRef self,
                  size_t argc,
                  const JSValueRef argv[],
                  JSValueRef* exception)
{
    if (gtk_window_get_type_hint(GTK_WINDOW(main_window)) == GDK_WINDOW_TYPE_HINT_DOCK)
        gtk_window_set_type_hint(GTK_WINDOW(main_window), GDK_WINDOW_TYPE_HINT_DESKTOP);
    gtk_window_set_type_hint(GTK_WINDOW(main_window), GDK_WINDOW_TYPE_HINT_DOCK);
    return JSValueMakeNull(context);
}

static JSValueRef
js_cb_exit(JSContextRef context,
           JSObjectRef function,
           JSObjectRef self,
           size_t argc,
           const JSValueRef argv[],
           JSValueRef* exception)
{
    gtk_main_quit();   
    return JSValueMakeNull(context);
}

static void sendWKEvent(char *line)
{
    WebKitDOMDocument *dom = webkit_web_view_get_dom_document(web_view);
    WebKitDOMHTMLElement *ele = webkit_dom_document_get_body(dom);
    if (ele)
    {
        WebKitDOMEvent  *event = webkit_dom_document_create_event(dom, "CustomEvent", NULL);
        webkit_dom_event_init_event(event, "sysWakeup", FALSE, TRUE); // The custom event should be canceled by the handler
        webkit_dom_node_dispatch_event(WEBKIT_DOM_NODE(ele), event, NULL);
    }
}

static gboolean
input_channel_in(GIOChannel *c, GIOCondition cond, gpointer data)
{
    gchar *line_buf;
    gsize  line_len;
    gsize  line_term;

    GIOStatus status = g_io_channel_read_line(c, &line_buf, &line_len, &line_term, NULL);
    
    if (status == G_IO_STATUS_NORMAL)
    {
        sendWKEvent(line_buf);
        g_free(line_buf);
    }
    else if (status == G_IO_STATUS_EOF)
    {
        fprintf(stderr, "input closed\n");
    }
    
    return status == G_IO_STATUS_EOF ? FALSE : TRUE;
}
    
int
main(int argc, char* argv[]) {
    signal(SIGCHLD, SIG_IGN);
    
    gtk_init(&argc, &argv);

    GdkScreen *screen = gdk_screen_get_default();
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    gint p_monitor    = gdk_screen_get_primary_monitor(screen);
    GdkRectangle geo;

    gdk_screen_get_monitor_workarea(screen, p_monitor, &geo);

    // Create a Window, set visual to RGBA
    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_type_hint(GTK_WINDOW(main_window), GDK_WINDOW_TYPE_HINT_DESKTOP);
    gtk_window_set_resizable(GTK_WINDOW(main_window), FALSE);
    gtk_widget_set_can_focus(GTK_WIDGET(main_window), TRUE);
    gtk_window_set_decorated(GTK_WINDOW(main_window), FALSE);
    gtk_window_set_position(GTK_WINDOW(main_window), GTK_WIN_POS_NONE);
    gtk_widget_set_size_request(main_window, geo.width, geo.height);
    gtk_window_move(GTK_WINDOW(main_window), geo.x, geo.y);
    
    if (visual == NULL || !gtk_widget_is_composited(main_window))
    {
        fprintf(stderr, "Your screen does not support alpha window, :(\n");
        visual = gdk_screen_get_system_visual(screen);
    }

    gtk_widget_set_visual(GTK_WIDGET(main_window), visual);
    /* Set transparent window background */
    GdkRGBA bg = {0,0,0,0};
    gtk_widget_override_background_color(GTK_WIDGET(main_window), GTK_STATE_FLAG_NORMAL, &bg);
    g_signal_connect(G_OBJECT(main_window), "destroy", G_CALLBACK(destroy_cb), NULL);

    /* Create a WebView, set it transparent, add it to the window */
    web_view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    gtk_container_add(GTK_CONTAINER(main_window), GTK_WIDGET(web_view));

    webkit_web_view_set_transparent(web_view, TRUE);
    WebKitWebSettings *wvsetting = webkit_web_view_get_settings(web_view);
    g_object_set (G_OBJECT(wvsetting), "enable-default-context-menu", FALSE, NULL);

    /* Create the native object with local callback properties */    
    JSGlobalContextRef js_global_context =
        webkit_web_frame_get_global_context(webkit_web_view_get_main_frame(web_view));

    JSStringRef native_object_name;
    jsNativeContext = JSGlobalContextCreateInGroup(
        JSContextGetGroup(
            webkit_web_frame_get_global_context(
                webkit_web_view_get_main_frame(web_view))),
        NULL);
    jsNativeObject = JSObjectMake(jsNativeContext, NULL, NULL);
    native_object_name = JSStringCreateWithUTF8CString(NATIVE_OBJECT_NAME);
    JSObjectSetProperty(jsNativeContext,
                        JSContextGetGlobalObject(jsNativeContext),
                        native_object_name,
                        jsNativeObject,
                        0, NULL);
    JSStringRelease(native_object_name);

    g_signal_connect(webkit_web_view_get_main_frame(web_view), "notify::load-status",
                     G_CALLBACK(load_status_cb), &jsNativeObject);
    g_signal_connect(web_view, "navigation-policy-decision-requested",
                     G_CALLBACK(navigation_cb), NULL);
    g_signal_connect(web_view, "console-message", G_CALLBACK(console_message_cb), NULL);
    g_signal_connect(web_view, "context-menu", G_CALLBACK(context_menu_cb), NULL);
    g_signal_connect(main_window, "notify::is-active", G_CALLBACK(main_window_focus_cb), NULL);
    
    /* Show it and continue running until the window closes */
    gtk_widget_grab_focus(GTK_WIDGET(web_view));

    register_native_method("DebugPrint", js_cb_debug_print);
    register_native_method("GetEnv", js_cb_get_env);
    register_native_method("GetDesktopFocus", js_cb_get_desktop_focus);
    register_native_method("ToggleDock", js_cb_toggle_dock);
    register_native_method("LoadPlugin", js_cb_load_plugin);
    register_native_method("Exit", js_cb_exit);

    /* Load home page */
    char __empty[] = "";
    char *home = getenv("WEBDS_HOME");
    if (home == NULL) home = __empty;
    char *cwd = g_get_current_dir();
    char *path = home[0] == '/' ? home : g_build_filename(cwd, getenv("WEBDS_HOME"), NULL);
    char *start = g_filename_to_uri(path, NULL, NULL);

    webkit_web_view_load_uri(web_view, start);

    g_free(cwd);
    if (path != home) g_free(path);
    g_free(start);

    gtk_widget_show(GTK_WIDGET(web_view));
    gtk_widget_set_visual(GTK_WIDGET(web_view), visual);
    gtk_widget_override_background_color(GTK_WIDGET(web_view), GTK_STATE_FLAG_NORMAL, &bg);
    
    gtk_widget_show(main_window);
    gtk_main();
    
    return 0;
}
