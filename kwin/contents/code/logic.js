/*
 * Come Back Window — KWin bağımsız saf mantık modülü
 *
 * Bu dosya KWin global'lerine dokunmaz. `node --test` ile sahte `ws`
 * nesneleri kullanılarak test edilir. KWin içinde `main.qml` tarafından
 * `import "code/logic.js" as Logic` ile yüklenir.
 *
 * Sözleşme:
 *   - `ws.stackingOrder`      : pencere listesi (alttan üste)
 *   - `ws.activeWindow`      : etkin pencere veya null
 *   - `ws.currentDesktop`    : {id: "..."} veya sayı
 *   - `ws.currentActivity`   : etkinlik kimliği (string)
 *   - `ws.screens`           : [{name, geometry:{x,y,width,height}, devicePixelRatio}]
 *   - `ws.clientArea(option, screen, desktop)` : {x,y,width,height}
 *   - `ws.showingDesktop`    : (varsa) mantıksal değer
 *   - `ws.slotToggleShowDesktop()` : (varsa) masaüstü aç/kapat
 *
 * Pencere nesnesi (KWin `Window`):
 *   normalWindow, skipTaskbar, desktopWindow, dock, notification,
 *   popupMenu, splash, dropdownMenu, tooltip, comboBox, dndIcon,
 *   appletPopup, onScreenDisplay, criticalNotification, utility,
 *   minimized, minimizable, maximizable, maximizeMode, tile,
 *   opacity, internalId, desktops[], activities[],
 *   setMaximize(v, h), closeWindow()
 */

/* --- Yönetilebilir pencere süzgeci ------------------------------------- */

var EXCLUDED_FLAGS = [
    "desktopWindow", "dock", "notification", "popupMenu", "splash",
    "dropdownMenu", "tooltip", "comboBox", "dndIcon", "appletPopup",
    "onScreenDisplay", "criticalNotification", "inputMethod", "outline"
];

function isManageable(w) {
    if (w === null || w === undefined) {
        return false;
    }
    if (w.normalWindow !== true) {
        return false;
    }
    if (w.skipTaskbar === true) {
        return false;
    }
    for (var i = 0; i < EXCLUDED_FLAGS.length; i++) {
        var flag = EXCLUDED_FLAGS[i];
        if (w[flag] === true) {
            return false;
        }
    }
    return true;
}

/* Geçerli sanal masaüstü + etkinlik süzgeci. */
function onCurrent(w, ctx) {
    if (w === null || w === undefined) {
        return false;
    }
    if (ctx === null || ctx === undefined) {
        return true;
    }
    if (Array.isArray(w.desktops) && w.desktops.length > 0 && ctx.currentDesktop !== undefined) {
        var onDesk = false;
        for (var i = 0; i < w.desktops.length; i++) {
            var d = w.desktops[i];
            var id = (d !== null && typeof d === "object") ? d.id : d;
            if (String(id) === String(ctx.currentDesktop)) {
                onDesk = true;
                break;
            }
        }
        if (!onDesk) {
            return false;
        }
    }
    if (Array.isArray(w.activities) && w.activities.length > 0 && ctx.currentActivity !== undefined) {
        var onAct = false;
        for (var j = 0; j < w.activities.length; j++) {
            if (String(w.activities[j]) === String(ctx.currentActivity)) {
                onAct = true;
                break;
            }
        }
        if (!onAct) {
            return false;
        }
    }
    return true;
}

function context(ws) {
    var desk = ws.currentDesktop;
    if (desk !== null && desk !== undefined && typeof desk === "object") {
        desk = desk.id;
    }
    return { currentDesktop: desk, currentActivity: ws.currentActivity };
}

function manageableWindows(ws) {
    var out = [];
    var list = ws.stackingOrder || [];
    var ctx = context(ws);
    for (var i = 0; i < list.length; i++) {
        if (isManageable(list[i]) && onCurrent(list[i], ctx)) {
            out.push(list[i]);
        }
    }
    return out;
}

/* --- Durum (state) yardımcıları --------------------------------------- */

function newState() {
    return {
        minimizedStack: [],   /* internalId listesi */
        shakeList: [],        /* internalId listesi */
        shakeActive: false,
        peekSaved: {},        /* internalId -> opaklık */
        peekActive: false
    };
}

function winId(w) {
    return String(w.internalId);
}

function indexOfId(list, id) {
    for (var i = 0; i < list.length; i++) {
        if (String(list[i]) === String(id)) {
            return i;
        }
    }
    return -1;
}

function removeId(list, id) {
    var idx = indexOfId(list, id);
    if (idx >= 0) {
        list.splice(idx, 1);
    }
}

function pushUnique(list, id) {
    if (indexOfId(list, id) < 0) {
        list.push(id);
    }
}

function findWindow(ws, id) {
    var list = ws.stackingOrder || [];
    for (var i = 0; i < list.length; i++) {
        if (String(list[i].internalId) === String(id)) {
            return list[i];
        }
    }
    return null;
}

/* Pencere kapanınca listelerden düşür. */
function onClosed(state, id) {
    removeId(state.minimizedStack, id);
    removeId(state.shakeList, id);
    delete state.peekSaved[String(id)];
}

/* Kullanıcı pencereyi başka yolla geri getirdiyse yığından düşür. */
function onMinimizedChanged(state, w) {
    if (w === null || w === undefined) {
        return;
    }
    if (w.minimized === false) {
        var id = winId(w);
        removeId(state.minimizedStack, id);
    }
}

/* --- F1: Masaüstünü göster / geri getir ------------------------------- */

function toggleShowDesktop(ws) {
    if (typeof ws.slotToggleShowDesktop === "function") {
        ws.slotToggleShowDesktop();
        return ws.showingDesktop;
    }
    ws.showingDesktop = !ws.showingDesktop;
    return ws.showingDesktop;
}

/* --- F2: Aktif pencereyi küçült --------------------------------------- */

function minimizeActive(ws, state) {
    var w = ws.activeWindow;
    if (!isManageable(w) || !onCurrent(w, context(ws))) {
        return false;
    }
    if (w.tile !== null && w.tile !== undefined) {
        w.tile = null;
    } else if (w.maximizeMode !== 0) {
        w.setMaximize(false, false);
    } else if (w.minimizable !== false) {
        w.minimized = true;
        pushUnique(state.minimizedStack, winId(w));
    } else {
        return false;
    }
    return true;
}

/* --- F3: Büyüt / eski haline ------------------------------------------ */

function maximizeActive(ws) {
    var w = ws.activeWindow;
    if (!isManageable(w) || !onCurrent(w, context(ws))) {
        return false;
    }
    if (w.maximizable === false) {
        return false;
    }
    if (w.maximizeMode === 3) {
        return false; /* zaten büyütülmüş */
    }
    w.setMaximize(true, true);
    return true;
}

/* --- F4: Aktif hariç hepsini küçült (aç/kapat) ------------------------ */

function minimizeOthers(ws, state) {
    if (state.shakeActive) {
        var restored = restoreList(ws, state.shakeList);
        state.shakeList = [];
        state.shakeActive = false;
        return restored;
    }

    var active = ws.activeWindow;
    if (!isManageable(active)) {
        return 0;
    }
    var windows = manageableWindows(ws);
    var count = 0;
    state.shakeList = [];
    for (var i = 0; i < windows.length; i++) {
        var w = windows[i];
        if (winId(w) === winId(active)) {
            continue;
        }
        if (w.minimized === true) {
            continue; /* zaten küçük: geri getirme listesine yazma */
        }
        if (w.minimizable === false) {
            continue;
        }
        w.minimized = true;
        pushUnique(state.minimizedStack, winId(w));
        pushUnique(state.shakeList, winId(w));
        count++;
    }
    state.shakeActive = true;
    return count;
}

/* --- F5: Küçültülenleri geri getir ------------------------------------ */

function restoreAllMinimized(ws, state) {
    var windows = manageableWindows(ws);
    var count = 0;

    for (var i = 0; i < windows.length; i++) {
        var w = windows[i];
        if (w.minimized === true && w.minimizable !== false) {
            w.minimized = false;
            count++;
        }
    }
    state.minimizedStack = [];
    state.shakeList = [];
    state.shakeActive = false;
    return count;
}

function restoreList(ws, ids) {
    var count = 0;
    for (var i = 0; i < ids.length; i++) {
        var w = findWindow(ws, ids[i]);
        if (w !== null && w.minimizable !== false && w.minimized === true) {
            w.minimized = false;
            count++;
        }
    }
    return count;
}

/* --- F6: Pencereyi kapat ---------------------------------------------- */

function closeActive(ws) {
    var w = ws.activeWindow;
    if (!isManageable(w) || !onCurrent(w, context(ws))) {
        return false;
    }
    w.closeWindow();
    return true;
}

/* --- Panel düzeni ----------------------------------------------------- */

var DOCK_WINDOW_TYPE = 2;   /* KWin::WindowType::Dock */

function rectOf(area) {
    if (area === null || area === undefined) {
        return null;
    }
    var x = Number(area.x);
    var y = Number(area.y);
    var w = Number(area.width);
    var h = Number(area.height);
    if (!isFinite(x) || !isFinite(y) || !isFinite(w) || !isFinite(h)) {
        return null;
    }
    return { x: x, y: y, width: w, height: h };
}

function intersects(a, b) {
    return a.x < b.x + b.width && a.x + a.width > b.x
        && a.y < b.y + b.height && a.y + a.height > b.y;
}

/*
 * KWin 6'da `workspace.panels` yoktur; paneller `stackingOrder` içinde
 * Dock türünde pencereler olarak görünür. Panel penceresini bulur.
 */
function panelWindow(ws, screenGeo) {
    var wins = ws.stackingOrder || [];
    for (var i = 0; i < wins.length; i++) {
        var w = wins[i];
        if (w === null || w === undefined) {
            continue;
        }
        var isDock = w.dock === true || w.windowType === DOCK_WINDOW_TYPE;
        if (!isDock) {
            continue;
        }
        var r = rectOf(w.frameGeometry !== undefined ? w.frameGeometry : w.geometry);
        if (r !== null && intersects(r, screenGeo)) {
            return r;
        }
    }
    return null;
}

/* Panel penceresinden hangi kenarda olduğunu ve kalınlığını çıkarır. */
function detectPanel(ws, screenGeo) {
    var panel = panelWindow(ws, screenGeo);

    if (panel === null) {
        return null;
    }
    var right = screenGeo.x + screenGeo.width;
    var bottom = screenGeo.y + screenGeo.height;
    var candidates = [
        { edge: "top", value: panel.y + panel.height - screenGeo.y,
          ok: panel.y <= screenGeo.y },
        { edge: "bottom", value: bottom - panel.y,
          ok: panel.y + panel.height >= bottom },
        { edge: "left", value: panel.x + panel.width - screenGeo.x,
          ok: panel.x <= screenGeo.x },
        { edge: "right", value: right - panel.x,
          ok: panel.x + panel.width >= right }
    ];
    var best = null;

    for (var i = 0; i < candidates.length; i++) {
        var c = candidates[i];

        if (!c.ok || c.value <= 0) {
            continue;
        }
        /* Kenar boyunca uzanan panel, ekranın yarısından kalın olamaz. */
        var limit = (c.edge === "left" || c.edge === "right")
            ? screenGeo.width / 2 : screenGeo.height / 2;
        if (c.value >= limit) {
            continue;
        }
        if (best === null || c.value > best.value) {
            best = c;
        }
    }
    if (best === null) {
        return null;
    }
    return { edge: best.edge, thickness: Math.round(best.value) };
}

function detectEdge(full, max, epsilon) {
    if (full === null || max === null) {
        return null;
    }    var e = epsilon || 1;
    var left = max.x - full.x;
    var top = max.y - full.y;
    var right = (full.x + full.width) - (max.x + max.width);
    var bottom = (full.y + full.height) - (max.y + max.height);
    var best = null;
    var bestValue = 0;
    var candidates = [
        { edge: "left", value: left },
        { edge: "right", value: right },
        { edge: "top", value: top },
        { edge: "bottom", value: bottom }
    ];
    for (var i = 0; i < candidates.length; i++) {
        if (candidates[i].value > bestValue) {
            bestValue = candidates[i].value;
            best = candidates[i].edge;
        }
    }
    if (best === null || bestValue < e) {
        return null;
    }
    return { edge: best, thickness: Math.round(bestValue) };
}

function panelLayout(ws, fallbackHeight) {
    var out = [];
    var screens = ws.screens || [];
    var fallback = (typeof fallbackHeight === "number" && fallbackHeight > 0)
        ? fallbackHeight : 44;
    var maxOption = 2;      /* KWin ClientAreaOption::MaximizeArea */
    var fullOption = 4;     /* KWin ClientAreaOption::FullScreenArea */
    var desktop = ws.currentDesktop;

    for (var i = 0; i < screens.length; i++) {
        var screen = screens[i];
        var geometry = rectOf(screen.geometry) || { x: 0, y: 0, width: 0, height: 0 };
        var full = null;
        var max = null;

        if (typeof ws.clientArea === "function") {
            full = rectOf(ws.clientArea(fullOption, screen, desktop));
            max = rectOf(ws.clientArea(maxOption, screen, desktop));
        }

        /* Önce gerçek panel penceresi, sonra istemci alanı farkı. */
        var panel = detectPanel(ws, geometry);

        if (panel === null) {
            panel = detectEdge(full, max, 1);
        }
        var edge = panel ? panel.edge : "none";
        var thickness = panel ? panel.thickness : fallback;

        out.push({
            output: String(screen.name),
            x: Math.round(geometry.x),
            y: Math.round(geometry.y),
            w: Math.round(geometry.width),
            h: Math.round(geometry.height),
            edge: edge,
            thickness: thickness,
            scale: Number(screen.devicePixelRatio) || 1
        });
    }
    return out;
}

/* --- F8: Aero Peek ---------------------------------------------------- */

function peekBegin(ws, state, opacity) {
    if (state.peekActive) {
        return 0;
    }
    var windows = manageableWindows(ws);
    var changed = 0;
    state.peekSaved = {};
    for (var i = 0; i < windows.length; i++) {
        var w = windows[i];
        var id = winId(w);
        state.peekSaved[id] = (typeof w.opacity === "number") ? w.opacity : 1;
        try {
            w.opacity = opacity;
            changed++;
        } catch (e) {
            /* opaklık yazılamıyor: yedek yol çağıran tarafta */
            state.peekSaved[id] = null;
        }
    }
    state.peekActive = true;
    return changed;
}

function peekEnd(ws, state) {
    if (!state.peekActive) {
        return 0;
    }
    var count = 0;
    for (var id in state.peekSaved) {
        if (!Object.prototype.hasOwnProperty.call(state.peekSaved, id)) {
            continue;
        }
        var saved = state.peekSaved[id];
        if (saved === null) {
            continue;
        }
        var w = findWindow(ws, id);
        if (w !== null) {
            try {
                w.opacity = saved;
                count++;
            } catch (e) {
                /* yoksay */
            }
        }
    }
    state.peekSaved = {};
    state.peekActive = false;
    return count;
}

/* --- F9: Aero Shake --------------------------------------------------- */

function shakeDetector(config) {
    var cfg = config || {};
    var windowMs = cfg.windowMs || 600;
    var minAmplitude = cfg.minAmplitude || 30;
    var changesNeeded = cfg.changes || 4;

    var anchorX = 0;
    var direction = 0;
    var times = [];
    var primed = false;

    function sign(v) {
        return v > 0 ? 1 : (v < 0 ? -1 : 0);
    }

    function prune(now) {
        while (times.length > 0 && (now - times[0]) > windowMs) {
            times.shift();
        }
    }

    return {
        reset: function() {
            anchorX = 0;
            direction = 0;
            times = [];
            primed = false;
        },
        feed: function(x, t) {
            if (!primed) {
                anchorX = x;
                primed = true;
                return false;
            }
            var delta = x - anchorX;
            var dir = sign(delta);
            if (dir === 0 || Math.abs(delta) < minAmplitude) {
                return false;
            }
            if (direction === 0) {
                direction = dir;
                anchorX = x;
                times.push(t);
            } else if (dir !== direction) {
                direction = dir;
                anchorX = x;
                times.push(t);
            }
            prune(t);
            if (times.length >= changesNeeded) {
                this.reset();
                return true;
            }
            return false;
        }
    };
}

/* --- Node uyumu ------------------------------------------------------- */

if (typeof module !== "undefined" && module.exports) {
    module.exports = {
        isManageable: isManageable,
        onCurrent: onCurrent,
        newState: newState,
        toggleShowDesktop: toggleShowDesktop,
        minimizeActive: minimizeActive,
        maximizeActive: maximizeActive,
        minimizeOthers: minimizeOthers,
        restoreAllMinimized: restoreAllMinimized,
        closeActive: closeActive,
        panelLayout: panelLayout,
        detectPanel: detectPanel,
        peekBegin: peekBegin,
        peekEnd: peekEnd,
        shakeDetector: shakeDetector,
        onClosed: onClosed,
        onMinimizedChanged: onMinimizedChanged,
        winId: winId
    };
}
