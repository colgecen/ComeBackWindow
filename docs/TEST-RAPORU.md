# Test Raporu — v1.0.0

**Tarih:** 15 Kasım 2026
**Depo sürümü:** `1b7cacd` + bu doküman
**Ortam:** Fedora 44, KDE Plasma 6 / KWin 6.7.5, Wayland, tek ekran
1920×1080 (eDP-1), alt panel 68 px

> Not: Otomatik testler `make check`, `make test-asan` ve
> `make test-integration` ile çalıştırıldı. Elle testler gerçek oturumda
> yapıldı; bu raporda yalnızca gerçekten gözlenen sonuçlar PASS olarak
> işaretlendi.

## 1. Otomatik testler

| Adım | Komut | Sonuç |
|---|---|---|
| Derleme | `make -j` | **PASS** (`-Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Werror`) |
| C birim testleri | `make test-unit` | **PASS** — 393 kontrol (8 dosya) |
| JS testleri | `make test-js` | **PASS** — 34 test |
| Statik denetim | `make lint` | **PASS** (cppcheck/clang-tidy/shellcheck SKIP) |
| Doğrulama | `make test-validate` | **PASS** (desktop, AppStream, XML, msgfmt) |
| ASan/UBSan | `make test-asan` | **PASS** (sızıntı yok) |
| Valgrind | `make test-valgrind` | **SKIP** — valgrind kurulu değil |
| Entegrasyon | `make test-integration` | **PASS** — 8 adım |
| Tümü | `tools/check-all.sh` | **PASS** |

### Entegrasyon testi adımları

| Adım | Sonuç |
|---|---|
| Uygulama D-Bus adını aldı | PASS |
| KWin betiği yüklendi | PASS |
| Ayarlar D-Bus üzerinden okunuyor | PASS |
| Şerit yüzeyi oluşturuldu (layer-shell) | PASS |
| MinimizeOthers | PASS |
| RestoreAll | PASS |
| ToggleDesktop (aç/kapat, iki yön) | PASS |
| Kapanışta betik boşaltıldı | PASS |

### Atlanan araçlar

| Araç | Neden |
|---|---|
| `cppcheck` | kurulu değil |
| `clang-tidy` | kurulu değil |
| `shellcheck` | kurulu değil |
| `valgrind` | kurulu değil |
| `rsvg-convert` | kurulu değil — ikonlar ImageMagick ile üretildi |

## 2. Elle testler

| # | Senaryo | Sonuç | Kanıt |
|---|---|---|---|
| 1.2 | Uygulama açılışı, betik yükleme | PASS | `KWin betiği yüklendi`, `script-loaded` |
| 1.3 | İlk açılış sihirbazı | PASS | Sihirbaz açıldı, ayarlar kaydedildi |
| 2.1 | Şeride tıkla → masaüstü açılır | PASS | `showingDesktop=true`, `desktop-shown` |
| 2.2 | Tekrar tıkla → masaüstü geri gelir | PASS | `showingDesktop=false`, `desktop-restored` |
| 2.3 | Şerit ekranın sağ alt köşesinde | PASS | Yüzey 6×68, panelin son diliminde (y=1012-1080) |
| 3.1 | Meta+D | PASS | KDE yerel kısayolu çalışıyor |
| 3.5 | Meta+Home (diğerlerini küçült) | PASS | Küçültülen yığını doldu |
| 3.6 | Meta+Shift+M (geri getir) | PASS | Yığın boşaldı |
| 6.1 | Tepsi menüsü 8 öğe | PASS | `dbusmenu` 8 öğe |
| 6.2-6.4 | Menü eylemleri (masaüstü/küçült/geri getir) | PASS | Canlı doğrulandı |
| 8.1 | Kısayol çakışması bildirimi | PASS | Çakışma tablosu günlükte |
| 8.3 | `--restore-shortcuts` | PASS | Tuşlar geri yazıldı ve okunarak doğrulandı |
| 9.1 | KWin yeniden başlatma → betik yeniden yüklenir | PASS | Yeni `script-loaded` olayı |
| 1.5 | Tek örnek | PASS | İkinci süreç mevcut örneğe yönlendi |
| 4.x | Aero Peek | **KISMEN** | Mantık testleri PASS; canlı hover otomasyonu yok |
| 5.x | Aero Shake | **KISMEN** | Mantık testleri PASS; canlı sallama denenmedi |
| 7.5 | İngilizce arayüz | **SKIP** | Canlı denenmedi; katalog ve QML sözlüğü hazır |
| 2.5 | Tam ekranda şerit gizleme | **SKIP** | Canlı denenmedi (kod yolu mevcut) |
| 10.1 | Çoklu ekran | **SKIP** | Tek ekranlı ortam |
| 10.3 | X11 oturumu | **SKIP** | Wayland oturumu |

## 3. Bilinen sınırlar

- KWin, `setForeignShortcutKeys` yazmalarını yok sayıyor; bu yüzden
  Meta+D ve Alt+F4 KDE'nin yerel eyleminde kalıyor, küçültme/büyütme
  `Meta+Shift+Down` / `Meta+Shift+Up` alternatiflerine kayıyor.
- AppStream meta verisinde genel ana sayfa adresi yok; doğrulamada bu
  uyarı bilgi düzeyine indiriliyor.
- Dil değişikliği uygulamanın yeniden başlatılmasını gerektirir.
- Bu ortamda X11, çoklu ekran, valgrind ve bazı statik denetim araçları
  bulunmadığı için ilgili maddeler SKIP olarak raporlandı.
