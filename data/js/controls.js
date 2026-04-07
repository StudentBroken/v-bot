// ── CONTROLS ──
function setStep(v) {
    state.step = v;
    document.querySelectorAll('.step-opt').forEach(el => {
        el.classList.toggle('active', parseFloat(el.innerText) === v);
    });
}

function step() { return state.step; }

async function jog(dx, dy) {
    // Only allow jog if motors enabled? Or auto-enable?
    // Let's safe guard: if motors off, enable them.
    if (!state.motorsEnabled) {
        await api('/api/motors', 'POST', { enable: true });
    }
    let spd = parseFloat($('jogSpeed').value);
    await api('/api/jog', 'POST', { dx, dy, speed: spd });
    poll();
}

async function homeXY() {
    await api('/api/gcode', 'POST', "G28");
}

async function togglePen() {
    const down = !state.penDown; // Toggle local state prediction
    await api('/api/pen', 'POST', { down });
    poll();
}

async function forceEnableMotors() {
    await api('/api/motors', 'POST', { enable: true });
}

async function sendCmd() {
    const cmd = $('gcodeCmd').value;
    if (!cmd) return;
    // Raw string body for webserver.cpp handling
    await fetch('/api/gcode', { method: 'POST', body: cmd });
    $('gcodeCmd').value = '';
}

async function eStop() {
    await api('/api/stop', 'POST');
}

async function setHomeHere() {
    if (confirm("Set current position as (0,0)?")) {
        await fetch('/api/gcode', { method: 'POST', body: "G92 X0 Y0" });
        poll();
    }
}

async function calcPos() {
    const st = await api('/api/status');
    if (st) {
        await api('/api/calibrate', 'POST', {
            action: 'set_lengths',
            left: st.leftCable,
            right: st.rightCable
        });
        alert('Position recalculated from cable lengths.');
        poll();
    }
}

// ── REEL CONTROL ──
async function startReel(motor, dir) {
    // motor: 'left' or 'right'
    // dir: 1 (out/release) or -1 (in/retract)
    let spd = parseFloat($('jogSpeed').value);
    if (!spd) spd = 2000;

    // We use the calibrate jog endpoint which allows individual motor moves
    // We send a LARGE distance so it keeps moving until we send stop
    const DIST = 10000 * dir;

    let payload = { action: 'jog', speed: spd };
    if (motor === 'left') payload.left = DIST;
    if (motor === 'right') payload.right = DIST;

    await api('/api/calibrate', 'POST', payload);
}

async function stopReel() {
    // Stop any active jog
    await api('/api/calibrate', 'POST', { action: 'stop_jog' });
}

async function overrideLengthsQuick() {
    const l = parseFloat($('quickL').value);
    const r = parseFloat($('quickR').value);
    if (isNaN(l) || isNaN(r)) return alert("Please enter valid lengths");

    await api('/api/calibrate', 'POST', { action: 'set_lengths', left: l, right: r });
    alert('Lengths updated!');
    poll();
}

// ── SPEED OVERRIDE ──
function updateSpeedVal(val) {
    $('speedVal').textContent = val + '%';
}

async function commitSpeedOverride(val) {
    const scale = parseFloat(val) / 100.0;
    await api('/api/override', 'POST', { scale });
}

// ── CONTINUOUS JOG (DRIVE) ──
// ── CONTINUOUS JOG (DRIVE) ──
let _isDriving = false;
let _driveLoopActive = false;

function startDrive(e, vx, vy) {
    if (e && e.preventDefault) e.preventDefault();
    if (_isDriving) return;

    _isDriving = true;
    _driveLoopActive = true;

    // UI Feedback
    if (e && e.target) e.target.classList.add('active');

    // Global Release Listeners (to catch drag-off)
    const stopHandler = () => stopDrive();
    window.addEventListener('mouseup', stopHandler, { once: true });
    window.addEventListener('touchend', stopHandler, { once: true });

    // Store handler to remove it later if needed (though once:true helps)
    // actually we might want to remove the specific one if the other fires?
    // simplest is just let them fire stopDrive is idempotent.

    // Calculate speed vector
    let speed = parseFloat($('jogSpeed').value) || 2000;
    let targetVx = vx * speed / 60.0; // mm/s
    let targetVy = vy * speed / 60.0;

    // Recursive Loop
    const loop = async () => {
        if (!_driveLoopActive) return;

        try {
            await api('/api/drive', 'POST', { vx: targetVx, vy: targetVy });
        } catch (err) {
            console.error(err);
        }

        if (_driveLoopActive) {
            // Schedule next immediately after response
            // limiting max rate to e.g. 50ms (20Hz)
            setTimeout(loop, 50);
        }
    };

    loop();
    state.busy = true;
}

function stopDrive() {
    if (!_isDriving) return;

    _isDriving = false;
    _driveLoopActive = false;

    // Remove UI active class from all jog buttons
    document.querySelectorAll('.jog-btn').forEach(b => b.classList.remove('active'));

    // Send Stop Command
    api('/api/drive', 'POST', { vx: 0, vy: 0 });
}
