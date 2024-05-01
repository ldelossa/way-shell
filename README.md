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

<https://private-user-images.githubusercontent.com/5642902/307428681-94a67342-7d9d-4491-a92d-6b38f021431e.mp4?jwt=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3MDg3MTYyMTQsIm5iZiI6MTcwODcxNTkxNCwicGF0aCI6Ii81NjQyOTAyLzMwNzQyODY4MS05NGE2NzM0Mi03ZDlkLTQ0OTEtYTkyZC02YjM4ZjAyMTQzMWUubXA0P1gtQW16LUFsZ29yaXRobT1BV1M0LUhNQUMtU0hBMjU2JlgtQW16LUNyZWRlbnRpYWw9QUtJQVZDT0RZTFNBNTNQUUs0WkElMkYyMDI0MDIyMyUyRnVzLWVhc3QtMSUyRnMzJTJGYXdzNF9yZXF1ZXN0JlgtQW16LURhdGU9MjAyNDAyMjNUMTkxODM0WiZYLUFtei1FeHBpcmVzPTMwMCZYLUFtei1TaWduYXR1cmU9ZmI1ZWU4ODk2YjUzMjg1NzRlNzY3OGU2ZGQ0Mjc5MzVlYmFmZDk2Y2Y2OGRmMmI4MWM1MTY2NjliMWY1ZjMyMiZYLUFtei1TaWduZWRIZWFkZXJzPWhvc3QmYWN0b3JfaWQ9MCZrZXlfaWQ9MCZyZXBvX2lkPTAifQ.eweT2hIGTyrOMeKVdMkux8L9EAI-Kvs7l4aeLGVejB0>

The demo above is using [SwayFX](https://github.com/WillPower3309/swayfx) which I really enjoy.

## Installing and Running Way-Shell

I'll focus on Fedora since this my daily driver distro.

I believe the instructions here can be translated for other distros with a bit
of effort.

I'd love any community members to contribute their own methods of installation
even if its just documentation.

Way-Shell depends on the following packages (Fedora)

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
