"use strict";

const test = require("node:test");
const assert = require("node:assert");
const Logic = require("../../kwin/contents/code/logic.js");
const { makeWindow, makeWorkspace } = require("./fake_ws.js");

test("isManageable: normal pencere kabul edilir", () => {
    assert.strictEqual(Logic.isManageable(makeWindow()), true);
});

test("isManageable: masaüstü, dock, bildirim ve skipTaskbar reddedilir", () => {
    assert.strictEqual(Logic.isManageable(makeWindow({ desktopWindow: true })), false);
    assert.strictEqual(Logic.isManageable(makeWindow({ dock: true })), false);
    assert.strictEqual(Logic.isManageable(makeWindow({ notification: true })), false);
    assert.strictEqual(Logic.isManageable(makeWindow({ skipTaskbar: true })), false);
    assert.strictEqual(Logic.isManageable(makeWindow({ popupMenu: true })), false);
    assert.strictEqual(Logic.isManageable(makeWindow({ splash: true })), false);
    assert.strictEqual(Logic.isManageable(makeWindow({ tooltip: true })), false);
    assert.strictEqual(Logic.isManageable(makeWindow({ normalWindow: false })), false);
    assert.strictEqual(Logic.isManageable(null), false);
});

test("onCurrent: başka masaüstü ve etkinlik dışlanır", () => {
    const ctx = { currentDesktop: "d1", currentActivity: "a1" };
    assert.strictEqual(Logic.onCurrent(makeWindow(), ctx), true);
    assert.strictEqual(
        Logic.onCurrent(makeWindow({ desktops: [{ id: "d2" }] }), ctx), false);
    assert.strictEqual(
        Logic.onCurrent(makeWindow({ activities: ["a2"] }), ctx), false);
    /* Tüm masaüstlerinde olan pencere (desktops boş) etkilenir. */
    assert.strictEqual(Logic.onCurrent(makeWindow({ desktops: [] }), ctx), true);
});

test("F1: masaüstü aç/kapat iki çağrıda eski hale döner", () => {
    const ws = makeWorkspace();
    assert.strictEqual(Logic.toggleShowDesktop(ws), true);
    assert.strictEqual(ws.toggleCount, 1);
    assert.strictEqual(Logic.toggleShowDesktop(ws), false);
    assert.strictEqual(ws.toggleCount, 2);
});

test("F2: büyütülmüş pencere eski boyuta döner, küçültülmez", () => {
    const w = makeWindow({ maximizeMode: 3 });
    const ws = makeWorkspace({ windows: [w], active: w });
    const state = Logic.newState();

    assert.strictEqual(Logic.minimizeActive(ws, state), true);
    assert.strictEqual(w.maximizeMode, 0);
    assert.strictEqual(w.minimized, false);
    assert.deepStrictEqual(state.minimizedStack, []);
});

test("F2: döşenmiş pencere döşemeden çıkar", () => {
    const w = makeWindow({ tile: { id: 1 }, maximizeMode: 2 });
    const ws = makeWorkspace({ windows: [w], active: w });
    const state = Logic.newState();

    assert.strictEqual(Logic.minimizeActive(ws, state), true);
    assert.strictEqual(w.tile, null);
    assert.strictEqual(w.minimized, false);
});

test("F2: normal pencere küçültülür ve yığına eklenir", () => {
    const w = makeWindow();
    const ws = makeWorkspace({ windows: [w], active: w });
    const state = Logic.newState();

    assert.strictEqual(Logic.minimizeActive(ws, state), true);
    assert.strictEqual(w.minimized, true);
    assert.deepStrictEqual(state.minimizedStack, [w.internalId]);
});

test("F2: küçültülemez pencereye dokunulmaz", () => {
    const w = makeWindow({ minimizable: false });
    const ws = makeWorkspace({ windows: [w], active: w });
    const state = Logic.newState();

    assert.strictEqual(Logic.minimizeActive(ws, state), false);
    assert.strictEqual(w.minimized, false);
});

test("F2: aktif pencere yoksa sessizce hiçbir şey yapmaz", () => {
    const ws = makeWorkspace({ windows: [] });
    assert.strictEqual(Logic.minimizeActive(ws, Logic.newState()), false);
});

test("F3: normal pencere büyütülür", () => {
    const w = makeWindow();
    const ws = makeWorkspace({ windows: [w], active: w });
    assert.strictEqual(Logic.maximizeActive(ws), true);
    assert.strictEqual(w.maximizeMode, 3);
});

test("F3: zaten büyütülmüş pencere değişmez", () => {
    const w = makeWindow({ maximizeMode: 3 });
    const ws = makeWorkspace({ windows: [w], active: w });
    assert.strictEqual(Logic.maximizeActive(ws), false);
    assert.strictEqual(w.maximizeMode, 3);
});

test("F3: döşenmiş pencere büyütülür", () => {
    const w = makeWindow({ tile: { id: 1 }, maximizeMode: 2 });
    const ws = makeWorkspace({ windows: [w], active: w });
    assert.strictEqual(Logic.maximizeActive(ws), true);
    assert.strictEqual(w.maximizeMode, 3);
});

test("F4: aktif hariç küçülür, ikinci çağrı yalnız listedekileri geri getirir", () => {
    const a = makeWindow({ id: "a", caption: "aktif" });
    const b = makeWindow({ id: "b" });
    const c = makeWindow({ id: "c" });
    const pre = makeWindow({ id: "pre", minimized: true });
    const ws = makeWorkspace({ windows: [a, b, c, pre], active: a });
    const state = Logic.newState();

    assert.strictEqual(Logic.minimizeOthers(ws, state), 2);
    assert.strictEqual(a.minimized, false);
    assert.strictEqual(b.minimized, true);
    assert.strictEqual(c.minimized, true);
    assert.strictEqual(pre.minimized, true);
    assert.deepStrictEqual(state.shakeList.sort(), ["b", "c"]);

    assert.strictEqual(Logic.minimizeOthers(ws, state), 2);
    assert.strictEqual(b.minimized, false);
    assert.strictEqual(c.minimized, false);
    /* Önceden küçük olan pencere geri gelmez. */
    assert.strictEqual(pre.minimized, true);
    assert.strictEqual(state.shakeActive, false);
});

test("F4: aktif pencere yoksa hata vermez", () => {
    const ws = makeWorkspace({ windows: [] });
    assert.strictEqual(Logic.minimizeOthers(ws, Logic.newState()), 0);
});

test("F5: tüm küçültülmüş pencereler (yığın dışı dahil) geri gelir", () => {
    const a = makeWindow({ id: "a", minimized: true });
    const b = makeWindow({ id: "b", minimized: true });
    const c = makeWindow({ id: "c" });
    const ws = makeWorkspace({ windows: [a, b, c] });
    const state = Logic.newState();
    state.minimizedStack = ["a"];

    assert.strictEqual(Logic.restoreAllMinimized(ws, state), 2);
    assert.strictEqual(a.minimized, false);
    assert.strictEqual(b.minimized, false);
    assert.strictEqual(c.minimized, false);
    assert.deepStrictEqual(state.minimizedStack, []);
});

test("F6: aktif pencere kapanır", () => {
    const w = makeWindow({ id: "a" });
    const ws = makeWorkspace({ windows: [w], active: w });
    assert.strictEqual(Logic.closeActive(ws), true);
    assert.strictEqual(w.closed, true);
});

test("F6: aktif pencere yoksa hata vermez", () => {
    const ws = makeWorkspace({ windows: [] });
    assert.strictEqual(Logic.closeActive(ws), false);
});

test("Kapsam: başka masaüstü/etkinlikteki pencereler etkilenmez", () => {
    const here = makeWindow({ id: "here" });
    const otherDesk = makeWindow({ id: "otherDesk", desktops: [{ id: "d2" }] });
    const otherAct = makeWindow({ id: "otherAct", activities: ["a2"] });
    const ws = makeWorkspace({
        windows: [here, otherDesk, otherAct],
        active: here
    });
    const state = Logic.newState();

    Logic.minimizeOthers(ws, state);
    assert.strictEqual(otherDesk.minimized, false);
    assert.strictEqual(otherAct.minimized, false);
});

test("Liste temizliği: kapanan pencere yığından düşer, çift ekleme yok", () => {
    const a = makeWindow({ id: "a" });
    const ws = makeWorkspace({ windows: [a], active: a });
    const state = Logic.newState();

    Logic.minimizeActive(ws, state);
    Logic.minimizeActive(ws, state);
    assert.deepStrictEqual(state.minimizedStack, ["a"]);

    Logic.onClosed(state, "a");
    assert.deepStrictEqual(state.minimizedStack, []);
});

test("Küçültülmüş pencere dışarıdan geri gelince yığından düşer", () => {
    const a = makeWindow({ id: "a" });
    const state = Logic.newState();
    state.minimizedStack = ["a"];

    a.minimized = false;
    Logic.onMinimizedChanged(state, a);
    assert.deepStrictEqual(state.minimizedStack, []);
});

test("panelLayout: alt panel", () => {
    const ws = makeWorkspace({
        clientArea(option) {
            if (option === 4) {
                return { x: 0, y: 0, width: 1920, height: 1080 };
            }
            return { x: 0, y: 0, width: 1920, height: 1036 };
        }
    });
    const layout = Logic.panelLayout(ws, 44);
    assert.strictEqual(layout.length, 1);
    assert.strictEqual(layout[0].edge, "bottom");
    assert.strictEqual(layout[0].thickness, 44);
    assert.strictEqual(layout[0].output, "DP-1");
});

test("panelLayout: üst, sol ve sağ panel", () => {
    const cases = [
        { max: { x: 0, y: 32, width: 1920, height: 1048 }, edge: "top", thickness: 32 },
        { max: { x: 48, y: 0, width: 1872, height: 1080 }, edge: "left", thickness: 48 },
        { max: { x: 0, y: 0, width: 1860, height: 1080 }, edge: "right", thickness: 60 }
    ];
    for (const c of cases) {
        const ws = makeWorkspace({
            clientArea(option) {
                if (option === 4) {
                    return { x: 0, y: 0, width: 1920, height: 1080 };
                }
                return c.max;
            }
        });
        const layout = Logic.panelLayout(ws, 44);
        assert.strictEqual(layout[0].edge, c.edge);
        assert.strictEqual(layout[0].thickness, c.thickness);
    }
});

test("panelLayout: panel yoksa yedek yükseklik", () => {
    const ws = makeWorkspace({
        clientArea() {
            return { x: 0, y: 0, width: 1920, height: 1080 };
        }
    });
    const layout = Logic.panelLayout(ws, 50);
    assert.strictEqual(layout[0].edge, "none");
    assert.strictEqual(layout[0].thickness, 50);
});

test("panelLayout: çoklu ekran ve ölçek", () => {
    const ws = makeWorkspace({
        screens: [
            {
                name: "DP-1",
                geometry: { x: 0, y: 0, width: 1920, height: 1080 },
                devicePixelRatio: 1
            },
            {
                name: "HDMI-A-1",
                geometry: { x: 1920, y: 0, width: 2560, height: 1440 },
                devicePixelRatio: 1.5
            }
        ],
        clientArea(option, screen) {
            if (option === 4) {
                return screen.geometry;
            }
            const g = screen.geometry;
            return { x: g.x, y: g.y, width: g.width, height: g.height - 40 };
        }
    });
    const layout = Logic.panelLayout(ws, 44);
    assert.strictEqual(layout.length, 2);
    assert.strictEqual(layout[1].output, "HDMI-A-1");
    assert.strictEqual(layout[1].x, 1920);
    assert.strictEqual(layout[1].scale, 1.5);
    assert.strictEqual(layout[1].edge, "bottom");
    assert.strictEqual(layout[1].thickness, 40);
});

test("Peek: opaklık düşer, orijinaller saklanır, peekEnd birebir geri yükler", () => {
    const a = makeWindow({ id: "a", opacity: 1 });
    const b = makeWindow({ id: "b", opacity: 0.9 });
    const ws = makeWorkspace({ windows: [a, b] });
    const state = Logic.newState();

    assert.strictEqual(Logic.peekBegin(ws, state, 0.08), 2);
    assert.strictEqual(a.opacity, 0.08);
    assert.strictEqual(b.opacity, 0.08);
    assert.strictEqual(state.peekSaved.a, 1);
    assert.strictEqual(state.peekSaved.b, 0.9);

    /* İki kez peekBegin sonucu bozmaz. */
    Logic.peekBegin(ws, state, 0.05);
    assert.strictEqual(a.opacity, 0.08);

    assert.strictEqual(Logic.peekEnd(ws, state), 2);
    assert.strictEqual(a.opacity, 1);
    assert.strictEqual(b.opacity, 0.9);
    assert.strictEqual(state.peekActive, false);
    assert.deepStrictEqual(state.peekSaved, {});
});

test("Peek: kapanan pencere geri yüklemede atlanır", () => {
    const a = makeWindow({ id: "a", opacity: 1 });
    const ws = makeWorkspace({ windows: [a] });
    const state = Logic.newState();

    Logic.peekBegin(ws, state, 0.1);
    ws.stackingOrder = [];
    assert.strictEqual(Logic.peekEnd(ws, state), 0);
});

test("shakeDetector: 600 ms içinde 4 yön değişimi algılanır", () => {
    const d = Logic.shakeDetector({ windowMs: 600, minAmplitude: 30, changes: 4 });
    let fired = false;

    const trace = [
        [100, 0], [100, 10],
        [150, 50], [100, 100],
        [150, 150], [100, 200],
        [150, 250]
    ];
    for (const [x, t] of trace) {
        if (d.feed(x, t)) {
            fired = true;
        }
    }
    assert.strictEqual(fired, true);
});

test("shakeDetector: yavaş hareket tetiklemez", () => {
    const d = Logic.shakeDetector({ windowMs: 600, minAmplitude: 30, changes: 4 });
    let fired = false;

    const trace = [
        [100, 0],
        [150, 300],
        [100, 600],
        [150, 900],
        [100, 1200],
        [150, 1500]
    ];
    for (const [x, t] of trace) {
        if (d.feed(x, t)) {
            fired = true;
        }
    }
    assert.strictEqual(fired, false);
});

test("shakeDetector: 30 px altı titreşim tetiklemez", () => {
    const d = Logic.shakeDetector({ windowMs: 600, minAmplitude: 30, changes: 4 });
    let fired = false;

    for (let i = 0; i < 20; i++) {
        const x = (i % 2 === 0) ? 100 : 115;
        if (d.feed(x, i * 20)) {
            fired = true;
        }
    }
    assert.strictEqual(fired, false);
});

test("shakeDetector: reset sonrası yeniden kullanılabilir", () => {
    const d = Logic.shakeDetector({ windowMs: 600, minAmplitude: 30, changes: 4 });
    d.feed(100, 0);
    d.reset();
    let fired = false;

    const trace = [
        [100, 0], [150, 20], [100, 40], [150, 60], [100, 80]
    ];
    for (const [x, t] of trace) {
        if (d.feed(x, t)) {
            fired = true;
        }
    }
    assert.strictEqual(fired, true);
});
