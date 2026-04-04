/*
 * wizer — Desktop quote/text widget
 *
 * Build:
 *   g++ widget.cpp -o wizer \
 *       $(pkg-config --cflags --libs gtk+-3.0 gtk-layer-shell-0 jsoncpp) \
 *       -std=c++17 -O2
 *
 * Nix:
 *   nix-shell -p gtk3 gtk3.dev gtk-layer-shell gtk-layer-shell.dev \
 *             jsoncpp pkg-config gcc gnumake
 *   make
 */

#include <gtk/gtk.h>
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <json/json.h>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <string>
#include <algorithm>
#include <cmath>
using namespace std;
// ── Config ────────────────────────────────────────────────────────────────────

enum class Layer { TOP, BOTTOM};

struct Config {
    std::string text        = "\"The only way to do great work\nis to love what you do.\"\n\n— Steve Jobs";
    std::string font        = "Serif Italic 20";
    std::string fg_color    = "#F0E6D3";
    std::string bg_color    = "#1A1A2E";
    std::string bg_image    = "";
    double      bg_opacity  = 0.88;
    int         margin_top  = 40;
    int         margin_left = 40;
    int         width       = 420;
    int         height      = 180;
    std::string path;
    Layer layer = Layer::TOP;
};

static Config cfg;

static void saveConfig() {
    if (cfg.path.empty()) return;
    Json::Value root;
    root["text"]        = cfg.text;
    root["font"]        = cfg.font;
    root["fg_color"]    = cfg.fg_color;
    root["bg_color"]    = cfg.bg_color;
    root["bg_image"]    = cfg.bg_image;
    root["bg_opacity"]  = cfg.bg_opacity;
    root["margin_top"]  = cfg.margin_top;
    root["margin_left"] = cfg.margin_left;
    root["width"]       = cfg.width;
    root["height"]      = cfg.height;
    root["layer"]       = cfg.layer == Layer::TOP ? "top" : "bottom";
    Json::StreamWriterBuilder wb;
    wb["indentation"]  = "  ";
    wb["commentStyle"] = "None";
    std::ofstream f(cfg.path);
    if (!f) { std::cerr << "[widget] Cannot write: " << cfg.path << "\n"; return; }
    f << Json::writeString(wb, root) << "\n";
    std::cout << "[widget] Saved → " << cfg.path << "\n";
}

static void loadConfig(const std::string& filepath) {
    cfg.path = filepath;
    std::ifstream f(filepath);
    if (!f) { saveConfig(); return; }
    Json::Value root;
    Json::CharReaderBuilder rb;
    std::string errs;
    if (!Json::parseFromStream(rb, f, &root, &errs)) {
        std::cerr << "[widget] JSON error: " << errs << "\n";
        return;
    }
    auto str = [&](const char* k, std::string& v) { if (root.isMember(k) && root[k].isString())  v = root[k].asString(); };
    auto num = [&](const char* k, int& v)          { if (root.isMember(k) && root[k].isNumeric()) v = root[k].asInt();    };
    auto dbl = [&](const char* k, double& v)       { if (root.isMember(k) && root[k].isNumeric()) v = root[k].asDouble(); };
    str("text",       cfg.text);        str("font",        cfg.font);
    str("fg_color",   cfg.fg_color);    str("bg_color",    cfg.bg_color);
    str("bg_image",   cfg.bg_image);
    dbl("bg_opacity", cfg.bg_opacity);
    num("margin_top", cfg.margin_top);  num("margin_left", cfg.margin_left);
    num("width",      cfg.width);       num("height",      cfg.height);
    std::string l1; str("layer", l1);
    cfg.layer = (l1 == "bottom"s) ? Layer::BOTTOM : Layer::TOP;
}

// ── State ─────────────────────────────────────────────────────────────────────

enum class Mode { NONE, DRAGGING, RESIZING };

struct {
    GtkWindow*  window  = nullptr;
    GtkWidget*  canvas  = nullptr;
    GdkPixbuf*  bgpix   = nullptr;

    bool        hovered = false;
    Mode        active  = Mode::NONE;
    Mode        pending = Mode::NONE;

    double      press_x = 0, press_y = 0;
    int         snap_ml = 0, snap_mt = 0;
    int         snap_w  = 0, snap_h  = 0;

    int         mon_w   = 1920, mon_h = 1080;
} st;

// ── Helpers ───────────────────────────────────────────────────────────────────

static void hexToRgb(const std::string& hex, double& r, double& g, double& b) {
    r = g = b = 0;
    std::string h = hex;
    if (!h.empty() && h[0] == '#') h = h.substr(1);
    if (h.size() == 6) {
        unsigned rv = 0, gv = 0, bv = 0;
        sscanf(h.c_str(), "%02x%02x%02x", &rv, &gv, &bv);
        r = rv / 255.0; g = gv / 255.0; b = bv / 255.0;
    }
}

static void reloadBgImage() {
    if (st.bgpix) { g_object_unref(st.bgpix); st.bgpix = nullptr; }
    if (cfg.bg_image.empty() || !std::filesystem::exists(cfg.bg_image)) return;
    GError* err = nullptr;
    st.bgpix = gdk_pixbuf_new_from_file_at_scale(
        cfg.bg_image.c_str(), cfg.width, cfg.height, FALSE, &err);
    if (err) { std::cerr << "[widget] Image: " << err->message << "\n"; g_error_free(err); }
}

static void setInputRegion(bool fullscreen) {
    GdkWindow* win = gtk_widget_get_window(GTK_WIDGET(st.window));
    if (!win) return;
    cairo_rectangle_int_t rect = fullscreen
        ? cairo_rectangle_int_t{ 0, 0, st.mon_w, st.mon_h }
        : cairo_rectangle_int_t{ cfg.margin_left, cfg.margin_top, cfg.width, cfg.height };
    cairo_region_t* region = cairo_region_create_rectangle(&rect);
    gdk_window_input_shape_combine_region(win, region, 0, 0);
    cairo_region_destroy(region);
}

static bool inWidget(double x, double y) {
    return x >= cfg.margin_left && x < cfg.margin_left + cfg.width
        && y >= cfg.margin_top  && y < cfg.margin_top  + cfg.height;
}

// ── Operations ────────────────────────────────────────────────────────────────

static void beginOp(Mode mode, double rx, double ry) {
    st.active  = mode;
    st.press_x = rx;       st.press_y = ry;
    st.snap_ml = cfg.margin_left; st.snap_mt = cfg.margin_top;
    st.snap_w  = cfg.width;       st.snap_h  = cfg.height;
    setInputRegion(true);
    gtk_widget_queue_draw(st.canvas);
}

static void endOp(bool save) {
    st.active  = Mode::NONE;
    st.pending = Mode::NONE;
    setInputRegion(false);
    if (save) saveConfig();
    gtk_widget_queue_draw(st.canvas);
}

// ── Draw ──────────────────────────────────────────────────────────────────────

static gboolean on_draw(GtkWidget*, cairo_t* cr, gpointer) {
    // Clear full canvas to transparent
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    cairo_save(cr);
    cairo_translate(cr, cfg.margin_left, cfg.margin_top);

    const int    w = cfg.width, h = cfg.height;
    const double R = 14.0;

    // Rounded rect path
    auto rrect = [&](double ins) {
        double r = std::max(1.0, R - ins);
        cairo_new_path(cr);
        cairo_move_to(cr,  r+ins,   ins);
        cairo_line_to(cr,  w-r-ins, ins);
        cairo_arc(cr,      w-r-ins, r+ins,   r, -G_PI/2, 0);
        cairo_line_to(cr,  w-ins,   h-r-ins);
        cairo_arc(cr,      w-r-ins, h-r-ins, r,  0,      G_PI/2);
        cairo_line_to(cr,  r+ins,   h-ins);
        cairo_arc(cr,      r+ins,   h-r-ins, r,  G_PI/2, G_PI);
        cairo_line_to(cr,  ins,     r+ins);
        cairo_arc(cr,      r+ins,   r+ins,   r,  G_PI,  -G_PI/2);
        cairo_close_path(cr);
    };

    rrect(0); cairo_clip(cr);

    // Background
    if (st.bgpix) {
        gdk_cairo_set_source_pixbuf(cr, st.bgpix, 0, 0);
        cairo_paint_with_alpha(cr, cfg.bg_opacity);
    } else {
        double r, g, b; hexToRgb(cfg.bg_color, r, g, b);
        cairo_set_source_rgba(cr, r, g, b, cfg.bg_opacity);
        cairo_paint(cr);
    }

    // Chrome (border glow + grip dots)
    bool active = st.active != Mode::NONE || st.pending != Mode::NONE;
    bool chrome = st.hovered || active;
    if (chrome) {
        rrect(0.75);
        cairo_set_source_rgba(cr, 1, 1, 1, active ? 0.38 : 0.10);
        cairo_set_line_width(cr, 1.5);
        cairo_stroke(cr);

        // double r, g, b; hexToRgb(cfg.fg_color, r, g, b);
        // cairo_set_source_rgba(cr, r, g, b, 0.50);
        // for (int i = 0; i < 3; ++i)
        //     for (int j = 0; j < 3; ++j) {
        //         cairo_arc(cr, w-19+i*6, h-19+j*6, 1.5, 0, 2*G_PI);
        //         cairo_fill(cr);
            // }
    }

    // Top accent line
    {
        double r, g, b; hexToRgb(cfg.fg_color, r, g, b);
        cairo_set_source_rgba(cr, r, g, b, 0.45);
        cairo_set_line_width(cr, 1.5);
        cairo_move_to(cr, R+12, 1); cairo_line_to(cr, w-R-12, 1);
        cairo_stroke(cr);
    }

    // Centered text
    {
        double r, g, b; hexToRgb(cfg.fg_color, r, g, b);
        cairo_set_source_rgb(cr, r, g, b);
        PangoLayout* lay = pango_cairo_create_layout(cr);
        pango_layout_set_text(lay, cfg.text.c_str(), -1);
        PangoFontDescription* fd = pango_font_description_from_string(cfg.font.c_str());
        pango_layout_set_font_description(lay, fd); pango_font_description_free(fd);
        pango_layout_set_width(lay, (w - 48) * PANGO_SCALE);
        pango_layout_set_wrap(lay, PANGO_WRAP_WORD_CHAR);
        pango_layout_set_alignment(lay, PANGO_ALIGN_CENTER);
        int tw, th; pango_layout_get_pixel_size(lay, &tw, &th);
        cairo_move_to(cr, 24.0, (h - th) / 2.0);
        pango_cairo_show_layout(cr, lay);
        g_object_unref(lay);
    }

    // Mode badge
    {
        const char* label = nullptr;
        if      (st.active == Mode::DRAGGING || st.pending == Mode::DRAGGING) label = " DRAG ";
        else if (st.active == Mode::RESIZING || st.pending == Mode::RESIZING) label = "RESIZE";
        if (label) {
            cairo_set_source_rgba(cr, 0, 0, 0, 0.62);
            cairo_rectangle(cr, w/2-34, 5, 68, 18); cairo_fill(cr);
            double r, g, b; hexToRgb(cfg.fg_color, r, g, b);
            cairo_set_source_rgba(cr, r, g, b, 0.95);
            PangoLayout* lay = pango_cairo_create_layout(cr);
            pango_layout_set_text(lay, label, -1);
            PangoFontDescription* fd = pango_font_description_from_string("Monospace Bold 8");
            pango_layout_set_font_description(lay, fd); pango_font_description_free(fd);
            int tw, th; pango_layout_get_pixel_size(lay, &tw, &th);
            cairo_move_to(cr, w/2-tw/2, 5+(18-th)/2);
            pango_cairo_show_layout(cr, lay); g_object_unref(lay);
        }
    }

    cairo_restore(cr);
    return FALSE;
}

// ── Menu ──────────────────────────────────────────────────────────────────────

static void show_menu(GdkEventButton* ev) {
    GtkWidget* menu = gtk_menu_new();

    auto add = [&](const char* lbl, GCallback cb, bool sensitive = true) {
        GtkWidget* item = gtk_menu_item_new_with_label(lbl);
        if (cb) g_signal_connect(item, "activate", cb, nullptr);
        gtk_widget_set_sensitive(item, sensitive);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        gtk_widget_show(item);
    };
    auto sep = [&]() {
        GtkWidget* s = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), s);
        gtk_widget_show(s);
    };

    add("⬡  Desktop Widget", nullptr, false); sep();
    add("✥  Drag Mode",   G_CALLBACK(+[](GtkMenuItem*, gpointer) { endOp(false); st.pending = Mode::DRAGGING; gtk_widget_queue_draw(st.canvas); }));
    add("⤢  Resize Mode", G_CALLBACK(+[](GtkMenuItem*, gpointer) { endOp(false); st.pending = Mode::RESIZING; gtk_widget_queue_draw(st.canvas); }));
    add("✕  Cancel",      G_CALLBACK(+[](GtkMenuItem*, gpointer) { endOp(false); })); sep();
    add("  Change layer",G_CALLBACK(+[](GtkMenuItem*, gpointer) { cfg.layer = (cfg.layer == Layer::BOTTOM)? Layer::TOP : Layer::BOTTOM; saveConfig();}));
    add("💾  Save Layout", G_CALLBACK(+[](GtkMenuItem*, gpointer) { saveConfig(); }));
    add("✕   Quit",       G_CALLBACK(+[](GtkMenuItem*, gpointer) { gtk_main_quit(); }));

    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)ev);
}

// ── Events ────────────────────────────────────────────────────────────────────

static gboolean on_press(GtkWidget*, GdkEventButton* ev, gpointer) {
    if (ev->button == 3 && inWidget(ev->x, ev->y)) {
        endOp(false);
        show_menu(ev);
        return TRUE;
    }
    if (ev->button == 1 && st.pending != Mode::NONE && inWidget(ev->x, ev->y)) {
        beginOp(st.pending, ev->x_root, ev->y_root);
        st.pending = Mode::NONE;
        return TRUE;
    }
    return FALSE;
}

static gboolean on_release(GtkWidget*, GdkEventButton* ev, gpointer) {
    if (ev->button == 1 && st.active != Mode::NONE) {
        endOp(true);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_motion(GtkWidget*, GdkEventMotion* ev, gpointer) {
    if (st.active == Mode::NONE) {
        bool inside = inWidget(ev->x, ev->y);
        if (inside != st.hovered) { st.hovered = inside; gtk_widget_queue_draw(st.canvas); }
        return FALSE;
    }

    int dx = (int)(ev->x_root - st.press_x);
    int dy = (int)(ev->y_root - st.press_y);

    if (st.active == Mode::DRAGGING) {
        cfg.margin_left = std::clamp(st.snap_ml + dx, 0, st.mon_w - cfg.width);
        cfg.margin_top  = std::clamp(st.snap_mt + dy, 0, st.mon_h - cfg.height);
    } else {
        cfg.width  = std::clamp(st.snap_w + dx, 160, st.mon_w - cfg.margin_left);
        cfg.height = std::clamp(st.snap_h + dy,  60, st.mon_h - cfg.margin_top);
        reloadBgImage();
    }
    gtk_widget_queue_draw(st.canvas);
    return TRUE;
}

static void on_realize(GtkWidget* widget, gpointer) {
    GdkWindow*  win  = gtk_widget_get_window(widget);
    GdkDisplay* disp = gdk_window_get_display(win);
    GdkMonitor* mon  = gdk_display_get_monitor_at_window(disp, win);
    if (mon) {
        GdkRectangle geom;
        gdk_monitor_get_geometry(mon, &geom);
        st.mon_w = geom.width;
        st.mon_h = geom.height;
        std::cout << "[widget] Monitor: " << st.mon_w << "×" << st.mon_h << "\n";
    }
    setInputRegion(false);
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    std::string cfgpath;
    if (argc >= 2 && std::string(argv[1]) != "--help") {
        cfgpath = argv[1];
    } else if (argc >= 2) {
        std::cout << "Usage: wizer [config.json]\n"
                     "Default: ~/.config/wizer/config.json\n\n"
                     "Config keys: text, font, fg_color, bg_color, bg_image,\n"
                     "             bg_opacity, margin_top, margin_left, width, height\n";
        return 0;
    } else {
        const char* home = getenv("HOME");
        std::string dir  = std::string(home ? home : ".") + "/.config/wizer";
        std::filesystem::create_directories(dir);
        cfgpath = dir + "/config.json";
    }

    loadConfig(cfgpath);
    gtk_init(&argc, &argv);

    // Window
    GtkWidget* win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    st.window = GTK_WINDOW(win);
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    gtk_window_set_title(GTK_WINDOW(win), "wizer");
    gtk_widget_set_app_paintable(win, TRUE);

    GdkScreen* scr = gtk_widget_get_screen(win);
    GdkVisual* vis = gdk_screen_get_rgba_visual(scr);
    if (vis) gtk_widget_set_visual(win, vis);

    // Layer shell — fullscreen TOP layer, all anchors
    gtk_layer_init_for_window(st.window);
    gtk_layer_set_layer(st.window, cfg.layer == Layer::TOP ? GTK_LAYER_SHELL_LAYER_TOP : GTK_LAYER_SHELL_LAYER_BOTTOM);
    gtk_layer_set_namespace(st.window, "wizer");
    gtk_layer_set_exclusive_zone(st.window, -1);
    gtk_layer_set_keyboard_mode(st.window, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    for (auto edge : { GTK_LAYER_SHELL_EDGE_TOP, GTK_LAYER_SHELL_EDGE_LEFT,
                       GTK_LAYER_SHELL_EDGE_BOTTOM, GTK_LAYER_SHELL_EDGE_RIGHT })
        gtk_layer_set_anchor(st.window, edge, TRUE);

    // Full-monitor drawing canvas
    GtkWidget* da = gtk_drawing_area_new();
    st.canvas = da;
    gtk_container_add(GTK_CONTAINER(win), da);
    gtk_widget_add_events(da, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
                            | GDK_POINTER_MOTION_MASK);

    g_signal_connect(da,  "draw",               G_CALLBACK(on_draw),    nullptr);
    g_signal_connect(da,  "button-press-event", G_CALLBACK(on_press),   nullptr);
    g_signal_connect(da,  "button-release-event",G_CALLBACK(on_release), nullptr);
    g_signal_connect(da,  "motion-notify-event",G_CALLBACK(on_motion),  nullptr);
    g_signal_connect(win, "realize",            G_CALLBACK(on_realize), nullptr);
    g_signal_connect(win, "destroy",            G_CALLBACK(gtk_main_quit), nullptr);

    reloadBgImage();
    gtk_widget_show_all(win);

    std::cout << "[widget] Config → " << cfgpath << "\n"
              << "[widget] Right-click the widget to interact.\n";
    gtk_main();
    return 0;
}
