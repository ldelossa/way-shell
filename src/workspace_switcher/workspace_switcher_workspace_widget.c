#include "./workspace_switcher_workspace_widget.h"

#include <adwaita.h>

enum signals { signals_n };

typedef struct _WorkspaceSwitcherWorkspaceWidget {
    GObject parent_instance;
    GtkBox *container;
    GtkLabel *label;
} WorkspaceSwitcherWorkspaceWidget;
static guint workspace_switcher_workspace_widget_signals[signals_n] = {0};
G_DEFINE_TYPE(WorkspaceSwitcherWorkspaceWidget,
              workspace_switcher_workspace_widget, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init and init methods.
static void workspace_switcher_workspace_widget_dispose(GObject *object) {
    G_OBJECT_CLASS(workspace_switcher_workspace_widget_parent_class)
        ->dispose(object);
}

static void workspace_switcher_workspace_widget_finalize(GObject *object) {
    G_OBJECT_CLASS(workspace_switcher_workspace_widget_parent_class)
        ->finalize(object);
}

static void workspace_switcher_workspace_widget_class_init(
    WorkspaceSwitcherWorkspaceWidgetClass *class) {
    GObjectClass *object_class = G_OBJECT_CLASS(class);
    object_class->dispose = workspace_switcher_workspace_widget_dispose;
    object_class->finalize = workspace_switcher_workspace_widget_finalize;
}

static void workspace_switcher_workspace_widget_init_layout(
    WorkspaceSwitcherWorkspaceWidget *self) {
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->container), "switcher-entry");

    self->label = GTK_LABEL(gtk_label_new(""));
    gtk_box_append(self->container, GTK_WIDGET(self->label));
}

static void workspace_switcher_workspace_widget_init(
    WorkspaceSwitcherWorkspaceWidget *self) {
    workspace_switcher_workspace_widget_init_layout(self);
}

void workspace_switcher_workspace_widget_set_workspace_name(
    WorkspaceSwitcherWorkspaceWidget *self, const gchar *name) {
    gtk_label_set_text(self->label, name);
}

GtkWidget *workspace_switcher_workspace_widget_get_widget(
    WorkspaceSwitcherWorkspaceWidget *self) {
    return GTK_WIDGET(self->container);
}
