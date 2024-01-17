#include <adwaita.h>

G_BEGIN_DECLS

struct _PanelStatusBarSoundButton;
#define PANEL_STATUS_BAR_SOUND_BUTTON_TYPE \
    panel_status_bar_sound_button_get_type()
G_DECLARE_FINAL_TYPE(PanelStatusBarSoundButton, panel_status_bar_sound_button,
                     PANEL, STATUS_BAR_SOUND_BUTTON, GObject);

G_END_DECLS

#include <adwaita.h>

struct _PanelStatusBarSoundButton {
    GObject parent_instance;
};
G_DEFINE_TYPE(PanelStatusBarSoundButton, panel_status_bar_sound_button, G_TYPE_OBJECT);

// stub out dispose, finalize, class_init, and init methods
static void panel_status_bar_sound_button_dispose(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(panel_status_bar_sound_button_parent_class)->dispose(gobject);
};

static void panel_status_bar_sound_button_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(panel_status_bar_sound_button_parent_class)->finalize(gobject);
};

static void panel_status_bar_sound_button_class_init(PanelStatusBarSoundButtonClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = panel_status_bar_sound_button_dispose;
    object_class->finalize = panel_status_bar_sound_button_finalize;
};

static void panel_status_bar_sound_button_init_layout(PanelStatusBarSoundButton *self) {
};

static void panel_status_bar_sound_button_init(PanelStatusBarSoundButton *self) {

}