
// -- Global State --
let wbState = {
    canvas: null,
    ctx: null,
    isDrawing: false,
    lastX: 0,
    lastY: 0,
    paths: [], // Array of {points: [{x,y}, ...], type: 'stroke'|'line' }
    currentPath: [],

    // Calibration (Machine Coordinates)
    tl: { x: null, y: null }, // Top-Left
    br: { x: null, y: null }, // Bottom-Right

    // Tools
    tool: 'brush', // brush, eraser, line

    // SVG Transform
    svgPath: null, // The active SVG path object (points)
    svgTransform: { x: 100, y: 100, scale: 1.0, rotate: 0 }
};

// -- Init --
document.addEventListener('DOMContentLoaded', () => {
    wbState.canvas = document.getElementById('wbCanvas');
    wbState.ctx = wbState.canvas.getContext('2d');

    // Events
    wbState.canvas.addEventListener('mousedown', wbStartDraw);
    wbState.canvas.addEventListener('mousemove', wbDraw);
    wbState.canvas.addEventListener('mouseup', wbStopDraw);
    wbState.canvas.addEventListener('mouseleave', wbStopDraw);

    // Touch support hooks could be added here

    wbRedraw();
});

// -- Tab Switching --
// -- Tab Switching logic moved to main.js --
// function switchTab(tabId) { ... }

function wbResizeCanvas() {
    // Fit canvas to container width (optional, for now fixed 800x600 is fine or responsive)
}

// -- Calibration --
// -- Calibration --
async function wbSetBound(corner) {
    // Get current pos from Status API
    try {
        const res = await fetch('/api/status');
        const data = await res.json();

        const x = data.x;
        const y = data.y;
        console.log(`Bound ${corner}: ${x}, ${y}`);

        if (corner === 'TL') {
            wbState.tl = { x, y };
            document.getElementById('wbCoordTL').innerText = `(${x.toFixed(0)}, ${y.toFixed(0)})`;
            document.getElementById('wbCoordTL').classList.add('success');
        } else {
            wbState.br = { x, y };
            document.getElementById('wbCoordBR').innerText = `(${x.toFixed(0)}, ${y.toFixed(0)})`;
            document.getElementById('wbCoordBR').classList.add('success');
        }

        // Update Canvas Aspect Ratio if both are set
        if (wbState.tl.x !== null && wbState.br.x !== null) {
            const physW = Math.abs(wbState.br.x - wbState.tl.x);
            const physH = Math.abs(wbState.br.y - wbState.tl.y);
            console.log(`Area: ${physW} x ${physH}`);

            if (physW > 1 && physH > 1) {
                // Show Container
                document.querySelector('.canvas-container').style.display = 'block';

                const aspect = physH / physW;
                const newH = Math.round(wbState.canvas.width * aspect);
                wbState.canvas.height = newH;

                // Set Grid Size (1cm = 10mm)
                // ppm = Pixels Per MM = CanvasWidth / PhysWidth
                const ppm = wbState.canvas.width / physW;
                const ppcm = ppm * 10; // Pixels per CM
                wbState.canvas.style.backgroundSize = `${ppcm}px ${ppcm}px`;

                wbRedraw();
            }
        }

    } catch (e) {
        console.error(e);
        alert('Failed to get position');
    }
}
// -- Drawing --
function wbGetPos(e) {
    const rect = wbState.canvas.getBoundingClientRect();
    const scaleX = wbState.canvas.width / rect.width;
    const scaleY = wbState.canvas.height / rect.height;
    return {
        x: (e.clientX - rect.left) * scaleX,
        y: (e.clientY - rect.top) * scaleY
    };
}

function wbStartDraw(e) {
    if (wbState.svgPath) return;
    wbState.isDrawing = true;
    const pos = wbGetPos(e);
    wbState.lastX = pos.x;
    wbState.lastY = pos.y;
    wbState.currentPath = [{ x: pos.x, y: pos.y }];

    const ctx = wbState.ctx;
    ctx.beginPath();
    ctx.moveTo(pos.x, pos.y);
}

function wbDraw(e) {
    if (!wbState.isDrawing) return;
    if (wbState.svgPath) return;

    const pos = wbGetPos(e);
    const ctx = wbState.ctx;

    ctx.lineWidth = (wbState.tool === 'brush') ? 2 : (wbState.tool === 'line' ? 2 : 10);
    ctx.lineCap = 'round';
    ctx.strokeStyle = (wbState.tool === 'eraser') ? '#ffffff' : '#000000';

    if (wbState.tool === 'line') {
        // Preview line: clear and redraw everything + current line
        wbRedraw();
        ctx.beginPath();
        ctx.moveTo(wbState.lastX, wbState.lastY);
        ctx.lineTo(pos.x, pos.y);
        ctx.stroke();
    } else {
        ctx.lineTo(pos.x, pos.y);
        ctx.stroke();
        wbState.currentPath.push({ x: pos.x, y: pos.y });
    }
}

function wbStopDraw(e) {
    if (!wbState.isDrawing) return;
    wbState.isDrawing = false;

    if (wbState.tool === 'line') {
        // Commit line
        const pos = wbGetPos(e);
        wbState.paths.push({
            type: 'line',
            points: [{ x: wbState.lastX, y: wbState.lastY }, { x: pos.x, y: pos.y }]
        });
        wbRedraw();
    } else if (wbState.currentPath.length > 0) {
        wbState.paths.push({
            type: wbState.tool,
            points: wbState.currentPath
        });
    }
    wbState.currentPath = [];
}

function wbRedraw() {
    if (!wbState.ctx) return;
    const ctx = wbState.ctx;
    const cw = wbState.canvas.width;
    const ch = wbState.canvas.height;

    ctx.clearRect(0, 0, cw, ch);

    // Replay paths
    wbState.paths.forEach(path => {
        const isEraser = (path.type === 'eraser');
        ctx.lineWidth = isEraser ? 10 : 2;
        ctx.lineCap = 'round';
        ctx.strokeStyle = isEraser ? '#ffffff' : '#000000';

        ctx.beginPath();
        if (path.points.length > 0) {
            ctx.moveTo(path.points[0].x, path.points[0].y);
            for (let i = 1; i < path.points.length; i++) {
                ctx.lineTo(path.points[i].x, path.points[i].y);
            }
        }
        ctx.stroke();
    });
}

function wbClear() {
    wbState.paths = [];
    wbState.svgPath = null;
    wbRedraw();
}

function wbDrawCalibrationPattern() {
    // Draw a box and X
    const cw = wbState.canvas.width;
    const ch = wbState.canvas.height;
    wbState.paths.push({
        type: 'line',
        points: [{ x: 0, y: 0 }, { x: cw, y: 0 }, { x: cw, y: ch }, { x: 0, y: ch }, { x: 0, y: 0 }]
    });
    wbState.paths.push({
        type: 'line',
        points: [{ x: 0, y: 0 }, { x: cw, y: ch }]
    });
    wbState.paths.push({
        type: 'line',
        points: [{ x: cw, y: 0 }, { x: 0, y: ch }]
    });
    wbRedraw();
}

// -- Tools --
function wbSetTool(t) {
    wbState.tool = t;
    document.querySelectorAll('.tool-row .btn').forEach(b => b.classList.remove('active'));
    const btnId = 'tool' + t.charAt(0).toUpperCase() + t.slice(1);
    const btn = document.getElementById(btnId);
    if (btn) btn.classList.add('active');
}

function wbGoToBound(corner) {
    const t = (corner === 'TL') ? wbState.tl : wbState.br;
    if (t.x !== null && t.y !== null) {
        // Send absolute move GCode
        const cmd = `G0 X${t.x} Y${t.y}`;
        api('/api/gcode', 'POST', cmd); // Raw string body
    } else {
        alert("Point not set");
    }
}

// -- SVG Stubs (Simplified) --
function wbUploadSvg(el) {
    alert("SVG Upload not fully implemented yet");
}
function wbSvgUpdate(val, type) { }
function wbSvgStamp() { }
function wbSvgCancel() { }

// -- Generation --
function wbGenerateGcode() {
    if (wbState.tl.x === null || wbState.br.x === null) {
        alert("Please set Top-Left and Bottom-Right corners first!");
        return null;
    }

    const physW = wbState.br.x - wbState.tl.x;
    const physH = wbState.br.y - wbState.tl.y;

    if (physW <= 0 || physH <= 0) {
        alert("Invalid calibration area. BR must be > TL (Coordinates: " + wbState.tl.x + "," + wbState.tl.y + " to " + wbState.br.x + "," + wbState.br.y + ")");
        return null;
    }

    // Canvas dimensions
    const cw = wbState.canvas.width;
    const ch = wbState.canvas.height;

    let gcode = [];
    gcode.push("; V-Bot Whiteboard Job");
    gcode.push(`; Area: ${physW}x${physH} @ ${wbState.tl.x},${wbState.tl.y}`);
    gcode.push("G90"); // Absolute
    gcode.push("G21"); // MM
    gcode.push("M5"); // Pen Up

    const scaleX = physW / cw;
    const scaleY = physH / ch;

    wbState.paths.forEach(path => {
        if (path.points.length < 2) return;

        // Move to start
        const p0 = path.points[0];
        const gx0 = wbState.tl.x + p0.x * scaleX;
        const gy0 = wbState.tl.y + p0.y * scaleY;

        gcode.push(`G0 X${gx0.toFixed(2)} Y${gy0.toFixed(2)}`);
        gcode.push("M3"); // Pen DOWN

        for (let i = 1; i < path.points.length; i++) {
            const p = path.points[i];
            const gx = wbState.tl.x + p.x * scaleX;
            const gy = wbState.tl.y + p.y * scaleY;
            gcode.push(`G1 X${gx.toFixed(2)} Y${gy.toFixed(2)}`); // F? Feedrate from UI override
        }

        gcode.push("M5"); // Pen Up
    });

    // Safe End: Lift Pen and Stop (Don't go to 0,0 - it's a singularity!)
    gcode.push("M5"); // Pen Up
    gcode.push("; End");

    return gcode.join('\n');
}

async function wbGenerateAndRun() {
    const gcode = wbGenerateGcode();
    if (!gcode) return; // Alert already shown in Generate

    console.log("Generated G-code:", gcode);

    // Upload
    const blob = new Blob([gcode], { type: 'text/plain' });
    const filename = "wb.gcode";
    const file = new File([blob], filename);

    const formData = new FormData();
    formData.add("file", file, filename);

    try {
        document.getElementById('jobStatus').style.display = 'block';
        document.getElementById('runFile').innerText = 'Uploading...';

        // Use raw fetch for upload to ensure correct parsing
        const res = await fetch('/api/upload', {
            method: 'POST',
            body: formData
        });

        if (res.ok) {
            console.log("Upload success");
            // 2. Run
            document.getElementById('runFile').innerText = 'Starting...';

            // Call run API
            const runRes = await api('/api/run', 'POST', { file: filename });
            console.log("Run response:", runRes);

            // Switch to Dashboard to see progress
            switchTab('dashboard');
        } else {
            console.error("Upload failed", res.status, res.statusText);
            alert('Upload failed: ' + res.statusText);
        }

    } catch (e) {
        console.error("Error in wbGenerateAndRun:", e);
        alert('Error: ' + e.message);
    }
}
