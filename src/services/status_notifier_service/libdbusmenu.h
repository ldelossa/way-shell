#pragma once

#include <adwaita.h>

#include "status_notifier_service.h"

void libdbusmenu_parse_layout(GVariant *layout, GMenuItem *parent_menu_item,
                              StatusNotifierItem *item);
