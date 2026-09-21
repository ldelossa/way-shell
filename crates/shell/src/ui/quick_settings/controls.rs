//! System tiles and display brightness; network and mixer insert their own controls.
use super::{grid::GridButton, menu::Menu};
use crate::services::{
    brightness::{BrightnessService, ControlKind},
    logind::LogindService,
    power_profiles::{PowerProfilesService, ProfilesState, profile_icon},
    theme::{Theme, ThemeService},
    wayland::WaylandService,
};
use gtk::prelude::*;
use std::{
    cell::{Cell, RefCell},
    rc::Rc,
};

#[derive(Clone)]
pub struct SystemServices {
    pub theme: ThemeService,
    pub logind: LogindService,
    pub profiles: PowerProfilesService,
    pub brightness: BrightnessService,
    pub wayland: WaylandService,
}

pub(super) struct Subscription {
    object: glib::Object,
    handler: Option<glib::SignalHandlerId>,
}
impl Subscription {
    pub(super) fn new(object: &impl IsA<glib::Object>, handler: glib::SignalHandlerId) -> Self {
        Self {
            object: object.as_ref().clone(),
            handler: Some(handler),
        }
    }
}
impl Drop for Subscription {
    fn drop(&mut self) {
        if let Some(handler) = self.handler.take() {
            self.object.disconnect(handler);
        }
    }
}

pub struct SystemControls {
    services: SystemServices,
    subscriptions: RefCell<Vec<Subscription>>,
    profiles: Rc<GridButton>,
    profiles_menu: Menu,
    profiles_state: RefCell<Option<ProfilesState>>,
    idle: Rc<GridButton>,
    nightlight: Rc<GridButton>,
    temperature: gtk::Scale,
    keyboard: Rc<GridButton>,
    keyboard_scale: gtk::Scale,
    keyboard_maximum: Cell<Option<u32>>,
    theme: Rc<GridButton>,
    brightness_row: gtk::Box,
    brightness_scale: gtk::Scale,
    refreshing: Cell<bool>,
    refresh_pending: Cell<bool>,
    stopped: Cell<bool>,
}
impl SystemControls {
    pub fn new(services: SystemServices) -> Rc<Self> {
        let profiles_menu = Menu::new("Performance", "power-profile-balanced-symbolic", false);
        let profiles = GridButton::new(
            "Performance",
            Some("Unavailable"),
            "power-profile-balanced-symbolic",
            Some(profiles_menu.widget().upcast_ref()),
        );
        let idle = GridButton::new("Idle Inhibitor", None, "radio-mixed-symbolic", None);
        let night_menu = Menu::new("Temperature", "night-light-symbolic", false);
        let temperature =
            gtk::Scale::with_range(gtk::Orientation::Horizontal, 1000.0, 6800.0, 500.0);
        temperature.set_value(3000.0);
        for value in (1000..=6000).step_by(1000) {
            temperature.add_mark(
                value as f64,
                gtk::PositionType::Bottom,
                Some(&format!("{}K", value / 1000)),
            );
        }
        temperature.add_mark(6800.0, gtk::PositionType::Bottom, Some("68K"));
        night_menu.options().append(&temperature);
        let nightlight = GridButton::new(
            "Night Light",
            None,
            "night-light-symbolic",
            Some(night_menu.widget().upcast_ref()),
        );
        let keyboard_menu = Menu::new("Keyboard Backlight", "keyboard-brightness-symbolic", false);
        let keyboard_scale = gtk::Scale::with_range(gtk::Orientation::Horizontal, 0.0, 1.0, 1.0);
        keyboard_scale.set_round_digits(0);
        keyboard_menu.options().append(&keyboard_scale);
        let keyboard = GridButton::new(
            "Keyboard",
            None,
            "keyboard-brightness-symbolic",
            Some(keyboard_menu.widget().upcast_ref()),
        );
        let theme = GridButton::new(
            "Dark Mode",
            None,
            "preferences-desktop-appearance-symbolic",
            None,
        );
        let brightness_row = gtk::Box::new(gtk::Orientation::Horizontal, 0);
        brightness_row.set_widget_name("brightness-container");
        brightness_row.append(&gtk::Image::from_icon_name("display-brightness-symbolic"));
        let brightness_scale = gtk::Scale::with_range(gtk::Orientation::Horizontal, 0.0, 1.0, 0.05);
        brightness_scale.set_hexpand(true);
        brightness_row.append(&brightness_scale);
        let this = Rc::new(Self {
            services,
            subscriptions: RefCell::new(Vec::new()),
            profiles,
            profiles_menu,
            profiles_state: RefCell::new(None),
            idle,
            nightlight,
            temperature,
            keyboard,
            keyboard_scale,
            keyboard_maximum: Cell::new(None),
            theme,
            brightness_row,
            brightness_scale,
            refreshing: Cell::new(false),
            refresh_pending: Cell::new(false),
            stopped: Cell::new(false),
        });
        this.watch(&this.services.theme, "theme-changed");
        this.watch(&this.services.logind, "changed");
        this.watch(&this.services.profiles, "changed");
        for signal in [
            "brightness-changed",
            "keyboard-brightness-changed",
            "availability-changed",
        ] {
            this.watch(&this.services.brightness, signal);
        }
        for signal in [
            "ready",
            "capabilities-changed",
            "gamma-control-enabled",
            "gamma-control-disabled",
        ] {
            this.watch(&this.services.wayland, signal);
        }
        this.clicked(this.theme.toggle(), |this| {
            let theme = if this.services.theme.theme() == Theme::Dark {
                Theme::Light
            } else {
                Theme::Dark
            };
            if let Err(error) = this.services.theme.set_theme(theme) {
                this.theme
                    .widget()
                    .set_tooltip_text(Some(&error.to_string()));
            }
        });
        this.clicked(this.idle.toggle(), |this| {
            this.services
                .logind
                .set_idle_inhibit(!this.services.logind.state().inhibited)
        });
        this.clicked(this.nightlight.toggle(), |this| {
            if this.services.wayland.gamma_enabled() {
                this.refreshing.set(true);
                this.temperature.set_value(3000.0);
                this.refreshing.set(false);
                this.services.wayland.disable_gamma();
            } else {
                this.apply_temperature();
            }
        });
        this.clicked(this.keyboard.toggle(), |this| {
            if let Some(device) = this.services.brightness.snapshot(ControlKind::Keyboard) {
                this.set_keyboard(u32::from(device.current == 0));
            }
        });
        let weak = Rc::downgrade(&this);
        this.temperature.connect_value_changed(move |_| {
            if let Some(this) = weak.upgrade()
                && !this.refreshing.get()
                && !this.stopped.get()
            {
                this.apply_temperature();
            }
        });
        let weak = Rc::downgrade(&this);
        this.keyboard_scale.connect_value_changed(move |scale| {
            if let Some(this) = weak.upgrade()
                && !this.refreshing.get()
                && !this.stopped.get()
            {
                this.set_keyboard(scale.value() as u32);
            }
        });
        let weak = Rc::downgrade(&this);
        this.brightness_scale.connect_value_changed(move |scale| {
            if let Some(this) = weak.upgrade()
                && !this.refreshing.get()
                && !this.stopped.get()
            {
                let weak = Rc::downgrade(&this);
                this.services
                    .brightness
                    .set_backlight(scale.value(), move |result| {
                        if let Some(this) = weak.upgrade()
                            && !this.stopped.get()
                        {
                            this.refresh();
                            this.brightness_row.set_tooltip_text(
                                result.err().map(|error| error.to_string()).as_deref(),
                            );
                        }
                    });
            }
        });
        this.refresh();
        this
    }
    /// Preserve the original dynamic-network, system, airplane, keyboard/theme order.
    pub fn ordered_buttons(
        &self,
        network: impl IntoIterator<Item = Rc<GridButton>>,
        airplane: Option<Rc<GridButton>>,
    ) -> Vec<Rc<GridButton>> {
        network
            .into_iter()
            .chain([
                self.profiles.clone(),
                self.idle.clone(),
                self.nightlight.clone(),
            ])
            .chain(airplane)
            .chain([self.keyboard.clone(), self.theme.clone()])
            .collect()
    }
    pub fn brightness_row(&self) -> &gtk::Box {
        &self.brightness_row
    }
    pub fn brightness_scale(&self) -> &gtk::Scale {
        &self.brightness_scale
    }
    pub fn keyboard_scale(&self) -> &gtk::Scale {
        &self.keyboard_scale
    }
    pub fn temperature_scale(&self) -> &gtk::Scale {
        &self.temperature
    }
    pub fn stop(&self) {
        if self.stopped.replace(true) {
            return;
        }
        self.subscriptions.borrow_mut().clear();
        self.disable_widgets();
    }
    fn disable_widgets(&self) {
        for button in self.ordered_buttons([], None) {
            button.set_sensitive(false);
        }
        self.brightness_row.set_sensitive(false);
    }
    fn watch(self: &Rc<Self>, object: &impl IsA<glib::Object>, signal: &str) {
        let weak = Rc::downgrade(self);
        let handler = object.connect_local(signal, false, move |_| {
            if let Some(this) = weak.upgrade() {
                this.refresh();
            }
            None
        });
        self.subscriptions
            .borrow_mut()
            .push(Subscription::new(object, handler));
    }
    fn clicked(self: &Rc<Self>, button: &gtk::Button, callback: impl Fn(&Rc<Self>) + 'static) {
        let weak = Rc::downgrade(self);
        button.connect_clicked(move |button| {
            if let Some(this) = weak.upgrade()
                && !this.stopped.get()
                && button.is_sensitive()
            {
                callback(&this);
            }
        });
    }
    fn apply_temperature(&self) {
        if let Err(error) = self
            .services
            .wayland
            .set_temperature(self.temperature.value() as u32)
        {
            self.nightlight.widget().set_tooltip_text(Some(&error));
        }
    }
    fn set_keyboard(self: &Rc<Self>, value: u32) {
        let weak = Rc::downgrade(self);
        self.services.brightness.set_keyboard(value, move |result| {
            if let Some(this) = weak.upgrade()
                && !this.stopped.get()
            {
                this.refresh();
                this.keyboard
                    .widget()
                    .set_tooltip_text(result.err().map(|error| error.to_string()).as_deref());
            }
        });
    }
    fn refresh(self: &Rc<Self>) {
        if self.stopped.get() {
            return;
        }
        self.refresh_pending.set(true);
        if self.refreshing.replace(true) {
            return;
        }
        while self.refresh_pending.replace(false) && !self.stopped.get() {
            self.theme
                .set_toggled(self.services.theme.theme() == Theme::Dark);
            let login = self.services.logind.state();
            self.idle.set_sensitive(login.available);
            self.idle.set_toggled(login.inhibited);
            self.nightlight
                .set_sensitive(self.services.wayland.gamma_available());
            self.temperature
                .set_sensitive(self.services.wayland.gamma_available());
            self.nightlight
                .set_toggled(self.services.wayland.gamma_enabled());
            self.refresh_profiles();
            let keyboard = self.services.brightness.snapshot(ControlKind::Keyboard);
            let keyboard_available = self.services.brightness.is_available(ControlKind::Keyboard);
            self.keyboard.set_sensitive(keyboard_available);
            self.keyboard_scale.set_sensitive(keyboard_available);
            self.keyboard.widget().set_tooltip_text(
                (!keyboard_available).then_some("Keyboard brightness is unavailable"),
            );
            self.keyboard
                .set_toggled(keyboard.as_ref().is_some_and(|device| device.current > 0));
            let maximum = keyboard.as_ref().map(|device| device.maximum);
            if self.keyboard_maximum.replace(maximum) != maximum {
                self.keyboard_scale.clear_marks();
                self.keyboard_scale
                    .set_range(0.0, maximum.unwrap_or(1).max(1) as f64);
                if let Some(maximum) = maximum.filter(|maximum| *maximum <= 20) {
                    for value in 0..=maximum {
                        self.keyboard_scale.add_mark(
                            value as f64,
                            gtk::PositionType::Bottom,
                            Some(&value.to_string()),
                        );
                    }
                }
            }
            self.keyboard_scale.set_value(
                keyboard
                    .as_ref()
                    .map_or(0.0, |device| device.current as f64),
            );
            let backlight = self
                .services
                .brightness
                .is_available(ControlKind::Backlight);
            self.brightness_row.set_visible(backlight);
            self.brightness_row.set_sensitive(backlight);
            self.brightness_scale.set_value(
                self.services
                    .brightness
                    .snapshot(ControlKind::Backlight)
                    .map_or(0.0, |device| device.fraction()),
            );
        }
        self.refreshing.set(false);
        if self.stopped.get() {
            self.disable_widgets();
        }
    }
    fn refresh_profiles(self: &Rc<Self>) {
        let state = self.services.profiles.state();
        let previous = self.profiles_state.replace(Some(state.clone()));
        self.profiles.set_sensitive(state.available());
        self.profiles
            .set_subtitle(state.active.as_deref().unwrap_or("Unavailable"));
        self.profiles
            .set_icon(profile_icon(state.active.as_deref()));
        if previous
            .as_ref()
            .is_some_and(|old| old.active != state.active)
        {
            self.profiles.revealer().set_reveal_child(false);
        }
        if previous
            .as_ref()
            .is_some_and(|old| old.profiles == state.profiles)
        {
            return;
        }
        while let Some(child) = self.profiles_menu.options().first_child() {
            self.profiles_menu.options().remove(&child);
        }
        for profile in state.profiles {
            let row = gtk::Box::new(gtk::Orientation::Horizontal, 0);
            row.add_css_class("quick-settings-menu-option-power-profiles");
            let button = gtk::Button::new();
            button.set_hexpand(true);
            let contents = gtk::Box::new(gtk::Orientation::Horizontal, 0);
            contents.append(&gtk::Image::from_icon_name(profile_icon(Some(&profile))));
            let label = gtk::Label::new(Some(&profile));
            label.set_xalign(0.0);
            label.set_ellipsize(gtk::pango::EllipsizeMode::End);
            contents.append(&label);
            button.set_child(Some(&contents));
            row.append(&button);
            self.clicked(&button, move |this| {
                let weak = Rc::downgrade(this);
                this.services.profiles.set_profile(&profile, move |result| {
                    if let Some(this) = weak.upgrade()
                        && !this.stopped.get()
                    {
                        this.profiles.widget().set_tooltip_text(
                            result.err().map(|error| error.to_string()).as_deref(),
                        );
                    }
                });
            });
            self.profiles_menu.options().append(&row);
        }
    }
}
impl Drop for SystemControls {
    fn drop(&mut self) {
        self.stop();
    }
}
