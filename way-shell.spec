%define _userunitdir /usr/lib/systemd/user

Name: way-shell
Version: 0.0.9
Release: 1%{?dist}
Summary: A Gnome-like desktop shell for Wayland compositors.
License: MIT

URL: https://github.com/ldelossa/way-shell
Source0: https://github.com/ldelossa/way-shell/archive/refs/tags/v0.0.9.tar.gz

BuildRequires: gcc
BuildRequires: make
BuildRequires: pkgconfig
BuildRequires: meson
BuildRequires: cmake
BuildRequires: gtk-doc
BuildRequires: wayland-devel
BuildRequires: glib2-devel

BuildRequires: pkgconfig(libadwaita-1)
BuildRequires: pkgconfig(upower-glib)
BuildRequires: pkgconfig(wireplumber-0.5)
BuildRequires: pkgconfig(json-glib-1.0)
BuildRequires: pkgconfig(libnm)
BuildRequires: pkgconfig(libpulse)
BuildRequires: pkgconfig(libpulse-simple)
BuildRequires: pkgconfig(libpulse-mainloop-glib)
BuildRequires: pkgconfig(wayland-client)
BuildRequires: pkgconfig(wayland-protocols)
BuildRequires: pkgconfig(gio-unix-2.0)
BuildRequires: pkgconfig(gtk4-layer-shell-0)

Requires: NetworkManager
Requires: wireplumber
Requires: upower
Requires: power-profiles-daemon
Requires: systemd
Requires: wayland-devel

%description
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

%prep
%setup -n way-shell-0.0.9

%build
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}

%post
glib-compile-schemas %{_datadir}/glib-2.0/schemas

%postun
glib-compile-schemas %{_datadir}/glib-2.0/schemas

%files
%{_bindir}/way-shell
%{_bindir}/way-sh
%{_datadir}/glib-2.0/schemas/org.ldelossa.way-shell.gschema.xml
%{_userunitdir}/way-shell.service

%changelog
* Mon May 27 2024 Louis DeLosSantos <louis.delos.deve@gmail.com>
- Initial packaging
