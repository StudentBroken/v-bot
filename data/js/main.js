// ── POLLING ──
async function poll() {
    const d = await api('/api/status');
    if (!d) {
        $('statusBadge').textContent = 'Disconnected';
        $('statusBadge').className = 'badge disconnected';
        return;
    }

    // Update UI
    $('statusBadge').textContent = 'Connected';
    $('statusBadge').className = 'badge connected';

    $('posX').textContent = d.x.toFixed(1);
    $('posY').textContent = d.y.toFixed(1);
    $('cableL').textContent = d.leftCable.toFixed(1);
    $('cableR').textContent = d.rightCable.toFixed(1);

    // Motor Status Visuals
    const mb = $('motorBadge');
    if (d.motorsEnabled) {
        mb.textContent = "ON";
        mb.className = "badge connected";
    } else {
        mb.textContent = "OFF";
        mb.className = "badge disconnected";
    }

    // Pen Switch
    const pt = $('penToggle');
    if (d.penDown) pt.classList.add('on'); else pt.classList.remove('on');

    // Status Badge
    let status = 'Idle';
    if (d.gcodeState === 1) status = 'Running';

    // Update Debug
    if ($('debugBody') && $('debugBody').style.display !== 'none') {
        $('dbgStepsL').textContent = d.stepsL;
        $('dbgStepsR').textContent = d.stepsR;
        $('dbgStepsPerMm').textContent = d.confStepsPerMm;
        $('dbgWidth').textContent = d.confWidth;

        // Calculate theoretical length
        let spmm = d.confStepsPerMm;
        if (spmm > 0) {
            $('dbgCalcL').textContent = (d.stepsL / spmm).toFixed(2);
            $('dbgCalcR').textContent = (d.stepsR / spmm).toFixed(2);
        }
    }
    if (d.gcodeState === 2) status = 'Paused';
    if (d.busy && d.gcodeState === 0) status = 'Moving';

    const sb = $('stateBadge');
    sb.textContent = status;
    sb.className = 'badge ' + (status === 'Running' ? 'connected' : (status === 'Moving' ? 'warn' : ''));

    // Job Progress
    if (d.gcodeState === 1 || d.gcodeState === 2) {
        $('jobStatus').style.display = 'block';
        $('runFile').textContent = d.gcodeFile || 'Unknown';
        $('runPct').textContent = d.gcodeProgress + '%';
        $('runProgress').style.width = d.gcodeProgress + '%';

        // Update slider if not being dragged (simple check: valid value)
        if (d.speedScale) {
            const pct = Math.round(d.speedScale * 100);
            const slider = $('speedSlider');
            // Only update if difference is significant to avoid fighting user input?
            // Actually, we usually want to reflect state.
            // But if user is dragging, we might interrupt.
            // Let's simpler: update text, update slider val only if very diff
            if (Math.abs(slider.value - pct) > 5 && document.activeElement !== slider) {
                slider.value = pct;
                updateSpeedVal(pct);
            }
        }
    } else {
        const js = $('jobStatus');
        if (js) js.style.display = 'none';
    }

    // Initial setup of quick anchor width if empty
    if ($('quickAnchorWidth') && !$('quickAnchorWidth').value && d.calStep === 0) {
        loadQuickConfig();
    }

    // Live update of Geometry Inputs (if not focused)
    // Live update of Geometry Inputs
    // Only update if empty to prevent overwriting user input while tabbing
    if ($('geoWidth') && $('geoWidth').value === '') {
        $('geoWidth').value = d.confWidth;
    }
    if ($('geoL') && $('geoL').value === '') {
        $('geoL').value = d.leftCable.toFixed(1);
    }
    if ($('geoR') && $('geoR').value === '') {
        $('geoR').value = d.rightCable.toFixed(1);
    }

    state = { ...state, ...d };
}

// ── TABS ──
function switchTab(name) {
    document.querySelectorAll('.tab-content').forEach(el => el.style.display = 'none');
    document.querySelectorAll('.tab-btn').forEach(el => el.classList.remove('active'));

    const tab = document.getElementById('tab-' + name);
    if (tab) {
        if (name === 'dashboard') tab.style.display = 'grid';
        else if (name === 'terminal') tab.style.display = 'flex';
        else tab.style.display = 'block';
    }

    // Find button
    const btns = document.querySelectorAll('.tab-btn');
    if (name === 'dashboard') btns[0].classList.add('active');
    if (name === 'whiteboard') {
        btns[1].classList.add('active');
        // Hack: trigger resize if function exists (from whiteboard.js)
        if (typeof wbResizeCanvas === 'function') wbResizeCanvas();
    }
    if (name === 'terminal') btns[2].classList.add('active');
}

// Start polling
setInterval(poll, 1000);
poll(); // Initial
refreshFiles();
initTerminal(); // Start terminal polling
