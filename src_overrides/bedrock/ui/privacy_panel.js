// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// The privacy panel popup. It draws the rows produced by
// bedrock/ui/site_privacy_panel.h. It counts nothing, estimates nothing and
// decides nothing: a number in this popup is a number the engine measured
// (invariant 28).
//
// Wire protocol:
//   in   window.bedrockSetModel(json)
'use strict';

function el(tag, className, text) {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
}

window.bedrockSetModel = function(json) {
  const model = typeof json === 'string' ? JSON.parse(json) : json;
  document.getElementById('host').textContent = model.host;

  const list = document.getElementById('rows');
  list.textContent = '';
  model.rows.forEach((row) => {
    const item = el('li', row.kind === 'count' ? 'count' : row.kind);
    if (!row.measured) item.className += ' unmeasured';
    item.appendChild(el('span', 'label', row.label));
    // "Not measured" is the model's own wording; the page never substitutes a
    // zero for it.
    item.appendChild(el('span', 'value', row.measured ? row.value : 'Not measured'));
    list.appendChild(item);
  });

  const parties = document.getElementById('parties');
  const partyList = document.getElementById('partyList');
  partyList.textContent = '';
  parties.hidden = model.blockedParties.length === 0;
  document.getElementById('partiesSummary').textContent =
      'Blocked third parties (' + model.blockedParties.length + ')';
  model.blockedParties.forEach((party) => {
    partyList.appendChild(el('li', null, party));
  });
};
