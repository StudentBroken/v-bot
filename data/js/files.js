// ── FILES ──
async function refreshFiles() {
    const d = await api('/api/files');
    const list = $('fileList');
    if (!list) return;
    list.innerHTML = '';
    if (d && d.files) {
        d.files.forEach(f => {
            const el = document.createElement('div');
            el.className = 'file-item';
            el.innerHTML = `<span>${f.name}</span> <span style="font-size:0.7em;color:var(--muted)">${(f.size / 1024).toFixed(1)}KB</span>`;
            el.onclick = () => runFile(f.name);
            list.appendChild(el);
        });
    }
    if (!d || !d.files || d.files.length === 0) {
        list.innerHTML = '<div style="padding:20px;text-align:center;color:var(--muted)">No files found</div>';
    }
}

async function runFile(name) {
    if (confirm(`Run ${name}?`)) {
        await api('/api/run', 'POST', { file: name });
    }
}

async function uploadFile(input) {
    if (input.files.length === 0) return;
    const file = input.files[0];
    const fd = new FormData();
    fd.append('file', file);
    await fetch('/api/upload', { method: 'POST', body: fd });
    refreshFiles();
}
