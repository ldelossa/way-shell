#include <adwaita.h>

G_BEGIN_DECLS

struct _QuickSettingsGrid;
#define QUICK_SETTINGS_GRID_TYPE quick_settings_grid_get_type()
G_DECLARE_FINAL_TYPE(QuickSettingsGrid, quick_settings_grid, QUICK_SETTINGS,
                     GRID, GObject);

G_END_DECLS

void quick_settings_grid_reinitialize(QuickSettingsGrid *grid);

GtkWidget *quick_settings_grid_get_widget(QuickSettingsGrid *grid);

void quick_settings_grid_add_setting(QuickSettingsGrid *grid, GtkWidget *w,
                                     GtkWidget *reveal);