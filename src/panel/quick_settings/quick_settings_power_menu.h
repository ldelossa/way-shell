#pragma once

#include <adwaita.h>
#include "gtk/gtkrevealer.h"

typedef struct _QuickSettingsPowerMenu {
    GtkRevealer *revealer;
    GtkBox *container;
    GtkBox *options_container;
    GtkBox *options_title;
    GtkBox *buttons_container;
    GtkButton *suspend;
    GtkButton *restart;
    GtkButton *power_off;
    GtkButton *logout;
} QuickSettingsPowerMenu;

void quick_settings_power_menu_init(QuickSettingsPowerMenu *menu);

void quick_settings_power_menu_free(QuickSettingsPowerMenu *menu);