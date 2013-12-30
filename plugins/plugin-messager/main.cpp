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

static JSValueRef
js_cb_messager_register_listener(JSContextRef context,
                                 JSObjectRef function,
                                 JSObjectRef self,
                                 size_t argc,
                                 const JSValueRef argv[],
                                 JSValueRef* exception)
{
    WebDSWebKit *wk = (WebDSWebKit *)JSObjectGetPrivate(self);
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
