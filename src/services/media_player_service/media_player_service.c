#include "media_player_service.h"

#include <adwaita.h>

#include "media_player_dbus.h"
#include "../../services/dbus_service.h"

static MediaPlayerService *global = NULL;

static gchar *player_object_path = "/org/mpris/MediaPlayer2";

enum signals { player_changed, player_removed, signals_n };

struct _MediaPlayerService {
    GObject parent_instance;
    GDBusConnection *conn;
    GHashTable *players_by_proxy;
    GHashTable *players_by_name;
    gboolean enabled;
};
static guint signals[signals_n] = {0};
G_DEFINE_TYPE(MediaPlayerService, media_player_service, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init methods
static void media_player_service_dispose(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(media_player_service_parent_class)->dispose(gobject);
};

static void media_player_service_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(media_player_service_parent_class)->finalize(gobject);
};

static void media_player_service_class_init(MediaPlayerServiceClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = media_player_service_dispose;
    object_class->finalize = media_player_service_finalize;

    // Sent when a player is added or an existing player's property is changed.
    signals[player_changed] = g_signal_new(
        "media-player-changed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);

    // Sent with a player is removed
    signals[player_removed] = g_signal_new(
        "media-player-removed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);
};

static void media_player_fill_metadata(GVariant *metadata,
                                       MediaPlayer *player) {
    GVariantIter *iter = g_variant_iter_new(metadata);
    GVariant *value;
    const gchar *key;
    while (g_variant_iter_next(iter, "{sv}", &key, &value)) {
        if (g_strcmp0(key, "xesam:album") == 0) {
            player->album = g_strdup(g_variant_get_string(value, NULL));
        }
        if (g_strcmp0(key, "xesam:title") == 0) {
            player->title = g_strdup(g_variant_get_string(value, NULL));
        }
        if (g_strcmp0(key, "xesam:artist") == 0) {
            GVariantIter *artist_iter = g_variant_iter_new(value);
            gchar *value;
			gchar *artist;

			g_variant_iter_next(artist_iter, "s", &artist);

            while (g_variant_iter_next(artist_iter, "s", &value)) {
				artist = g_strconcat(artist, ", ", value, NULL);
            }

            g_variant_iter_free(artist_iter);

			player->artist = artist;
        }
        if (g_strcmp0(key, "mpris:artUrl") == 0) {
            player->art_url = g_strdup(g_variant_get_string(value, NULL));
        }
        g_variant_unref(value);
    }
}

static void on_media_player_property_changed(
    DbusMediaPlayer2Player *player_proxy, GParamSpec *pspec,
    MediaPlayerService *self) {
    g_debug(
        "media_player_service.c:on_media_player_property_changed(): property "
        "changed: %s",
        pspec->name);

    MediaPlayer *player =
        g_hash_table_lookup(self->players_by_proxy, player_proxy);
    if (!player) return;

    // if update field is PlaybackStatus update it
    if (g_strcmp0(pspec->name, "playback-status") == 0) {
        g_free(player->playback_status);

        player->playback_status = g_strdup(
            dbus_media_player2_player_get_playback_status(player_proxy));

        g_debug(
            "media_player_service.c:on_media_player_property_changed(): "
            "[%s] playback status changed: %s",
            player->name, player->playback_status);
    }

    // if update field is metadata, update it
    if (g_strcmp0(pspec->name, "metadata") == 0) {
        GVariant *metadata =
            dbus_media_player2_player_get_metadata(player_proxy);

        media_player_fill_metadata(metadata, player);

        g_variant_unref(metadata);

        g_debug(
            "media_player_service.c:on_media_player_property_changed(): "
            "[%s] properties changed: "
            "metadata: album: %s, title: %s, art_url: %s",
            player->name, player->album, player->title, player->art_url);
    }

    // emit event
    g_signal_emit(self, signals[player_changed], 0, player);
}

static void media_player_added(gchar *name, const gchar *object_path,
                               MediaPlayerService *self) {
    g_debug(
        "media_player_service.c:media_player_added(): media player added: %s",
        name);

    GError *err = NULL;
    DbusMediaPlayer2 *mediaplayer2 = NULL;
    DbusMediaPlayer2Player *mediaplayer2_player = NULL;
    MediaPlayer *media_player = g_malloc0(sizeof(MediaPlayer));

    // instantiate interface org.mpris.MediaPlayer2 proxy
    mediaplayer2 =
        dbus_media_player2_proxy_new_sync(self->conn, G_DBUS_PROXY_FLAGS_NONE,
                                          name, player_object_path, NULL, &err);
    if (err) {
        g_warning("media_player_service.c:media_player_added(): error: %s",
                  err->message);
        g_error_free(err);
        return;
    }

    // instantiate interface org.mpris.MediaPlayer2.Player proxy
    mediaplayer2_player = dbus_media_player2_player_proxy_new_sync(
        self->conn, G_DBUS_PROXY_FLAGS_NONE, name, player_object_path, NULL,
        &err);
    if (err) {
        g_warning("media_player_service.c:media_player_added(): error: %s",
                  err->message);
        g_error_free(err);
        return;
    }

    // fill in our domain MediaPlayer object
    media_player->proxy = mediaplayer2;
    media_player->player = mediaplayer2_player;
    media_player->name = g_strdup(name);

    media_player->playback_status = g_strdup(
        dbus_media_player2_player_get_playback_status(mediaplayer2_player));

    media_player_fill_metadata(
        dbus_media_player2_player_get_metadata(mediaplayer2_player),
        media_player);

    g_debug(
        "media_player_service.c:media_player_added(): media player added: "
        "name: %s, playback_status: %s, album: %s, title: %s, art_url: %s",
        media_player->name, media_player->playback_status, media_player->album,
        media_player->title, media_player->art_url);

    // inventory new MediaPlayer both by MediaPlayer2.Player proxy pointer and
    // the owning service's name.
    g_hash_table_insert(self->players_by_proxy, mediaplayer2_player,
                        media_player);
    g_hash_table_insert(self->players_by_name, media_player->name,
                        media_player);

    // notify subscribers that new media player is available.
    g_signal_emit(self, signals[player_changed], 0, media_player);

    // watch for MediaPlayer2.Player property changes.
    g_signal_connect(mediaplayer2_player, "notify::playback-status",
                     G_CALLBACK(on_media_player_property_changed), self);
    g_signal_connect(mediaplayer2_player, "notify::metadata",
                     G_CALLBACK(on_media_player_property_changed), self);
}

static void media_player_removed(gchar *name, MediaPlayerService *self) {
    g_debug(
        "media_player_service.c:media_player_removed(): media player removed: "
        "%s",
        name);

    MediaPlayer *player = g_hash_table_lookup(self->players_by_name, name);
    if (!player) return;

    // kill signals for media's player proxy property change,
    // important to do this, it seems like another metadata event is sent when
    // a media player is destroyed, this would cause an issue if the metadata
    // handler is called with a dead player2 dbus proxy.
    g_signal_handlers_disconnect_by_func(
        player->player, on_media_player_property_changed, self);

    g_hash_table_remove(self->players_by_proxy, player->player);
    g_hash_table_remove(self->players_by_name, player->name);

    // emit event
    g_signal_emit(self, signals[player_removed], 0, player);
}

static void media_player_service_on_name_owner_changed(
    GDBusConnection *conn, const gchar *sender_name, const gchar *object_path,
    const gchar *interface_name, const gchar *signal_name, GVariant *parameters,
    gpointer user_data) {
    g_debug(
        "media_player_service.c:media_player_service_on_name_owner_changed(): "
        "NameOwnerChanged signal received");

    gchar *name;
    gchar *old_owner;
    gchar *new_owner;

    gboolean added = false;

    g_variant_get(parameters, "(&s&s&s)", &name, &old_owner, &new_owner);

    // if the name does not start with the prefix 'org.mpris.MediaPlayer2'
    // we can just return, we aren't interested
    if (!g_str_has_prefix(name, "org.mpris.MediaPlayer2")) {
        return;
    }
    g_debug(
        "media_player_service.c:media_player_service_on_name_owner_changed(): "
        "name: %s, old_owner: %s, new_owner: %s",
        name, old_owner, new_owner);

    // determine if this media player is being added or removed
    if (g_strcmp0(old_owner, "") == 0 && g_strcmp0(new_owner, "") != 0)
        added = true;
    else if (g_strcmp0(old_owner, "") != 0 && g_strcmp0(new_owner, "") == 0)
        added = false;

    g_debug(
        "media_player_service.c:media_player_service_on_name_owner_changed(): "
        "media player %s: %s",
        added ? "added" : "removed", name);

    if (added)
        media_player_added(name, object_path, user_data);
    else
        media_player_removed(name, user_data);
}

static void media_player_service_dbus_connect(MediaPlayerService *self) {
    GError *error = NULL;

    g_debug(
        "media_player_service.c:media_player_service_dbus_connect(): "
        "connecting to dbus");

	DBUSService *dbus = dbus_service_get_global();
	self->conn = dbus_service_get_session_bus(dbus);

    // we need to listen for NameOwnerChanged to determine if we see
    // service own or release name that starts with org.mpris.MediaPlayer2.
    g_dbus_connection_signal_subscribe(
        self->conn, "org.freedesktop.DBus", "org.freedesktop.DBus",
        "NameOwnerChanged", "/org/freedesktop/DBus", NULL,
        G_DBUS_SIGNAL_FLAGS_NONE, media_player_service_on_name_owner_changed,
        self, NULL);
}

static void media_player_service_init(MediaPlayerService *self) {
    g_debug(
        "media_player_service.c:media_player_service_init(): initializing "
        "media player service");

    self->players_by_proxy = g_hash_table_new(g_direct_hash, g_direct_equal);
    self->players_by_name = g_hash_table_new(g_str_hash, g_str_equal);

    media_player_service_dbus_connect(self);
}

int media_player_service_global_init(void) {
    if (!global) {
        global = g_object_new(MEDIA_PLAYER_SERVICE_TYPE, NULL);
    }

    return 0;
}

MediaPlayerService *media_player_service_get_global() { return global; }

void media_player_service_player_play_finish(GObject *source_object,
                                             GAsyncResult *res, gpointer data) {
    GError *err = NULL;

    dbus_media_player2_player_call_play_finish(
        (DbusMediaPlayer2Player *)source_object, res, &err);
}

void media_player_service_player_play(MediaPlayerService *self, gchar *name) {
    MediaPlayer *player = g_hash_table_lookup(self->players_by_name, name);
    if (!player) return;

    dbus_media_player2_player_call_play(
        player->player, NULL, media_player_service_player_play_finish, self);
}

void media_player_service_player_pause_finish(GObject *source_object,
                                              GAsyncResult *res,
                                              gpointer data) {
    GError *err = NULL;

    dbus_media_player2_player_call_pause_finish(
        (DbusMediaPlayer2Player *)source_object, res, &err);
}

void media_player_service_player_pause(MediaPlayerService *self, gchar *name) {
    MediaPlayer *player = g_hash_table_lookup(self->players_by_name, name);
    if (!player) return;

    dbus_media_player2_player_call_pause(
        player->player, NULL, media_player_service_player_pause_finish, self);
}

void media_player_service_player_playpause_finish(GObject *source_object,
                                                  GAsyncResult *res,
                                                  gpointer data) {
    GError *err = NULL;

    dbus_media_player2_player_call_play_pause_finish(
        (DbusMediaPlayer2Player *)source_object, res, &err);
}

void media_player_service_player_playpause(MediaPlayerService *self,
                                           gchar *name) {
    MediaPlayer *player = g_hash_table_lookup(self->players_by_name, name);
    if (!player) return;

    dbus_media_player2_player_call_play_pause(
        player->player, NULL, media_player_service_player_playpause_finish,
        self);
}

void media_player_service_player_stop_finish(GObject *source_object,
                                             GAsyncResult *res, gpointer data) {
    GError *err = NULL;

    dbus_media_player2_player_call_stop_finish(
        (DbusMediaPlayer2Player *)source_object, res, &err);
}

void media_player_service_player_stop(MediaPlayerService *self, gchar *name) {
    MediaPlayer *player = g_hash_table_lookup(self->players_by_name, name);
    if (!player) return;

    dbus_media_player2_player_call_stop(
        player->player, NULL, media_player_service_player_stop_finish, self);
}

void media_player_service_player_next_finish(GObject *source_object,
                                             GAsyncResult *res, gpointer data) {
    GError *err = NULL;

    dbus_media_player2_player_call_next_finish(
        (DbusMediaPlayer2Player *)source_object, res, &err);
}

void media_player_service_player_next(MediaPlayerService *self, gchar *name) {
    MediaPlayer *player = g_hash_table_lookup(self->players_by_name, name);
    if (!player) return;

    dbus_media_player2_player_call_next(
        player->player, NULL, media_player_service_player_next_finish, self);
}

void media_player_service_player_previous_finish(GObject *source_object,
                                                 GAsyncResult *res,
                                                 gpointer data) {
    GError *err = NULL;

    dbus_media_player2_player_call_previous_finish(
        (DbusMediaPlayer2Player *)source_object, res, &err);
}

void media_player_service_player_previous(MediaPlayerService *self,
                                          gchar *name) {
    MediaPlayer *player = g_hash_table_lookup(self->players_by_name, name);
    if (!player) return;

    dbus_media_player2_player_call_previous(
        player->player, NULL, media_player_service_player_previous_finish,
        self);
}

void media_player_service_player_raise_finish(GObject *source_object,
                                              GAsyncResult *res,
                                              gpointer data) {
    GError *err = NULL;

    dbus_media_player2_call_raise_finish((DbusMediaPlayer2 *)source_object, res,
                                         &err);
}

void media_player_service_player_raise(MediaPlayerService *self, gchar *name) {
    MediaPlayer *player = g_hash_table_lookup(self->players_by_name, name);
    if (!player) return;

    dbus_media_player2_call_raise(
        player->proxy, NULL, media_player_service_player_raise_finish, NULL);
}
