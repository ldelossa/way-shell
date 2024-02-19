#include "quick_settings_power_menu.h"

#include <adwaita.h>

#include "../../../services/logind_service/logind_service.h"
#include "../quick_settings.h"
#include "../quick_settings_mediator.h"
#include "../quick_settings_menu_widget.h"
#include "gtk/gtkrevealer.h"

enum signals { signals_n };

typedef struct _QuickSettingsPowerMenu {
    QuickSettingsMenuWidget menu;
    GtkButton *suspend;
    GtkButton *restart;
    GtkButton *power_off;
    GtkButton *logout;
    GSettings *settings;
    GPtrArray *revealer;
} QuickSettingsPowerMenu;
G_DEFINE_TYPE(QuickSettingsPowerMenu, quick_settings_power_menu, G_TYPE_OBJECT);

// stub out empty dispose, finalize, class_init, and init methods for this
// GObject.
static void quick_settings_power_menu_dispose(GObject *gobject) {
    g_debug(
        "quick_settings_power_menu.c:quick_settings_power_menu_dispose() "
        "called.");

    // Chain-up
    G_OBJECT_CLASS(quick_settings_power_menu_parent_class)->dispose(gobject);
};

static void quick_settings_power_menu_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(quick_settings_power_menu_parent_class)->finalize(gobject);
};

static void quick_settings_power_menu_class_init(
    QuickSettingsPowerMenuClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = quick_settings_power_menu_dispose;
    object_class->finalize = quick_settings_power_menu_finalize;
};

static void on_suspend_clicked(GtkButton *button,
                               QuickSettingsPowerMenu *self) {
    g_debug("quick_settings_power_menu.c:on_suspend_clicked():");

    gchar *suspend_method = NULL;
    GSettings *setting = g_settings_new("org.ldelossa.way-shell.system");

    suspend_method = g_settings_get_string(setting, "suspend-method");

    LogindService *logind = logind_service_get_global();
    QuickSettingsMediator *qs = quick_settings_get_global_mediator();
    quick_settings_mediator_req_close(qs);

    // check if suspention method is actually available from logind
    if (strcmp(suspend_method, "suspend") == 0) {
        if (logind_service_can_suspend(logind)) {
            logind_service_suspend(logind);
            return;
        }
    } else if (strcmp(suspend_method, "hibernate") == 0) {
        if (logind_service_can_hibernate(logind)) {
            logind_service_hibernate(logind);
            return;
        }
    } else if (strcmp(suspend_method, "hybrid-sleep") == 0) {
        if (logind_service_can_hybrid_sleep(logind)) {
            logind_service_hybrid_sleep(logind);
            return;
        }
    } else if (strcmp(suspend_method, "suspend-then-hibernate") == 0) {
        if (logind_service_can_suspendthenhibernate(logind)) {
            logind_service_suspendthenhibernate(logind);
            return;
        }
    }

    // if we got here the desired suspend method does not exist, try each
    // suspend in our priority
    if (logind_service_can_suspend(logind)) {
        logind_service_suspend(logind);
        return;
    } else if (logind_service_can_hybrid_sleep(logind)) {
        logind_service_hybrid_sleep(logind);
        return;
    } else if (logind_service_can_suspendthenhibernate(logind)) {
        logind_service_suspendthenhibernate(logind);
        return;
    } else if (logind_service_can_hibernate(logind)) {
        logind_service_hibernate(logind);
        return;
    }
}

static void on_restart_clicked(GtkButton *button,
                               QuickSettingsPowerMenu *self) {
    g_debug("quick_settings_power_menu.c:on_restart_clicked():");

    LogindService *logind = logind_service_get_global();
    QuickSettingsMediator *qs = quick_settings_get_global_mediator();
    quick_settings_mediator_req_close(qs);

    if (logind_service_can_reboot(logind)) {
        logind_service_reboot(logind);
    }
}

static void on_poweroff_clicked(GtkButton *button,
                                QuickSettingsPowerMenu *self) {
    g_debug("quick_settings_power_menu.c:on_poweroff_clicked():");

    LogindService *logind = logind_service_get_global();
    QuickSettingsMediator *qs = quick_settings_get_global_mediator();
    quick_settings_mediator_req_close(qs);

    if (logind_service_can_power_off(logind)) {
        logind_service_power_off(logind);
    }
}

static void on_logout_clicked(GtkButton *button, QuickSettingsPowerMenu *self) {
    g_debug("quick_settings_power_menu.c:on_logout_clicked():");

    LogindService *logind = logind_service_get_global();
    QuickSettingsMediator *qs = quick_settings_get_global_mediator();
    quick_settings_mediator_req_close(qs);

    logind_service_kill_session(logind);
}

static void on_power_button_clicked(GtkButton *button, GtkRevealer *revealer) {
    if (!gtk_revealer_get_reveal_child(revealer)) {
        gtk_revealer_set_reveal_child(revealer, TRUE);
    } else {
        gtk_revealer_set_reveal_child(revealer, FALSE);
    }
}

static void on_revealer_reveal_child(GtkRevealer *revealer, GParamSpec *pspec,
                                     QuickSettingsPowerMenu *self) {
    if (gtk_revealer_get_reveal_child(revealer)) {
        for (guint i = 0; i < self->revealer->len; i++) {
            GtkRevealer *r = g_ptr_array_index(self->revealer, i);
            if (r != revealer) {
                gtk_revealer_set_reveal_child(r, FALSE);
            }
        }
    }
}

GtkWidget *make_power_button(QuickSettingsPowerMenu *self, gchar *title,
                             GCallback cb) {
    GtkBox *container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    // make a gtk revealer
    GtkWidget *revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(revealer),
                                     GTK_REVEALER_TRANSITION_TYPE_SWING_DOWN);
    gtk_revealer_set_transition_duration(GTK_REVEALER(revealer), 250);
    gtk_widget_set_hexpand(revealer, TRUE);

    // append it to our revealers
    g_ptr_array_add(self->revealer, revealer);

    // wire gtk revealer's reveal notify signal
    g_signal_connect(revealer, "notify::reveal-child",
                     G_CALLBACK(on_revealer_reveal_child), self);

    // make button
    GtkWidget *button = gtk_button_new_with_label(title);
    gtk_label_set_xalign(GTK_LABEL(gtk_button_get_child(GTK_BUTTON(button))),
                         0.0);
    gtk_widget_set_hexpand(button, TRUE);

    // on click wire into power button click
    g_signal_connect(button, "clicked", G_CALLBACK(on_power_button_clicked),
                     revealer);

    // make confirm button
    GtkWidget *confirm_button = gtk_button_new_with_label("Confirm");
    gtk_widget_add_css_class(confirm_button, "confirm-button");

    gtk_widget_set_hexpand(confirm_button, TRUE);

    g_signal_connect(confirm_button, "clicked", cb, NULL);

    // confirm button is revealer's child
    gtk_revealer_set_child(GTK_REVEALER(revealer), confirm_button);

    // button first and then revealer and child of container
    gtk_box_append(container, button);
    gtk_box_append(container, revealer);

    return GTK_WIDGET(container);
}

static void on_quick_settings_hidden(QuickSettingsMediator *mediator,
                                     QuickSettings *qs, GdkMonitor *mon,
                                     QuickSettingsPowerMenu *self) {
    g_debug("quick_settings_power_menu.c:on_quick_settings_hidden() called.");

    for (guint i = 0; i < self->revealer->len; i++) {
        GtkRevealer *r = g_ptr_array_index(self->revealer, i);
        gtk_revealer_set_reveal_child(r, FALSE);
    }
}

static void quick_settings_power_menu_init_layout(
    QuickSettingsPowerMenu *self) {
    g_debug(
        "quick_settings_power_menu.c:quick_settings_power_menu_init() "
        "called.");

    quick_settings_menu_widget_init(&self->menu, false);
    quick_settings_menu_widget_set_title(&self->menu, "Power Off");
    quick_settings_menu_widget_set_icon(&self->menu,
                                        "system-shutdown-symbolic");

    // create power buttons
    GtkWidget *suspend_button =
        make_power_button(self, "Suspend", G_CALLBACK(on_suspend_clicked));

    GtkWidget *reset_button =
        make_power_button(self, "Restart", G_CALLBACK(on_restart_clicked));

    GtkWidget *power_off_button =
        make_power_button(self, "Power Off", G_CALLBACK(on_poweroff_clicked));

    GtkWidget *logout_button =
        make_power_button(self, "Log Out...", G_CALLBACK(on_logout_clicked));

    // wire it up
    gtk_box_append(self->menu.options, GTK_WIDGET(suspend_button));
    gtk_box_append(self->menu.options, GTK_WIDGET(reset_button));
    gtk_box_append(self->menu.options, GTK_WIDGET(power_off_button));

    // append separator
    GtkSeparator *separator =
        GTK_SEPARATOR(gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_widget_set_margin_start(GTK_WIDGET(separator), 12);
    gtk_box_append(self->menu.options, GTK_WIDGET(logout_button));

    // wire into quick settings hidden event and close all revealers
    QuickSettingsMediator *qs = quick_settings_get_global_mediator();
    g_signal_connect(qs, "quick-settings-hidden",
                     G_CALLBACK(on_quick_settings_hidden), self);
}

void quick_settings_power_menu_reinitialize(QuickSettingsPowerMenu *self) {
    // kill signals to quick settings mediator
    QuickSettingsMediator *qs = quick_settings_get_global_mediator();
    g_signal_handlers_disconnect_by_func(qs, on_quick_settings_hidden, self);

    // if we got here, all the revealers are dead because the parent component
    // was destroyed, so just set array size to zero
    g_ptr_array_set_size(self->revealer, 0);

    quick_settings_power_menu_init_layout(self);
}

static void quick_settings_power_menu_init(QuickSettingsPowerMenu *self) {
    self->revealer = g_ptr_array_new();

    quick_settings_power_menu_init_layout(self);
}

GtkWidget *quick_settings_power_menu_get_widget(QuickSettingsPowerMenu *self) {
    return GTK_WIDGET(self->menu.container);
}