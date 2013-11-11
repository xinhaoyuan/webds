#include <util.hpp>
#include <sstream>
#include <stdio.h>
#include <string.h>

using namespace std;

int
copy_from_jsstring_object(JSContextRef context, JSValueRef obj, string &result)
{
    if (!JSValueIsString(context, obj)) return 1;
    JSStringRef js_str = JSValueToStringCopy(context, obj, NULL);
    size_t size = JSStringGetMaximumUTF8CStringSize(js_str);
    char *buf = (char *)malloc(size);
    if (buf)
        size = JSStringGetUTF8CString(js_str, buf, size);
    else return 1;
    result = string(buf, size - 1);
    free(buf);
    return 0;
}

JSValueRef jsstring_object_from_string(JSContextRef context, const string &str) {
    JSStringRef js_str = JSStringCreateWithUTF8CString(str.c_str());
    JSValueRef obj = JSValueMakeString(context, js_str);
    JSStringRelease(js_str);
    return obj;
}

void
encode_path(string &result, const string &path) {
    ostringstream oss;
    for (int i = 0; i < path.length(); ++ i) {
        if (path[i] != '/' && path[i] != '_') {
            oss << path[i];
        }
        else if (path[i] != '/' || i != path.length() - 1)
        {
            oss << '_' << (path[i] == '/' ? '1' : '2');
        }
    }
    result = oss.str();
}

void
decode_path(string &result, const string &epath) {
    ostringstream oss;
    int e = 0;
    for (int i = 0; i < epath.length(); ++ i) {
        if (epath[i] == '_')
            e = 1;
        else if (e) {
            oss << (epath[i] == '1' ? '/' : '_');
            e = 0;
        } else oss << epath[i];
    }
    result = oss.str();
}
