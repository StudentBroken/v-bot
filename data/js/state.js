let state = {
    x: 0, y: 0, l: 0, r: 0,
    motors: false, pen: false,
    status: 'Idle',
    busy: false,
    step: 10,
    motorsEnabled: false
};

function toggleDebug() {
    const el = $('debugBody');
    el.style.display = el.style.display === 'none' ? 'flex' : 'none';
}
