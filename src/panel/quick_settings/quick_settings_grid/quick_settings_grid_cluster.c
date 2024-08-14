#include "quick_settings_grid_cluster.h"

#include <adwaita.h>

#include "../quick_settings.h"
#include "gtk/gtk.h"
#include "quick_settings_grid_button.h"

enum signals { empty, removed, remove_button_req, will_reveal, signals_n };

typedef struct _QuickSettingsGridCluster {
    GObject parent_instance;
    GtkBox *container;
    GtkCenterBox *center_box;
    QuickSettingsGridButton *left;
    QuickSettingsGridButton *right;
    QuickSettingsGridButton *dummy_left;
    QuickSettingsGridButton *dummy_right;
    QuickSettingsClusterOnRevealFunc on_reveal_left;
    QuickSettingsClusterOnRevealFunc on_reveal_right;
} QuickSettingsGridCluster;

static guint signals[signals_n] = {0};
G_DEFINE_TYPE(QuickSettingsGridCluster, quick_settings_grid_cluster,
              G_TYPE_OBJECT);

void quick_settings_grid_cluster_hide_all(QuickSettingsGridCluster *self) {
    if (self->left->revealer)
        gtk_revealer_set_reveal_child(self->left->revealer, false);
    if (self->right->revealer)
        gtk_revealer_set_reveal_child(self->right->revealer, false);
}

static void on_quick_settings_hidden(QuickSettings *qs,
                                     QuickSettingsGridCluster *self) {
    g_debug("quick_settings_grid_cluster.c:on_quick_settings_hidden() called");
    quick_settings_grid_cluster_hide_all(self);
}

// stub out dispose, finalize, class_init, and init methods
static void quick_settings_grid_cluster_dispose(GObject *gobject) {
    QuickSettingsGridCluster *self = QUICK_SETTINGS_GRID_CLUSTER(gobject);

    // disconnect from quick settings global mediator signal
    QuickSettings *qs = quick_settings_get_global();

    g_signal_handlers_disconnect_by_func(
        qs, G_CALLBACK(on_quick_settings_hidden), self);

    if (self->left != self->dummy_left) {
        quick_settings_grid_button_free(self->left);
    }
    if (self->right != self->dummy_left) {
        quick_settings_grid_button_free(self->right);
    }

    quick_settings_grid_button_free(self->dummy_left);
    quick_settings_grid_button_free(self->dummy_right);

    // Chain-up
    G_OBJECT_CLASS(quick_settings_grid_cluster_parent_class)->dispose(gobject);
};

static void quick_settings_grid_cluster_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(quick_settings_grid_cluster_parent_class)->finalize(gobject);
};

static void quick_settings_grid_cluster_class_init(
    QuickSettingsGridClusterClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = quick_settings_grid_cluster_dispose;
    object_class->finalize = quick_settings_grid_cluster_finalize;

    // emmitted when a cluster has both buttons removed.
    //
    // primarily used to tell a QuickSettingsGrid to remove this cluster from
    // their inventory.
    signals[empty] =
        g_signal_new("empty", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0,
                     NULL, NULL, NULL, G_TYPE_NONE, 0);

    // emitted by a button within the cluster instructing the cluster to remove
    // itself.
    //
    // The single argument pointer can be compared with
    // QuickSettingsGridCluster.left and .right fields to determine which button
    // to remove.
    signals[remove_button_req] = g_signal_new(
        "remove_button_req", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);

    // emitted by a cluster when it removes one of its buttons.
    //
    // this is used primarily to inform the QuickSettingsGrid that it should
    // compact the grid.
    signals[removed] =
        g_signal_new("removed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0,
                     NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_INT);

    // emitted by a cluster right before it reveals one of its's buttons
    // reveal widgets.
    //
    // this is used primarily to instruct a QuickSettingsGrid that all other
    // cluster's must hide their revealed widgets.
    signals[will_reveal] = g_signal_new("will_reveal", G_TYPE_FROM_CLASS(klass),
                                        G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL,
                                        G_TYPE_NONE, 1, G_TYPE_POINTER);
};

static void on_will_reveal(QuickSettingsGridCluster *self,
                           QuickSettingsGridButton *button) {
    g_debug("quick_settings_grid_cluster.c:on_will_reveal() called");

    if (self->right == button) {
        if (self->left->revealer) {
            gtk_revealer_set_reveal_child(self->left->revealer, false);
        }
    }

    if (self->left == button) {
        if (self->right->revealer) {
            gtk_revealer_set_reveal_child(self->right->revealer, false);
        }
    }
}

int quick_settings_grid_cluster_add_button(
    QuickSettingsGridCluster *self, enum QuickSettingsGridClusterSide side,
    QuickSettingsGridButton *button) {
    if (side == QUICK_SETTINGS_GRID_CLUSTER_NONE) return -1;

    if (side == QUICK_SETTINGS_GRID_CLUSTER_LEFT) {
        // we need to ref this or else the removal of the dummy will unref it
        // and the dummy button will be destroyed.
        g_object_ref(self->dummy_left->container);

        // ref the button we are about to display, it will be unref'd when it
        // is removed from the center box.
        g_object_ref(button->container);

        gtk_center_box_set_start_widget(self->center_box,
                                        GTK_WIDGET(button->container));

        self->on_reveal_left = button->on_reveal;

        self->left = button;

    } else {
        // see above comment
        g_object_ref(self->dummy_right->container);

        // see above comment
        g_object_ref(button->container);

        gtk_center_box_set_end_widget(self->center_box,
                                      GTK_WIDGET(button->container));

        self->on_reveal_right = button->on_reveal;

        self->right = button;
    }

    button->cluster = self;

    // keep a ref to the button's container
    g_object_ref(button->container);

    // if button has a GtkRevealer append it to container
    if (button->revealer) {
        gtk_box_append(self->container, GTK_WIDGET(button->revealer));
    }

    return 0;
}

void quick_settings_grid_cluster_slide_left(QuickSettingsGridCluster *self) {
    // we will only slide non-dummy buttons.
    if (self->right == self->dummy_right) {
        return;
    }

    g_object_ref(self->left->container);
    g_object_ref(self->right->container);

    // set end widget to a right dummy
    gtk_center_box_set_end_widget(self->center_box,
                                  GTK_WIDGET(self->dummy_right->container));

    // set left widget to our right button
    gtk_center_box_set_start_widget(self->center_box,
                                    GTK_WIDGET(self->right->container));

    // our right button is now our left button
    self->left = self->right;

    // our right button is now a dummy
    self->right = self->dummy_right;
}

int quick_settings_grid_cluster_remove_button(QuickSettingsGridCluster *self,
                                              QuickSettingsGridButton *button) {
    enum QuickSettingsGridClusterSide side = QUICK_SETTINGS_GRID_CLUSTER_NONE;

    if (self->left == button) {
        side = QUICK_SETTINGS_GRID_CLUSTER_LEFT;

        // ref the start widget, unref'ing it is the callers descision as they
        // may be removing this button just to move it somewhere else.
        g_object_ref(gtk_center_box_get_start_widget(self->center_box));

        gtk_center_box_set_start_widget(
            self->center_box, GTK_WIDGET(self->dummy_left->container));

        self->left = self->dummy_left;

        // check if we have button on the right and slide it left.
        quick_settings_grid_cluster_slide_left(self);

    } else if (self->right == button) {
        side = QUICK_SETTINGS_GRID_CLUSTER_RIGHT;

        // this will unref the object for us
        gtk_center_box_set_end_widget(self->center_box,
                                      GTK_WIDGET(self->dummy_right->container));
        self->right = self->dummy_right;
    }

    // remove revealer if present, don't kill the ref incase this is just a
    // button move.
    if (button->revealer) {
        g_object_ref(button->revealer);
        gtk_box_remove(self->container, GTK_WIDGET(button->revealer));
    }

    g_signal_emit(self, signals[removed], 0, side);

    return 0;
}

static void quick_settings_grid_cluster_init_layout(
    QuickSettingsGridCluster *self) {
    // create main container
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));

    // create center box for buttons
    self->center_box = GTK_CENTER_BOX(gtk_center_box_new());
    gtk_box_append(self->container, GTK_WIDGET(self->center_box));

    // add transparent dummy buttons to keep spacing consistent.
    self->dummy_left =
        quick_settings_grid_button_new(QUICK_SETTINGS_BUTTON_GENERIC, "dummy",
                                       "dummy", "image-missing", NULL, NULL);
    gtk_widget_add_css_class(GTK_WIDGET(self->dummy_left->container),
                             "quick-settings-grid-button-transparent");
    self->dummy_right =
        quick_settings_grid_button_new(QUICK_SETTINGS_BUTTON_GENERIC, "dummy",
                                       "dummy", "image-missing", NULL, NULL);
    gtk_widget_add_css_class(GTK_WIDGET(self->dummy_right->container),
                             "quick-settings-grid-button-transparent");

    self->left = self->dummy_left;
    self->right = self->dummy_right;

    // add dummy to both start and end widgets
    gtk_center_box_set_start_widget(self->center_box,
                                    GTK_WIDGET(self->dummy_left->container));
    gtk_center_box_set_end_widget(self->center_box,
                                  GTK_WIDGET(self->dummy_right->container));

    // connect to quick settings mediator's hidden event
    QuickSettings *qs = quick_settings_get_global();
    g_signal_connect(qs, "quick-settings-hidden",
                     G_CALLBACK(on_quick_settings_hidden), self);

    // wire into our own 'remove_button_req' to handle emittions of this signal
    // from our buttons.
    g_signal_connect(self, "remove_button_req",
                     G_CALLBACK(quick_settings_grid_cluster_remove_button),
                     NULL);

    // connect to own's will-reveal signal
    g_signal_connect(self, "will-reveal", G_CALLBACK(on_will_reveal), NULL);
}

static void quick_settings_grid_cluster_init(QuickSettingsGridCluster *self) {
    quick_settings_grid_cluster_init_layout(self);
}

GtkWidget *quick_settings_grid_cluster_get_widget(
    QuickSettingsGridCluster *self) {
    return GTK_WIDGET(self->container);
}

gboolean quick_settings_grid_cluster_is_full(QuickSettingsGridCluster *self) {
    if (self->left != self->dummy_left && self->right != self->dummy_right)
        return true;
    return false;
}

enum QuickSettingsGridClusterSide quick_settings_grid_cluster_vacant_side(
    QuickSettingsGridCluster *self) {
    if (self->left == self->dummy_left && self->right == self->dummy_right)
        return QUICK_SETTINGS_GRID_CLUSTER_BOTH;
    if (self->left == self->dummy_left) return QUICK_SETTINGS_GRID_CLUSTER_LEFT;
    if (self->right == self->dummy_right)
        return QUICK_SETTINGS_GRID_CLUSTER_RIGHT;

    return QUICK_SETTINGS_GRID_CLUSTER_NONE;
}

GtkCenterBox *quick_settings_grid_cluster_get_center_box(
    QuickSettingsGridCluster *self) {
    return self->center_box;
}

QuickSettingsGridButton *quick_settings_grid_cluster_get_left_button(
    QuickSettingsGridCluster *self) {
    return self->left;
}

QuickSettingsGridButton *quick_settings_grid_cluster_get_right_button(
    QuickSettingsGridCluster *self) {
    return self->right;
}
