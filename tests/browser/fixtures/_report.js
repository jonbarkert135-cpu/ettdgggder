// Fixtures POST their result to /report on their own origin (the local test
// server that served the page) and also render it, so the same file is useful
// when opened by hand. Nothing here contacts anything but 127.0.0.1.
function report(text) {
  document.getElementById('result').textContent = text;
  fetch('/report', {method: 'POST', body: text});
}
