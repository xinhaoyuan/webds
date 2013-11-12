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
#include "webds_launcher.hpp"

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
    fprintf(stderr, "Finished\n");
    
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


static JSValueRef
js_cb_resize_window(JSContextRef context,
                    JSObjectRef function,
                    JSObjectRef self,
                    size_t argc,
                    const JSValueRef argv[],
                    JSValueRef* exception)
{
    WebDSWebKit *wk = (WebDSWebKit *)JSObjectGetPrivate(function);
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(wk->get_webview()));
    if (toplevel != NULL &&
        argc == 3 && JSValueIsNumber(context, argv[1]) && JSValueIsNumber(context, argv[2]))
    {
        double widthF = JSValueToNumber(context, argv[0], NULL);
        double heightF = JSValueToNumber(context, argv[1], NULL);
        int width = (int)widthF;
        int height = (int)heightF;
        fprintf(stderr, "SET SIZE %d %d\n", width, height);
        gtk_widget_set_size_request(toplevel, width, height);
    }
    return JSValueMakeNull(context);
}

static JSValueRef
js_cb_hide_window(JSContextRef context,
                    JSObjectRef function,
                    JSObjectRef self,
                    size_t argc,
                    const JSValueRef argv[],
                    JSValueRef* exception)
{
    WebDSWebKit *wk = (WebDSWebKit *)JSObjectGetPrivate(function);
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(wk->get_webview()));
    if (toplevel != NULL)
    {
        gtk_widget_hide(toplevel);
    }
    return JSValueMakeNull(context);
}

static JSValueRef
js_cb_show_window(JSContextRef context,
                    JSObjectRef function,
                    JSObjectRef self,
                    size_t argc,
                    const JSValueRef argv[],
                    JSValueRef* exception)
{
    WebDSWebKit *wk = (WebDSWebKit *)JSObjectGetPrivate(function);
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(wk->get_webview()));
    if (toplevel != NULL)
    {
        gtk_widget_show(toplevel);
        gtk_window_present(GTK_WINDOW(toplevel));
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

static void get_path_uri(const string &path, string &uri) {
    /* Load home page */
    char *cwd = g_get_current_dir();
    char *abs_path = (path.c_str()[0] == '/') ?
        g_build_filename(path.c_str(), NULL) :
        g_build_filename(cwd, path.c_str(), NULL);
    char *uri_c_str = g_filename_to_uri(abs_path, NULL, NULL);

    uri = uri_c_str;

    g_free(cwd);
    g_free(abs_path);
    g_free(uri_c_str);
}

int
main(int argc, char* argv[]) {
    signal(SIGCHLD, SIG_IGN);
    
    gtk_init(&argc, &argv);

    string desktop_start_uri, launcher_start_uri;

    /* Load home page */
    get_path_uri(getenv("WEBDS_DESKTOP_PAGE"), desktop_start_uri);
    get_path_uri(getenv("WEBDS_LAUNCHER_PAGE"), launcher_start_uri);

    WebDSWebKit::add_native_func("load_plugin", js_cb_load_plugin);
    WebDSWebKit::add_native_func("show_window", js_cb_show_window);
    WebDSWebKit::add_native_func("hide_window", js_cb_hide_window);
    WebDSWebKit::add_native_func("resize_window", js_cb_resize_window);
    WebDSWebKit::add_native_func("get_env", js_cb_get_env);
    WebDSWebKit::add_native_func("exit", js_cb_exit);
    
    WebDSDesktop *desktop = new WebDSDesktop(desktop_start_uri);
    // WebDSLauncher *launcher = new WebDSLauncher(launcher_start_uri);
    
    gtk_main();
    
    return 0;
}
