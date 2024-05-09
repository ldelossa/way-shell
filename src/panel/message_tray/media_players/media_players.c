#include "media_players.h"

#include <adwaita.h>

#include "../../../services/media_player_service/media_player_service.h"

typedef struct _MediaPlayerWidget {
    int index;
    gchar *name;
    gchar *identity;
    GtkLabel *artist;
    GtkLabel *title;
    GtkCenterBox *container;
    GtkButton *icon;
    GtkButton *play_pause;
    GtkButton *previous;
    GtkButton *next;
} MediaPlayerWidget;

static void media_player_widget_on_playpause_clicked(GtkButton *button,
                                                     gpointer user_data) {
    MediaPlayerWidget *self = (MediaPlayerWidget *)user_data;
    MediaPlayerService *srv = media_player_service_get_global();
    media_player_service_player_playpause(srv, self->name);
}

static void media_player_widget_on_previous_clicked(GtkButton *button,
                                                    gpointer user_data) {
    MediaPlayerWidget *self = (MediaPlayerWidget *)user_data;
    MediaPlayerService *srv = media_player_service_get_global();
    media_player_service_player_previous(srv, self->name);
}

static void media_player_widget_on_next_clicked(GtkButton *button,
                                                gpointer user_data) {
    MediaPlayerWidget *self = (MediaPlayerWidget *)user_data;
    MediaPlayerService *srv = media_player_service_get_global();
    media_player_service_player_next(srv, self->name);
}

static void media_player_widget_on_raise(GtkButton *button,
                                         gpointer user_data) {
    MediaPlayerWidget *self = (MediaPlayerWidget *)user_data;
    MediaPlayerService *srv = media_player_service_get_global();
    media_player_service_player_raise(srv, self->name);
}

MediaPlayerWidget *media_player_widget_new() {
    MediaPlayerWidget *self = g_new0(MediaPlayerWidget, 1);

    self->container = GTK_CENTER_BOX(gtk_center_box_new());
    gtk_widget_add_css_class(GTK_WIDGET(self->container), "media-player");
    gtk_widget_set_hexpand(GTK_WIDGET(self->container), TRUE);

    self->icon = GTK_BUTTON(
        gtk_button_new_from_icon_name("applications-multimedia-symbolic"));
    gtk_widget_set_hexpand(GTK_WIDGET(self->icon), TRUE);
    gtk_widget_set_size_request(GTK_WIDGET(self->icon), 60, -1);
    gtk_image_set_pixel_size(
        GTK_IMAGE(gtk_widget_get_first_child(GTK_WIDGET(self->icon))), 48);

    GtkBox *button_container =
        GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_hexpand(GTK_WIDGET(button_container), TRUE);

    self->play_pause = GTK_BUTTON(
        gtk_button_new_from_icon_name("media-playback-pause-symbolic"));
    self->previous =
        GTK_BUTTON(gtk_button_new_from_icon_name("go-previous-symbolic"));
    self->next = GTK_BUTTON(gtk_button_new_from_icon_name("go-next-symbolic"));
    gtk_box_append(button_container, GTK_WIDGET(self->previous));
    gtk_box_append(button_container, GTK_WIDGET(self->play_pause));
    gtk_box_append(button_container, GTK_WIDGET(self->next));

    // wire up buttons
    g_signal_connect(self->icon, "clicked",
                     G_CALLBACK(media_player_widget_on_raise), self);
    g_signal_connect(self->play_pause, "clicked",
                     G_CALLBACK(media_player_widget_on_playpause_clicked),
                     self);
    g_signal_connect(self->previous, "clicked",
                     G_CALLBACK(media_player_widget_on_previous_clicked), self);
    g_signal_connect(self->next, "clicked",
                     G_CALLBACK(media_player_widget_on_next_clicked), self);

    GtkBox *label_container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(label_container),
                             "media-player-labels");
    gtk_widget_set_hexpand(GTK_WIDGET(label_container), TRUE);
    gtk_widget_set_valign(GTK_WIDGET(label_container), GTK_ALIGN_CENTER);

    self->artist = GTK_LABEL(gtk_label_new(""));
    gtk_widget_add_css_class(GTK_WIDGET(self->artist), "media-player-artist");
    gtk_label_set_ellipsize(self->artist, PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(self->artist, 15);
    gtk_label_set_width_chars(self->artist, 12);
    gtk_label_set_xalign(self->artist, 0);

    self->title = GTK_LABEL(gtk_label_new(""));
    gtk_widget_add_css_class(GTK_WIDGET(self->title), "media-player-title");
    gtk_label_set_ellipsize(self->title, PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(self->title, 15);
    gtk_label_set_width_chars(self->title, 12);
    gtk_label_set_xalign(self->title, 0);
    gtk_box_append(label_container, GTK_WIDGET(self->title));
    gtk_box_append(label_container, GTK_WIDGET(self->artist));

    gtk_center_box_set_start_widget(self->container, GTK_WIDGET(self->icon));
    gtk_center_box_set_center_widget(self->container,
                                     GTK_WIDGET(label_container));
    gtk_center_box_set_end_widget(self->container,
                                  GTK_WIDGET(button_container));

    return self;
}

static void media_player_widget_free(MediaPlayerWidget *self) {
    g_free(self->name);
    g_free(self);
}

static void media_player_widget_fill(MediaPlayerWidget *self,
                                     MediaPlayer *player) {
    // set name
    self->name = g_strdup(player->name);

    self->identity = g_strdup(player->identity);

    // update play/pause icon depending on playback state
    if (g_strcmp0(player->playback_status, "Playing") == 0) {
        gtk_button_set_icon_name(self->play_pause,
                                 "media-playback-pause-symbolic");
    } else {
        gtk_button_set_icon_name(self->play_pause,
                                 "media-playback-start-symbolic");
    }
    gtk_label_set_text(self->artist, player->artist);
    gtk_label_set_text(self->title, player->title);
}

struct _MediaPlayers {
    GObject parent_instance;
    GtkBox *container;
    GtkScrolledWindow *scrolled_win;
    GtkBox *media_player_list;
    GHashTable *media_players;
};
G_DEFINE_TYPE(MediaPlayers, media_players, G_TYPE_OBJECT);

typedef struct pending_update {
    MediaPlayers *self;
    MediaPlayer *pending_update;
} pending_update;

static void on_image_loaded(GObject *obj, GAsyncResult *res,
                            gpointer user_data) {
    g_debug("media_players.c:on_image_loaded() called.");

    pending_update *update = (pending_update *)user_data;
    GFileInputStream *file_input_stream;
    GError *error = NULL;
    GdkPixbuf *pixbuf;

    MediaPlayerWidget *widget = g_hash_table_lookup(
        update->self->media_players, update->pending_update->name);
    if (!widget) {
        return;
    }

    file_input_stream = g_file_read_finish(G_FILE(obj), res, &error);
    if (error) {
        g_warning("Error loading image: %s", error->message);
        goto fill;
    }

    pixbuf = gdk_pixbuf_new_from_stream(G_INPUT_STREAM(file_input_stream), NULL,
                                        &error);
    if (error) {
        g_warning("Failed to create pixbuf from stream: %s", error->message);
        g_object_unref(file_input_stream);
        goto fill;
    }

    GtkImage *icon_image =
        GTK_IMAGE(gtk_widget_get_first_child(GTK_WIDGET(widget->icon)));

    gtk_image_set_from_pixbuf(icon_image, pixbuf);

    g_object_unref(file_input_stream);
    g_object_unref(pixbuf);

fill:
    media_player_widget_fill(widget, update->pending_update);
    g_free(update);
}

static void media_players_on_media_player_changed(MediaPlayerService *serv,
                                                  MediaPlayer *player,
                                                  MediaPlayers *self) {
    g_debug("media_players.c:media_players_on_media_player_changed() called.");

    MediaPlayerWidget *widget =
        g_hash_table_lookup(self->media_players, player->name);

    if (!widget) {
        widget = media_player_widget_new();

        gtk_box_append(self->media_player_list, GTK_WIDGET(widget->container));

        g_hash_table_insert(self->media_players, player->name, widget);
    }

    // if art url exists, do a request to get the image, and finish filling
    // when the image request callback is called
    if (player->art_url) {
        pending_update *update = g_malloc(sizeof(pending_update));
        update->self = self;
        update->pending_update = player;
        GFile *file;
        file = g_file_new_for_uri(player->art_url);
        g_file_read_async(file, G_PRIORITY_DEFAULT, NULL, on_image_loaded,
                          update);
        return;
    }

    media_player_widget_fill(widget, player);
}

static void media_players_on_media_player_removed(MediaPlayerService *serv,
                                                  MediaPlayer *player,
                                                  MediaPlayers *self) {
    MediaPlayerWidget *widget =
        g_hash_table_lookup(self->media_players, player->name);

    if (!widget) {
        return;
    }

    gtk_box_remove(self->media_player_list, GTK_WIDGET(widget->container));

    g_hash_table_remove(self->media_players, player->name);

    media_player_widget_free(widget);
}

// stub out dispose, finalize, class_init and init methods.
static void media_players_dispose(GObject *object) {
    G_OBJECT_CLASS(media_players_parent_class)->dispose(object);

    // kill signals
    MediaPlayerService *srv = media_player_service_get_global();
    g_signal_handlers_disconnect_by_func(
        srv, media_players_on_media_player_changed, object);
    g_signal_handlers_disconnect_by_func(
        srv, media_players_on_media_player_removed, object);

    // free all media players in list
    MediaPlayers *self = MEDIA_PLAYERS(object);
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, self->media_players);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        media_player_widget_free(value);
    }
    g_hash_table_destroy(self->media_players);
}

static void media_players_finalize(GObject *object) {
    G_OBJECT_CLASS(media_players_parent_class)->finalize(object);
}

static void media_players_class_init(MediaPlayersClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = media_players_dispose;
    object_class->finalize = media_players_finalize;
}

static void media_players_init_layout(MediaPlayers *self) {
    // create widget container
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_set_name(GTK_WIDGET(self->container), "media-players");
    gtk_widget_set_hexpand(GTK_WIDGET(self->container), TRUE);
    // gtk_widget_set_size_request(GTK_WIDGET(self->container), -1, 140);

    // create horizontal scrolled window where media players will reside in.
    self->scrolled_win = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new());
    gtk_widget_set_name(GTK_WIDGET(self->scrolled_win),
                        "media-players-scrolled-window");
    gtk_widget_set_size_request(GTK_WIDGET(self->scrolled_win), -1, 140);
    gtk_widget_set_hexpand(GTK_WIDGET(self->scrolled_win), TRUE);
    gtk_scrolled_window_set_policy(self->scrolled_win, GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);

    self->media_player_list = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_hexpand(GTK_WIDGET(self->media_player_list), TRUE);

    // wire it up
    gtk_box_append(self->container, GTK_WIDGET(self->media_player_list));
    gtk_scrolled_window_set_child(self->scrolled_win,
                                  GTK_WIDGET(self->media_player_list));

    MediaPlayerService *srv = media_player_service_get_global();

    // listen for media player changed and removed events
    g_signal_connect(srv, "media-player-changed",
                     G_CALLBACK(media_players_on_media_player_changed), self);
    g_signal_connect(srv, "media-player-removed",
                     G_CALLBACK(media_players_on_media_player_removed), self);
}

static void media_players_init(MediaPlayers *self) {
    self->media_players = g_hash_table_new(g_str_hash, g_str_equal);

    media_players_init_layout(self);
}

GtkWidget *media_players_get_widget(MediaPlayers *self) {
    return GTK_WIDGET(self->container);
}
