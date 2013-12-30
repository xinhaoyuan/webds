#include <util.hpp>
#include <webds_webkit.hpp>

using namespace std;

WebDSWebKit::WebDSWebKit() {
    _webview = WEBKIT_WEB_VIEW(webkit_web_view_new());
    _page_loaded = false;

    webkit_web_view_set_transparent(_webview, TRUE);
    WebKitWebSettings *wvsetting = webkit_web_view_get_settings(_webview);
    g_object_set(G_OBJECT(wvsetting), "enable-default-context-menu", FALSE, NULL);

    /* Create the native object with local callback properties */
    JSGlobalContextRef js_global_context =
        webkit_web_frame_get_global_context(webkit_web_view_get_main_frame(_webview));

    JSStringRef native_func_name;
    JSStringRef native_object_name;
    native_object_name = JSStringCreateWithUTF8CString(NATIVE_OBJECT_NAME);
    native_func_name = JSStringCreateWithUTF8CString(NATIVE_FUNC_NAME);

    _js_native_context = JSGlobalContextCreateInGroup(JSContextGetGroup(js_global_context), NULL);
    _js_native_class = JSClassCreate(&kJSClassDefinitionEmpty);
    _js_native_object = JSObjectMake(_js_native_context, _js_native_class, this);
    _js_native_func = JSObjectMakeFunctionWithCallback(_js_native_context, NULL, _js_native_call_entry);
    JSObjectSetProperty(_js_native_context,
                        _js_native_object,
                        native_func_name,
                        _js_native_func,
                        0, NULL);
    JSObjectSetProperty(_js_native_context,
                        JSContextGetGlobalObject(_js_native_context),
                        native_object_name,
                        _js_native_object,
                        0, NULL);
    JSStringRelease(native_func_name);
    JSStringRelease(native_object_name);

    g_signal_connect(webkit_web_view_get_main_frame(_webview), "notify::load-status",
                     G_CALLBACK(_load_status_cb), this);
    g_signal_connect(_webview, "navigation-policy-decision-requested",
                     G_CALLBACK(_navigation_cb), this);
    g_signal_connect(_webview, "console-message", G_CALLBACK(_console_message_cb), this);
    g_signal_connect(_webview, "context-menu", G_CALLBACK(_context_menu_cb), this);
    // g_signal_connect(_window, "notify::is-active", G_CALLBACK(_window_focus_cb), this);

    gtk_widget_show(GTK_WIDGET(_webview));
    gtk_widget_grab_focus(GTK_WIDGET(_webview));
}

void
WebDSWebKit::_load_status_cb(WebKitWebFrame *frame,
                             gboolean arg1,
                             gpointer data)
{
    WebDSWebKit *_this = (WebDSWebKit *)data;
    if (webkit_web_frame_get_load_status(frame) == WEBKIT_LOAD_COMMITTED)
    {
        JSStringRef name;
        name = JSStringCreateWithUTF8CString(NATIVE_OBJECT_NAME);
        _this->_js_native_context = webkit_web_frame_get_global_context(frame);
        
        JSObjectSetProperty(_this->_js_native_context,
                            JSContextGetGlobalObject(_this->_js_native_context),
                            name,
                            _this->_js_native_object,
                            0, NULL);
        JSStringRelease(name);
    }
    else if (webkit_web_frame_get_load_status(frame) == WEBKIT_LOAD_FINISHED)
    {
        /* Load finished */
        if (!_this->_page_loaded)
        {
            _this->_page_loaded = true;
            // XXX
            // g_io_add_watch(input_channel, G_IO_IN, input_channel_in, NULL);
        }
    }
}


WebDSWebKit::~WebDSWebKit() {
    // XXX
}

WebDSWebKit::native_func_map_t WebDSWebKit::_native_func_map;


G_LOCK_DEFINE_STATIC(_native_func_map_lock);
void WebDSWebKit::_native_func_map_lock() { G_LOCK(_native_func_map_lock) ;}
void WebDSWebKit::_native_func_map_unlock() { G_UNLOCK(_native_func_map_lock); }

JSValueRef
WebDSWebKit::_js_native_call_entry(JSContextRef context,
                                   JSObjectRef function,
                                   JSObjectRef self,
                                   size_t argc,
                                   const JSValueRef argv[],
                                   JSValueRef* exception) {
    // no need to get ``this'' object now
    // WebDSWebKit *_this = (WebDSWebKit *)JSObjectGetPrivate(self);
    
    if (argc < 1 || !JSValueIsString(context, argv[0])) return JSValueMakeNull(context);
    string func_name;
    copy_from_jsstring_object(context, argv[0], func_name);

    JSObjectCallAsFunctionCallback cb;
    _native_func_map_lock();
    native_func_map_t::iterator it = _native_func_map.find(func_name);
    if (it == _native_func_map.end())
        cb = NULL;
    else cb = it->second;
    _native_func_map_unlock();

    if (cb == NULL) return JSValueMakeNull(context);
    else return cb(context, function, self, argc, argv, exception);
}

int WebDSWebKit::add_native_func(const std::string &name,
                                     JSObjectCallAsFunctionCallback func) {
    int ret = 0;
    
    _native_func_map_lock();
    if (_native_func_map.find(name) == _native_func_map.end())
        _native_func_map[name] = func;
    else ret = 1;
    _native_func_map_unlock();

    return ret;
}

WebKitWebView *WebDSWebKit::get_webview() const {
    return _webview;
}

gboolean
WebDSWebKit::_navigation_cb(WebKitWebView                *web_view,
                               WebKitWebFrame            *frame,
                               WebKitNetworkRequest      *request,
                               WebKitWebNavigationAction *navigation_action,
                               WebKitWebPolicyDecision   *policy_decision,
                               gpointer                   data)
{
    WebDSWebKit *_this = (WebDSWebKit *)data;
    if (webkit_web_frame_get_parent(frame) == NULL)
    {
        fprintf(stderr, "request to %s\n", webkit_network_request_get_uri(request));
        if (_this->_page_loaded)
            /* always ignore */
            webkit_web_policy_decision_ignore(policy_decision);
        else webkit_web_policy_decision_use(policy_decision);
        return TRUE;
    }
    else return FALSE;
}

gboolean WebDSWebKit::_console_message_cb(WebKitWebView *web_view,
                                          gchar         *message,
                                          gint           line,
                                          gchar         *source_id,
                                          gpointer       user_data)
{
    fprintf(stderr, "WebKit JS Console[%s:%d]:\n  %s\n", source_id, line, message);
    return TRUE;
}

gboolean WebDSWebKit::_context_menu_cb(WebKitWebView       *web_view,
                                       GtkWidget           *default_menu,
                                       WebKitHitTestResult *hit_test_result,
                                       gboolean             triggered_with_keyboard,
                                       gpointer             user_data)
{ return TRUE; }

int WebDSWebKit::send_event(const string &data) {
    WebKitDOMDocument *dom = webkit_web_view_get_dom_document(_webview);
    WebKitDOMHTMLElement *ele = webkit_dom_document_get_body(dom);
    if (ele)
    {
        WebKitDOMEvent  *event = webkit_dom_document_create_event(dom, "CustomEvent", NULL);
        webkit_dom_event_init_event(event, NATIVE_EVENT_NAME, FALSE, TRUE); // The custom event should be canceled by the handler
        webkit_dom_node_dispatch_event(WEBKIT_DOM_NODE(ele), event, NULL);
    }
}
