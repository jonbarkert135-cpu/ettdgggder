// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// The Privacy Center. Every number here is a count the engine recorded on this
// device; this file adds nothing, extrapolates nothing and rounds nothing
// (invariant 28). Formatting comes from the model too, so "12,481" is written
// once, in C++.
//
// Wire protocol:
//   in   window.bedrockSetModel(json)
//   out  chrome.send('bedrockPrivacyCenter', [action])
'use strict';

function send(action) {
  if (window.chrome && chrome.send) {
    chrome.send('bedrockPrivacyCenter', [action]);
  }
}

function el(tag, className, text) {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
}

window.bedrockSetModel = function(json) {
  const model = typeof json === 'string' ? JSON.parse(json) : json;
  document.getElementById('level').textContent = model.level;

  const grid = document.getElementById('grid');
  grid.textContent = '';
  model.rows.forEach((row) => {
    // A zero stays a zero and looks like one: a browser that blocked nothing
    // on this profile should not be dressed up as if it had.
    const card = el('div', row.value === 0 ? 'metric quiet' : 'metric');
    card.appendChild(el('div', 'n', row.formatted));
    card.appendChild(el('div', 'l', row.label));
    grid.appendChild(card);
  });

  document.getElementById('note').textContent = model.note;
};

document.addEventListener('DOMContentLoaded', () => {
  document.getElementById('export').addEventListener('click', () => send('export'));
  document.getElementById('reset').addEventListener('click', () => send('reset'));
  send('ready');
});
