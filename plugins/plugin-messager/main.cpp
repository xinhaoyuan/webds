#include <plugin.h>
#include <spawn.h>
#include <stdio.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>

#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

#include <util.hpp>
#include <webds_webkit.hpp>

#include <gio/gunixsocketaddress.h>

#define READ_BUFFER_SIZE 4096

using namespace std;

webds_host_interface_t host;
webds_plugin_interface_s plugin =
{
    NULL,
};

struct messager_record {
    WebDSWebKit *wk;
    string socket_filename;
    string callback_name;
    GSocketService *service;
};

struct message {
    messager_record *messager;
    GSocketConnection *socket_conn;
    GIOChannel *io_channel;
    char *read_buffer;
    ostringstream msg_buffer;
};

pthread_mutex_t msgr_lock = PTHREAD_MUTEX_INITIALIZER;
map<string, messager_record *> msgr;

static gboolean
_process_message(GIOChannel *source,
                 GIOCondition condition,
                 gpointer data)
{
    message *msg = (message *)data;
    if (condition & G_IO_IN) {
        while (true) {
            gsize size;
            GIOStatus s = g_io_channel_read_chars(msg->io_channel,
                                                  msg->read_buffer,
                                                  READ_BUFFER_SIZE,
                                                  &size,
                                                  NULL);
            if (s > 0) msg->msg_buffer << string(msg->read_buffer, size);
            if (s == G_IO_STATUS_EOF) {
                ostringstream script;
                script << msg->messager->callback_name << "(\"";
                string msg_text = msg->msg_buffer.str();
                for (int i = 0; i < msg_text.length(); ++ i) {
                    if (msg_text[i] == '"' || msg_text[i] == '\n') script << '\\';
                    script << msg_text[i];
                }
                script << "\");";
                WebKitWebView *wv = msg->messager->wk->get_webview();
                string eval_str = script.str();
                fprintf(stderr, "eval: %s\n", eval_str.c_str());
                webkit_web_view_execute_script(wv, eval_str.c_str());
                goto eom;
            } else if (s == G_IO_STATUS_AGAIN) {
                break;
            } else if (s == G_IO_STATUS_ERROR) {
                fprintf(stderr, "message reading error\n");
                goto eom;
            }
        }
    }

    if (condition & (G_IO_HUP | G_IO_ERR)) {
        fprintf(stderr, "message connection closed abnormally, canceled\n");
        goto eom;
    }
    return true;
  eom:
    g_io_channel_shutdown(msg->io_channel, false, NULL);
    g_io_channel_unref(msg->io_channel);
    g_object_unref(msg->socket_conn);
    free(msg->read_buffer);
    delete msg;
    return false;
}

static gboolean
_incoming_message(GSocketService    *service,
                  GSocketConnection *connection,
                  GObject           *source_object,
                  gpointer           user_data)
{
    messager_record *rec = (messager_record *)user_data;
    message *msg = new message();
    msg->messager = rec;
    msg->socket_conn = connection;
    msg->io_channel = g_io_channel_unix_new(g_socket_get_fd(g_socket_connection_get_socket(connection)));
    msg->read_buffer = (char *)malloc(sizeof(char) * READ_BUFFER_SIZE);
    g_io_add_watch(msg->io_channel, G_IO_IN, _process_message, msg);
    g_object_ref(connection);
    return true;
}

static JSValueRef
js_cb_messager_register_listener(JSContextRef context,
                                 JSObjectRef function,
                                 JSObjectRef self,
                                 size_t argc,
                                 const JSValueRef argv[],
                                 JSValueRef* exception)
{
    WebDSWebKit *wk = (WebDSWebKit *)JSObjectGetPrivate(self);
    if (argc != 3) return JSValueMakeNull(context);
    if (!JSValueIsString(context, argv[1]) || !JSValueIsString(context, argv[2]))
        return JSValueMakeNull(context);
    string socket_filename;
    string callback_name;
    copy_from_jsstring_object(context, argv[1], socket_filename);
    copy_from_jsstring_object(context, argv[2], callback_name);
    

    messager_record *rec; 
    pthread_mutex_lock(&msgr_lock);
    if (msgr.find(socket_filename) != msgr.end()) {
        pthread_mutex_unlock(&msgr_lock);
        return JSValueMakeNull(context);
    } else {
        msgr[socket_filename] = rec = new messager_record();
        pthread_mutex_unlock(&msgr_lock);
    }

    rec->wk = wk;
    rec->socket_filename = socket_filename;
    rec->callback_name = callback_name;
    rec->service = g_socket_service_new();

    unlink(socket_filename.c_str());
    GSocketAddress *addr = g_unix_socket_address_new(socket_filename.c_str());
    if (!g_socket_listener_add_address(G_SOCKET_LISTENER(rec->service),
                                       addr,
                                       G_SOCKET_TYPE_STREAM,
                                       G_SOCKET_PROTOCOL_DEFAULT,
                                       NULL, NULL, NULL))
    {
        fprintf(stderr, "error during listening to the socket address\n");
        g_object_unref(addr);
        g_object_unref(rec->service);
        delete rec;
        msgr.erase(socket_filename);
        return JSValueMakeNull(context);
    }

    g_signal_connect(G_OBJECT(rec->service), "incoming", G_CALLBACK(_incoming_message), rec);
    g_socket_service_start(rec->service);

    fprintf(stderr, "setup a messaging socket at %s\n", socket_filename.c_str());
    return JSValueMakeBoolean(context, true);
}

extern "C" {
    
    webds_plugin_interface_t
    init(webds_host_interface_t host_interface)
    {
        host = host_interface;
        host->register_native_method("messager_register_listener", js_cb_messager_register_listener);
        return &plugin;
    }

}
