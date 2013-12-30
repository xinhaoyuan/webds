#ifndef __WEBDS_WEBKIT_HPP__
#define __WEBDS_WEBKIT_HPP__

#include <string>
#include <map>

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include "plugin.h"

#define NATIVE_OBJECT_NAME  "sys"
#define NATIVE_FUNC_NAME    "call"
#define NATIVE_EVENT_NAME "sys_event"

class WebDSWebKit {
private:
    WebKitWebView     *_webview;
    JSGlobalContextRef _js_native_context;
    JSClassRef         _js_native_class;
    JSObjectRef        _js_native_object;
    JSObjectRef        _js_native_func;
    bool               _page_loaded;

    typedef std::map<std::string,
                     JSObjectCallAsFunctionCallback> native_func_map_t;
    static void _native_func_map_lock();
    static void _native_func_map_unlock();
    static native_func_map_t _native_func_map;
    static JSValueRef
    _js_native_call_entry(JSContextRef context,
                          JSObjectRef function,
                          JSObjectRef self,
                          size_t argc,
                          const JSValueRef argv[],
                          JSValueRef* exception);

    static void _window_focus_cb(GObject    *gobject,
                                 GParamSpec *pspec,
                                 gpointer    data);
    static gboolean _context_menu_cb(WebKitWebView       *web_view,
                                     GtkWidget           *default_menu,
                                     WebKitHitTestResult *hit_test_result,
                                     gboolean             triggered_with_keyboard,
                                     gpointer             user_data);
    static gboolean _console_message_cb(WebKitWebView *web_view,
                                        gchar         *message,
                                        gint           line,
                                        gchar         *source_id,
                                        gpointer       user_data);
    static gboolean _navigation_cb(WebKitWebView             *web_view,
                                   WebKitWebFrame            *frame,
                                   WebKitNetworkRequest      *request,
                                   WebKitWebNavigationAction *navigation_action,
                                   WebKitWebPolicyDecision   *policy_decision,
                                   gpointer                   user_data);
    static void _load_status_cb(WebKitWebFrame *frame,
                                gboolean arg1,
                                gpointer data);
    
public:

    static int add_native_func(const std::string &name, JSObjectCallAsFunctionCallback func);
    WebKitWebView *get_webview() const;
    int send_event(const std::string &data);
    
    WebDSWebKit();
    ~WebDSWebKit();
};

#endif
