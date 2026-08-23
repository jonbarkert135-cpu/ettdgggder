// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// The new tab page. It draws the model produced by bedrock/ui/new_tab.h and
// sends back what the user did. It holds no engine list, no shortcut rule and
// no privacy decision: whether history may be shown is decided in C++
// (invariant 28).
//
// Wire protocol, both directions through the WebUI host:
//   in   window.bedrockSetModel(json)
//   out  chrome.send('bedrockNewTab', [action, value])
//        action: 'search' | 'engine' | 'open'
'use strict';

let model = null;

function send(action, value) {
  if (window.chrome && chrome.send) {
    chrome.send('bedrockNewTab', [action, String(value)]);
  }
}

function el(tag, className, text) {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
}

function renderBookmarks() {
  const bar = document.getElementById('bookmarks');
  bar.textContent = '';
  bar.hidden = model.bookmarks.length === 0;
  model.bookmarks.forEach((mark) => {
    const link = el('a');
    link.href = mark.url;
    link.title = mark.url;
    link.appendChild(el('span', 'mark', mark.initial));
    link.appendChild(el('span', null, mark.title));
    link.addEventListener('click', (event) => {
      event.preventDefault();
      send('open', mark.url);
    });
    bar.appendChild(link);
  });
}

function renderTiles() {
  const tiles = document.getElementById('tiles');
  const note = document.getElementById('note');
  tiles.textContent = '';
  model.shortcuts.forEach((tile) => {
    const link = el('a', tile.pinned ? 'tile pin' : 'tile');
    link.href = tile.url;
    link.title = tile.url;
    link.appendChild(el('span', 'disc', tile.initial));
    link.appendChild(el('span', 'name', tile.title));
    link.addEventListener('click', (event) => {
      event.preventDefault();
      send('open', tile.url);
    });
    tiles.appendChild(link);
  });
  // Say why the row is empty rather than let the page look broken.
  note.hidden = !model.historyHidden;
  if (model.historyHidden) {
    note.textContent =
        'Shortcuts are built from browsing history, so this window does not ' +
        'show them. Bookmarks stay, because you put them there.';
  }
}

function closeMenu() {
  document.getElementById('menu').hidden = true;
  document.getElementById('engine').setAttribute('aria-expanded', 'false');
}

function renderEngine() {
  document.getElementById('engineLabel').textContent =
      model.engineLabel || 'Search';
  const menu = document.getElementById('menu');
  menu.textContent = '';
  model.engines.forEach((engine) => {
    const item = document.createElement('li');
    const button = el('button', null, engine.label);
    button.type = 'button';
    if (engine.selected) button.appendChild(el('span', 'tick', '\u2713'));
    button.addEventListener('click', () => {
      closeMenu();
      send('engine', engine.id);
    });
    item.appendChild(button);
    menu.appendChild(item);
  });
}

window.bedrockSetModel = function(json) {
  model = typeof json === 'string' ? JSON.parse(json) : json;
  renderBookmarks();
  renderEngine();
  renderTiles();
};

document.addEventListener('DOMContentLoaded', () => {
  document.getElementById('form').addEventListener('submit', (event) => {
    event.preventDefault();
    const input = document.getElementById('q');
    const text = input.value.trim();
    // Search or address is one field, and the host parses it: guessing here
    // would mean a second, worse copy of the omnibox rules.
    if (text) send('search', text);
  });
  const engine = document.getElementById('engine');
  engine.addEventListener('click', () => {
    const menu = document.getElementById('menu');
    const open = menu.hidden;
    menu.hidden = !open;
    engine.setAttribute('aria-expanded', open ? 'true' : 'false');
  });
  document.addEventListener('keydown', (event) => {
    if (event.key === 'Escape') closeMenu();
  });
  send('ready', '');
});
