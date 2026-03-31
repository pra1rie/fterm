#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <vte/vte.h>
#include <pango/pango.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "parser.h"

#define FTERM_VERSION "fterm v0.6"

char default_path[4096] = {0}; // $HOME/.config/fterm
typedef void (*action_fn)(void);

struct key_action {
    guint mod, key;
    action_fn action;
};

static void action_clipboard_copy(void);
static void action_clipboard_paste(void);
static void action_config_reload(void);
static void action_zoom_increment(void);
static void action_zoom_decrement(void);
static void action_zoom_reset(void);
static void action_alpha_increment(void);
static void action_alpha_decrement(void);
static void action_alpha_reset(void);

static struct {
    const char *name;
    action_fn action;
} keyname_mappings[] = {
    { "clipboard_copy", action_clipboard_copy },
    { "clipboard_paste", action_clipboard_paste },
    { "config_reload", action_config_reload },
    { "zoom_increment", action_zoom_increment },
    { "zoom_decrement", action_zoom_decrement },
    { "zoom_reset", action_zoom_reset },
    { "alpha_increment", action_alpha_increment },
    { "alpha_decrement", action_alpha_decrement },
    { "alpha_reset", action_alpha_reset },
};

// uwurawrxd
static struct key_action keys[256];
static int num_keys = 0, using_default = 0;
static int width = 900, height = 500;
static char *title = "Terminal";
static char *shell = "/usr/bin/bash";
static char *font = "monospace 15";
static float alpha = 1.0, cell_width = 1.0, cell_height = 1.0;
static int hide_cursor = 0, blink_cursor = 0, bright_bold = 0;
static char *cmd[32] = {0};
static char *colors[16] = {
    "#000", "#800", "#080", "#880", "#008", "#808", "#088", "#ccc",
    "#888", "#f00", "#0f0", "#ff0", "#00f", "#f0f", "#0ff", "#fff",
};

#define GET_VAR_OR(C, N, T, D) \
    (get_var_type(C, N, T).type == T_NIL)? D : get_var_type(C, N, T)

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

static inline action_fn get_action_from_name(const char *name) {
    for (int i = 0; i < sizeof(keyname_mappings)/sizeof(*keyname_mappings); ++i) {
        if (!strcmp(name, keyname_mappings[i].name))
            return keyname_mappings[i].action;
    }
    return NULL;
}

static inline guint parse_key_modifier(char *key, int *ret_offset) {
    int mod = 0;
    char *k = strtok(key, "+");
    while (k) {
        { // remove whitespaces around token
            while (isspace(*k)) ++k;
            int len = strlen(k);
            while (isspace(k[len-1])) k[--len] = '\0';
        }
        *ret_offset = (k-key);
        if (!strcasecmp(k, "control"))    mod |= GDK_CONTROL_MASK;
        else if (!strcasecmp(k, "shift")) mod |= GDK_SHIFT_MASK;
        else if (!strcasecmp(k, "alt"))   mod |= GDK_MOD1_MASK;
        else return mod;
        k = strtok(NULL, "+");
    }
    return mod;
}

static inline void parse_keys(struct config *cfg) {
    num_keys = 0;
    for (int i = 0; i < cfg->nkeys; ++i) {
        struct key key = cfg->keys[i];
        action_fn action = get_action_from_name(key.action);
        if (!action) {
            fprintf(stderr, "error: unknown action '%s'\n", key.action);
            continue;
        }
        int offset = 0;
        guint modifier = parse_key_modifier(key.keyname, &offset);
        guint keycode = gdk_keyval_from_name(key.keyname+offset);
        keys[num_keys++] = (struct key_action) { modifier, keycode, action };
    }
}

static inline void lookup_config(void) {
    char config_path[STRING_MAX] = {0};
    if (config) sprintf(config_path, "%s", config);
    else sprintf(config_path, "%s/fterm.cfg", default_path);
    if (access(config_path, F_OK) == 0) {
        struct config cfg = init_config(config_path);
        width = GET_VAR_OR(&cfg, "width", T_INT, width).as_int;
        height = GET_VAR_OR(&cfg, "height", T_INT, height).as_int;
        alpha = GET_VAR_OR(&cfg, "alpha", T_REAL, alpha).as_real;
        cell_width = GET_VAR_OR(&cfg, "cell_width", T_REAL, cell_width).as_real;
        cell_height = GET_VAR_OR(&cfg, "cell_height", T_REAL, cell_height).as_real;
        title = strdup(GET_VAR_OR(&cfg, "title", T_STR, title).as_str);
        shell = strdup(GET_VAR_OR(&cfg, "shell", T_STR, shell).as_str);
        font = strdup(GET_VAR_OR(&cfg, "font", T_STR, font).as_str);
        bright_bold = GET_VAR_OR(&cfg, "bright_bold", T_INT, bright_bold).as_int;
        hide_cursor = GET_VAR_OR(&cfg, "hide_cursor", T_INT, hide_cursor).as_int;
        blink_cursor = GET_VAR_OR(&cfg, "blink_cursor", T_INT, blink_cursor).as_int;
        parse_colors(&cfg);
        parse_keys(&cfg);
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
    PangoFontDescription *font_desc = pango_font_description_from_string(font);
    vte_terminal_set_font(term, font_desc);
    pango_font_description_free(font_desc);
    vte_terminal_set_font_scale(term, scale);
    vte_terminal_set_bold_is_bright(term, bright_bold);
    int blink = blink_cursor? VTE_CURSOR_BLINK_ON : VTE_CURSOR_BLINK_OFF;
    vte_terminal_set_cursor_blink_mode(term, blink);
    vte_terminal_set_mouse_autohide(term, hide_cursor);
    vte_terminal_set_cell_width_scale(term, cell_width);
    vte_terminal_set_cell_height_scale(term, cell_height);
    gtk_widget_set_visual(wn, gdk_screen_get_rgba_visual(gtk_widget_get_screen(wn)));
    set_alpha_scale(alpha);
}

static void action_clipboard_copy(void) {
    vte_terminal_copy_clipboard_format(term, VTE_FORMAT_TEXT);
}

static void action_clipboard_paste(void) {
    vte_terminal_paste_clipboard(term);
}

static void action_config_reload(void) {
    reload_config(TRUE);
}

static void action_zoom_increment(void) {
    vte_terminal_set_font_scale(term, scale += 0.1);
}

static void action_zoom_decrement(void) {
    vte_terminal_set_font_scale(term, scale -= 0.1);
}

static void action_zoom_reset(void) {
    vte_terminal_set_font_scale(term, scale = 1.0);
}

static void action_alpha_increment(void) {
    set_alpha_scale(background.alpha + 0.05);
}

static void action_alpha_decrement(void) {
    set_alpha_scale(background.alpha - 0.05);
}

static void action_alpha_reset(void) {
    set_alpha_scale(alpha);
}

static gboolean keypress(GtkWidget *w, GdkEventKey *e) {
    GdkModifierType mod = e->state & gtk_accelerator_get_default_mod_mask();
    for (int i = 0; i < num_keys; ++i) {
        if (mod == keys[i].mod && e->keyval == keys[i].key) {
            keys[i].action();
            return TRUE;
        }
    }
    return FALSE;
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

    sprintf(default_path, "%s/.config/fterm", getenv("HOME"));
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
