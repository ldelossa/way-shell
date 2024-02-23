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

<https://private-user-images.githubusercontent.com/5642902/307390891-d82cb4fe-6fb0-413f-9275-5d01733b8685.mp4?jwt=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3MDg3MDc5OTQsIm5iZiI6MTcwODcwNzY5NCwicGF0aCI6Ii81NjQyOTAyLzMwNzM5MDg5MS1kODJjYjRmZS02ZmIwLTQxM2YtOTI3NS01ZDAxNzMzYjg2ODUubXA0P1gtQW16LUFsZ29yaXRobT1BV1M0LUhNQUMtU0hBMjU2JlgtQW16LUNyZWRlbnRpYWw9QUtJQVZDT0RZTFNBNTNQUUs0WkElMkYyMDI0MDIyMyUyRnVzLWVhc3QtMSUyRnMzJTJGYXdzNF9yZXF1ZXN0JlgtQW16LURhdGU9MjAyNDAyMjNUMTcwMTM0WiZYLUFtei1FeHBpcmVzPTMwMCZYLUFtei1TaWduYXR1cmU9MjExZjY3ODE0ODMzZmM3NmJiMDYzOTQyMWVkYTU0MGE1NjI5M2RlMTFhMWQ5MTVjY2E1MzhjYjRhYmQ3YTYyZSZYLUFtei1TaWduZWRIZWFkZXJzPWhvc3QmYWN0b3JfaWQ9MCZrZXlfaWQ9MCZyZXBvX2lkPTAifQ.JY7n4jMhZwgFnRyZsoYALXqeTbncTIxM3h6V1kkxJ9o>

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

Here is an example of how I configure Sway once way-shell is installed and its
binaries are in $PATH

```shell
# way-shell configuration
exec way-shell
bindsym XF86AudioRaiseVolume exec way-sh volume up
bindsym XF86AudioLowerVolume exec way-sh volume down
bindsym XF86MonBrightnessDown exec way-sh brightness down
bindsym XF86MonBrightnessUp exec way-sh brightness up
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
- [ ] Media Player integration (control DBus announced media players)
- [ ] Bluetooth integration (pair with discoverable bluetooth devices)
- [ ] Themeing (provide CSS override directory, light/dark theme switch button, and allow a script to be ran after the theme is switched)

## Road to v2.0

- [ ] Figure out proper rpm and deb packaging
- [ ] Lock Screen (Wayland protocol based) implementation
- [ ] App/Workspace switcher (alt+tab replacement)
- [ ] Workspace Overview/Application launcher overlay widget
- [ ] Option to turn on idle inhibitor when video/audio sources are detected.
