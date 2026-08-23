// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// The settings page. It draws the model from bedrock/settings/settings_page.h
// and sends back which key the user set to what. It holds no setting list, no
// default and no idea what any key means (invariant 28) — that table lives in
// ConfigSurface, where the config file, the policy and the command line read
// it too.
//
// Wire protocol:
//   in   window.bedrockSetModel(json)
//   out  chrome.send('bedrockSettings', [action, a, b])
//        action: 'section' | 'set' | 'extension'
'use strict';

let model = null;

function send(action, a, b) {
  if (window.chrome && chrome.send) {
    chrome.send('bedrockSettings', [action, String(a), String(b === undefined ? '' : b)]);
  }
}

function el(tag, className, text) {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
}

function originLine(row) {
  if (row.locked) return 'Managed by your organisation — set by policy';
  switch (row.origin) {
    case 'command-line': return 'Set on the command line for this session';
    case 'config-file': return 'Set in the configuration file';
    case 'default': return 'Default: ' + row.default;
    default: return '';
  }
}

function control(row) {
  // Named options get a segmented control; anything else is a text field. The
  // page never invents an option that the model did not offer.
  if (row.values.length > 0) {
    const seg = el('div', 'seg');
    seg.setAttribute('role', 'group');
    row.values.forEach((value) => {
      const button = el('button', null, value);
      button.type = 'button';
      button.setAttribute('aria-pressed', value === row.value ? 'true' : 'false');
      button.disabled = row.locked;
      button.addEventListener('click', () => send('set', row.key, value));
      seg.appendChild(button);
    });
    return seg;
  }
  const input = document.createElement('input');
  input.type = 'text';
  input.value = row.value;
  input.disabled = row.locked;
  input.setAttribute('aria-label', row.label);
  input.addEventListener('change', () => send('set', row.key, input.value));
  return input;
}

function renderNav() {
  const nav = document.getElementById('nav');
  nav.textContent = '';
  model.nav.forEach((section) => {
    const button = el('button', null, section.title);
    button.type = 'button';
    if (section.active) button.setAttribute('aria-current', 'page');
    button.addEventListener('click', () => send('section', section.id));
    nav.appendChild(button);
  });
}

function renderRows() {
  const rows = document.getElementById('rows');
  rows.textContent = '';
  model.rows.forEach((row) => {
    const line = el('div', 'row');
    const left = el('div');
    left.appendChild(el('div', 'k', row.label));
    const meta = originLine(row);
    if (meta) left.appendChild(el('div', row.locked ? 'meta managed' : 'meta', meta));
    line.appendChild(left);
    line.appendChild(control(row));
    rows.appendChild(line);
  });
}

function renderExtensions() {
  const cards = document.getElementById('cards');
  cards.textContent = '';
  model.extensions.forEach((ext) => {
    const card = el('div', 'card');
    card.appendChild(el('div', 'glyph', (ext.name || '?').charAt(0).toUpperCase()));
    const body = el('div', 'body');
    body.appendChild(el('div', 'name', ext.name + '  ' + ext.version));
    // What it can read is the fact that matters about an extension.
    body.appendChild(el('div', 'facts',
        'Reads: ' + ext.hostAccess +
        (ext.privateWindows ? ' · allowed in private windows' : '')));
    card.appendChild(body);
    card.appendChild(el('span', 'risk ' + ext.risk.toLowerCase(), ext.risk));
    const toggle = el('button', 'toggle', ext.enabled ? 'On' : 'Off');
    toggle.type = 'button';
    toggle.setAttribute('aria-pressed', ext.enabled ? 'true' : 'false');
    toggle.addEventListener('click', () => send('extension', ext.id, !ext.enabled));
    card.appendChild(toggle);
    cards.appendChild(card);
  });
}

window.bedrockSetModel = function(json) {
  model = typeof json === 'string' ? JSON.parse(json) : json;
  document.getElementById('title').textContent = model.title;
  document.getElementById('summary').textContent = model.summary;
  renderNav();
  renderRows();
  renderExtensions();
  document.getElementById('note').textContent =
      model.section === 'extensions' && model.extensions.length === 0
          ? 'No extensions installed. Each one you add can read the pages you open.'
          : '';
};

document.addEventListener('DOMContentLoaded', () => send('ready', ''));
