#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <vte/vte.h>
#include <pango/pango.h>
#include <stdlib.h>
#include <unistd.h>
#include "parser.h"

#define FTERM_VERSION "fterm v0.2"

// uwurawrxd
static int using_default = 0;
static int width = 900, height = 500;
static char *title = "Terminal";
static char *shell = "/usr/bin/bash";
static char *font = "monospace 15";
static float alpha = 1.0;
static char *cmd[32] = {0};
static char *colors[16] = {
    "#000", "#800", "#080", "#880", "#008", "#808", "#088", "#ccc",
    "#888", "#f00", "#0f0", "#ff0", "#00f", "#f0f", "#0ff", "#fff",
};

#define GET_VAR_OR(C, N, T, D) \
    (get_var(C, N).type == T_NIL)? D : get_var_type(C, N, T)

GtkWidget *wn;
VteTerminal *term;
GdkRGBA palette[16], background;
char *config = NULL;
char *xid = NULL;
float scale = 1.0;

static inline void set_alpha_scale(float alpha) {
    background.alpha = alpha < 0 ? 0 : alpha > 1 ? 1 : alpha; // god programmer
    vte_terminal_set_colors(term, &palette[15], &background, palette, 16);
}

static inline void parse_colors(struct config *cfg) {
    for (int i = 0; i < 16; ++i) {
        char var[32] = {0};
        sprintf(var, "color%d", i);
        colors[i] = strdup(GET_VAR_OR(cfg, var, T_STR, colors[i]).as_str);
    }
}

static inline void lookup_config() {
    char config_path[STRING_MAX] = {0};
    if (config) sprintf(config_path, "%s", config);
    else sprintf(config_path, "%s/.config/fterm/fterm.cfg", getenv("HOME"));
    if (access(config_path, F_OK) == 0) {
        struct config cfg = init_config(config_path);
        width = GET_VAR_OR(&cfg, "width", T_INT, width).as_int;
        height = GET_VAR_OR(&cfg, "height", T_INT, height).as_int;
        alpha = GET_VAR_OR(&cfg, "alpha", T_REAL, alpha).as_real;
        title = strdup(GET_VAR_OR(&cfg, "title", T_STR, title).as_str);
        shell = strdup(GET_VAR_OR(&cfg, "shell", T_STR, shell).as_str);
        font = strdup(GET_VAR_OR(&cfg, "font", T_STR, font).as_str);
        parse_colors(&cfg);
        free_config(&cfg);
        using_default = 0;
    } else {
        using_default = 1;
        fprintf(stderr, "error: could not find config file, using default values\n");
    }
}

static inline void reload_config(Bool is_reload) {
    if (using_default) return;
    if (is_reload) {
        for (int i = 0; i < 16; ++i)
            free(colors[i]);
        free(title);
        free(shell);
        free(font);
    }

    lookup_config();
    for (int i = 0; i < 16; ++i)
        gdk_rgba_parse(palette+i, colors[i]);
    background = palette[0];
    cmd[0] = shell;
    scale = 1.0;
    gtk_window_set_default_size(GTK_WINDOW(wn), width, height);
    gtk_window_set_title(GTK_WINDOW(wn), title);
    vte_terminal_set_font(term, pango_font_description_from_string(font));
    vte_terminal_set_font_scale(term, scale);
    vte_terminal_set_bold_is_bright(term, TRUE);
    vte_terminal_set_cursor_blink_mode(term, VTE_CURSOR_BLINK_OFF);
    gtk_widget_set_visual(wn, gdk_screen_get_rgba_visual(gtk_widget_get_screen(wn)));
    set_alpha_scale(alpha);
}

#define MODN(k) (e->keyval == k && m == (GDK_CONTROL_MASK))
#define MODS(k) (e->keyval == k && m == (GDK_CONTROL_MASK|GDK_SHIFT_MASK))
static gboolean keypress(GtkWidget *w, GdkEventKey *e) {
    GdkModifierType m = e->state & gtk_accelerator_get_default_mod_mask();
    if (MODS(GDK_KEY_C))
        vte_terminal_copy_clipboard_format(term, VTE_FORMAT_TEXT);
    else if (MODS(GDK_KEY_V))
        vte_terminal_paste_clipboard(term);
    else if (MODN(GDK_KEY_equal))
        vte_terminal_set_font_scale(term, scale += 0.1);
    else if (MODN(GDK_KEY_minus))
        vte_terminal_set_font_scale(term, scale -= 0.1);
    else if (MODN(GDK_KEY_0))
        vte_terminal_set_font_scale(term, scale = 1.0);
    else if (MODN(GDK_KEY_F5))
        reload_config(TRUE);
    else if (MODS(GDK_KEY_less))
        set_alpha_scale(background.alpha - 0.05);
    else if (MODS(GDK_KEY_greater))
        set_alpha_scale(background.alpha + 0.05);
    else if (MODS(GDK_KEY_question))
        set_alpha_scale(alpha);
    else return FALSE;
    return TRUE;
}

static void usage(const char *prgname) {
    printf("usage: %s [flags] [command [args...]]\n", prgname);
    printf("flags:\n");
    printf("    -h, --help               show this help and exit\n");
    printf("    -v, --version            print version information\n");
    printf("    -c, --config path        load config file from path\n");
    printf("    -w, --window window_id   embed fterm into another X11 window\n");
    exit(0);
}

#define FLAG(F) (strcmp(argv[i], F) == 0)
#define FLAGN(F, N) (strcmp(argv[i], F) == 0 && i+N < argc)

int main(int argc, char **argv) {
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            if (FLAG("-h") || FLAG("--help")) {
                usage(argv[0]);
            } else if (FLAG("-v") || FLAG("--version")) {
                printf("%s\n", FTERM_VERSION);
                exit(0);
            } else if (FLAGN("-c", 1) || FLAGN("--config", 1)) {
                config = argv[++i];
            } else if (FLAGN("-w", 1) || FLAGN("--window", 1)) {
                xid = argv[++i];
            } else {
                cmd[1] = "-c";
                for (int j = i; j < MIN(argc, 30); ++j)
                    cmd[(j-i)+2] = argv[j];
                break;
            }
        }
    }

    gtk_init(&argc, &argv);
    wn = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    term = VTE_TERMINAL(vte_terminal_new());
    gtk_container_add(GTK_CONTAINER(wn), GTK_WIDGET(term));

    reload_config(FALSE);
    vte_terminal_spawn_async(term, VTE_PTY_DEFAULT, NULL, cmd, NULL,
            G_SPAWN_DEFAULT, NULL, NULL, NULL, -1, NULL, NULL, NULL);

    g_signal_connect(wn, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(term, "child-exited", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(wn, "key-press-event", G_CALLBACK(keypress), NULL);
    gtk_widget_show_all(wn);
    if (xid) {
        Window p = strtol(xid, NULL, 0);
        Display *d = gdk_x11_display_get_xdisplay(gdk_display_get_default());
        Window w = gdk_x11_window_get_xid(gtk_widget_get_window(wn));
        XReparentWindow(d, w, p, 0, 0);
    }
    gtk_main();
    return 0;
}
