#!/usr/bin/env bash
# Come Back Window — uçtan uca entegrasyon testleri
#
# Gerçek oturumda çalışır: uygulamayı geçici bir yapılandırma diziniyle
# başlatır, KWin betiğinin yüklendiğini doğrular, iki test penceresiyle
# küçültme/geri getirme ve masaüstü aç/kapat yollarını sınar.
# Compositor yeniden başlatılmaz; yalnızca D-Bus ve pencere işlemleri
# kullanılır. Gerekli araçlar yoksa SKIP döner.
set -u

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
APP="$ROOT/build/comebackwindow"
TESTWIN="$ROOT/tests/helpers/testwin"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/cbw-integration-XXXXXX")
APP_PID=""
WIN_PIDS=""

say() { printf '%s\n' "$*"; }
fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }

cleanup() {
    for pid in $WIN_PIDS; do
        kill "$pid" 2>/dev/null || true
    done
    if [ -n "$APP_PID" ] && kill -0 "$APP_PID" 2>/dev/null; then
        shown=$(busctl --user get-property org.kde.KWin /KWin \
                    org.kde.KWin showingDesktop 2>/dev/null | awk '{print $2}')
        if [ "$shown" = "true" ]; then
            "$APP" --toggle-desktop >/dev/null 2>&1 || true
        fi
        kill "$APP_PID" 2>/dev/null || true
        wait "$APP_PID" 2>/dev/null || true
    fi
    rm -rf "$TMP"
}
trap cleanup EXIT

wait_for() {
    for _ in $(seq 1 80); do
        if eval "$1" >/dev/null 2>&1; then
            return 0
        fi
        sleep 0.25
    done
    return 1
}

desktop_shown() {
    busctl --user get-property org.kde.KWin /KWin org.kde.KWin \
        showingDesktop 2>/dev/null | awk '{print $2}'
}

desktop_is() {
    [ "$(desktop_shown)" = "$1" ]
}

state_value() { # $1: JSON alanı (dizi uzunluğu)
    "$APP" --dump-state 2>/dev/null | python3 -c \
        'import json,sys; d=json.load(sys.stdin); print(len(d.get(sys.argv[1], [])))' "$1"
}

[ -x "$APP" ] || fail "uygulama derlenmemiş: $APP"
[ -x "$TESTWIN" ] || fail "testwin derlenmemiş: $TESTWIN"
command -v busctl >/dev/null || { say "SKIP test-integration (busctl yok)"; exit 0; }
command -v python3 >/dev/null || { say "SKIP test-integration (python3 yok)"; exit 0; }
busctl --user list 2>/dev/null | grep -q "org.kde.KWin " \
    || { say "SKIP test-integration (KWin D-Bus yok)"; exit 0; }
[ -n "${WAYLAND_DISPLAY:-}" ] \
    || { say "SKIP test-integration (Wayland oturumu yok)"; exit 0; }

cat > "$TMP/config.ini" <<'EOF'
[comebackwindow]
autostart=false
strip_enabled=true
strip_width=6
fallback_height=44
peek_enabled=true
peek_delay_ms=500
peek_opacity=0.080
shake_enabled=false
tray_enabled=false
language=tr
first_run_done=true
EOF

say "== uygulama başlatılıyor =="
CBW_CONFIG_DIR="$TMP" CBW_DATA_DIR="$ROOT" "$APP" --debug \
    > "$TMP/app.log" 2>&1 &
APP_PID=$!

wait_for "busctl --user call org.comebackwindow.App /org/comebackwindow/App \
          org.comebackwindow.App Ping | grep -q true" \
    || fail "uygulama D-Bus adını almadı; günlük: $TMP/app.log"
say "PASS uygulama D-Bus adını aldı"

wait_for "busctl --user call org.kde.KWin /Scripting org.kde.kwin.Scripting \
          isScriptLoaded s comebackwindow | grep -q true" \
    || fail "KWin betiği yüklenmedi; günlük: $TMP/app.log"
say "PASS KWin betiği yüklendi"

busctl --user call org.comebackwindow.App /org/comebackwindow/App \
    org.comebackwindow.App GetSettings \
    | grep -q "command_channel" \
    || fail "GetSettings beklenen JSON'u döndürmedi"
say "PASS ayarlar D-Bus üzerinden okunuyor"

if grep -q "şerit yüzeyi oluşturuldu\|şerit yüzeyi yeniden kullanıldı" \
        "$TMP/app.log"; then
    say "PASS şerit yüzeyi oluşturuldu"
else
    say "SKIP şerit yüzeyi (layer-shell yok)"
fi

say "== test pencereleri açılıyor =="
"$TESTWIN" --title "cbw-test-1" --seconds 30 > "$TMP/win1.log" 2>&1 &
WIN_PIDS="$WIN_PIDS $!"
"$TESTWIN" --title "cbw-test-2" --seconds 30 > "$TMP/win2.log" 2>&1 &
WIN_PIDS="$WIN_PIDS $!"
wait_for "grep -q HAZIR $TMP/win1.log" || fail "ilk test penceresi açılmadı"
wait_for "grep -q HAZIR $TMP/win2.log" || fail "ikinci test penceresi açılmadı"
sleep 1

say "== diğerlerini küçült =="
"$APP" --minimize-others >/dev/null 2>&1
sleep 1.5
[ "$(state_value minimizedStack)" -ge 1 ] \
    || fail "MinimizeOthers küçültülen pencere bildirmedi"
say "PASS MinimizeOthers"

say "== hepsini geri getir =="
"$APP" --restore-all >/dev/null 2>&1
sleep 1.5
[ "$(state_value minimizedStack)" -eq 0 ] \
    || fail "RestoreAll küçültülenleri geri getirmedi"
say "PASS RestoreAll"

say "== masaüstü aç/kapat =="
"$APP" --toggle-desktop >/dev/null 2>&1
wait_for "desktop_is true" || fail "masaüstü gösterilemedi"
"$APP" --toggle-desktop >/dev/null 2>&1
wait_for "desktop_is false" || fail "masaüstü geri getirilemedi"
say "PASS ToggleDesktop (aç/kapat)"

say "== kapanış =="
kill "$APP_PID" 2>/dev/null || true
wait "$APP_PID" 2>/dev/null || true
APP_PID=""
wait_for "busctl --user call org.kde.KWin /Scripting org.kde.kwin.Scripting \
          isScriptLoaded s comebackwindow | grep -q false" \
    || fail "uygulama kapanınca betik boşaltılmadı"
say "PASS betik boşaltıldı"

say "test-integration: TÜMÜ GEÇTİ"
