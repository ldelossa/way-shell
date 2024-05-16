#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

struct _OSD;
#define OSD_TYPE osd_get_type()
G_DECLARE_FINAL_TYPE(OSD, osd, OSD, OSD, GObject);

G_END_DECLS

void osd_reinitialize(OSD *self);

void osd_activate(AdwApplication *app, gpointer user_data);

OSD *osd_get_global();

void osd_set_hidden(OSD *self);
