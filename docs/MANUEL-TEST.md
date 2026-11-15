# Manuel Test Listesi

Bu liste v1.0.0 kabul testlerini adım adım tanımlar. Her maddeyi
`[ ]` → `[x]` olarak işaretleyin; sonuçları `docs/TEST-RAPORU.md`
dosyasına yazın.

## Ortam

- KDE Plasma 6 (Wayland) oturumu, KWin 6.x
- Uygulama: `./comebackwindow` (depodan) veya `make install` sonrası
  `comebackwindow`
- Şerit için gerçek bir panel (alt/üst/sol/sağ) önerilir

## 1. Kurulum ve ilk açılış

| # | Adım | Beklenen |
|---|---|---|
| 1.1 | `make && make check` | Tüm adımlar yeşil |
| 1.2 | `./comebackwindow --debug` | Günlükte "KWin betiği yüklendi" |
| 1.3 | İlk açılışta sihirbaz | Dil, başlangıç/şerit ve efekt sayfaları; "Başla" kaydeder |
| 1.4 | İkinci kez başlatma | Sihirbaz açılmaz |
| 1.5 | Uygulama çalışırken ikinci örnek | Yeni süreç çıkar, mevcut örnek etkinleşir |

## 2. Masaüstünü Göster şeridi

| # | Adım | Beklenen |
|---|---|---|
| 2.1 | Sağ alt köşedeki ince şeride tıkla | Masaüstü açılır (`showingDesktop=true`) |
| 2.2 | Tekrar tıkla | Masaüstü geri gelir (`showingDesktop=false`) |
| 2.3 | Panele bak | Şerit ekranın tam sağ alt köşesinde, panelin en ucunda |
| 2.4 | `busctl --user get-property org.kde.KWin /KWin org.kde.KWin showingDesktop` | Tıklama sonrası değer değişir |
| 2.5 | Tam ekran pencere açıkken | Şerit gizlenir |

## 3. Kısayollar

| # | Adım | Beklenen |
|---|---|---|
| 3.1 | `Meta+D` | Masaüstü aç/kapat |
| 3.2 | `Meta+M` | Masaüstü aç/kapat (alternatif) |
| 3.3 | `Meta+Down` / `Meta+Shift+Down` | Aktif pencere küçülür |
| 3.4 | `Meta+Up` / `Meta+Shift+Up` | Aktif pencere büyür |
| 3.5 | `Meta+Home` | Aktif hariç hepsi küçülür |
| 3.6 | `Meta+Shift+M` | Küçültülenler geri gelir |
| 3.7 | `Alt+F4` | Aktif pencere kapanır |
| 3.8 | Sistem Ayarları > Kısayollar | "Come Back Window" girdileri görünür |

## 4. Aero Peek

| # | Adım | Beklenen |
|---|---|---|
| 4.1 | İmleci şeritte beklet (500 ms) | Pencereler ayarlanan opaklığa iner |
| 4.2 | İmleci şeritten çıkar | Opaklık eski değerine döner |
| 4.3 | Peek sırasında şeride tıkla | Peek onaylanır, masaüstü aç/kapat olmaz |
| 4.4 | Masaüstü açıkken peek | Peek başlamaz |

## 5. Aero Shake

| # | Adım | Beklenen |
|---|---|---|
| 5.1 | Ayarlardan Aero Shake'i aç | Ayar kaydedilir |
| 5.2 | Bir pencereyi hızlıca sağa-sola salla | Diğer pencereler küçülür |
| 5.3 | Aynı pencereyi tekrar salla | Küçültülenler geri gelir |
| 5.4 | Yavaş hareket | Tetiklenmez |

## 6. Tepsi menüsü

| # | Adım | Beklenen |
|---|---|---|
| 6.1 | Tepsi simgesine tıkla | 8 öğeli menü açılır |
| 6.2 | "Masaüstünü Göster" | Masaüstü aç/kapat |
| 6.3 | "Diğerlerini Küçült" / "Küçültülenleri Geri Getir" | İşlev çalışır |
| 6.4 | "Ayarlar…" | Ayarlar penceresi açılır |
| 6.5 | "İlk Açılış Sihirbazı…" | Sihirbaz açılır |
| 6.6 | "Hakkında…" | Bildirimde sürüm görünür |
| 6.7 | "Çıkış" | Uygulama kapanır, betik boşaltılır |

## 7. Ayarlar

| # | Adım | Beklenen |
|---|---|---|
| 7.1 | Şerit genişliğini değiştir, Kaydet | Şerit yeniden çizilir |
| 7.2 | Peek gecikmesi/opaklığı değiştir | Yeni değerler uygulanır |
| 7.3 | Otomatik başlatmayı kapat | `~/.config/autostart` girdisi silinir |
| 7.4 | Otomatik başlatmayı aç | Girdi yeniden oluşur |
| 7.5 | Dil: English, Kaydet ve yeniden başlat | Arayüz İngilizce |
| 7.6 | Dil: Otomatik | Sistem yereline döner |
| 7.7 | `~/.config/comebackwindow/config.ini` | Değerler kalıcı |

## 8. Kısayol çakışması

| # | Adım | Beklenen |
|---|---|---|
| 8.1 | KDE'de Meta+D zaten bağlı | Çakışma bildirimi bir kez gösterilir |
| 8.2 | Günlükte çakışma tablosu | Hangi tuşun kullanıldığı yazar |
| 8.3 | `./comebackwindow --restore-shortcuts` | Özgün tuşlar geri yazılır ve doğrulanır |

## 9. Dayanıklılık

| # | Adım | Beklenen |
|---|---|---|
| 9.1 | KWin'i yeniden başlat | Betik yeniden yüklenir, şerit geri gelir |
| 9.2 | Ayar dosyasını boz (`config.ini`) | Yedeklenir, varsayılanlarla açılır |
| 9.3 | KWin betiği silinmişken başlat | Bildirim + geri çekilmeli yeniden deneme |
| 9.4 | Oturum D-Bus'ını kes | Uygulama günlüğe yazıp temiz çıkar |

## 10. Çoklu ekran ve X11

| # | Adım | Beklenen |
|---|---|---|
| 10.1 | İkinci ekran bağla | Her ekran için ayrı şerit |
| 10.2 | Paneli üste/sola taşı | Şerit yeni kenara uyar |
| 10.3 | Plasma 6 X11 oturumu | X11 yedek şeridi çalışır |
