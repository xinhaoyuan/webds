#ifndef __WEBDS_PLUGIN_H__
#define __WEBDS_PLUGIN_H__

#include <stdlib.h>
#include <JavaScriptCore/JavaScript.h>

typedef JSValueRef(*webds_js_callback_f)(JSContextRef context,
                                          JSObjectRef function,
                                          JSObjectRef self,
                                          size_t argc,
                                          const JSValueRef argv[],
                                          JSValueRef* exception);

typedef struct webds_plugin_interface_s *webds_plugin_interface_t;
typedef struct webds_plugin_interface_s
{
    void(*exit)(webds_plugin_interface_t);
} webds_plugin_interface_s;

typedef struct webds_host_interface_s *webds_host_interface_t;
typedef struct webds_host_interface_s
{
    int(*const register_native_method)(const char *, webds_js_callback_f);
} webds_host_interface_s;

typedef webds_plugin_interface_t(*webds_plugin_init_f)(webds_host_interface_t);

#endif
