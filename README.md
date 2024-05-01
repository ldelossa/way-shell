# Way-Shell

A Gnome inspired desktop shell for Wayland compositors/window managers written
in C and Gtk4.

Way-Shell expects a Gnome-like environment to be available.
This means DBus must be running and the following services must be available:

- Logind
- NetworkManager
- WirePlumber/Pipewire
- PowerProfiles Daemon
- UPower

If you're using Fedora these services should be available by default.

Currently Way-Shell only supports Sway but this will change as the project
matures.

Way-Shell is in its very early stages of development so expect some crashes and
bugs. However, it is my daily driver and I'm rather quick to fix show stopper
issues.

The demo above is using [SwayFX](https://github.com/WillPower3309/swayfx) which I really enjoy.

## Installing and Running Way-Shell

I'll focus on Fedora since this my daily driver distro.

I believe the instructions here can be translated for other distros with a bit
of effort.

I'd love any community members to contribute their own methods of installation
even if its just documentation.

Way-Shell depends on the following packages (Fedora 40).
Please note that Fedora 40 packages the latest wireplumber libraries and (wireplumber-0.5) and Way-Shell will fail to build if a previous version is being linked. 

    libadwaita-devel \
    upower-devel \
    wireplumber-devel \
    json-glib-devel \
    NetworkManager-libnm-devel \
    pulseaudio-libs-devel \
    meson \
    cmake \
    gtk-doc

Way-Shell also depends on [gtk4-layer-shell](https://github.com/wmww/gtk4-layer-shell)
which isn't currently packaged for Fedora.

Currently, the easiest way to run Way-Shell is to use a [toolbox](https://github.com/containers/toolbox).

A makefile exists in `contrib/toolbox/fedora` which will automate the setup
of a sway-shell toolbox container. Just run 'make' in that directory after you
installed toolbox.

You may need to adjust the container's `FROM` statement to match your Fedora
version.

Running inside a toolbox is not perfect tho.

Inside the toolbox we can't resolve application icons so the mixer panel will
show a generic 'media' icon for audio sources.

You'll also need to interface with DConf inside the container.

Given the docker file in that folder you could always just run those steps
or, a translation of those steps for your distro, and install Way-Shell on your
host machine.

Eventually, I hope to get proper deb/rpm packaging for this project but its not
on my immediate radar.

Contributions welcome!

## Video Demonstration

[![IMAGE ALT TEXT HERE](https://img.youtube.com/vi/sOooD4Q3mYU/0.jpg)](https://www.youtube.com/watch?v=sOooD4Q3mYU)

## Configuring Sway

Sway is the ultimate controller of keybinds and must be configured to
specifically work with Way-Shell.

Sway integrates with Way-Shell via its CLI `way-sh`.

Below is the expected Sway config which Way-Shell supports.

```shell
set $mod Mod4
# way-shell configuration
bindsym XF86AudioRaiseVolume exec way-sh volume up
bindsym XF86AudioMute exec way-sh volume mute
bindsym XF86AudioLowerVolume exec way-sh volume down
bindsym XF86MonBrightnessDown exec way-sh brightness down
bindsym XF86MonBrightnessUp exec way-sh brightness up
bindcode --release 133 exec way-sh activities toggle
bindsym $mod+Tab exec way-sh app-switcher toggle
bindsym $mod+o exec way-sh output-switcher toggle
bindsym $mod+w exec way-sh workspace-switcher toggle
bindsym $mod+a exec way-sh workspace-app-switcher toggle
```

## Using Way-Shell

Way-Shell aims to feel like a "natural" desktop environment.

The below table lists useful keymaps to keep in mind.

| Keybinding | Action |
|------------|--------|
| Super + Tab | Open the app switcher (release Super to close) |
| Super + g  | (In app switcher) move to next instance of an application if it exists |
| Super + Shift + g  | (In app switcher) move to previous instance of an application if it exists |
| Super + w | Open the workspace switcher (repeat to toggle close) |
| Super + a | Open the workspace app switcher (repeat to toggle close) |
| Super + o | Open the output switcher (repeat to toggle close) |

All widgets which provide a list of selections (workspace switcher, app switcher, output switcher)
support the following keybinds.

| Keybinding | Action |
|------------|--------|
| Tab | next item |
| Shift + Tab  | previous item |
| Ctrl + n | next item |
| Ctrl + p  | previous item |
| UpArrow | next item |
| DownArrow  | previous item |
| Esc | Clear search, close widget if search is empty |

## Road to v1.0

- [x] Multi-monitor support
- [x] Scrolling workspaces bar
- [x] Notifications daemon implementation and message tray
- [x] Calendar (in message tray)
- [x] Quick Settings with button grid
- [x] Network Manager integration
- [x] Logind integration (restart/suspend/sleep/logout/brightness/idle inhibitor)
- [x] In-panel wireplumber mixer (change audio input/output routes)
- [x] PowerProfiles daemon integration
- [x] Brightness and Audio sliders
- [x] DConf integration (configuration is driven via dconf)
- [x] CLI interface (way-sh)
- [x] Media Player integration (control DBus announced media players)
- [ ] Bluetooth integration (pair with discoverable bluetooth devices)
- [x] Themeing (provide CSS override directory, light/dark theme switch button, and allow a script to be ran after the theme is switched)
- [ ] Figure out proper rpm and deb packaging
- [ ] Lock Screen (Wayland protocol based) implementation
- [x] App/Workspace switcher (alt+tab)
- [x] Workspace Overview/Application launcher overlay widget
- [ ] Option to turn on idle inhibitor when video/audio sources are detected.
