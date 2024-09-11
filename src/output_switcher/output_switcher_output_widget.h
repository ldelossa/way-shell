
#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

struct _OutputSwitcherOutputWidget;
#define OUTPUT_SWITCHER_OUTPUT_WIDGET_TYPE \
    output_switcher_output_widget_get_type()
G_DECLARE_FINAL_TYPE(OutputSwitcherOutputWidget, output_switcher_output_widget,
                     OutputSwitcher, OutputWidget, GObject);

G_END_DECLS

void output_switcher_output_widget_set_output_name(
    OutputSwitcherOutputWidget *self, const gchar *name);

char *output_switcher_output_widget_get_output_name(
    OutputSwitcherOutputWidget *self);

GtkWidget *output_switcher_output_widget_get_widget(
    OutputSwitcherOutputWidget *self);
