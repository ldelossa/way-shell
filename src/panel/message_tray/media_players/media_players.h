#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

// The top-of-screen Panel which exists on each monitor of the current
// GdkSeat.
struct _MediaPlayers;
#define MEDIA_PLAYERS_TYPE media_players_get_type()
G_DECLARE_FINAL_TYPE(MediaPlayers, media_players, MEDIA, PLAYERS, GObject);

G_END_DECLS

GtkWidget *media_players_get_widget(MediaPlayers *self);
