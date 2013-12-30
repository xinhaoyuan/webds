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

using namespace std;

webds_host_interface_t host;
webds_plugin_interface_s plugin =
{
    NULL,
};

typedef struct listener_record_s = {
    WebDSWebKit     *wk;
    string           socket_path;
    GSocketListener *listener;
} listener_record_s;

G_LOCK_DEFINE_STATIC(_local_lock);
void _local_lock() { G_LOCK(_local_lock) ;}
void _local_unlock() { G_UNLOCK(_local_lock); }

static map<string, listener_record_s *> _records;

static JSValueRef
js_cb_messager_register_listener(JSContextRef context,
                                 JSObjectRef function,
                                 JSObjectRef self,
                                 size_t argc,
                                 const JSValueRef argv[],
                                 JSValueRef* exception)
{
    WebDSWebKit *wk = (WebDSWebKit *)JSObjectGetPrivate(self);
    if (argc != 2 || !JSValueIsString(argv[1]))
        return JSValueMakeNull(context);
    
    string socket_path;
    copy_from_jsstring_object(context, argv[1], socket_path);
    listener_record_s *rec;
    
    _local_lock();
    if (_records.find(socket_path) != _records.end()) {
        _local_unlock();
        return JSValueMakeNull(context);
    } else {
        rec = _records[socket_path] = new listener_record_s();
        _local_unlock();
    }

    rec->wk = wk;
    rec->socket_path = socket_path;

    // XXX start listening

    return JSValueMakeBoolean(context, true);
}

static JSValueRef
js_cb_messager_remove_listener(JSContextRef context,
                               JSObjectRef function,
                               JSObjectRef self,
                               size_t argc,
                               const JSValueRef argv[],
                               JSValueRef* exception) {
    WebDSWebKit *wk = (WebDSWebKit *)JSObjectGetPrivate(self);
    if (argc != 2 || !JSValueIsString(argv[1]))
        return JSValueMakeNull(context);
    
    string socket_path;
    copy_from_jsstring_object(context, argv[1], socket_path);
    listener_record_s *rec;

    _local_lock();
    if (_records.find(socket_path) == _records.end()) {
        _local_unlock();
        return JSValueMakeNull(context);
    } else {
        rec = _records[socket_path];
        _records.erase(socket_path);
        _local_unlock();
    }

    // XXX

    g_socket_listener_close(rec->listener);
    
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
