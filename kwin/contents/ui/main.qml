/*
 * Come Back Window — KWin kapsayıcı betiği
 *
 * Tüm pencere mantığı `code/logic.js` içindedir. Bu dosya yalnızca KWin
 * sinyallerini bağlar, kısayolları kaydeder, animasyonları yürütür ve
 * uygulamaya D-Bus üzerinden rapor verir.
 *
 * Faz 0 bulguları (KWin 6.7 / Plasma 6):
 *   - QML'de `workspace` global'i yok; `KWinApi.Workspace` singleton'ı var.
 *   - `callDBus` global'i yok; `KWinApi.DBusCall` öğesi kullanılır.
 *   - `Workspace.showingDesktop` yok; masaüstü durumu `org.kde.KWin`
 *     `/KWin` `showingDesktop` özelliğinden okunur.
 *   - `Workspace.currentDesktopChanged(previous, current, output)`.
 */
import QtQuick
import org.kde.kwin 3.0 as KWinApi
import "../code/logic.js" as Logic

Item {
    id: root

    readonly property string appService: "org.comebackwindow.App"
    readonly property string appPath: "/org/comebackwindow/App"
    readonly property string appInterface: "org.comebackwindow.App"

    property var ws: KWinApi.Workspace
    property var state: Logic.newState()
    property var settings: ({
        peek_enabled: true,
        peek_delay_ms: 500,
        peek_opacity: 0.08,
        shake_enabled: false,
        strip_enabled: true,
        strip_width: 6,
        fallback_height: 44,
        native_show_desktop: true,
        native_close: true,
        native_minimize: true,
        native_maximize: true,
        command_channel: "invoke"
    })
    property bool peekActive: false
    property bool peekAnimating: false
    property var peekOriginals: ({})
    property var peekTargets: ({})
    property int peekElapsed: 0
    property var shakeDetectors: ({})
    property var shakeHooked: ({})
    property string lastError: ""
    property bool desktopShown: false
    property bool shuttingDown: false

    /* --- D-Bus yardımcıları ------------------------------------------ */

    function dbusCall(method, args, callback, failure) {
        var obj = Qt.createQmlObject(
            'import QtQuick; import org.kde.kwin 3.0 as KWinApi; KWinApi.DBusCall {}',
            root);
        if (obj === null) {
            return null;
        }
        obj.service = root.appService;
        obj.path = root.appPath;
        obj.dbusInterface = root.appInterface;
        obj.method = method;
        obj.arguments = args || [];
        var done = false;
        function cleanup() {
            if (!done) {
                done = true;
                obj.destroy();
            }
        }
        obj.finished.connect(function(ret) {
            cleanup();
            if (callback) {
                callback(ret);
            }
        });
        obj.failed.connect(function() {
            cleanup();
            if (failure) {
                failure();
            }
        });
        obj.call();
        return obj;
    }

    function kwinPropertyGet(prop, callback) {
        var obj = Qt.createQmlObject(
            'import QtQuick; import org.kde.kwin 3.0 as KWinApi; KWinApi.DBusCall {}',
            root);
        obj.service = "org.kde.KWin";
        obj.path = "/KWin";
        obj.dbusInterface = "org.freedesktop.DBus.Properties";
        obj.method = "Get";
        obj.arguments = ["org.kde.KWin", prop];
        obj.finished.connect(function(ret) {
            obj.destroy();
            if (callback) {
                callback(ret && ret.length > 0 ? ret[0] : null);
            }
        });
        obj.failed.connect(function() {
            obj.destroy();
            if (callback) {
                callback(null);
            }
        });
        obj.call();
    }

    function reportEvent(name, data) {
        root.dbusCall("ReportEvent", [name, JSON.stringify(data || {})]);
    }

    function logError(msg) {
        root.lastError = String(msg);
        console.warn("comebackwindow: " + msg);
    }

    /* --- Ayarlar ----------------------------------------------------- */

    function applySettings(json) {
        if (!json || json.length === 0) {
            return;
        }
        var parsed = null;
        try {
            parsed = JSON.parse(json);
        } catch (e) {
            root.logError("ayarlar çözümlenemedi: " + e);
            return;
        }
        if (!parsed) {
            return;
        }
        root.settings = parsed;
        root.applyShortcutPassivity();
    }

    /* Etkin arayüz dili; "auto" ise sistem yereline göre seçilir. */
    function uiLanguage() {
        var lang = root.settings.language;

        if (lang === "auto" || !lang) {
            var name = Qt.locale().name || "tr_TR";
            return name.substring(0, 2);
        }
        return String(lang);
    }

    function fetchSettings() {
        root.dbusCall("GetSettings", [], function(ret) {
            if (ret && ret.length > 0) {
                root.applySettings(ret[0]);
            }
        });
    }

    function applyShortcutPassivity() {
        var nativeDesktop = root.settings.native_show_desktop === true;
        var nativeClose = root.settings.native_close === true;
        var nativeMinimize = root.settings.native_minimize !== false;
        var nativeMaximize = root.settings.native_maximize !== false;

        showDesktop.sequence = nativeDesktop ? "" : "Meta+D";
        showDesktopAlt.sequence = "Meta+M";
        closeShortcut.sequence = nativeClose ? "" : "Alt+F4";
        minimizeShortcut.sequence = nativeMinimize ? "Meta+Shift+Down"
                                                   : "Meta+Down";
        maximizeShortcut.sequence = nativeMaximize ? "Meta+Shift+Up"
                                                   : "Meta+Up";
        desktopWatchTimer.running = nativeDesktop;
    }

    /* --- F1: masaüstünü göster / geri getir -------------------------- */

    function toggleDesktop() {
        Logic.toggleShowDesktop(root.ws);
        root.desktopShown = !root.desktopShown;
        root.reportEvent(root.desktopShown ? "desktop-shown" : "desktop-restored",
                         {});
        desktopVerifyTimer.restart();
    }

    /* KWin durumu gecikmeli yayınlar; kısa süre sonra doğrula. */
    Timer {
        id: desktopVerifyTimer
        interval: 350
        repeat: false
        onTriggered: root.verifyDesktopState()
    }

    /* Yerel Show Desktop kısayolu kullanılıyorsa durumu yokla. */
    Timer {
        id: desktopWatchTimer
        interval: 500
        repeat: true
        running: false
        onTriggered: root.verifyDesktopState()
    }

    function verifyDesktopState() {
        root.kwinPropertyGet("showingDesktop", function(value) {
            if (value === null) {
                return;
            }
            if (value !== root.desktopShown) {
                root.desktopShown = value === true;
                root.reportEvent(value === true ? "desktop-shown"
                                                : "desktop-restored", {});
            }
        });
    }

    /* --- Kısayol işleyicileri ---------------------------------------- */

    function handleMinimize() {
        var changed = Logic.minimizeActive(root.ws, root.state);
        if (changed) {
            root.reportEvent("minimized", {});
            root.scheduleLayoutReport();
        }
    }

    function handleMaximize() {
        if (Logic.maximizeActive(root.ws)) {
            root.reportEvent("maximized", {});
            root.scheduleLayoutReport();
        }
    }

    function handleMinimizeOthers() {
        var count = Logic.minimizeOthers(root.ws, root.state);
        if (count > 0) {
            root.reportEvent(root.state.shakeActive ? "others-minimized" : "others-restored",
                             { count: count });
        }
        root.scheduleLayoutReport();
    }

    function handleRestoreAll() {
        var count = Logic.restoreAllMinimized(root.ws, root.state);
        if (count > 0) {
            root.reportEvent("restored", { count: count });
        }
        root.scheduleLayoutReport();
    }

    function handleClose() {
        if (Logic.closeActive(root.ws)) {
            root.reportEvent("closed", {});
        }
    }

    KWinApi.ShortcutHandler {
        id: showDesktop
        name: "cbw-show-desktop"
        text: "Come Back Window: " + Logic.tr(root.uiLanguage(), "show-desktop")
        sequence: "Meta+D"
        onActivated: root.toggleDesktop()
    }

    KWinApi.ShortcutHandler {
        id: showDesktopAlt
        name: "cbw-show-desktop-alt"
        text: "Come Back Window: " + Logic.tr(root.uiLanguage(), "show-desktop-alt")
        sequence: "Meta+M"
        onActivated: root.toggleDesktop()
    }

    KWinApi.ShortcutHandler {
        id: minimizeShortcut
        name: "cbw-minimize"
        text: "Come Back Window: " + Logic.tr(root.uiLanguage(), "minimize")
        sequence: "Meta+Down"
        onActivated: root.handleMinimize()
    }

    KWinApi.ShortcutHandler {
        id: maximizeShortcut
        name: "cbw-maximize"
        text: "Come Back Window: " + Logic.tr(root.uiLanguage(), "maximize")
        sequence: "Meta+Up"
        onActivated: root.handleMaximize()
    }

    KWinApi.ShortcutHandler {
        id: minimizeOthersShortcut
        name: "cbw-minimize-others"
        text: "Come Back Window: " + Logic.tr(root.uiLanguage(), "minimize-others")
        sequence: "Meta+Home"
        onActivated: root.handleMinimizeOthers()
    }

    KWinApi.ShortcutHandler {
        id: restoreAllShortcut
        name: "cbw-restore-all"
        text: "Come Back Window: " + Logic.tr(root.uiLanguage(), "restore-all")
        sequence: "Meta+Shift+M"
        onActivated: root.handleRestoreAll()
    }

    KWinApi.ShortcutHandler {
        id: closeShortcut
        name: "cbw-close"
        text: "Come Back Window: " + Logic.tr(root.uiLanguage(), "close")
        sequence: "Alt+F4"
        onActivated: root.handleClose()
    }

    /* Uygulamadan gelen gizli komut kısayolları (kanal A). */
    KWinApi.ShortcutHandler {
        id: cmdToggle
        name: "cbw-cmd-toggle-desktop"
        text: "Come Back Window: " + Logic.tr(root.uiLanguage(), "cmd-desktop")
        sequence: ""
        onActivated: root.toggleDesktop()
    }

    KWinApi.ShortcutHandler {
        id: cmdPeekBegin
        name: "cbw-cmd-peek-begin"
        text: "Come Back Window: " + Logic.tr(root.uiLanguage(), "cmd-peek-begin")
        sequence: ""
        onActivated: root.peekBegin()
    }

    KWinApi.ShortcutHandler {
        id: cmdPeekEnd
        name: "cbw-cmd-peek-end"
        text: "Come Back Window: " + Logic.tr(root.uiLanguage(), "cmd-peek-end")
        sequence: ""
        onActivated: root.peekEnd()
    }

    KWinApi.ShortcutHandler {
        id: cmdPeekToggle
        name: "cbw-cmd-peek-toggle"
        text: "Come Back Window: " + Logic.tr(root.uiLanguage(), "cmd-peek-toggle")
        sequence: ""
        onActivated: root.peekActive ? root.peekEnd() : root.peekBegin()
    }

    KWinApi.ShortcutHandler {
        id: cmdMinimizeOthers
        name: "cbw-cmd-minimize-others"
        text: "Come Back Window: " + Logic.tr(root.uiLanguage(), "cmd-minimize-others")
        sequence: ""
        onActivated: root.handleMinimizeOthers()
    }

    KWinApi.ShortcutHandler {
        id: cmdRestoreAll
        name: "cbw-cmd-restore-all"
        text: "Come Back Window: " + Logic.tr(root.uiLanguage(), "cmd-restore-all")
        sequence: ""
        onActivated: root.handleRestoreAll()
    }

    KWinApi.ShortcutHandler {
        id: cmdSettingsChanged
        name: "cbw-cmd-settings-changed"
        text: "Come Back Window: " + Logic.tr(root.uiLanguage(), "cmd-settings-changed")
        sequence: ""
        onActivated: root.fetchSettings()
    }

    KWinApi.ShortcutHandler {
        id: cmdDumpState
        name: "cbw-cmd-dump-state"
        text: "Come Back Window: " + Logic.tr(root.uiLanguage(), "cmd-dump-state")
        sequence: ""
        onActivated: root.reportState()
    }

    /* --- Panel düzeni raporu ---------------------------------------- */

    Timer {
        id: layoutTimer
        interval: 100
        repeat: false
        onTriggered: root.reportLayout()
    }

    function scheduleLayoutReport() {
        layoutTimer.restart();
    }

    function activeFullScreenOutput() {
        var active = root.ws.activeWindow;
        if (!active || active.fullScreen !== true) {
            return "";
        }
        return active.output ? String(active.output.name) : "";
    }

    function reportLayout() {
        var entries = Logic.panelLayout(root.ws, root.settings.fallback_height);
        var fsOutput = root.activeFullScreenOutput();
        for (var i = 0; i < entries.length; i++) {
            entries[i].fullscreen = (fsOutput !== "" && entries[i].output === fsOutput);
        }
        root.dbusCall("ReportLayout", [JSON.stringify(entries)]);
    }

    /* --- Peek -------------------------------------------------------- */

    function peekWindows() {
        return Logic.manageableWindows ? Logic.manageableWindows(root.ws) : [];
    }

    function peekCapture() {
        var windows = root.peekWindows();
        root.peekOriginals = {};
        root.peekTargets = {};
        var active = root.ws.activeWindow;
        var excluded = null;
        if (root.settings.peek_exclude_active === true && active) {
            excluded = String(active.internalId);
        }
        for (var i = 0; i < windows.length; i++) {
            var w = windows[i];
            var id = String(w.internalId);
            if (excluded !== null && id === excluded) {
                continue;
            }
            var from = (typeof w.opacity === "number") ? w.opacity : 1;
            root.peekOriginals[id] = from;
            root.peekTargets[id] = root.settings.peek_opacity;
            root.state.peekSaved[id] = from;
        }
    }

    function peekApply(fraction) {
        var windows = root.peekWindows();
        for (var i = 0; i < windows.length; i++) {
            var w = windows[i];
            var id = String(w.internalId);
            if (!Object.prototype.hasOwnProperty.call(root.peekOriginals, id)) {
                continue;
            }
            var from = root.peekOriginals[id];
            var to = root.peekTargets[id];
            try {
                w.opacity = from + (to - from) * fraction;
            } catch (e) {
                /* opaklık yazılamıyor: yedek yol */
                return false;
            }
        }
        return true;
    }

    function peekRestoreNow() {
        var windows = root.peekWindows();
        for (var i = 0; i < windows.length; i++) {
            var w = windows[i];
            var id = String(w.internalId);
            if (Object.prototype.hasOwnProperty.call(root.peekOriginals, id)) {
                try {
                    w.opacity = root.peekOriginals[id];
                } catch (e) {
                    /* yoksay */
                }
            }
        }
        root.peekOriginals = {};
        root.peekTargets = {};
        root.state.peekSaved = {};
        root.state.peekActive = false;
        root.peekActive = false;
        root.peekAnimating = false;
        root.peekElapsed = 0;
        peekAnimTimer.stop();
        peekGuardTimer.stop();
    }

    function peekBegin() {
        if (root.settings.peek_enabled !== true) {
            return;
        }
        if (root.peekActive) {
            return;
        }
        root.peekCapture();
        root.peekActive = true;
        root.state.peekActive = true;
        root.peekAnimating = true;
        root.peekElapsed = 0;
        peekAnimTimer.start();
        peekGuardTimer.start();
        root.reportEvent("peek-begin", {});
    }

    function peekEnd(immediate) {
        if (!root.peekActive) {
            return;
        }
        peekGuardTimer.stop();
        if (immediate === true) {
            peekAnimTimer.stop();
            root.peekRestoreNow();
            root.reportEvent("peek-end", {});
            return;
        }
        root.peekAnimating = false;
        root.peekElapsed = 0;
        peekAnimTimer.start();
        root.reportEvent("peek-end", {});
    }

    Timer {
        id: peekAnimTimer
        interval: 16
        repeat: true
        onTriggered: {
            if (!root.peekAnimating && !root.peekActive) {
                stop();
                return;
            }
            root.peekElapsed += 16;
            var fraction = Math.min(1, root.peekElapsed / 150);
            var reverse = !root.peekAnimating && root.peekActive;
            if (reverse) {
                fraction = 1 - fraction;
            }
            if (!root.peekApply(fraction)) {
                /* Opaklık yazılamıyor: showDesktop yedeği uygulama tarafında. */
                root.peekRestoreNow();
                root.reportEvent("peek-fallback", {});
                return;
            }
            if (root.peekElapsed >= 150) {
                if (root.peekAnimating) {
                    root.peekAnimating = false;
                    stop();
                } else {
                    root.peekRestoreNow();
                    root.reportEvent("peek-end", {});
                }
            }
        }
    }

    /* Peek bekçisi: uygulama yanıt vermezse hemen geri yükle. */
    Timer {
        id: peekGuardTimer
        interval: 1000
        repeat: true
        onTriggered: root.pingGuard()
    }

    property bool pingPending: false
    property bool pingAnswered: false

    Timer {
        id: pingTimeout
        interval: 800
        repeat: false
        onTriggered: {
            if (root.pingPending && !root.pingAnswered) {
                root.pingPending = false;
                root.peekRestoreNow();
                root.reportEvent("peek-guard-restored", {});
            }
        }
    }

    function pingGuard() {
        if (!root.peekActive) {
            return;
        }
        root.pingPending = true;
        root.pingAnswered = false;
        pingTimeout.restart();
        root.dbusCall("Ping", [], function() {
            root.pingAnswered = true;
            root.pingPending = false;
        }, function() {
            root.pingPending = false;
            root.peekRestoreNow();
            root.reportEvent("peek-guard-restored", {});
        });
    }

    /* --- Kanal B: komut yoklaması ------------------------------------ */

    Timer {
        id: pollTimer
        interval: 50
        repeat: true
        running: root.settings.command_channel === "poll"
        onTriggered: root.pollCommands()
    }

    property bool pollPending: false

    function pollCommands() {
        if (root.pollPending) {
            return;
        }
        root.pollPending = true;
        root.dbusCall("PollCommands", [], function(ret) {
            root.pollPending = false;
            if (!ret || ret.length === 0) {
                return;
            }
            var list = ret[0];
            if (!list) {
                return;
            }
            for (var i = 0; i < list.length; i++) {
                root.runCommand(String(list[i]));
            }
        }, function() {
            root.pollPending = false;
        });
    }

    function runCommand(cmd) {
        switch (cmd) {
        case "toggle-desktop": root.toggleDesktop(); break;
        case "peek-begin": root.peekBegin(); break;
        case "peek-end": root.peekEnd(); break;
        case "minimize-others": root.handleMinimizeOthers(); break;
        case "restore-all": root.handleRestoreAll(); break;
        case "settings-changed": root.fetchSettings(); break;
        case "dump-state": root.reportState(); break;
        default: root.logError("bilinmeyen komut: " + cmd); break;
        }
    }

    /* --- Aero Shake -------------------------------------------------- */

    function shakeConfig() {
        return {
            windowMs: 600,
            minAmplitude: 30,
            changes: 4
        };
    }

    function hookWindow(w) {
        if (!w) {
            return;
        }
        var id = String(w.internalId);
        if (root.shakeHooked[id]) {
            return;
        }
        root.shakeHooked[id] = true;
        w.interactiveMoveResizeStepped.connect(function(geometry) {
            root.onMoveStep(w, geometry);
        });
        w.closed.connect(function() {
            delete root.shakeHooked[id];
            delete root.shakeDetectors[id];
            Logic.onClosed(root.state, id);
            root.scheduleLayoutReport();
        });
        w.minimizedChanged.connect(function() {
            Logic.onMinimizedChanged(root.state, w);
            root.scheduleLayoutReport();
        });
        w.maximizedChanged.connect(function() {
            root.scheduleLayoutReport();
        });
        w.fullScreenChanged.connect(function() {
            root.scheduleLayoutReport();
        });
    }

    function ensureDetector(id) {
        if (!root.shakeDetectors[id]) {
            root.shakeDetectors[id] = Logic.shakeDetector(root.shakeConfig());
        }
        return root.shakeDetectors[id];
    }

    function onMoveStep(w, geometry) {
        if (root.settings.shake_enabled !== true) {
            return;
        }
        if (!w || !w.move) {
            return;
        }
        var id = String(w.internalId);
        var detector = root.ensureDetector(id);
        var now = Date.now();
        var x = geometry ? geometry.x : w.x;
        if (detector.feed(x, now)) {
            var active = root.ws.activeWindow;
            if (active && String(active.internalId) !== id) {
                return;
            }
            var count = Logic.minimizeOthers(root.ws, root.state);
            if (count > 0) {
                root.reportEvent("shake", { count: count });
                root.scheduleLayoutReport();
            }
        }
    }

    /* --- Durum dökümü ------------------------------------------------ */

    function windowInfo(w) {
        return {
            id: String(w.internalId),
            caption: w.caption,
            minimized: w.minimized === true,
            maximized: w.maximizeMode === 3,
            maximizable: w.maximizable !== false,
            minimizable: w.minimizable !== false,
            opacity: w.opacity,
            fullScreen: w.fullScreen === true,
            tile: w.tile ? true : false
        };
    }

    function reportState() {
        var windows = root.ws.stackingOrder || [];
        var list = [];
        for (var i = 0; i < windows.length; i++) {
            if (Logic.isManageable(windows[i])) {
                list.push(root.windowInfo(windows[i]));
            }
        }
        var active = root.ws.activeWindow;
        var payload = {
            showingDesktop: root.desktopShown,
            active: active ? String(active.internalId) : null,
            minimizedStack: root.state.minimizedStack.slice(),
            shakeList: root.state.shakeList.slice(),
            shakeActive: root.state.shakeActive,
            peekActive: root.peekActive,
            windows: list
        };
        root.dbusCall("ReportState", [JSON.stringify(payload)]);
    }

    /* --- Yaşam döngüsü ----------------------------------------------- */

    function connectWorkspace() {
        var list = root.ws.stackingOrder || [];
        for (var i = 0; i < list.length; i++) {
            root.hookWindow(list[i]);
        }
        root.ws.windowAdded.connect(root.onWorkspaceWindowAdded);
        root.ws.windowRemoved.connect(root.onWorkspaceLayoutChange);
        root.ws.windowActivated.connect(root.onWorkspaceLayoutChange);
        root.ws.currentDesktopChanged.connect(root.onWorkspaceLayoutChange);
        root.ws.currentActivityChanged.connect(root.onWorkspaceLayoutChange);
        root.ws.screensChanged.connect(root.onWorkspaceLayoutChange);
        root.ws.screenOrderChanged.connect(root.onWorkspaceLayoutChange);
        root.ws.virtualScreenGeometryChanged.connect(root.onWorkspaceLayoutChange);
        root.ws.screensChanged.connect(root.onWorkspaceScreensChanged);
        root.reconnectScreens();
    }

    /*
     * Betik yeniden yüklenirken eski örnek bir süre çalışmaya devam eder;
     * işleyicileri bırakmazsak her olay iki kez işlenir (ör. masaüstü iki kez
     * açılıp kapanır). Bu yüzden bağlantılar kapanışta çözülür.
     */
    function disconnectWorkspace() {
        var ws = root.ws;
        if (!ws) {
            return;
        }
        ws.windowAdded.disconnect(root.onWorkspaceWindowAdded);
        ws.windowRemoved.disconnect(root.onWorkspaceLayoutChange);
        ws.windowActivated.disconnect(root.onWorkspaceLayoutChange);
        ws.currentDesktopChanged.disconnect(root.onWorkspaceLayoutChange);
        ws.currentActivityChanged.disconnect(root.onWorkspaceLayoutChange);
        ws.screensChanged.disconnect(root.onWorkspaceLayoutChange);
        ws.screenOrderChanged.disconnect(root.onWorkspaceLayoutChange);
        ws.virtualScreenGeometryChanged.disconnect(root.onWorkspaceLayoutChange);
        ws.screensChanged.disconnect(root.onWorkspaceScreensChanged);
    }

    function onWorkspaceWindowAdded(w) {
        root.hookWindow(w);
        root.scheduleLayoutReport();
    }

    function onWorkspaceLayoutChange() {
        root.scheduleLayoutReport();
    }

    function onWorkspaceScreensChanged() {
        root.reconnectScreens();
    }

    function reconnectScreens() {
        var screens = root.ws.screens || [];
        for (var i = 0; i < screens.length; i++) {
            if (!screens[i].geometryChanged) {
                continue;
            }
            if (screens[i]._cbwHooked) {
                continue;
            }
            screens[i]._cbwHooked = true;
            screens[i].geometryChanged.connect(function() {
                root.scheduleLayoutReport();
            });
        }
    }

    Timer {
        id: settingsRefreshTimer
        interval: 2000
        repeat: true
        running: true
        onTriggered: root.fetchSettings()
    }

    Component.onCompleted: {
        root.connectWorkspace();
        root.fetchSettings();
        root.reportLayout();
        root.kwinPropertyGet("showingDesktop", function(value) {
            root.desktopShown = value === true;
        });
        root.reportEvent("script-loaded", { version: 2 });
    }

    Component.onDestruction: {
        root.shuttingDown = true;
        root.disconnectWorkspace();
        root.peekRestoreNow();
        settingsRefreshTimer.stop();
        pollTimer.stop();
    }
}
