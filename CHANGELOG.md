# Sürüm Notları

## 1.0.0 — 15 Kasım 2026

İlk kararlı sürüm. KDE Plasma 6 / Wayland odaklıdır.

### Özellikler

- **Masaüstünü Göster**: `Meta+D`, `Meta+M`, tepsi menüsü ve sağ alt
  köşedeki ince şerit ile aç/kapat.
- **Pencere kısayolları**: küçült (`Meta+Down`), büyüt (`Meta+Up`),
  kapat (`Alt+F4`), aktif hariç hepsini küçült (`Meta+Home`),
  küçültülenleri geri getir (`Meta+Shift+M`). Çakışma varsa
  `Meta+Shift+Down` / `Meta+Shift+Up` alternatifleri kullanılır.
- **Şerit**: wlr-layer-shell tabanlı, panel kenarını ve kalınlığını
  algılar; panelin üstüne binmez. Tam ekran pencerede gizlenir.
  Wayland yoksa X11 yedek arka ucu devreye girer.
- **Aero Peek**: şeritte bekleyince pencereler şeffaflaşır; tıklama
  peek'i onaylar.
- **Aero Shake**: pencereyi sallayınca diğerleri küçülür (varsayılan
  kapalı).
- **Tepsi**: StatusNotifierItem + dbusmenu ile 8 öğeli menü.
- **Ayarlar ve ilk açılış sihirbazı**: GTK4, mevcut GMainLoop üzerinde.
- **Kısayol çakışma yönetimi**: KDE'nin yerel kısayollarını korur,
  yedekler ve `--restore-shortcuts` ile geri yükler.
- **Otomatik başlatma**: oturum açılışında çalıştır dosyası yönetimi.
- **Yazı modu**: şerit çubuğu yerine köşeye yapışık dikey yazı; metin
  ayarlardan girilir, şeride tam sığar, hover'da zemin saydamlaşıp yalnız
  yazı kalır.
- **Renk ve saydamlık**: şerit gövdesi ve yazı için ayrı renk ve opaklık
  ayarları.
- **Türkçe / İngilizce arayüz**: gettext katalogları (`po/tr.po`,
  `po/en.po`) ve KWin betiği için `logic.js` sözlüğü.
- **Marka**: geri oku logosu, 16-256 px simgeler, tanıtım kapak görseli.

### Altyapı

- C11, GLib/GIO, Wayland istemcisi, GTK4; `-Wall -Wextra -Wpedantic
  -Wshadow -Wformat=2 -Werror`.
- `make check`: 393 C kontrolü, 34 JS testi, lint ve doğrulama.
- `make test-asan`, `make test-valgrind` (varsa), `make test-integration`.
- KWin betiği içerik özetli önbellek dizininden yüklenir; yeniden
  yüklemede çift örnek oluşmaz.
- Bozuk ayar dosyası yedeklenir; D-Bus kapanışında temiz çıkış.
- Fedora RPM spec, Arch PKGBUILD, desktop girdisi ve AppStream meta verisi.

### Bilinen sınırlar

- KWin, `setForeignShortcutKeys` yazmalarını yok saydığı için Meta+D ve
  Alt+F4 KDE'nin yerel eyleminde kalır.
- Dil değişikliği yeniden başlatma gerektirir.
- X11 oturumu ve çoklu ekran bu sürümde canlı test edilmedi.
