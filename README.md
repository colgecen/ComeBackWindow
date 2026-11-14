# Come Back Window

Windows tarzı pencere kısayolları, Aero Peek, Aero Shake ve "Masaüstünü Göster"
şeridi — KDE Plasma 6 (Wayland) için.

Come Back Window, Windows'ta alışık olduğunuz pencere davranışlarını KDE
Plasma 6'ya taşır:

- Masaüstünü göster / geri getir (Meta+D, Meta+M)
- Aktif pencereyi küçült, büyüt, eski boyuta getir, kapat
- Aktif hariç hepsini küçült (Meta+Home) ve geri getir (Meta+Shift+M)
- Görev çubuğunun en sağındaki ince "Masaüstünü Göster" şeridi
- Aero Peek: şeridin üstünde beklerseniz pencereler şeffaflaşır
- Aero Shake: pencereyi sallayınca diğerleri küçülür (varsayılan kapalı)
- Turuncu "geri" logosu, Türkçe/İngilizce arayüz, ilk açılış penceresi

Uygulama C11 ile yazılmıştır (GLib/GIO, GTK4, Wayland layer-shell, Xlib).
Pencere yönetimi KWin'in kendi betik API'si üzerinden yapılır; bu Wayland'de
pencere yönetiminin tek meşru yoludur.

## Durum

Bu depo v1.0.0 sürümünü hedefler. Kurulum, mimari ve test ayrıntıları için
`docs/` klasörüne ve `README`'nin devamına bakın (sonraki commit'lerde
genişletilir).

## Lisans

MIT — bkz. `LICENSE`.
