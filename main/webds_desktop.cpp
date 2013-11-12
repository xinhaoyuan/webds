#include "webds_desktop.hpp"
#include "util.hpp"

using namespace std;

WebDSDesktop::WebDSDesktop(const string &page_uri, int monitor) {
    GdkScreen *screen = gdk_screen_get_default();
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (monitor < 0) monitor = gdk_screen_get_primary_monitor(screen);
    GdkRectangle geo;

    gdk_screen_get_monitor_workarea(screen, monitor, &geo);

    _wk = new WebDSWebKit();
    // Create a Window, set visual to RGBA
    _window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
    gtk_window_set_type_hint(_window, GDK_WINDOW_TYPE_HINT_DESKTOP);
    gtk_window_set_resizable(_window, FALSE);
    gtk_widget_set_can_focus(GTK_WIDGET(_window), TRUE);
    gtk_window_set_decorated(_window, FALSE);
    gtk_window_set_position(_window, GTK_WIN_POS_NONE);
    gtk_widget_set_size_request(GTK_WIDGET(_window), geo.width, geo.height);
    gtk_window_move(_window, geo.x, geo.y);
    
    if (visual == NULL || !gtk_widget_is_composited(GTK_WIDGET(_window)))
    {
        fprintf(stderr, "Your screen does not support alpha window, :(\n");
        visual = gdk_screen_get_system_visual(screen);
    }

    g_signal_connect(G_OBJECT(_window), "notify::is-active", G_CALLBACK(_window_focus_cb), this);

    gtk_widget_set_visual(GTK_WIDGET(_window), visual);
    /* Set transparent window background */
    GdkRGBA bg = {0,0,0,0};
    gtk_widget_override_background_color(GTK_WIDGET(_window), GTK_STATE_FLAG_NORMAL, &bg);
    
    gtk_container_add(GTK_CONTAINER(_window), GTK_WIDGET(_wk->get_webview()));
    
    gtk_widget_grab_focus(GTK_WIDGET(_wk->get_webview()));
    gtk_widget_show(GTK_WIDGET(_window));

    webkit_web_view_load_uri(_wk->get_webview(), page_uri.c_str());
}

WebDSDesktop::~WebDSDesktop() {
    delete _wk;
}

void WebDSDesktop::_window_focus_cb(GObject    *gobject,
                                    GParamSpec *pspec,
                                    gpointer    data) {
    WebDSDesktop *_this = (WebDSDesktop *)data;
    if (gtk_window_has_toplevel_focus(_this->_window))
    {
        gtk_widget_grab_focus(GTK_WIDGET(_this->_wk->get_webview()));
    }
}
