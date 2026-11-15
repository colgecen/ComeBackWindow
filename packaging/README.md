# Paketleme

Bu dizin, kaynak arşivinden dağıtım paketi üretmek için kullanılan
dosyaları içerir. Uygulama `make install` ile `PREFIX` (varsayılan
`/usr`) altına kurulur.

## Kaynak arşivi

```sh
make dist                    # comebackwindow-<sürüm>.tar.gz
```

## Fedora / RHEL

```sh
rpmbuild -ta comebackwindow-1.0.0.tar.gz
```

`comebackwindow.spec` dosyası `packaging/` içindedir; `rpmbuild` çağrısına
`--define "_sourcedir $PWD"` ekleyerek arşivi gösterebilirsiniz.

## Arch Linux

```sh
cd packaging
makepkg -si
```

`PKGBUILD`, kaynak arşivini `$srcdir/comebackwindow-$pkgver` altında
bekler. `makepkg` için arşivi bu dizine koyun veya `source=()` satırını
yerel arşive işaret edecek şekilde düzenleyin.

## Bağımlılıklar

- Derleme: `gcc`, `make`, `pkg-config`, `glib2-devel`, `gtk4-devel`,
  `wayland-devel`, `libX11-devel`, `gettext`, `rsvg-convert` (veya
  ImageMagick)
- Çalışma: GLib/GIO, GTK4, Wayland istemcisi, libX11, KWin 6
