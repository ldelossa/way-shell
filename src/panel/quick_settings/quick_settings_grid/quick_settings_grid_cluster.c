#include "quick_settings_grid_cluster.h"

#include <adwaita.h>

#include "gtk/gtkrevealer.h"
#include "quick_settings_grid_button.h"

enum signals { empty, remove_button_req, will_reveal, signals_n };

typedef struct _QuickSettingsGridCluster {
    GObject parent_instance;
    GtkBox *container;
    GtkCenterBox *center_box;
    QuickSettingsGridButton *left;
    QuickSettingsGridButton *right;
    GtkRevealer *left_revealer;
    GtkRevealer *right_revealer;
    QuickSettingsGridButton *dummy_left;
    QuickSettingsGridButton *dummy_right;
} QuickSettingsGridCluster;

static guint signals[signals_n] = {0};
G_DEFINE_TYPE(QuickSettingsGridCluster, quick_settings_grid_cluster,
              G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init methods
static void quick_settings_grid_cluster_dispose(GObject *gobject) {
    QuickSettingsGridCluster *self = QUICK_SETTINGS_GRID_CLUSTER(gobject);

    quick_settings_grid_button_free(self->left);
    quick_settings_grid_button_free(self->right);

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

    // emitted by a cluster right before it reveals one of its's buttons
    // reveal widgets.
    //
    // this is used primarily to instruct a QuickSettingsGrid that all other
    // cluster's must hide their revealed widgets.
    signals[will_reveal] = g_signal_new(
        "will_reveal", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0, NULL,
        NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);
};

int quick_settings_grid_cluster_add_button(
    QuickSettingsGridCluster *self, enum QuickSettingsGridClusterSide side,
    QuickSettingsGridButton *button, GtkWidget *reveal) {
    GtkRevealer *revealer = NULL;

    if (side == QUICK_SETTINGS_GRID_CLUSTER_NONE) return -1;

    if (side == QUICK_SETTINGS_GRID_CLUSTER_LEFT) {
        // we need to ref this or else the removal of the dummy will unref it
        // and the dummy button will be destroyed.
        g_object_ref(self->dummy_left->container);

        // ref the button we are about to display, it will be unref'd when it
        // is removed from the center box.
        g_object_ref(button->container);

        revealer = self->left_revealer;
        gtk_center_box_set_start_widget(self->center_box,
                                        GTK_WIDGET(button->container));
        self->left = button;
    } else {
        // see above comment
        g_object_ref(self->dummy_right->container);

        // see above comment
        g_object_ref(button->container);

        revealer = self->right_revealer;
        gtk_center_box_set_end_widget(self->center_box,
                                      GTK_WIDGET(button->container));
        self->right = button;
    }

    if (reveal) {
        gtk_revealer_set_reveal_child(revealer, false);
        gtk_revealer_set_child(revealer, reveal);
    }

    button->cluster = self;

    // keep a ref to the button's container
    g_object_ref(button->container);

    return 0;
}

static void slide_button_left(QuickSettingsGridCluster *self) {
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
    GtkRevealer *revealer = NULL;

    if (self->left == button) {
        // this will unref the object for us
        gtk_center_box_set_start_widget(
            self->center_box, GTK_WIDGET(self->dummy_left->container));
        revealer = self->left_revealer;
        self->left = self->dummy_left;

        // check if we have button on the right to slide left.
        slide_button_left(self);
    } else if (self->right == button) {
        // this will unref the object for us
        gtk_center_box_set_end_widget(self->center_box,
                                      GTK_WIDGET(self->dummy_right->container));
        revealer = self->right_revealer;
        self->right = self->dummy_right;
    }

    gtk_revealer_set_reveal_child(revealer, false);
    gtk_revealer_set_child(revealer, NULL);

    if (self->left == self->dummy_left && self->right == self->dummy_right) {
        g_signal_emit(self, signals[empty], 0);
    }

    return 0;
}

static void quick_settings_grid_cluster_init(QuickSettingsGridCluster *self) {
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    self->center_box = GTK_CENTER_BOX(gtk_center_box_new());
    gtk_box_append(self->container, GTK_WIDGET(self->center_box));

    self->left_revealer = GTK_REVEALER(gtk_revealer_new());
    gtk_revealer_set_transition_type(self->left_revealer,
                                     GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(self->left_revealer, 250);

    self->right_revealer = GTK_REVEALER(gtk_revealer_new());
    gtk_revealer_set_transition_type(self->right_revealer,
                                     GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(self->right_revealer, 250);

    // add transparent dummy buttons to keep spacing consistent.
    self->dummy_left =
        quick_settings_grid_button_new(QUICK_SETTINGS_BUTTON_GENERIC, "dummy",
                                       "dummy", "image-missing", false);
    gtk_widget_add_css_class(GTK_WIDGET(self->dummy_left->container),
                             "quick-settings-grid-button-transparent");
    self->dummy_right =
        quick_settings_grid_button_new(QUICK_SETTINGS_BUTTON_GENERIC, "dummy",
                                       "dummy", "image-missing", false);
    gtk_widget_add_css_class(GTK_WIDGET(self->dummy_right->container),
                             "quick-settings-grid-button-transparent");

    self->left = self->dummy_left;
    self->right = self->dummy_right;

    // add dummy to both start and end widgets
    gtk_center_box_set_start_widget(self->center_box,
                                    GTK_WIDGET(self->dummy_left->container));
    gtk_center_box_set_end_widget(self->center_box,
                                  GTK_WIDGET(self->dummy_right->container));

    // wire into our own 'remove_button_req' to handle emittions of this signal
    // from our buttons.
    g_signal_connect(self, "remove_button_req",
                     G_CALLBACK(quick_settings_grid_cluster_remove_button),
                     NULL);
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

GtkRevealer *quick_settings_grid_cluster_get_left_revealer(
    QuickSettingsGridCluster *self) {
    return self->left_revealer;
}

GtkRevealer *quick_settings_grid_cluster_get_right_revealer(
    QuickSettingsGridCluster *self) {
    return self->right_revealer;
}