// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// The first-run page. It renders the model produced by
// bedrock/onboarding/first_run_page.h and sends back the id of the option the
// user pressed. It holds no option list, no default and no privacy rule: if
// this file has to decide something, the decision is in the wrong language
// (invariant 28).
//
// Wire protocol, both directions through the WebUI host:
//   in   window.bedrockSetModel(json)   the whole state, after every change
//   out  chrome.send('bedrockFirstRun', [field, value])
'use strict';

const STEPS = ['welcome', 'privacy', 'search', 'theme', 'import', 'finish'];

let model = null;

function send(field, value) {
  // The host is the only source of truth: nothing is drawn until it answers.
  if (window.chrome && chrome.send) {
    chrome.send('bedrockFirstRun', [field, String(value)]);
  }
}

function el(tag, className, text) {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
}

function optionList(options, field, groupLabel) {
  const group = el('div', 'card');
  group.setAttribute('role', 'radiogroup');
  group.setAttribute('aria-label', groupLabel);
  for (const option of options) {
    const button = el('button', 'opt');
    button.type = 'button';
    button.setAttribute('role', 'radio');
    button.setAttribute('aria-checked', option.selected ? 'true' : 'false');
    button.appendChild(el('span', 'name', option.label));
    if (option.detail) button.appendChild(el('span', 'detail', option.detail));
    button.addEventListener('click', () => send(field, option.id));
    group.appendChild(button);
  }
  return group;
}

function disclosure(facts) {
  const list = el('dl', 'disclose');
  const rows = [
    ['Search provider', facts.provider],
    ['Suggestions', facts.suggestions],
    ['Safe browsing', facts.safeBrowsing],
    ['Privacy', facts.privacy],
  ];
  for (const [term, value] of rows) {
    list.appendChild(el('dt', null, term));
    list.appendChild(el('dd', null, value));
  }
  return list;
}

function suggestionsToggle(on) {
  const label = el('label', 'toggle');
  const box = el('input');
  box.type = 'checkbox';
  box.checked = on;
  box.addEventListener('change', () => send('suggestions', box.checked));
  label.appendChild(box);
  label.appendChild(
      el('span', null, 'Send what I type to the provider as I type it'));
  return label;
}

function privacyNotes() {
  const wrap = document.createDocumentFragment();
  wrap.appendChild(el('p', null, model.privacyHeadline));
  const list = el('ul', 'notes');
  for (const note of model.privacyNotes) list.appendChild(el('li', null, note));
  wrap.appendChild(list);
  return wrap;
}

function bodyFor(step) {
  const body = el('div');
  if (step === 'welcome' || step === 'finish') {
    body.appendChild(privacyNotes());
    return body;
  }
  if (step === 'privacy') {
    body.appendChild(optionList(model.privacyOptions, 'privacy', 'Privacy level'));
    body.appendChild(privacyNotes());
    return body;
  }
  if (step === 'search') {
    body.appendChild(optionList(model.engineOptions, 'engine', 'Search engine'));
    body.appendChild(suggestionsToggle(model.suggestions));
    body.appendChild(disclosure(model.disclosure));
    return body;
  }
  if (step === 'theme') {
    body.appendChild(optionList(model.themeOptions, 'theme', 'Theme'));
    return body;
  }
  if (step === 'import') {
    body.appendChild(optionList(model.importOptions, 'import', 'Import from'));
    return body;
  }
  return body;
}

function render() {
  if (!model) return;
  const rail = document.getElementById('rail');
  rail.textContent = '';
  const at = STEPS.indexOf(model.step);
  STEPS.forEach((_, index) => {
    rail.appendChild(el('span', index <= at ? 'at' : null, ''));
  });

  document.getElementById('title').textContent = model.title;
  document.getElementById('lead').textContent = model.lead;

  const body = document.getElementById('body');
  body.textContent = '';
  body.appendChild(bodyFor(model.step));

  const back = document.getElementById('back');
  back.disabled = at === 0 || model.done;
  const next = document.getElementById('next');
  next.textContent = model.step === 'finish' ? 'Start browsing' : 'Continue';
  next.disabled = model.done;
}

window.bedrockSetModel = function(json) {
  model = typeof json === 'string' ? JSON.parse(json) : json;
  render();
};

document.addEventListener('DOMContentLoaded', () => {
  document.getElementById('back').addEventListener('click', () => send('step', 'back'));
  document.getElementById('next').addEventListener('click', () => send('step', 'next'));
  // The page starts empty and asks for the state; it never invents one.
  send('ready', 'true');
  if (window.bedrockInitialModel) window.bedrockSetModel(window.bedrockInitialModel);
});
