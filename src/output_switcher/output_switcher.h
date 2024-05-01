#pragma once

#include <adwaita.h>

#include "output_switcher_output_widget.h"

G_BEGIN_DECLS

struct _OutputSwitcher;
#define OUTPUT_SWITCHER_TYPE output_switcher_get_type()
G_DECLARE_FINAL_TYPE(OutputSwitcher, output_switcher, Output, Switcher,
                     GObject);

struct _OutputModel;
#define OUTPUT_MODEL_TYPE output_model_get_type()
G_DECLARE_FINAL_TYPE(OutputModel, output_model, Output, Model,
                     GObject);

G_END_DECLS

void output_switcher_activate(AdwApplication *app, gpointer user_data);

OutputSwitcher *output_switcher_get_global();

void output_switcher_show(OutputSwitcher *self);

void output_switcher_hide(OutputSwitcher *self);

void output_switcher_toggle(OutputSwitcher *self);
