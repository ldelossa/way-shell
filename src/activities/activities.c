#include "./activities.h"

#include <adwaita.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

#include "../services/window_manager_service/sway/window_manager_service_sway.h"
#include "../services/window_manager_service/window_manager_service.h"
#include "./activities_app_widget.h"
#include "gtk/gtkrevealer.h"

static Activities *global = NULL;

enum signals {
    activities_will_show,
    activities_visible,
    activities_will_hide,
    activities_hidden,
    signals_n
};

typedef struct _Activities {
    GObject parent_instance;
    AdwWindow *win;
    GtkRevealer *revealer;
    GtkWidget *container;

    // Search
    GtkSearchEntry *search_entry;
    GtkScrolledWindow *search_result_scrolled;
    GtkFlowBox *search_result_flbox;

    // App Carousel
    AdwCarousel *app_carousel;
    AdwCarouselIndicatorDots *app_carousel_dots;
    GPtrArray *app_carousel_pages;

    // Cached pixel buffer of the configured desktop wallpaper.
    GdkPixbuf *desktop_wallpaper;

    // window-manager.desktop-wallpayer setting
    GSettings *settings;
} Activities;
static guint activities_signals[signals_n] = {0};
G_DEFINE_TYPE(Activities, activities, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init and init methods.
static void activities_dispose(GObject *object) {
    G_OBJECT_CLASS(activities_parent_class)->dispose(object);
}

static void activities_finalize(GObject *object) {
    G_OBJECT_CLASS(activities_parent_class)->finalize(object);
}

static void activities_class_init(ActivitiesClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = activities_dispose;
    object_class->finalize = activities_finalize;

    activities_signals[activities_will_show] =
        g_signal_new("activities-will-show", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

    activities_signals[activities_visible] =
        g_signal_new("activities-visible", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

    activities_signals[activities_will_hide] =
        g_signal_new("activities-will-hide", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

    activities_signals[activities_hidden] =
        g_signal_new("activities-hidden", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void fill_app_infos(Activities *self) {
    // remove the current pages from the carousel and reset the pages array.
    for (guint i = 0; i < self->app_carousel_pages->len; i++) {
        GtkWidget *page = g_ptr_array_index(self->app_carousel_pages, i);
        adw_carousel_remove(self->app_carousel, page);
    }
    g_ptr_array_set_size(self->app_carousel_pages, 0);

    // empty search result box
    gtk_flow_box_remove_all(self->search_result_flbox);

    int page_size = 24;
    int cur_page = -1;
    int cur_row = 0;
    int cur_col = 0;
    int i = 0;
    GList *app_infos = g_app_info_get_all();

    for (GList *l = app_infos; l != NULL; l = l->next) {
        GAppInfo *app_info = G_APP_INFO(l->data);
        GtkGrid *page = NULL;

        GDesktopAppInfo *desktop_app_info = G_DESKTOP_APP_INFO(app_info);

        // perform sanity checks making sure this is an app we would actually
        // expect the user to launch
        if (!desktop_app_info) {
            continue;
        }
        if (g_desktop_app_info_get_nodisplay(desktop_app_info)) {
            continue;
        }
        if (g_strcmp0(g_desktop_app_info_get_string(desktop_app_info, "Type"),
                      "Application") != 0) {
            continue;
        }

        // Unfortunately sharing a single widget between both GtkFlowBox (Search
        // Results) and GtkGrid (App Carousel )does not work, so create one for
        // each.
        ActivitiesAppWidget *app_widget =
            g_object_new(ACTIVITIES_APP_WIDGET_TYPE, NULL);
        activities_app_widget_set_app_info(app_widget, app_info);

        ActivitiesAppWidget *app_widget_search =
            g_object_new(ACTIVITIES_APP_WIDGET_TYPE, NULL);
        activities_app_widget_set_app_info(app_widget_search, app_info);
        GtkWidget *w_search = activities_app_widget(app_widget_search);

        // add our search widget to the search flbox
        gtk_flow_box_insert(self->search_result_flbox, w_search, -1);

        // check if we should make a new page
        if (i % page_size == 0) {
            cur_page++;
            cur_row = 0;
            cur_col = 0;
            page = GTK_GRID(gtk_grid_new());
            g_ptr_array_insert(self->app_carousel_pages, cur_page, page);
        }
        page = GTK_GRID(g_ptr_array_index(self->app_carousel_pages, cur_page));

        gtk_grid_attach(page, activities_app_widget(app_widget), cur_col,
                        cur_row, 1, 1);

        cur_col++;
        if (cur_col == 7) {
            cur_row++;
            cur_col = 0;
        }
        i++;
    }

    // add every page to the carousel
    for (guint i = 0; i < self->app_carousel_pages->len; i++) {
        GtkWidget *page = g_ptr_array_index(self->app_carousel_pages, i);

        // deferring layout configuration of the GtkGrid seemed to be less buggy
        // then at creation time in the loop above.
        gtk_grid_set_column_spacing(GTK_GRID(page), 20);
        gtk_grid_set_row_spacing(GTK_GRID(page), 20);
        gtk_widget_set_halign(GTK_WIDGET(page), GTK_ALIGN_CENTER);
        gtk_widget_set_valign(GTK_WIDGET(page), GTK_ALIGN_START);
        gtk_widget_set_hexpand(GTK_WIDGET(page), true);
        gtk_widget_set_vexpand(GTK_WIDGET(page), true);

        adw_carousel_append(self->app_carousel, GTK_WIDGET(page));
    }
}

static void activities_init_layout(Activities *self);

static void on_window_destroy(GtkWindow *win, Activities *self) {
    g_debug("activities.c:on_window_destroy called");

    activities_init_layout(self);
}

static void on_revealer_child_revealed(GtkRevealer *revealer, GParamSpec *pspec,
                                       Activities *self) {
    g_debug("activities.c:on_revealer_child_revealed called");
    gboolean is_revealed = gtk_revealer_get_child_revealed(revealer);
    if (!is_revealed) {
        gtk_widget_set_visible(GTK_WIDGET(self->win), false);
        g_signal_emit(self, activities_signals[activities_hidden], 0);
    } else {
        g_signal_emit(self, activities_signals[activities_visible], 0);
    }
}

gboolean flow_box_filter_func(GtkFlowBoxChild *child, GtkSearchEntry *entry) {
    GtkWidget *w = gtk_flow_box_child_get_child(child);
    if (!w) return false;

    // get pointer to ActivitiesAppWidget from GtkWidget
    ActivitiesAppWidget *app_widget = activities_app_widget_from_widget(w);

    GAppInfo *info = activities_app_widget_get_app_info(app_widget);

    const char *display_name = g_app_info_get_display_name(info);
    const char *search_text = gtk_editable_get_text(GTK_EDITABLE(entry));

    gboolean match = g_str_match_string(search_text, display_name, true);
    return match;
}

static void on_search_next_match(GtkSearchEntry *entry, Activities *self) {
    GList *selected_list =
        gtk_flow_box_get_selected_children(self->search_result_flbox);
    if (!selected_list) return;

    GtkFlowBoxChild *selected = GTK_FLOW_BOX_CHILD(selected_list->data);
    if (!selected) return;

    GtkFlowBoxChild *next =
        GTK_FLOW_BOX_CHILD(gtk_widget_get_next_sibling(GTK_WIDGET(selected)));
    while (next) {
        if (gtk_widget_get_child_visible(GTK_WIDGET(next))) {
            gtk_flow_box_select_child(self->search_result_flbox, next);
            break;
        }
        next =
            GTK_FLOW_BOX_CHILD(gtk_widget_get_next_sibling(GTK_WIDGET(next)));
    }
}

static void on_search_previous_match(GtkSearchEntry *entry, Activities *self) {
    GList *selected_list =
        gtk_flow_box_get_selected_children(self->search_result_flbox);
    if (!selected_list) return;

    GtkFlowBoxChild *selected = GTK_FLOW_BOX_CHILD(selected_list->data);
    if (!selected) return;

    GtkFlowBoxChild *prev =
        GTK_FLOW_BOX_CHILD(gtk_widget_get_prev_sibling(GTK_WIDGET(selected)));
    while (prev) {
        if (gtk_widget_get_child_visible(GTK_WIDGET(prev))) {
            gtk_flow_box_select_child(self->search_result_flbox, prev);
            break;
        }
        prev =
            GTK_FLOW_BOX_CHILD(gtk_widget_get_prev_sibling(GTK_WIDGET(prev)));
    }
}

static void on_search_stop(GtkSearchEntry *entry, Activities *self) {
    // hide search results and show apps
    gtk_widget_set_visible(GTK_WIDGET(self->search_result_scrolled), false);
    gtk_widget_set_visible(GTK_WIDGET(self->app_carousel), true);
    gtk_widget_set_visible(GTK_WIDGET(self->app_carousel_dots), true);
}

static void on_search_changed(GtkSearchEntry *entry, Activities *self) {
    gtk_widget_set_visible(GTK_WIDGET(self->app_carousel), false);
    gtk_widget_set_visible(GTK_WIDGET(self->app_carousel_dots), false);

    const char *search_text = gtk_editable_get_text(GTK_EDITABLE(entry));
    // if search string is empty call on_search_stop
    if (strlen(search_text) == 0) {
        on_search_stop(entry, self);
        return;
    }

    // invalidate any search filter done before
    gtk_flow_box_invalidate_filter(self->search_result_flbox);

    // apply search filter
    gtk_flow_box_set_filter_func(self->search_result_flbox,
                                 (GtkFlowBoxFilterFunc)flow_box_filter_func,
                                 entry, NULL);

    // using Gtk4 syntax loop over GtkFlowBox children and find first visible
    // child.
    for (GtkWidget *child =
             gtk_widget_get_first_child(GTK_WIDGET(self->search_result_flbox));
         child; child = gtk_widget_get_next_sibling(child)) {
        // important to use 'gtk_widget_get_child_visible' after looking at
        // GtkFlowBox source just checking 'gtk_widget_get_visible' does not
        // work
        if (gtk_widget_get_child_visible(child)) {
            gtk_flow_box_select_child(self->search_result_flbox,
                                      GTK_FLOW_BOX_CHILD(child));
            break;
        }
    }

    // make search flow box visible
    gtk_widget_set_visible(GTK_WIDGET(self->search_result_scrolled), true);
}

static void on_search_entry_activate(GtkSearchEntry *entry, Activities *self) {
    GList *selected_list =
        gtk_flow_box_get_selected_children(self->search_result_flbox);
    // get first GtkFlowBoxChild in list
    GtkFlowBoxChild *selected = GTK_FLOW_BOX_CHILD(selected_list->data);
    if (!selected) return;

    GtkWidget *w = gtk_flow_box_child_get_child(selected);
    if (!w) return;

    // get pointer to ActivitiesAppWidget from GtkWidget
    ActivitiesAppWidget *app_widget = activities_app_widget_from_widget(w);

    // simulate click
    activities_app_widget_simulate_click(app_widget);
}

static void on_workspace_button_clicked(GtkButton *button, Activities *self) {
    g_debug("activities.c:on_workspace_button_clicked: called");

    WMServiceSway *sway = wm_service_sway_get_global();
    WMWorkspace *ws = g_object_get_data(G_OBJECT(button), "workspace");
    wm_service_sway_focus_workspace(sway, ws);

    activities_hide(self);
}

static void activities_init_layout(Activities *self) {
    self->win = ADW_WINDOW(adw_window_new());
    gtk_widget_set_hexpand(GTK_WIDGET(self->win), true);
    gtk_widget_set_vexpand(GTK_WIDGET(self->win), true);
    gtk_widget_set_name(GTK_WIDGET(self->win), "activities");

    // configure layershell, full screen overlay
    gtk_layer_init_for_window(GTK_WINDOW(self->win));
    gtk_layer_set_layer((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_anchor(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_LEFT,
                         true);
    gtk_layer_set_anchor(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_RIGHT,
                         true);
    gtk_layer_set_anchor(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_TOP, true);
    gtk_layer_set_anchor(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_BOTTOM,
                         true);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(self->win),
                                GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);

    // wire into window destroy event
    g_signal_connect(self->win, "destroy", G_CALLBACK(on_window_destroy), self);

    // main widget display
    self->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(GTK_WIDGET(self->container), "activities-container");

    // configure revealer that reveals activity widget
    self->revealer = GTK_REVEALER(gtk_revealer_new());
    gtk_revealer_set_transition_type(self->revealer,
                                     GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(self->revealer, 300);
    gtk_widget_set_hexpand(GTK_WIDGET(self->revealer), true);
    gtk_widget_set_vexpand(GTK_WIDGET(self->revealer), true);
    // hook into child-revealed property
    g_signal_connect(self->revealer, "notify::child-revealed",
                     G_CALLBACK(on_revealer_child_revealed), self);
    // add container as revealer's child
    gtk_revealer_set_child(self->revealer, self->container);

    // search bar entry container
    GtkBox *search_container =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_name(GTK_WIDGET(search_container), "search-container");
    gtk_widget_set_hexpand(GTK_WIDGET(search_container), true);

    // search bar entry
    self->search_entry = GTK_SEARCH_ENTRY(gtk_search_entry_new());
    gtk_widget_set_name(GTK_WIDGET(self->search_entry), "search-entry");
    gtk_widget_set_halign(GTK_WIDGET(self->search_entry), GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(GTK_WIDGET(self->search_entry), 480, 120);

    gtk_entry_set_placeholder_text(
        GTK_ENTRY(self->search_entry),
        "Search Applications...ctrl-g for next, ctrl-s-g for prev");

    // wire into search-changed signal for GtkSearchEntry
    g_signal_connect(self->search_entry, "search-changed",
                     G_CALLBACK(on_search_changed), self);

    // wire into stop-search signal for GtkSearchEntry
    g_signal_connect(self->search_entry, "stop-search",
                     G_CALLBACK(on_search_stop), self);

    // wire into 'activate' signal
    g_signal_connect(self->search_entry, "activate",
                     G_CALLBACK(on_search_entry_activate), self);

    // wire into 'next-match' signal
    g_signal_connect(self->search_entry, "next-match",
                     G_CALLBACK(on_search_next_match), self);

    // wire into 'previous-match' signal
    g_signal_connect(self->search_entry, "previous-match",
                     G_CALLBACK(on_search_previous_match), self);

    gtk_box_append(search_container, GTK_WIDGET(self->search_entry));

    // setup search result area bindings.
    self->search_result_flbox = GTK_FLOW_BOX(gtk_flow_box_new());
    gtk_flow_box_set_column_spacing(self->search_result_flbox, 20);
    gtk_flow_box_set_row_spacing(self->search_result_flbox, 20);
    gtk_widget_set_halign(GTK_WIDGET(self->search_result_flbox),
                          GTK_ALIGN_CENTER);
    gtk_widget_set_valign(GTK_WIDGET(self->search_result_flbox),
                          GTK_ALIGN_START);
    gtk_flow_box_set_homogeneous(self->search_result_flbox, true);

    // create app carousel and dots
    self->app_carousel = ADW_CAROUSEL(adw_carousel_new());
    self->app_carousel_dots =
        ADW_CAROUSEL_INDICATOR_DOTS(adw_carousel_indicator_dots_new());
    adw_carousel_indicator_dots_set_carousel(self->app_carousel_dots,
                                             self->app_carousel);

    // wire up main container
    gtk_box_append(GTK_BOX(self->container), GTK_WIDGET(search_container));

    // wrap search_result_flbox in a vertical scroll window
    self->search_result_scrolled =
        GTK_SCROLLED_WINDOW(gtk_scrolled_window_new());
    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(self->search_result_scrolled),
        GTK_WIDGET(self->search_result_flbox));
    gtk_widget_set_vexpand(GTK_WIDGET(self->search_result_scrolled), true);
    // starts hiden until a search takes place
    gtk_widget_set_visible(GTK_WIDGET(self->search_result_scrolled), false);

    gtk_box_append(GTK_BOX(self->container),
                   GTK_WIDGET(self->search_result_scrolled));
    gtk_box_append(GTK_BOX(self->container), GTK_WIDGET(self->app_carousel));
    gtk_box_append(GTK_BOX(self->container),
                   GTK_WIDGET(self->app_carousel_dots));

    fill_app_infos(self);

    adw_window_set_content(self->win, GTK_WIDGET(self->revealer));
}

static void activities_init(Activities *self) {
    self->app_carousel_pages = g_ptr_array_new();

    self->settings = g_settings_new("org.ldelossa.way-shell.window-manager");

    activities_init_layout(self);
}

void activities_show(Activities *self) {
    g_debug("activities.c:activities_show called");

    g_signal_emit(self, activities_signals[activities_will_show], 0);

    gtk_window_present(GTK_WINDOW(self->win));
    gtk_revealer_set_reveal_child(self->revealer, true);
    gtk_widget_grab_focus(GTK_WIDGET(self->search_entry));
}

void activities_hide(Activities *self) {
    g_debug("activities.c:activities_hide called");

    g_signal_emit(self, activities_signals[activities_will_hide], 0);

    gtk_revealer_set_reveal_child(self->revealer, false);

    gtk_editable_set_text(GTK_EDITABLE(self->search_entry), "");
    on_search_stop(self->search_entry, self);
}

void activities_toggle(Activities *self) {
    g_debug("activities.c:activities_toggle called");

    if (gtk_widget_get_visible(GTK_WIDGET(self->win)))
        activities_hide(self);
    else
        activities_show(self);
}

Activities *activities_get_global() { return global; }

void activities_activate(AdwApplication *app, gpointer user_data) {
    global = g_object_new(ACTIVITIES_TYPE, NULL);
}
