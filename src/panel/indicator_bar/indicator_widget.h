#pragma once

#include <adwaita.h>

#include "../../services/status_notifier_service/status_notifier_service.h"

G_BEGIN_DECLS

struct _IndicatorWidget;
#define INDICATOR_WIDGET_TYPE indicator_widget_get_type()
G_DECLARE_FINAL_TYPE(IndicatorWidget, indicator_widget, INDICATOR, WIDGET,
                     GObject);

G_END_DECLS

void indicator_widget_set_sni(IndicatorWidget *self, StatusNotifierItem *sni);

StatusNotifierItem *indicator_widget_get_sni(IndicatorWidget *self);

GtkWidget *indicator_widget_get_widget(IndicatorWidget *self);
