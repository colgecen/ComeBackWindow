# Karar Günlüğü

Bu dosya, spesifikasyonda açık bırakılan noktalarda verilen kararları ve
Faz 0 doğrulama sonuçlarını kaydeder. Her satır tarih + karar + gerekçe içerir.

## Faz 0 doğrulama sonuçları (Fedora 44 KDE Plasma, KWin 6.7.5, Wayland)

Aşağıdaki sonuçlar gerçek oturumda `busctl`/`gdbus` ve küçük deneme betikleri
ile alınmıştır. Spesifikasyondaki ⚠️ işaretli varsayımlar burada kesinleşti.

| # | Varsayım | Sonuç | Uygulanan yol |
|---|---|---|---|
| V1 | `loadDeclarativeScript(path, pluginName)` | ✔ Çalışıyor. **Birinci argüman dizin değil, QML dosyasının tam yolu** olmalı ("açılacak dosya bir dizin" hatası dizin verilince alındı). `start()` `/Scripting` üzerinde tüm betikleri başlatır; `/Scripting/Script<id>` nesnesinde `run`/`stop` var. | `kwin_script.c` `contents/ui/main.qml` dosya yolunu geçirir. |
| V2 | QML API şekli | ⚠️ **KWin 6.7'de `workspace` global'i yok** (`ReferenceError: workspace is not defined`). QML'de `import org.kde.kwin 3.0 as KWinApi` ile `KWinApi.Workspace` singleton'ı, `KWinApi.ShortcutHandler`, `KWinApi.DBusCall` kullanılır. Declarative betikte `callDBus` global'i **yoktur**; D-Bus çağrıları `KWinApi.DBusCall` öğesi ile yapılır (context property `KWin` yalnız `readConfig` sunar). `Workspace.showingDesktop` **yoktur**; masaüstü durumu `org.kde.KWin` `/KWin` `showingDesktop` özelliğinden okunur, değiştirmek için `Workspace.slotToggleShowDesktop()` veya `/KWin.showDesktop(b)` kullanılır. `Window.opacity` yazılabilir (denendi: 1 → 0.08 → 1). `Window.minimized` yazılabilir (denendi). `Window.setMaximize(v,h)` ve `Window.closeWindow()` çağrılabilir. | `main.qml` bu API'ye göre yazıldı; `logic.js` saf kalır. |
| V3 | Komut kanalı A (`invokeShortcut`) | ✔ `org.kde.kglobalaccel` `/kglobalaccel` üzerinde `invokeShortcut` **yok**; ancak `/component/kwin` nesnesinde `org.kde.kglobalaccel.Component.invokeShortcut(s)` var ve betiğin `ShortcutHandler`'ını tetikliyor (denendi). | Kanal A: `org.kde.kglobalaccel` `/component/kwin` `org.kde.kglobalaccel.Component.InvokeShortcut`. |
| V4 | Çakışma API'si | Kısmi. `globalShortcutsByKey` okuma ✔ (Meta+Aşağı → `kwin`/`Window Quick Tile Bottom`, Meta+Yukarı → `Window Quick Tile Top`, Meta+D → `Show Desktop`, Alt+F4 → `Window Close`). Ancak `setForeignShortcutKeys`/`setShortcut`/`setShortcutKeys` çağrıları bu oturumda **etkisiz** (dönüş boş, `kglobalshortcutsrc` değişmiyor). | Okuma ile çakışma tespiti; yazma denendikten sonra **doğrulanır**; doğrulanamazsa bildirim + `kcmshell6 keys` (spesifikasyon 10.4 adım 3 yedeği). `--restore-shortcuts` yedeği geri yazmayı dener ve sonucu doğrular. |
| V5 | layer-shell | ✔ `zwlr_layer_shell_v1` sürüm 5 mevcut. Bu makinede **panel yok** (plasmashell `panels()` boş, `MaximizeArea == FullScreenArea`), bu yüzden panelin üstüne çizim fiziksel olarak doğrulanamadı; `OVERLAY` katmanı kullanılır, görünürlük ekran görüntüsü ile doğrulandı. | `strip_wayland.c`: `layer=OVERLAY`, `exclusive_zone=-1`, `keyboard_interactivity=NONE`, `anchor` köşeye göre. |
| V6 | Panel kalınlığı | ✔ Formül çalışıyor (`MaximizeArea` − `FullScreenArea`), bu makinede panel olmadığı için fark 0 → yedek yükseklik 44 px. | `logic.js: panelLayout` panel yoksa `edge:"none"` ve yedek yükseklik döndürür; uygulama 30 s sonra yedekle açar. |
| V7 | Ekran adı eşleşmesi | ✔ Betik `LogicalOutput.name` = `eDP-1`, `wl_output` adı da aynı. | İsimle eşleşme, bulunamazsa geometri eşleşmesi (yedek). |
| V8 | Tepsi | ✔ `org.kde.StatusNotifierWatcher` var, host kayıtlı, 3 SNI öğesi görünüyor. | SNI + dbusmenu doğrudan GDBus ile. |
| V9 | `kcmshell6 keys` | ✔ `/usr/bin/kcmshell6` var. | Doğrudan çağrılır; yoksa `systemsettings kcm_keys`. |
| V10 | Paket adları | Fedora 44'te `gtk4-devel`, `wayland-devel`, `libX11-devel`, `glib2-devel` var. `rsvg2-tools`, `shellcheck`, `cppcheck`, `valgrind`, `clang-tools-extra` bu makinede kurulu değil (testlerde SKIP). | 4. bölümdeki listeler korunur; lint hedefi eksik araçları SKIP olarak raporlar. |

Ek doğrulamalar:

- `wl_compositor`, `wl_shm`, `wl_seat`, `wp_cursor_shape_manager_v1`,
  `wp_fractional_scale_manager_v1`, `zxdg_output_manager_v1` mevcut.
- KWin 6.7'de `Window.internalId` bir `QUuid`'dir; JS'te `String(w.internalId)`
  ile `{...}` biçiminde kararlı kimlik elde edilir.
- `Workspace.currentDesktopChanged(previous, current, output)` imzası üç
  argümanlıdır (spesifikasyondaki tek argüman varsayımı düzeltildi).

## Tasarım kararları

- **2026-11-14 — D-Bus sinyali yerine "ayar değişti" kısayolu:** QML
  tarafında D-Bus sinyali dinlemek için hazır bir öğe yok. Uygulama ayarlar
  değişince betiğin `cbw-cmd-settings-changed` kısayolunu (kanal A) tetikler;
  betik de `GetSettings()` ile yeni değerleri çeker. Sinyal yolu başarısız
  olursa betik 2 saniyede bir `GetSettings()` yoklar (yedek).
- **2026-11-14 — Kanal B yoklaması:** Uygulama, kanal A çağrısını
  doğrulayamazsa `command_channel=poll` ayarını yayınlar; betik 50 ms'de bir
  `PollCommands()` çağırır. Böylece seçim çalışma anında otomatik yapılır.
- **2026-11-14 — JSON ayrıştırma:** json-glib bağımlılığı eklememek için
  `layout.c` içinde küçük, test edilebilir bir JSON okuyucu yazıldı. Yalnız
  `ReportLayout` biçimini destekler; bozuk girdide boş liste döner.
- **2026-11-14 — Şerit genişliği mantıksal piksel:** `strip_width` mantıksal
  pikseldir; Wayland tamponu `wl_output` ölçeği ile çarpılır.
- **2026-11-14 — Peek yedeği:** `Window.opacity` yazılabildiği için birincil
  yol opaklık animasyonudur; yazma başarısız olursa `showDesktop(true/false)`
  geçici gösterimi kullanılır (spesifikasyon 8.5).
- **2026-11-14 — Tek örnek:** `G_BUS_NAME_OWNER_FLAGS_NONE`; ad alınamazsa
  `Activate` çağrılır ve süreç 0 ile çıkar.
- **2026-11-14 — Kurulum veri dizini:** Geliştirmede `CBW_DATA_DIR`, kurulumda
  `$(PREFIX)/share/comebackwindow`.
- **2026-11-14 — Kısayol çakışması:** `allShortcutInfos` tablosu
  `/component/kwin` üzerinden okunur. Yerel eylem aynı Qt tuş kodunu
  kullanıyorsa çakışma vardır. Yerel eylem yalnızca bizim eylemimiz de aynı
  tuşa bağlıysa serbest bırakılmaya çalışılır; yazma `setForeignShortcutKeys`
  ile yapılır ve **okuma ile doğrulanır**. Bu oturumda yazma yok sayıldığı
  için yerel kısayol korunur: Masaüstünü Göster ve Pencereyi Kapat yerel
  KWin eylemine bırakılır (`native_show_desktop`, `native_close`), küçültme
  ve büyütme ise çakışmayan `Meta+Shift+Down` / `Meta+Shift+Up` tuşlarına
  kaydırılır (`native_minimize`, `native_maximize`).
- **2026-11-14 — Kısayol yedeği:** Yerel bir kısayol gerçekten serbest
  bırakılabilirse özgün tuşlar `~/.config/comebackwindow/shortcuts-backup.ini`
  dosyasına yazılır. `--restore-shortcuts` bu dosyayı okur, yazar ve sonucu
  doğrular; doğrulanamazsa dosya korunur ve hata döner.
- **2026-11-15 — Çeviri kaynak dili:** v1.0.0'da kaynak dil Türkçedir;
  `msgid` metinleri Türkçe, İngilizce karşılıklar `po/en.po` içindedir.
  Böylece mevcut arayüz metinleri değiştirilmeden gettext altyapısı
  eklenebildi; `po/tr.po` kaynak dili kataloğu olarak boş `msgstr` taşır ve
  gettext zaten `msgid`e düşer. Arayüz dili ayarı `auto` değilse
  `LANGUAGE` ortam değişkeni gettext ilk çağrısından önce ayarlanır.
- **2026-11-15 — KWin betiği metinleri:** KWin QML betiği uygulamanın
  gettext kataloğunu yükleyemediği için kısayol adları `logic.js` içindeki
  küçük bir sözlükten (`STRINGS`) seçilir; `tr()` bilinmeyen anahtarda
  Türkçeye düşer, `auto` dilde sistem yereli `Qt.locale()` ile çözülür.
- **2026-11-15 — Dayanıklılık:** Okunamayan ayar dosyası silinmez;
  `config.ini.bozuk-<zaman>` adına taşınır ve uygulama varsayılanlarla
  açılır. Oturum D-Bus bağlantısı kapanırsa uygulama yarı ölü kalmak yerine
  günlüğe yazıp temiz biçimde çıkar (oturum açılışında yeniden başlar).
  KWin betiği yüklenemezse üstel geri çekilmeyle (1→60 sn) yeniden denenir
  ve kullanıcı bir kez bilgilendirilir.
- **2026-11-15 — Şerit konumu:** Şerit panelin üstüne değil, ekranın sağ alt
  köşesine yapışır ve panel kalınlığı kadar yükseklikte panelin son
  dilimini kaplar (Windows'taki "Masaüstünü Göster" düğmesi gibi). Panel
  marjini kaldırıldı; şerit her zaman görünür kenara sıfır boşlukla oturur.
  Görünürlük için koyu yarı saydam zemin + masaüstüne bakan turuncu çizgi
  kullanılır; 6 px genişlikte ok çizilmez.
