# v1.0.0 Kabul Listesi

Her kriter, `docs/TEST-RAPORU.md` içindeki kanıtlarla eşleştirilmiştir.

| # | Kriter | Durum | Kanıt |
|---|---|---|---|
| 1 | KDE Plasma 6 / Wayland ortamında çalışır | PASS | KWin 6.7.5, entegrasyon testi |
| 2 | Masaüstünü Göster aç/kapat (kısayol) | PASS | Meta+D canlı denendi |
| 3 | Masaüstünü Göster aç/kapat (şerit) | PASS | `showingDesktop` iki yönde değişti |
| 4 | Şerit panelin üstüne binmez | PASS | 6×68 yüzey, 68 px panel |
| 5 | Pencere küçült / büyüt | PASS | Mantık testleri + kısayol yönlendirme |
| 6 | Aktif pencereyi kapat | PASS | KDE yerel Alt+F4 |
| 7 | Aktif hariç hepsini küçült | PASS | Entegrasyon testi |
| 8 | Küçültülenleri geri getir | PASS | Entegrasyon testi |
| 9 | Aero Peek | PASS (mantık) | JS testleri; canlı hover SKIP |
| 10 | Aero Shake | PASS (mantık) | JS testleri; canlı sallama SKIP |
| 11 | Tepsi simgesi ve menüsü | PASS | 8 öğe, eylemler canlı denendi |
| 12 | Ayarlar penceresi | PASS | GTK4 pencere, kaydetme |
| 13 | İlk açılış sihirbazı | PASS | Canlı denendi |
| 14 | Kısayol çakışma yönetimi | PASS | Çakışma tablosu + `--restore-shortcuts` |
| 15 | Otomatik başlatma | PASS | Birim testleri + dosya yönetimi |
| 16 | Türkçe/İngilizce arayüz | PASS (altyapı) | Kataloglar + QML sözlüğü; en canlı SKIP |
| 17 | KWin yeniden başlatmaya dayanıklılık | PASS | Betik yeniden yüklendi |
| 18 | Bozuk ayar dosyası toparlama | PASS | Birim testi |
| 19 | Tek örnek | PASS | Canlı denendi |
| 20 | Kurulabilir paket | PASS | `make install DESTDIR=...`, spec + PKGBUILD |
| 21 | Testler yeşil | PASS | `make check`, ASan, entegrasyon |
| 22 | Valgrind temizliği | SKIP | valgrind kurulu değil |
| 23 | X11 oturumu | SKIP | Ortamda X11 oturumu yok |
| 24 | Çoklu ekran | SKIP | Tek ekranlı ortam |

## Kapsam dışı

- KDE Plasma 5 ve GNOME: v1.0.0 kapsamı dışında (spesifikasyon §18).
