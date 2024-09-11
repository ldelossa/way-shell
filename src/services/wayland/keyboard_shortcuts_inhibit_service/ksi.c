#include "ksi.h"

#include <adwaita.h>

#include "../core.h"

static WaylandKSIService *global = NULL;

struct _WaylandKSIService {
    GObject parent_instance;
    WaylandCoreService *core;
    GdkToplevel *inhibitor;
};

G_DEFINE_TYPE(WaylandKSIService, wayland_ksi_service, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init methods for this GObject
static void wayland_ksi_service_dispose(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(wayland_ksi_service_parent_class)->dispose(gobject);
};

static void wayland_ksi_service_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(wayland_ksi_service_parent_class)->finalize(gobject);
};

static void wayland_ksi_service_class_init(WaylandKSIServiceClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = wayland_ksi_service_dispose;
    object_class->finalize = wayland_ksi_service_finalize;
};

static void wayland_ksi_service_init(WaylandKSIService *self) {};

int wayland_ksi_service_global_init(WaylandCoreService *core) {
    global = g_object_new(WAYLAND_KSI_SERVICE_TYPE, NULL);
    global->core = core;
    return 0;
};

WaylandKSIService *wayland_ksi_service_get_global() { return global; };

gboolean wayland_ksi_inhibit(WaylandKSIService *self, GtkWidget *widget) {
    // its actually easier to just go through GDK as it abstracts the Wayland
    // bits for us.
    g_debug("ksi.c:wayland_ksi_inhibit: called ");

    if (self->inhibitor) return false;

    GtkNative *gdk_native = gtk_widget_get_native(widget);
    if (!gdk_native) {
        g_critical(
            "ksi.c:wayland_ksi_inhibit: "
            "failed to get native widget");
        return false;
    }

    GdkSurface *gdk_surface = gtk_native_get_surface(gdk_native);
    if (!gdk_surface) {
        g_critical(
            "ksi.c:wayland_ksi_inhibit: "
            "failed to get GDK surface");
        return false;
    }

    if (!GDK_IS_TOPLEVEL(gdk_surface)) {
        g_critical(
            "ksi.c:wayland_ksi_inhibit: "
            "widget is not a GDK toplevel");
        return false;
    }

    g_debug(
        "ksi.c:wayland_ksi_inhibit(): "
        "inhibiting system shortcuts");

    GdkToplevel *toplevel = GDK_TOPLEVEL(gdk_surface);
    gdk_toplevel_inhibit_system_shortcuts(toplevel, NULL);
    self->inhibitor = toplevel;
    return true;
}

gboolean wayland_ksi_inhibit_destroy(WaylandKSIService *self) {
    if (!self->inhibitor) return false;
    gdk_toplevel_restore_system_shortcuts(self->inhibitor);
    self->inhibitor = NULL;
    return true;
}
