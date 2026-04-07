window.$ = (id) => document.getElementById(id);

async function api(path, method = 'GET', body = null) {
  try {
    const opts = { method };
    if (body) {
      opts.headers = { 'Content-Type': 'application/json' };
      opts.body = JSON.stringify(body);
    }
    const res = await fetch(path, opts);
    return await res.json();
  } catch (e) {
    console.error(e);
    return null;
  }
}
