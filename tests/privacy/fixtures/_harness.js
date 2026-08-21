// Shared fixture harness. Each fixture calls report(object).
//
// The result is delivered two ways: rendered into <pre id="result"> so a human
// can open the fixture in a normal window and read it, and POSTed to /report on
// the page's own origin, which is how the runner collects it. The POST is the
// only network call any fixture makes and it goes to the local test server on
// 127.0.0.1 that served the page -- nothing leaves the machine, because a
// privacy suite that phones home to measure privacy has already failed
// (roadmap item 75).
//
// Why not scrape the DOM instead: `--dump-dom` never returns in current
// Chrome-for-Testing new-headless, so a page that reports itself is the only
// transport that works without pulling in a CDP client library.
function report(values) {
  const pre = document.getElementById('result');
  pre.textContent = JSON.stringify(values, null, 1);
  pre.dataset.done = '1';
  document.title = 'done';
  try {
    fetch('/report', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(values),
    });
  } catch (e) { /* the DOM copy is still readable by hand */ }
}
function safe(fn, fallback) {
  try { const v = fn(); return v === undefined ? (fallback ?? null) : v; }
  catch (e) { return 'error:' + e.name; }
}
