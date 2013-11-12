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
#include "util.hpp"
#include "webds_webkit.hpp"
#include "webds_desktop.hpp"

using namespace std;

extern char **environ;

#define NATIVE_OBJECT_NAME "sys"

static int register_native_method(const char *method_name, JSObjectCallAsFunctionCallback func);
webds_host_interface_s host_interface = { register_native_method };

G_LOCK_DEFINE_STATIC(plugin_lock);
static map<string, void *> plugin_handle_map;
static map<string, webds_plugin_interface_t> plugin_interface_map;

static int
load_plugin(const char *plugin_name, const char *plugin_file_name)
{
    webds_plugin_init_f _init_f;
    webds_plugin_interface_t interface;
    void *handle;

    fprintf(stderr, "Load plugin so file: %s\n", plugin_file_name);

    dlerror();
    handle = dlopen(plugin_file_name, RTLD_LAZY);
    if (handle == NULL)
    {
        fprintf(stderr, "Failed, %s\n", dlerror());
        goto failed;
    }

    G_LOCK(plugin_lock);
    if (plugin_handle_map.find(plugin_name) != plugin_handle_map.end())
    {
        G_UNLOCK(plugin_lock);
        fprintf(stderr, "Failed, already loaded\n");
        goto failed;
    }
    plugin_handle_map[plugin_name] = handle;
    G_UNLOCK(plugin_lock);
        
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

    // no need to lock here
    plugin_interface_map[plugin_name] = interface;
    fprintf(stderr, "Successed\n");
    return 0;

  failed:
    return -1;
}

static int
register_native_method(const char *method_name, JSObjectCallAsFunctionCallback func)
{ return WebDSWebKit::add_native_func(method_name, func); }


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
    if (argc == 2 && JSValueIsString(context, argv[1]))
    {
        string name;
        copy_from_jsstring_object(context, argv[1], name);
        char *env = getenv(name.c_str());

        JSStringRef str = JSStringCreateWithUTF8CString(env);
        JSValueRef ret = JSValueMakeString(context, str);
        JSStringRelease(str);

        return ret;
    } else return JSValueMakeNull(context);
}


// static JSValueRef
// js_cb_resize_window(JSContextRef context,
//                     JSObjectRef function,
//                     JSObjectRef self,
//                     size_t argc,
//                     const JSValueRef argv[],
//                     JSValueRef* exception)
// {
//     if (argc == 2 && JSValueIsNumber(context, argv[0]) && JSValueIsNumber(context, argv[1]))
//     {
//         double widthF = JSValueToNumber(context, argv[0], NULL);
//         double heightF = JSValueToNumber(context, argv[1], NULL);
//         int width = (int)widthF;
//         int height = (int)heightF;
//         fprintf(stderr, "SET SIZE %d %d\n", width, height);
//         gtk_widget_set_size_request(GTK_WIDGET(container), width, height);
//     }
//     return JSValueMakeNull(context);
// }

// (plugin_name, plugin_file_name)
static JSValueRef
js_cb_load_plugin(JSContextRef context,
                  JSObjectRef function,
                  JSObjectRef self,
                  size_t argc,
                  const JSValueRef argv[],
                  JSValueRef* exception)
{
    if (argc == 3 && JSValueIsString(context, argv[1]) && JSValueIsString(context, argv[2]))
    {
        string plugin_name, plugin_file_name;
        copy_from_jsstring_object(context, argv[1], plugin_name);
        copy_from_jsstring_object(context, argv[2], plugin_file_name);

        // Try to load the plugin
        bool r = load_plugin(plugin_name.c_str(), plugin_file_name.c_str()) == 0;
        return JSValueMakeBoolean(context, r);
    }
    return JSValueMakeBoolean(context, false);
}

// static JSValueRef
// js_cb_get_desktop_focus(JSContextRef context,
//                         JSObjectRef function,
//                         JSObjectRef self,
//                         size_t argc,
//                         const JSValueRef argv[],
//                         JSValueRef* exception)
// {
//     gtk_window_present(GTK_WINDOW(main_window));
//     return JSValueMakeNull(context);
// }

// static JSValueRef
// js_cb_toggle_dock(JSContextRef context,
//                   JSObjectRef function,
//                   JSObjectRef self,
//                   size_t argc,
//                   const JSValueRef argv[],
//                   JSValueRef* exception)
// {
//     if (gtk_window_get_type_hint(GTK_WINDOW(main_window)) == GDK_WINDOW_TYPE_HINT_DOCK)
//         gtk_window_set_type_hint(GTK_WINDOW(main_window), GDK_WINDOW_TYPE_HINT_DESKTOP);
//     gtk_window_set_type_hint(GTK_WINDOW(main_window), GDK_WINDOW_TYPE_HINT_DOCK);
//     return JSValueMakeNull(context);
// }

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

// static void sendWKEvent(char *line)
// {
//     WebKitDOMDocument *dom = webkit_web_view_get_dom_document(web_view);
//     WebKitDOMHTMLElement *ele = webkit_dom_document_get_body(dom);
//     if (ele)
//     {
//         WebKitDOMEvent  *event = webkit_dom_document_create_event(dom, "CustomEvent", NULL);
//         webkit_dom_event_init_event(event, "sysWakeup", FALSE, TRUE); // The custom event should be canceled by the handler
//         webkit_dom_node_dispatch_event(WEBKIT_DOM_NODE(ele), event, NULL);
//     }
// }

// static gboolean
// input_channel_in(GIOChannel *c, GIOCondition cond, gpointer data)
// {
//     gchar *line_buf;
//     gsize  line_len;
//     gsize  line_term;

//     GIOStatus status = g_io_channel_read_line(c, &line_buf, &line_len, &line_term, NULL);
    
//     if (status == G_IO_STATUS_NORMAL)
//     {
//         sendWKEvent(line_buf);
//         g_free(line_buf);
//     }
//     else if (status == G_IO_STATUS_EOF)
//     {
//         fprintf(stderr, "input closed\n");
//     }
    
//     return status == G_IO_STATUS_EOF ? FALSE : TRUE;
// }
    
int
main(int argc, char* argv[]) {
    signal(SIGCHLD, SIG_IGN);
    
    gtk_init(&argc, &argv);

    string start_uri;

    /* Load home page */
    char __empty[] = "";
    char *home = getenv("WEBDS_HOME");
    if (home == NULL) home = __empty;
    char *cwd = g_get_current_dir();
    char *path = home[0] == '/' ? home : g_build_filename(cwd, getenv("WEBDS_HOME"), NULL);
    char *start = g_filename_to_uri(path, NULL, NULL);

    start_uri = start;

    g_free(cwd);
    if (path != home) g_free(path);
    g_free(start);

    WebDSWebKit::add_native_func("load_plugin", js_cb_load_plugin);
    WebDSWebKit::add_native_func("get_env", js_cb_get_env);
    WebDSWebKit::add_native_func("exit", js_cb_exit);
    
    WebDSDesktop *desktop = new WebDSDesktop(start_uri);
    
    gtk_main();
    
    return 0;
}
