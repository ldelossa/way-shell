#include "./dialog_overlay.h"

#include <adwaita.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

static DialogOverlay *global = NULL;

enum signals { signals_n };
typedef struct _DialogOverlay {
    GObject parent_instance;
    AdwWindow *win;
    GtkRevealer *revealer;
    GtkBox *dialog;
    GtkLabel *heading;
    GtkLabel *body;
    GtkButton *confirm;
    GtkButton *cancel;
    GCallback pending_cb;
} DialogOverlay;
static guint dialog_overlay_signals[signals_n] = {0};
G_DEFINE_TYPE(DialogOverlay, dialog_overlay, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init and init methods.
static void dialog_overlay_dispose(GObject *object) {
    G_OBJECT_CLASS(dialog_overlay_parent_class)->dispose(object);
}

static void dialog_overlay_finalize(GObject *object) {
    G_OBJECT_CLASS(dialog_overlay_parent_class)->finalize(object);
}

static void dialog_overlay_class_init(DialogOverlayClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = dialog_overlay_dispose;
    object_class->finalize = dialog_overlay_finalize;
}

static void dialog_overlay_init_layout(DialogOverlay *self);

static void on_window_destroy(GtkWindow *win, DialogOverlay *self) {
    g_debug("activities.c:on_window_destroy called");
    gtk_widget_set_visible(GTK_WIDGET(self->win), false);
}

static void on_confirm_clicked(GtkButton *button, DialogOverlay *self) {
    g_debug("activities.c:on_confirm_clicked called");

    if (self->pending_cb) {
        self->pending_cb();
        self->pending_cb = NULL;
    }

    gtk_widget_set_visible(GTK_WIDGET(self->win), false);
}

static void on_cancel_clicked(GtkButton *button, DialogOverlay *self) {
    g_debug("activities.c:on_cancel_clicked called");
    gtk_widget_set_visible(GTK_WIDGET(self->win), false);
}

static void dialog_overlay_init_layout(DialogOverlay *self) {
    self->win = ADW_WINDOW(adw_window_new());
    gtk_widget_set_name(GTK_WIDGET(self->win), "dialog-overlay");

    // configure layershell, full screen overlay
    gtk_layer_init_for_window(GTK_WINDOW(self->win));
    gtk_layer_set_layer((GTK_WINDOW(self->win)), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_LEFT,
                         true);
    gtk_layer_set_anchor(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_RIGHT,
                         true);
    gtk_layer_set_anchor(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_TOP, true);
    gtk_layer_set_anchor(GTK_WINDOW(self->win), GTK_LAYER_SHELL_EDGE_BOTTOM,
                         true);
    // take keyboard exclusively so user cannot type during dialog
    gtk_layer_set_keyboard_mode(GTK_WINDOW(self->win),
                                GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);

    gtk_widget_set_visible(GTK_WIDGET(self->win), false);

    self->dialog = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->dialog), "dialog-overlay-container");
    gtk_widget_set_halign(GTK_WIDGET(self->dialog), GTK_ALIGN_CENTER);
    gtk_widget_set_valign(GTK_WIDGET(self->dialog), GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(GTK_WIDGET(self->dialog), 420, 200);

    self->heading = GTK_LABEL(gtk_label_new("heading"));
    gtk_widget_add_css_class(GTK_WIDGET(self->heading), "dialog-overlay-heading");
    gtk_widget_set_halign(GTK_WIDGET(self->heading), GTK_ALIGN_CENTER);
    gtk_widget_set_valign(GTK_WIDGET(self->heading), GTK_ALIGN_CENTER);
    gtk_label_set_xalign(self->heading, 0.5);

    self->body = GTK_LABEL(gtk_label_new("body"));
    gtk_widget_add_css_class(GTK_WIDGET(self->body), "dialog-overlay-body");
    gtk_widget_set_halign(GTK_WIDGET(self->body), GTK_ALIGN_CENTER);
    gtk_widget_set_valign(GTK_WIDGET(self->body), GTK_ALIGN_CENTER);
    gtk_label_set_xalign(self->body, 0.5);

    GtkCenterBox *buttons_container = GTK_CENTER_BOX(gtk_center_box_new());
    gtk_widget_set_hexpand(GTK_WIDGET(buttons_container), true);
    gtk_widget_set_vexpand(GTK_WIDGET(buttons_container), true);

    self->confirm = GTK_BUTTON(gtk_button_new_with_label("Confirm"));
    gtk_widget_add_css_class(GTK_WIDGET(self->confirm), "dialog-overlay-confirm");
    gtk_widget_set_hexpand(GTK_WIDGET(self->confirm), true);

    self->cancel = GTK_BUTTON(gtk_button_new_with_label("Cancel"));
    gtk_widget_add_css_class(GTK_WIDGET(self->cancel), "dialog-overlay-cancel");
    gtk_widget_set_hexpand(GTK_WIDGET(self->cancel), true);

    gtk_center_box_set_start_widget(buttons_container,
                                    GTK_WIDGET(self->cancel));
    gtk_center_box_set_end_widget(buttons_container, GTK_WIDGET(self->confirm));

    // wire up buttons to handlers
    g_signal_connect(self->confirm, "clicked", G_CALLBACK(on_confirm_clicked),
                     self);
    g_signal_connect(self->cancel, "clicked", G_CALLBACK(on_cancel_clicked),
                     self);

    // wire up dialog
    gtk_box_append(self->dialog, GTK_WIDGET(self->heading));
    gtk_box_append(self->dialog, GTK_WIDGET(self->body));
    gtk_box_append(self->dialog, GTK_WIDGET(buttons_container));

    // set adw window contents
    adw_window_set_content(self->win, GTK_WIDGET(self->dialog));

    // wire into window destroy event
    g_signal_connect(self->win, "destroy", G_CALLBACK(on_window_destroy), self);
}

void dialog_overlay_present(DialogOverlay *self, gchar *heading, gchar *body,
                            GCallback cb) {
    // set header
    gtk_label_set_text(self->heading, g_strdup(heading));
    // set body
    gtk_label_set_text(self->body, g_strdup(body));
    // set callback
    self->pending_cb = cb;

    // show dialog
    gtk_widget_set_visible(GTK_WIDGET(self->win), true);
}

static void dialog_overlay_init(DialogOverlay *self) {
    dialog_overlay_init_layout(self);
}

// This is the constructor for the DialogOverlay type.
void dialog_overlay_activate(AdwApplication *app, gpointer user_data) {
    DialogOverlay *self = g_object_new(DIALOG_OVERLAY_TYPE, NULL);
    global = self;
}

// This is the constructor for the DialogOverlay type.
DialogOverlay *dialog_overlay_get_global() { return global; }
