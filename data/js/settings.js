// ── SETTINGS ──
async function openSettings() {
    console.log("Opening settings...");
    const modal = $('settingsModal');
    if (modal) modal.classList.add('open');

    const s = await api('/api/settings');
    if (s) {
        if ($('s_anchorWidth')) $('s_anchorWidth').value = s.anchor_width_mm;
        if ($('s_gondolaWidth')) $('s_gondolaWidth').value = s.gondola_width_mm;
        if ($('s_maxSpeed')) $('s_maxSpeed').value = s.max_speed_mm_min;
        if ($('s_accel')) $('s_accel').value = s.acceleration;
        if ($('s_stepsPerMm')) $('s_stepsPerMm').value = s.steps_per_mm;
        if ($('ssid')) $('ssid').value = s.wifi_ssid;

        // Also update main UI quick config
        if ($('quickAnchorWidth')) $('quickAnchorWidth').value = s.anchor_width_mm;
    }
}

function closeSettings() {
    $('settingsModal').classList.remove('open');
}

async function saveQuickWidth() {
    const width = parseFloat($('quickAnchorWidth').value);
    if (!width) return;

    const s = await api('/api/settings');
    s.anchor_width_mm = width;
    await api('/api/settings', 'POST', s);
    alert('Anchor width saved!');
}

async function saveSettings() {
    const s = await api('/api/settings');
    s.anchor_width_mm = parseFloat($('s_anchorWidth').value);
    s.gondola_width_mm = parseFloat($('s_gondolaWidth').value);
    s.max_speed_mm_min = parseFloat($('s_maxSpeed').value);
    s.acceleration = parseFloat($('s_accel').value);
    s.steps_per_mm = parseFloat($('s_stepsPerMm').value);

    s.wifi_ssid = $('ssid').value;
    s.wifi_password = $('pass').value;
    await api('/api/settings', 'POST', s);
    closeSettings();
    alert('Settings saved and applied!');
}

async function overrideLengths() {
    const l = parseFloat($('setL').value);
    const r = parseFloat($('setR').value);
    await api('/api/calibrate', 'POST', { action: 'set_lengths', left: l, right: r });
    alert('Lengths updated');
}

// ── UNIFIED GEOMETRY ──
async function saveGeometry() {
    const width = parseFloat($('geoWidth').value);
    const l = parseFloat($('geoL').value);
    const r = parseFloat($('geoR').value);

    if (!width || width < 100) return alert("Please enter a valid Anchor Width (e.g. 1300)");
    if (!l || !r) return alert("Please measure and enter both Cable Lengths.");

    if (!confirm(`Confirm Machine Geometry?\n\nWidth: ${width}mm\nLeft: ${l}mm\nRight: ${r}mm\n\nThis will save settings and calibrate position.`)) return;

    await api('/api/geometry', 'POST', { width, left: l, right: r });
    alert("Saved & Calibrated!");
    poll();
}

// Initial setup of quick anchor width if empty
async function loadQuickConfig() {
    const s = await api('/api/settings');
    if (s && $('quickAnchorWidth')) {
        $('quickAnchorWidth').value = s.anchor_width_mm;
    }
}
