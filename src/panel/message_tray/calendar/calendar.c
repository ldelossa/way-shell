#include "calendar.h"

#include <adwaita.h>

#include "../../../services/clock_service.h"

struct _Calendar {
    // A reference to the system's date/time
    GDateTime *now;
    // Dirty is set true when the calendar is not set to `now`.
    gboolean dirty;
    GObject parent_instance;
    GtkBox *container;
    GtkCalendar *calendar;
    GtkButton *today;
    struct {
        GtkCenterBox *container;
        GtkButton *back;
        GtkButton *forward;
        GtkLabel *date;
    } month_selector;
};
G_DEFINE_TYPE(Calendar, calendar, G_TYPE_OBJECT);

static void calendar_handle_clock_tick(ClockService *cs, GDateTime *now,
                                       Calendar *self);

// stub out dispose, finalize, class_init, and init methods
static void calendar_dispose(GObject *gobject) {
    Calendar *self = MESSAGE_TRAY_CALENDAR(gobject);

    // cancel signals
    ClockService *cs = clock_service_get_global();
    g_signal_handlers_disconnect_by_func(cs, calendar_handle_clock_tick, self);

    // Chain-up
    G_OBJECT_CLASS(calendar_parent_class)->dispose(gobject);
};

static void calendar_finalize(GObject *gobject) {
    // Chain-up
    G_OBJECT_CLASS(calendar_parent_class)->finalize(gobject);
};

static void calendar_class_init(CalendarClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = calendar_dispose;
    object_class->finalize = calendar_finalize;
};

static void calendar_set_dirty(Calendar *self) {
    GDateTime *current = gtk_calendar_get_date(self->calendar);

    if (g_date_time_get_month(current) == g_date_time_get_month(self->now) &&
        g_date_time_get_year(current) == g_date_time_get_year(self->now) &&
        g_date_time_get_day_of_month(current) ==
            g_date_time_get_day_of_month(self->now)) {
        gtk_widget_remove_css_class(GTK_WIDGET(self->today),
                                    "calendar-today-dirty");
        self->dirty = FALSE;
    } else {
        gtk_widget_add_css_class(GTK_WIDGET(self->today),
                                 "calendar-today-dirty");
        self->dirty = TRUE;
    }
}

static void calendar_handle_clock_tick(ClockService *cs, GDateTime *now,
                                       Calendar *self) {
    // check if day has changed from self->now
    if (g_date_time_get_day_of_year(now) ==
        g_date_time_get_day_of_year(self->now)) {
        return;
    }

    gtk_label_set_text(self->month_selector.date,
                       g_date_time_format(now, "%B %Y"));
    // update the action row title
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->today),
                                  g_date_time_format(now, "%A"));
    // update the action row subtitle
    adw_action_row_set_subtitle(ADW_ACTION_ROW(self->today),
                                g_date_time_format(now, "%B %d %Y"));


    // make a copy of GDateTime
    self->now = g_date_time_add(now, 0);
    calendar_set_dirty(self);
};

static void on_month_selector_back(GtkButton *button, Calendar *self) {
    GDateTime *current = gtk_calendar_get_date(self->calendar);
    GDateTime *new = g_date_time_add_months(current, -1);

    gtk_label_set_text(self->month_selector.date,
                       g_date_time_format(new, "%B %Y"));
    gtk_calendar_select_day(self->calendar, new);

    calendar_set_dirty(self);
}

static void on_month_selector_forward(GtkButton *button, Calendar *self) {
    GDateTime *current = gtk_calendar_get_date(self->calendar);
    GDateTime *new = g_date_time_add_months(current, 1);

    gtk_label_set_text(self->month_selector.date,
                       g_date_time_format(new, "%B %Y"));
    gtk_calendar_select_day(self->calendar, new);

    calendar_set_dirty(self);
}

static void calendar_init_month_selector(Calendar *self) {
    // create widgets
    self->month_selector.container = GTK_CENTER_BOX(gtk_center_box_new());
    gtk_widget_set_name(GTK_WIDGET(self->month_selector.container),
                        "calendar-month-selector");
    self->month_selector.back = GTK_BUTTON(gtk_button_new());
    self->month_selector.forward = GTK_BUTTON(gtk_button_new());
    self->month_selector.date =
        GTK_LABEL(gtk_label_new(g_date_time_format(self->now, "%B %Y")));

    // set button icons
    gtk_button_set_child(self->month_selector.back,
                         gtk_image_new_from_icon_name("pan-start-symbolic"));
    gtk_button_set_child(self->month_selector.forward,
                         gtk_image_new_from_icon_name("pan-end-symbolic"));

    // set button signals
    g_signal_connect(self->month_selector.back, "clicked",
                     G_CALLBACK(on_month_selector_back), self);
    g_signal_connect(self->month_selector.forward, "clicked",
                     G_CALLBACK(on_month_selector_forward), self);

    // lay it out
    gtk_center_box_set_start_widget(self->month_selector.container,
                                    GTK_WIDGET(self->month_selector.back));
    gtk_center_box_set_center_widget(self->month_selector.container,
                                     GTK_WIDGET(self->month_selector.date));
    gtk_center_box_set_end_widget(self->month_selector.container,
                                  GTK_WIDGET(self->month_selector.forward));
}

static void on_today_button_clicked(GtkButton *button, Calendar *self) {
    if (self->dirty) {
        gtk_calendar_select_day(self->calendar, self->now);
        gtk_label_set_text(self->month_selector.date,
                           g_date_time_format(self->now, "%B %Y"));
        gtk_calendar_clear_marks(self->calendar);
    }
    calendar_set_dirty(self);
}

static void calendar_init_today_button(Calendar *self) {
    self->today = GTK_BUTTON(gtk_button_new());
    // gtk_widget_add_css_class(GTK_WIDGET(self->today), "calendar-today");
    gtk_widget_set_name(GTK_WIDGET(self->today), "calendar-today");
    gtk_widget_set_hexpand(GTK_WIDGET(self->today), true);

    AdwActionRow *row = ADW_ACTION_ROW(adw_action_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row),
                                  g_date_time_format(self->now, "%A"));
    adw_action_row_set_subtitle(row, g_date_time_format(self->now, "%B %d %Y"));
    gtk_button_set_child(self->today, GTK_WIDGET(row));

    // wire up button to click
    g_signal_connect(self->today, "clicked",
                     G_CALLBACK(on_today_button_clicked), self);
}

static void on_day_selected(GtkCalendar *calendar, Calendar *self) {
    GDateTime *current = gtk_calendar_get_date(self->calendar);
    gtk_label_set_text(self->month_selector.date,
                       g_date_time_format(current, "%B %Y"));
    calendar_set_dirty(self);
}

static void calendar_init_calendar(Calendar *self) {
    self->calendar = GTK_CALENDAR(gtk_calendar_new());
	gtk_widget_set_size_request(GTK_WIDGET(self->calendar), -1, 300);
    gtk_calendar_set_show_heading(self->calendar, false);
    gtk_calendar_set_show_week_numbers(self->calendar, false);
    gtk_widget_set_hexpand(GTK_WIDGET(self->calendar), true);
    gtk_widget_set_vexpand(GTK_WIDGET(self->calendar), true);

    // wire up day-selected signal
    g_signal_connect(self->calendar, "day-selected",
                     G_CALLBACK(on_day_selected), self);
}

static void calendar_init(Calendar *self) {
    self->now = g_date_time_new_now_local();
    self->dirty = FALSE;
    self->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(self->container), "calendar-area");

    // today button
    calendar_init_today_button(self);

    // month selector
    calendar_init_month_selector(self);

    // calendar widget
    calendar_init_calendar(self);

    // lay it out.
    gtk_box_append(self->container, GTK_WIDGET(self->today));
    gtk_box_append(self->container, GTK_WIDGET(self->month_selector.container));
    gtk_box_append(self->container, GTK_WIDGET(self->calendar));

    // wire up to global clock service
    ClockService *cs = clock_service_get_global();
    g_signal_connect(cs, "tick", G_CALLBACK(calendar_handle_clock_tick), self);
};

Calendar *calendar_new() { return g_object_new(CALENDAR_TYPE, NULL); }

GtkWidget *calendar_get_widget(Calendar *self) {
    return GTK_WIDGET(self->container);
}
