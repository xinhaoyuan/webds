#ifndef __WEBDS_DESKTOP_HPP__
#define __WEBDS_DESKTOP_HPP__

#include <string>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "webds_webkit.hpp"

class WebDSDesktop {
private:
    WebDSWebKit *_wk;
    GtkWindow   *_window;

    static void _window_focus_cb(GObject    *gobject,
                                 GParamSpec *pspec,
                                 gpointer    data);
public:
    WebDSDesktop(const std::string &page_uri, int monitor = -1);
    ~WebDSDesktop();
};

#endif
