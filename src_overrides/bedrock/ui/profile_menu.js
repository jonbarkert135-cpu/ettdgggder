// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// The profile menu. It draws the model from bedrock/ui/profile_menu.h and
// sends back what was pressed. The sync line and the window-mode sentence come
// from the model, because both are claims about what the browser does and
// neither belongs in a page (invariant 28).
//
// Wire protocol:
//   in   window.bedrockSetModel(json)
//   out  chrome.send('bedrockProfile', [action, id])
'use strict';

function send(action, id) {
  if (window.chrome && chrome.send) {
    chrome.send('bedrockProfile', [action, String(id === undefined ? '' : id)]);
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

  document.getElementById('initial').textContent = model.active.initial;
  document.getElementById('name').textContent = model.active.name;
  document.getElementById('sync').textContent =
      model.active.ephemeral
          ? 'Temporary profile — everything here is discarded on close'
          : model.sync;

  const mode = document.getElementById('mode');
  mode.hidden = !model.modeLabel;
  document.getElementById('modeLabel').textContent = model.modeLabel;
  document.getElementById('modeSay').textContent = model.modeSentence;

  const list = document.getElementById('others');
  list.textContent = '';
  model.others.forEach((profile) => {
    const item = document.createElement('li');
    const button = document.createElement('button');
    button.type = 'button';
    button.appendChild(el('span', 'mini', profile.initial));
    button.appendChild(el('span', null, profile.name));
    // The kind is only worth showing when it adds something the name does not.
    if (profile.kind.toLowerCase() !== profile.name.toLowerCase())
      button.appendChild(el('span', 'kind', profile.kind));
    button.addEventListener('click', () => send('switch', profile.id));
    item.appendChild(button);
    list.appendChild(item);
  });
};

document.addEventListener('DOMContentLoaded', () => {
  document.getElementById('create').addEventListener('click', () => send('create'));
  document.getElementById('manage').addEventListener('click', () => send('manage'));
  send('ready');
});
