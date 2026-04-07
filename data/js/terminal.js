// ── G-CODE TERMINAL ──

let _termInterval = null;

// Allow manual init or auto init
window.addEventListener('load', initTerminal);

function initTerminal() {
    console.log("Initializing Terminal...");
    if (document.getElementById('termOutput')) {
        const out = document.getElementById('termOutput');
        const d = document.createElement('div');
        d.textContent = "> [UI] Terminal Connected.";
        d.style.color = '#888';
        out.appendChild(d);
        startTermPolling();
    } else {
        console.warn("Terminal DOM not found, retrying in 500ms...");
        setTimeout(initTerminal, 500);
    }
}

function startTermPolling() {
    if (_termInterval) clearInterval(_termInterval);
    _termInterval = setInterval(pollTerm, 1000); // 1s poll
    pollTerm();
}

async function pollTerm() {
    // Only poll if tab is active? Or always to catch history?
    // Let's poll always for now, or maybe only when tab visible to save bandwidth.
    // For now: Check if tab is active
    if (!document.getElementById('tab-terminal') || document.getElementById('tab-terminal').style.display === 'none') {
        return;
    }

    try {
        const res = await fetch('/api/console');
        if (!res.ok) return;
        const json = await res.json();

        if (json.lines && json.lines.length > 0) {
            const out = document.getElementById('termOutput');

            json.lines.forEach(line => {
                const div = document.createElement('div');
                div.textContent = line;
                // Add timestamp?
                // div.textContent = `[${new Date().toLocaleTimeString()}] ${line}`;
                out.appendChild(div);
            });

            // Limit scrollback to 500 lines
            while (out.children.length > 500) {
                out.removeChild(out.firstChild);
            }

            // Auto Scroll
            if (document.getElementById('termAutoScroll').checked) {
                out.scrollTop = out.scrollHeight;
            }
        }
    } catch (e) {
        console.warn("Terminal poll failed:", e);
    }
}

async function termSend() {
    const input = document.getElementById('termInput');
    const cmd = input.value.trim();
    if (!cmd) return;

    // Echo to generic 'Send' logic, but we also want to display it locally instantly
    // The backend log will likely echo it too if we print it?
    // Let's rely on backend echo to verify receipt if GCodeParser prints it.
    // GCodeParser prints: "[GCode] Executing queued: ..."

    // Add local echo anyway for responsiveness
    // const out = document.getElementById('termOutput');
    // const div = document.createElement('div');
    // div.textContent = `> ${cmd}`;
    // div.style.color = '#fff';
    // out.appendChild(div);
    // if (document.getElementById('termAutoScroll').checked) out.scrollTop = out.scrollHeight;

    input.value = '';

    // Use existing API from controls.js or raw fetch
    await fetch('/api/gcode', { method: 'POST', body: cmd });

    // Force poll soon
    setTimeout(pollTerm, 200);
}

function termClear() {
    const out = document.getElementById('termOutput');
    out.innerHTML = '<div>> Terminal Cleared.</div>';
}
