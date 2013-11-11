#ifndef __WEBDS_UTIL_H__
#define __WEBDS_UTIL_H__

#include <plugin.h>
#include <string>

int        copy_from_jsstring_object(JSContextRef context, JSValueRef obj, std::string &result);
JSValueRef jsstring_object_from_string(JSContextRef context, const std::string &str);
    
void encode_path(std::string &result, const std::string &path);
void decode_path(std::string &result, const std::string &epath);

#endif
