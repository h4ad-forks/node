'use strict';

const {
  Symbol,
  Date,
  DatePrototypeGetMilliseconds,
  DatePrototypeToUTCString,
} = primordials;

const { setUnrefTimeout } = require('internal/timers');
const trace_events = internalBinding('trace_events');

const {
  CHAR_LOWERCASE_B,
  CHAR_LOWERCASE_E,
} = require('internal/constants');

let utcCache;

function utcDate() {
  if (!utcCache) cache();
  return utcCache;
}

function cache() {
  const d = new Date();
  utcCache = DatePrototypeToUTCString(d);
  setUnrefTimeout(resetCache, 1000 - DatePrototypeGetMilliseconds(d));
}

function resetCache() {
  utcCache = undefined;
}

let traceEventId = 0;

function getNextTraceEventId() {
  return ++traceEventId;
}

function isTraceHTTPEnabled() {
  console.log(trace_events.isTraceCategoryEnabled('node.http'));
  console.log(trace_events.tracingCategories);

  return trace_events.tracingCategories[0] > 0;
}

const traceEventCategory = 'node,node.http';

function traceBegin(...args) {
  trace_events.trace(CHAR_LOWERCASE_B, traceEventCategory, ...args);
}

function traceEnd(...args) {
  trace_events.trace(CHAR_LOWERCASE_E, traceEventCategory, ...args);
}

module.exports = {
  kOutHeaders: Symbol('kOutHeaders'),
  kNeedDrain: Symbol('kNeedDrain'),
  utcDate,
  traceBegin,
  traceEnd,
  getNextTraceEventId,
  isTraceHTTPEnabled,
};
