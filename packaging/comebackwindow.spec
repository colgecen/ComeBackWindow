Name:           comebackwindow
Version:        1.0.5
Release:        1%{?dist}
Summary:        Windows-style window shortcuts for KDE Plasma 6

License:        MIT
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gio-unix-2.0)
BuildRequires:  pkgconfig(gtk4)
BuildRequires:  pkgconfig(cairo)
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  pkgconfig(x11)
BuildRequires:  wayland-devel
BuildRequires:  gettext
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib
BuildRequires:  rsvg-convert

Requires:       gtk4
Requires:       glib2
Requires:       wayland
Requires:       libX11
Requires:       kwin

%description
Come Back Window brings Windows-style window behaviour to KDE Plasma 6 on
Wayland: show desktop, minimize, maximize, close, minimize all others and
restore them, plus a thin Show Desktop strip, Aero Peek and Aero Shake.

%prep
%setup -q

%build
%make_build PREFIX=%{_prefix}

%install
%make_install PREFIX=%{_prefix}

%find_lang %{name}

%check
make check

%files -f %{name}.lang
%license LICENSE
%doc README.md
%{_bindir}/comebackwindow
%{_datadir}/comebackwindow/
%{_datadir}/applications/org.comebackwindow.App.desktop
%{_datadir}/metainfo/org.comebackwindow.App.metainfo.xml
%{_datadir}/icons/hicolor/scalable/apps/comebackwindow.svg
%{_datadir}/icons/hicolor/*/apps/comebackwindow.png

%changelog
* Sun Nov 15 2026 Come Back Window contributors - 1.0.0-1
- First release

* Mon Nov 16 2026 Come Back Window contributors - 1.0.1-1
- Fix settings save crash and add launcher entry

* Mon Nov 16 2026 Come Back Window contributors - 1.0.2-1
- Find KWin data next to the build output

* Mon Nov 16 2026 Come Back Window contributors - 1.0.3-1
- Standard icon source, macOS-style screenshots and two-column settings

* Mon Nov 16 2026 Come Back Window contributors - 1.0.4-1
- AppImage data paths, desktop launch and autostart fixes

* Mon Nov 16 2026 Come Back Window contributors - 1.0.5-1
- Self-registering AppImage desktop entry
