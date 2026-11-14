/*
 * Sahte KWin çalışma alanı — JS testleri için.
 */
"use strict";

function makeWindow(opts) {
    const o = opts || {};
    const win = {
        internalId: o.id || "id-" + Math.random().toString(36).slice(2, 8),
        caption: o.caption || "pencere",
        normalWindow: o.normalWindow !== undefined ? o.normalWindow : true,
        skipTaskbar: o.skipTaskbar === true,
        desktopWindow: o.desktopWindow === true,
        dock: o.dock === true,
        notification: o.notification === true,
        popupMenu: o.popupMenu === true,
        splash: o.splash === true,
        dropdownMenu: o.dropdownMenu === true,
        tooltip: o.tooltip === true,
        comboBox: o.comboBox === true,
        dndIcon: o.dndIcon === true,
        appletPopup: o.appletPopup === true,
        onScreenDisplay: o.onScreenDisplay === true,
        criticalNotification: o.criticalNotification === true,
        utility: o.utility === true,
        inputMethod: false,
        outline: false,
        minimized: o.minimized === true,
        minimizable: o.minimizable !== false,
        maximizable: o.maximizable !== false,
        maximizeMode: o.maximizeMode || 0,
        tile: o.tile || null,
        opacity: o.opacity === undefined ? 1 : o.opacity,
        desktops: o.desktops || [{ id: "d1" }],
        activities: o.activities || ["a1"],
        closed: false,
        closedCb: null,
        setMaximize(v, h) {
            this.maximizeMode = (v ? 2 : 0) | (h ? 1 : 0);
            if (v && h) {
                this.maximizeMode = 3;
            }
        },
        closeWindow() {
            this.closed = true;
            if (this.closedCb) {
                this.closedCb(this);
            }
        }
    };
    return win;
}

function makeWorkspace(opts) {
    const o = opts || {};
    const ws = {
        stackingOrder: o.windows || [],
        activeWindow: o.active || null,
        currentDesktop: o.desktop || { id: "d1" },
        currentActivity: o.activity || "a1",
        showingDesktop: o.showingDesktop === true,
        screens: o.screens || [
            {
                name: "DP-1",
                geometry: { x: 0, y: 0, width: 1920, height: 1080 },
                devicePixelRatio: 1
            }
        ],
        toggleCount: 0,
        slotToggleShowDesktop() {
            this.toggleCount++;
            this.showingDesktop = !this.showingDesktop;
        }
    };
    if (o.clientArea) {
        ws.clientArea = o.clientArea;
    }
    return ws;
}

module.exports = { makeWindow, makeWorkspace };
