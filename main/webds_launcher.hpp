#ifndef __WEBDS_LAUNCHER_HPP__
#define __WEBDS_LAUNCHER_HPP__

#include <string>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "webds_webkit.hpp"

class WebDSLauncher {
private:
    WebDSWebKit *_wk;
    GtkWindow   *_window;

    static void _window_focus_cb(GObject    *gobject,
                                 GParamSpec *pspec,
                                 gpointer    data);
public:
    WebDSLauncher(const std::string &page_uri);
    ~WebDSLauncher();
};

#endif
