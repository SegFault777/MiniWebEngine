/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine v0.9 JavaScript prelude: web platform APIs implemented in JavaScript.
 * tools/gen_prelude.py turns this file into src/mwe_js_prelude.h (a C string); it runs once before the page's
 * scripts. Limits are listed in README.md ("JavaScript"). Only features the engine supports are used here. */
(function (g) {
  'use strict';
  function def(o, n, f) { Object.defineProperty(o, n, { value: f, writable: true, configurable: true, enumerable: false }); }
  function missing(o, n, f) { if (typeof o[n] === 'undefined') def(o, n, f); }

  /* ---- Object / Array / String / Number additions ---- */
  missing(Object, 'fromEntries', function (it) { var o = {}; for (var e of it) o[e[0]] = e[1]; return o; });
  missing(Object, 'hasOwn', function (o, k) { return Object.prototype.hasOwnProperty.call(o, k); });
  missing(Object, 'groupBy', function (it, f) { var o = {}, i = 0; for (var x of it) { var k = f(x, i++); (o[k] = o[k] || []).push(x); } return o; });
  var AP = Array.prototype;
  missing(AP, 'flatMap', function (f, t) { var r = []; for (var i = 0; i < this.length; i++) { var v = f.call(t, this[i], i, this); if (Array.isArray(v)) { for (var j = 0; j < v.length; j++) r.push(v[j]); } else r.push(v); } return r; });
  missing(AP, 'findLast', function (f, t) { for (var i = this.length - 1; i >= 0; i--) if (f.call(t, this[i], i, this)) return this[i]; return undefined; });
  missing(AP, 'findLastIndex', function (f, t) { for (var i = this.length - 1; i >= 0; i--) if (f.call(t, this[i], i, this)) return i; return -1; });
  missing(AP, 'toSorted', function (f) { return this.slice().sort(f); });
  missing(AP, 'toReversed', function () { return this.slice().reverse(); });
  missing(AP, 'toSpliced', function () { var a = this.slice(); a.splice.apply(a, arguments); return a; });
  missing(AP, 'with', function (i, v) { var a = this.slice(); if (i < 0) i += a.length; if (i < 0 || i >= a.length) throw new RangeError('Invalid index'); a[i] = v; return a; });
  missing(String, 'fromCodePoint', function () {
    var s = '';
    for (var i = 0; i < arguments.length; i++) {
      var c = arguments[i];
      if (c > 0xFFFF) { c -= 0x10000; s += String.fromCharCode(0xD800 + (c >> 10), 0xDC00 + (c & 1023)); } else s += String.fromCharCode(c);
    }
    return s;
  });
  missing(String, 'raw', function (s) { var r = s.raw || s, o = ''; for (var i = 0; i < r.length; i++) { o += r[i]; if (i + 1 < r.length && i + 1 < arguments.length) o += arguments[i + 1]; } return o; });
  missing(String.prototype, 'normalize', function () { return String(this); });
  missing(String.prototype, 'isWellFormed', function () { return true; });
  function group(s, sep) { var neg = s[0] === '-'; if (neg) s = s.slice(1); var o = ''; while (s.length > 3) { o = sep + s.slice(-3) + o; s = s.slice(0, -3); } return (neg ? '-' : '') + s + o; }
  function fmtNum(n, opt) {
    opt = opt || {};
    if (n !== n) return 'NaN';
    var style = opt.style || 'decimal', minF = opt.minimumFractionDigits, maxF = opt.maximumFractionDigits;
    if (style === 'percent') n = n * 100;
    if (minF === undefined) minF = style === 'currency' ? 2 : 0;
    if (maxF === undefined) maxF = Math.max(minF, style === 'currency' ? 2 : style === 'percent' ? 0 : 3);
    var neg = n < 0; if (neg) n = -n;
    var f = n.toFixed(maxF), p = f.split('.'), ip = p[0], fp = p[1] || '';
    while (fp.length > minF && fp[fp.length - 1] === '0') fp = fp.slice(0, -1);
    var s = (opt.useGrouping === false ? ip : group(ip, ',')) + (fp ? '.' + fp : '');
    if (style === 'currency') s = ({ USD: '$', EUR: '\u20ac', GBP: '\u00a3', JPY: '\u00a5', KRW: '\u20a9' }[opt.currency] || (opt.currency + '\u00a0')) + s;
    if (style === 'percent') s += '%';
    return (neg ? '-' : '') + s;
  }
  def(Number.prototype, 'toLocaleString', function (loc, opt) { return fmtNum(Number(this), opt); });
  var pad2 = function (n) { return (n < 10 ? '0' : '') + n; };
  var MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
  var DAYS = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
  function fmtDate(d, opt, which) {
    opt = opt || {};
    var o = '', hasDate = opt.year || opt.month || opt.day || opt.weekday, hasTime = opt.hour || opt.minute || opt.second;
    if (opt.dateStyle || opt.timeStyle) { hasDate = !!opt.dateStyle; hasTime = !!opt.timeStyle; }
    if (!hasDate && !hasTime) { hasDate = which !== 'time'; hasTime = which !== 'date'; opt = {}; }
    if (hasDate) {
      if (opt.month === 'long' || opt.month === 'short' || opt.dateStyle === 'long' || opt.dateStyle === 'medium') {
        var m = MONTHS[d.getMonth()]; if (opt.month === 'short' || opt.dateStyle === 'medium') m = m.slice(0, 3);
        o = (opt.weekday ? DAYS[d.getDay()].slice(0, opt.weekday === 'long' ? 99 : 3) + ', ' : '') + m + ' ' + d.getDate() + ', ' + d.getFullYear();
      } else o = (d.getMonth() + 1) + '/' + d.getDate() + '/' + d.getFullYear();
    }
    if (hasTime) {
      var h = d.getHours(), ap = h >= 12 ? 'PM' : 'AM'; h = h % 12 || 12;
      o += (o ? ', ' : '') + h + ':' + pad2(d.getMinutes()) + (opt.second || opt.timeStyle === 'medium' ? ':' + pad2(d.getSeconds()) : '') + ' ' + ap;
    }
    return o;
  }
  def(Date.prototype, 'toLocaleDateString', function (l, o) { return fmtDate(this, o, 'date'); });
  def(Date.prototype, 'toLocaleTimeString', function (l, o) { return fmtDate(this, o, 'time'); });
  def(Date.prototype, 'toLocaleString', function (l, o) { return fmtDate(this, o); });
  if (typeof g.Intl === 'undefined') {
    g.Intl = {
      NumberFormat: function (l, o) { if (!(this instanceof g.Intl.NumberFormat)) return new g.Intl.NumberFormat(l, o); this.format = function (n) { return fmtNum(Number(n), o); }; this.resolvedOptions = function () { return { locale: 'en-US' }; }; },
      DateTimeFormat: function (l, o) { if (!(this instanceof g.Intl.DateTimeFormat)) return new g.Intl.DateTimeFormat(l, o); this.format = function (d) { return fmtDate(new Date(d), o); }; this.resolvedOptions = function () { return { locale: 'en-US', timeZone: 'UTC' }; }; },
      PluralRules: function () { this.select = function (n) { return n === 1 ? 'one' : 'other'; }; },
      Collator: function () { this.compare = function (a, b) { return a < b ? -1 : a > b ? 1 : 0; }; },
      ListFormat: function (l, o) { this.format = function (a) { a = Array.from(a); var w = o && o.type === 'disjunction' ? 'or' : 'and'; return a.length < 3 ? a.join(' ' + w + ' ') : a.slice(0, -1).join(', ') + ', ' + w + ' ' + a[a.length - 1]; }; },
      RelativeTimeFormat: function () { this.format = function (v, u) { var a = Math.abs(v); return v < 0 ? a + ' ' + u + (a === 1 ? '' : 's') + ' ago' : 'in ' + a + ' ' + u + (a === 1 ? '' : 's'); }; }
    };
  }



  missing(Object, 'getOwnPropertyNames', function (o) { return Object.keys(o); });
  /* <template>.content: a fragment holding clones of the template's children (the parser keeps them as children) */
  if (g.document && g.document.createElement) {
    try {
      var EP = Object.getPrototypeOf(g.document.createElement('div'));
      if (EP && typeof EP.content === 'undefined') Object.defineProperty(EP, 'content', { configurable: true, enumerable: false, get: function () {
        if (String(this.tagName).toUpperCase() !== 'TEMPLATE') return undefined;
        var f = g.document.createDocumentFragment(), n = this.childNodes;
        for (var i = 0; i < n.length; i++) f.appendChild(n[i].cloneNode(true));
        return f;
      } });
    } catch (e) {}
  }

  /* ---- Reflect, getOwnPropertyDescriptors ---- */
  missing(Object, 'getOwnPropertyDescriptors', function (o) { var r = {}; for (var k of Object.getOwnPropertyNames(o)) r[k] = Object.getOwnPropertyDescriptor(o, k); return r; });
  if (typeof g.Reflect === 'undefined') g.Reflect = {
    ownKeys: function (o) { return Object.getOwnPropertyNames(o); }, has: function (o, k) { return k in o; },
    get: function (o, k) { return o[k]; }, set: function (o, k, v) { o[k] = v; return true; },
    getPrototypeOf: function (o) { return Object.getPrototypeOf(o); }, setPrototypeOf: function (o, p) { Object.setPrototypeOf(o, p); return true; },
    defineProperty: function (o, k, d) { try { Object.defineProperty(o, k, d); return true; } catch (e) { return false; } },
    deleteProperty: function (o, k) { return delete o[k]; }, getOwnPropertyDescriptor: function (o, k) { return Object.getOwnPropertyDescriptor(o, k); },
    apply: function (f, t, a) { return f.apply(t, a); }, construct: function (C, a) { return new (Function.prototype.bind.apply(C, [null].concat(Array.from(a))))(); },
    isExtensible: function (o) { return Object.isExtensible(o); }, preventExtensions: function (o) { Object.preventExtensions(o); return true; }
  };

  /* ---- Errors ---- */
  var anyNative = Promise.any;
  Promise.any = function (it) { return anyNative.call(Promise, it).catch(function (e) { if (e && e.errors !== undefined && !(e instanceof g.AggregateError)) throw new g.AggregateError(e.errors, e.message); throw e; }); };
  g.AggregateError = class AggregateError extends Error { constructor(errors, msg) { super(msg); this.name = 'AggregateError'; this.errors = Array.from(errors || []); } };

  /* ---- base64, text encoding, typed arrays (array-backed: values are not wrapped to the element width) ---- */
  var B64 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
  g.btoa = function (s) {
    s = String(s); var o = '';
    for (var i = 0; i < s.length; i += 3) {
      var a = s.charCodeAt(i), b = s.charCodeAt(i + 1), c = s.charCodeAt(i + 2);
      if (a > 255 || b > 255 || c > 255) throw new Error('Invalid character');
      o += B64[a >> 2] + B64[((a & 3) << 4) | ((b || 0) >> 4)] + (i + 1 < s.length ? B64[((b & 15) << 2) | ((c || 0) >> 6)] : '=') + (i + 2 < s.length ? B64[c & 63] : '=');
    }
    return o;
  };
  g.atob = function (s) {
    s = String(s).replace(/[\s=]+/g, ''); var o = '', acc = 0, nb = 0;
    for (var i = 0; i < s.length; i++) {
      var v = B64.indexOf(s[i]); if (v < 0) throw new Error('The string to be decoded is not correctly encoded.');
      acc = (acc << 6) | v; nb += 6;
      if (nb >= 8) { nb -= 8; o += String.fromCharCode((acc >> nb) & 255); }
    }
    return o;
  };
  function typed(name, bytes) {
    var T = function (a, off, len) {
      var r;
      if (typeof a === 'number') { r = new Array(a); for (var i = 0; i < a; i++) r[i] = 0; }
      else if (a && typeof a.byteLength === 'number' && !Array.isArray(a)) { var n = Math.floor(a.byteLength / bytes); r = new Array(n); for (var j = 0; j < n; j++) r[j] = 0; }
      else r = Array.from(a || []);
      r.BYTES_PER_ELEMENT = bytes; r.byteLength = r.length * bytes; r.byteOffset = 0;
      def(r, 'subarray', function (s, e) { return T(this.slice(s, e)); });
      def(r, 'set', function (src, o) { o = o || 0; for (var k = 0; k < src.length; k++) this[o + k] = src[k]; });
      return r;
    };
    T.BYTES_PER_ELEMENT = bytes; T.from = function (a, f) { return T(Array.from(a, f)); }; T.of = function () { return T(Array.from(arguments)); };
    g[name] = T;
  }
  typed('Uint8Array', 1); typed('Int8Array', 1); typed('Uint8ClampedArray', 1); typed('Int16Array', 2); typed('Uint16Array', 2);
  typed('Int32Array', 4); typed('Uint32Array', 4); typed('Float32Array', 4); typed('Float64Array', 8); typed('BigInt64Array', 8);
  g.ArrayBuffer = function ArrayBuffer(n) { if (!(this instanceof ArrayBuffer)) throw new TypeError("Constructor ArrayBuffer requires 'new'"); this.byteLength = n | 0; };
  g.TextEncoder = class TextEncoder {
    get encoding() { return 'utf-8'; }
    encode(s) {
      s = String(s === undefined ? '' : s); var o = [];
      for (var i = 0; i < s.length; i++) {
        var c = s.charCodeAt(i);
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < s.length) { c = 0x10000 + ((c - 0xD800) << 10) + (s.charCodeAt(++i) - 0xDC00); }
        if (c < 0x80) o.push(c); else if (c < 0x800) o.push(0xC0 | (c >> 6), 0x80 | (c & 63));
        else if (c < 0x10000) o.push(0xE0 | (c >> 12), 0x80 | ((c >> 6) & 63), 0x80 | (c & 63));
        else o.push(0xF0 | (c >> 18), 0x80 | ((c >> 12) & 63), 0x80 | ((c >> 6) & 63), 0x80 | (c & 63));
      }
      return g.Uint8Array(o);
    }
  };
  g.TextDecoder = class TextDecoder {
    constructor(label) { this.encoding = label || 'utf-8'; }
    decode(b) {
      if (!b) return ''; if (b.buffer && Array.isArray(b.buffer)) b = b.buffer;
      var s = '', i = 0;
      while (i < b.length) {
        var c = b[i++], n = c < 0x80 ? 0 : c < 0xE0 ? 1 : c < 0xF0 ? 2 : 3;
        if (n) c = c & (0x3F >> n);
        while (n-- > 0 && i < b.length) c = (c << 6) | (b[i++] & 63);
        s += c > 0xFFFF ? String.fromCodePoint(c) : String.fromCharCode(c);
      }
      return s;
    }
  };

  /* ---- structuredClone ---- */
  g.structuredClone = function (v) {
    var seen = new Map();
    function cl(x) {
      if (x === null || typeof x !== 'object') { if (typeof x === 'function') throw new Error('DataCloneError: function could not be cloned'); return x; }
      if (seen.has(x)) return seen.get(x);
      var r;
      if (Array.isArray(x)) { r = []; seen.set(x, r); for (var i = 0; i < x.length; i++) r[i] = cl(x[i]); return r; }
      if (x instanceof Date) return new Date(x.getTime());
      if (x instanceof Map) { r = new Map(); seen.set(x, r); x.forEach(function (val, k) { r.set(cl(k), cl(val)); }); return r; }
      if (x instanceof Set) { r = new Set(); seen.set(x, r); x.forEach(function (val) { r.add(cl(val)); }); return r; }
      r = {}; seen.set(x, r);
      for (var k of Object.keys(x)) r[k] = cl(x[k]);
      return r;
    }
    return cl(v);
  };

  /* ---- URLSearchParams / URL ---- */
  var enc = function (s) { return encodeURIComponent(s).replace(/%20/g, '+').replace(/[!'()~]/g, function (c) { return '%' + c.charCodeAt(0).toString(16).toUpperCase(); }); };
  var dec = function (s) { try { return decodeURIComponent(s.replace(/\+/g, ' ')); } catch (e) { return s; } };
  class URLSearchParams {
    constructor(init) {
      this._l = []; this._u = null;
      if (init && typeof init === 'object') {
        if (init instanceof URLSearchParams) this._l = init._l.map(function (p) { return [p[0], p[1]]; });
        else if (typeof init[Symbol.iterator] === 'function') { for (var p of init) this._l.push([String(p[0]), String(p[1])]); }
        else for (var k of Object.keys(init)) this._l.push([k, String(init[k])]);
      } else if (init) {
        init = String(init); if (init[0] === '?') init = init.slice(1);
        if (init) for (var part of init.split('&')) { if (!part) continue; var i = part.indexOf('='); this._l.push(i < 0 ? [dec(part), ''] : [dec(part.slice(0, i)), dec(part.slice(i + 1))]); }
      }
    }
    _sync() { if (this._u) this._u._setSearch(this.toString()); }
    get size() { return this._l.length; }
    append(k, v) { this._l.push([String(k), String(v)]); this._sync(); }
    delete(k) { this._l = this._l.filter(function (p) { return p[0] !== k; }); this._sync(); }
    get(k) { for (var p of this._l) if (p[0] === k) return p[1]; return null; }
    getAll(k) { return this._l.filter(function (p) { return p[0] === k; }).map(function (p) { return p[1]; }); }
    has(k) { return this.get(k) !== null; }
    set(k, v) { var f = false; k = String(k); v = String(v); this._l = this._l.filter(function (p) { if (p[0] !== k) return true; if (f) return false; p[1] = v; f = true; return true; }); if (!f) this._l.push([k, v]); this._sync(); }
    sort() { this._l.sort(function (a, b) { return a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0; }); this._sync(); }
    forEach(f, t) { for (var p of this._l) f.call(t, p[1], p[0], this); }
    keys() { return this._l.map(function (p) { return p[0]; }); }
    values() { return this._l.map(function (p) { return p[1]; }); }
    entries() { return this._l.map(function (p) { return [p[0], p[1]]; }); }
    [Symbol.iterator]() { return this.entries()[Symbol.iterator](); }
    toString() { return this._l.map(function (p) { return enc(p[0]) + '=' + enc(p[1]); }).join('&'); }
  }
  Object.defineProperty(Array.prototype, Symbol.iterator, { value: function () { var a = this, i = 0; return { next: function () { return i < a.length ? { value: a[i++], done: false } : { value: undefined, done: true }; } }; }, writable: true, configurable: true, enumerable: false });
  g.URLSearchParams = URLSearchParams;
  var DEFPORT = { 'http:': '80', 'https:': '443', 'ftp:': '21', 'ws:': '80', 'wss:': '443' };
  function normPath(p) {
    var out = [], parts = p.split('/');
    for (var i = 0; i < parts.length; i++) {
      var s = parts[i];
      if (s === '..') { if (out.length > 1) out.pop(); if (i === parts.length - 1) out.push(''); }
      else if (s === '.') { if (i === parts.length - 1) out.push(''); }
      else out.push(s);
    }
    var r = out.join('/'); return r[0] === '/' ? r : '/' + r;
  }
  class URL {
    constructor(u, base) {
      u = String(u).trim();
      var m = /^([a-zA-Z][a-zA-Z0-9+.\-]*:)(.*)$/.exec(u);
      if (!m) {
        if (base === undefined) throw new TypeError("Failed to construct 'URL': Invalid URL");
        var b = base instanceof URL ? base : new URL(base);
        if (u[0] === '#') u = b.pathname + b.search + u;
        else if (u[0] === '?') u = b.pathname + u;
        else if (u.slice(0, 2) === '//') u = b.protocol + u;
        else if (u[0] !== '/') u = b.pathname.slice(0, b.pathname.lastIndexOf('/') + 1) + u;
        if (u.slice(0, 2) !== '//' && u.indexOf(':') < 0 || u[0] === '/' && u[1] !== '/') u = b.protocol + '//' + b.host + u; else if (u.indexOf(':') < 0) u = b.protocol + u;
        m = /^([a-zA-Z][a-zA-Z0-9+.\-]*:)(.*)$/.exec(u);
      }
      this.protocol = m[1].toLowerCase(); var rest = m[2], h = '';
      var hi = rest.indexOf('#'); if (hi >= 0) { h = rest.slice(hi); rest = rest.slice(0, hi); }
      var q = '', qi = rest.indexOf('?'); if (qi >= 0) { q = rest.slice(qi); rest = rest.slice(0, qi); }
      this.username = ''; this.password = ''; this.hostname = ''; this.port = '';
      if (rest.slice(0, 2) === '//') {
        rest = rest.slice(2); var si = rest.indexOf('/'), auth = si < 0 ? rest : rest.slice(0, si); rest = si < 0 ? '' : rest.slice(si);
        var at = auth.lastIndexOf('@'); if (at >= 0) { var cred = auth.slice(0, at).split(':'); this.username = cred[0]; this.password = cred[1] || ''; auth = auth.slice(at + 1); }
        var pi = auth.lastIndexOf(':'); if (pi >= 0 && auth.indexOf(']') < pi) { this.port = auth.slice(pi + 1); auth = auth.slice(0, pi); }
        if (this.port === DEFPORT[this.protocol]) this.port = '';
        this.hostname = auth.toLowerCase();
        if (!this.hostname && this.protocol !== 'file:') throw new TypeError("Failed to construct 'URL': Invalid URL");
        this.pathname = normPath(rest || '/');
      } else this.pathname = rest;
      this.search = q.length > 1 ? q : ''; this.hash = h.length > 1 ? h : '';
      this._sp = null;
    }
    get host() { return this.hostname + (this.port ? ':' + this.port : ''); }
    get origin() { return DEFPORT[this.protocol] !== undefined ? this.protocol + '//' + this.host : 'null'; }
    get href() { return this.protocol + (this.hostname || this.protocol === 'file:' ? '//' : '') + (this.username ? this.username + (this.password ? ':' + this.password : '') + '@' : '') + this.host + this.pathname + this.search + this.hash; }
    get searchParams() { if (!this._sp) { this._sp = new URLSearchParams(this.search); this._sp._u = this; } return this._sp; }
    _setSearch(s) { this.search = s ? '?' + s : ''; }
    toString() { return this.href; }
    toJSON() { return this.href; }
    static canParse(u, b) { try { new URL(u, b); return true; } catch (e) { return false; } }
  }
  g.URL = URL;

  /* ---- AbortController ---- */
  class AbortSignal {
    constructor() { this.aborted = false; this.reason = undefined; this.onabort = null; this._l = []; }
    addEventListener(t, f) { if (t === 'abort') this._l.push(f); }
    removeEventListener(t, f) { this._l = this._l.filter(function (x) { return x !== f; }); }
    throwIfAborted() { if (this.aborted) throw this.reason; }
    _abort(r) {
      if (this.aborted) return; this.aborted = true;
      if (r === undefined) { r = new Error('The operation was aborted.'); r.name = 'AbortError'; }
      this.reason = r; var e = { type: 'abort', target: this };
      if (typeof this.onabort === 'function') this.onabort(e); for (var f of this._l.slice()) f.call(this, e);
    }
    static abort(r) { var s = new AbortSignal(); s._abort(r); return s; }
    static timeout(ms) { var s = new AbortSignal(); setTimeout(function () { var e = new Error('The operation timed out.'); e.name = 'TimeoutError'; s._abort(e); }, ms); return s; }
  }
  g.AbortSignal = AbortSignal;
  g.AbortController = class AbortController { constructor() { this.signal = new AbortSignal(); } abort(r) { this.signal._abort(r); } };

  /* ---- observers, matchMedia, customElements, misc ---- */
  g.IntersectionObserver = class IntersectionObserver {
    constructor(cb, o) { this._cb = cb; this._t = []; this.root = null; this.rootMargin = '0px'; this.thresholds = [0]; }
    observe(el) {   /* no scroll tracking: every observed element is reported as visible once (so lazy loaders run) */
      this._t.push(el); var self = this;
      setTimeout(function () { if (self._t.indexOf(el) >= 0) self._cb([{ target: el, isIntersecting: true, intersectionRatio: 1, time: Date.now(), boundingClientRect: el.getBoundingClientRect ? el.getBoundingClientRect() : {}, rootBounds: null, intersectionRect: {} }], self); }, 0);
    }
    unobserve(el) { this._t = this._t.filter(function (x) { return x !== el; }); }
    disconnect() { this._t = []; }
    takeRecords() { return []; }
  };
  g.ResizeObserver = class ResizeObserver {
    constructor(cb) { this._cb = cb; this._t = []; }
    observe(el) { this._t.push(el); var self = this; setTimeout(function () { if (self._t.indexOf(el) >= 0) { var r = el.getBoundingClientRect ? el.getBoundingClientRect() : { width: 0, height: 0 }; self._cb([{ target: el, contentRect: r, borderBoxSize: [{ inlineSize: r.width, blockSize: r.height }], contentBoxSize: [{ inlineSize: r.width, blockSize: r.height }] }], self); } }, 0); }
    unobserve(el) { this._t = this._t.filter(function (x) { return x !== el; }); }
    disconnect() { this._t = []; }
  };
  g.MutationObserver = class MutationObserver { constructor(cb) { this._cb = cb; } observe() {} disconnect() {} takeRecords() { return []; } };
  g.PerformanceObserver = class PerformanceObserver { constructor() {} observe() {} disconnect() {} };
  g.matchMedia = function (q) {
    var w = g.innerWidth || 1024, h = g.innerHeight || 768, res = true, m;
    String(q).split(/\s+and\s+/).forEach(function (c) {
      c = c.replace(/[()]/g, '').trim();
      if (m = /^(min|max)-width\s*:\s*([\d.]+)px$/.exec(c)) { res = res && (m[1] === 'min' ? w >= +m[2] : w <= +m[2]); }
      else if (m = /^(min|max)-height\s*:\s*([\d.]+)px$/.exec(c)) { res = res && (m[1] === 'min' ? h >= +m[2] : h <= +m[2]); }
      else if (/^prefers-color-scheme\s*:\s*dark$/.test(c) || /^prefers-reduced-motion\s*:\s*reduce$/.test(c) || /^hover\s*:\s*none$/.test(c) || /^pointer\s*:\s*(coarse|none)$/.test(c)) res = false;
      else if (/^orientation\s*:\s*portrait$/.test(c)) res = h >= w;
      else if (/^orientation\s*:\s*landscape$/.test(c)) res = w > h;
    });
    return { matches: res, media: String(q), onchange: null, addEventListener: function () {}, removeEventListener: function () {}, addListener: function () {}, removeListener: function () {}, dispatchEvent: function () { return true; } };
  };
  var ce = {};
  g.customElements = { define: function (n, c) { if (ce[n]) throw new Error("NotSupportedError: '" + n + "' has already been defined"); ce[n] = c; }, get: function (n) { return ce[n]; }, whenDefined: function (n) { return Promise.resolve(ce[n]); }, upgrade: function () {} };
  if (typeof g.requestIdleCallback === 'undefined') { g.requestIdleCallback = function (f) { return setTimeout(function () { f({ didTimeout: false, timeRemaining: function () { return 10; } }); }, 1); }; g.cancelIdleCallback = function (i) { clearTimeout(i); }; }
  if (typeof g.reportError === 'undefined') g.reportError = function (e) { console.error(e); };
  if (g.performance) { missing(g.performance, 'mark', function () { return {}; }); missing(g.performance, 'measure', function () { return {}; }); missing(g.performance, 'getEntriesByName', function () { return []; }); missing(g.performance, 'getEntriesByType', function () { return []; }); }

  /* ---- fetch / Headers / Response / Request / FormData / Blob / XMLHttpRequest (synchronous host callback) ---- */
  class Headers {
    constructor(init) { this._m = {}; if (init) { if (init instanceof Headers) init = init._m; if (Array.isArray(init)) for (var p of init) this.append(p[0], p[1]); else for (var k of Object.keys(init)) this.append(k, init[k]); } }
    append(k, v) { k = String(k).toLowerCase(); this._m[k] = this._m[k] !== undefined ? this._m[k] + ', ' + v : String(v); }
    set(k, v) { this._m[String(k).toLowerCase()] = String(v); }
    get(k) { var v = this._m[String(k).toLowerCase()]; return v === undefined ? null : v; }
    has(k) { return this._m[String(k).toLowerCase()] !== undefined; }
    delete(k) { delete this._m[String(k).toLowerCase()]; }
    forEach(f, t) { for (var k of Object.keys(this._m)) f.call(t, this._m[k], k, this); }
    entries() { var m = this._m; return Object.keys(m).sort().map(function (k) { return [k, m[k]]; }); }
    keys() { return this.entries().map(function (e) { return e[0]; }); }
    values() { return this.entries().map(function (e) { return e[1]; }); }
    [Symbol.iterator]() { return this.entries()[Symbol.iterator](); }
  }
  g.Headers = Headers;
  g.Blob = class Blob { constructor(parts, o) { this._s = (parts || []).map(function (p) { return typeof p === 'string' ? p : p && p._s !== undefined ? p._s : String(p); }).join(''); this.type = (o && o.type) || ''; this.size = this._s.length; } text() { return Promise.resolve(this._s); } slice(a, b, t) { return new g.Blob([this._s.slice(a, b)], { type: t || this.type }); } };
  g.File = class File extends g.Blob { constructor(parts, name, o) { super(parts, o); this.name = name; this.lastModified = Date.now(); } };
  g.FormData = class FormData {
    constructor() { this._l = []; }
    append(k, v) { this._l.push([String(k), v]); } set(k, v) { this.delete(k); this.append(k, v); } delete(k) { this._l = this._l.filter(function (p) { return p[0] !== k; }); }
    get(k) { for (var p of this._l) if (p[0] === k) return p[1]; return null; } getAll(k) { return this._l.filter(function (p) { return p[0] === k; }).map(function (p) { return p[1]; }); } has(k) { return this.get(k) !== null; }
    entries() { return this._l.map(function (p) { return [p[0], p[1]]; }); } forEach(f, t) { for (var p of this._l) f.call(t, p[1], p[0], this); }
    [Symbol.iterator]() { return this.entries()[Symbol.iterator](); }
  };
  class Response {
    constructor(body, init) {
      init = init || {}; this._b = body === undefined || body === null ? '' : typeof body === 'string' ? body : body instanceof URLSearchParams ? body.toString() : body._s !== undefined ? body._s : String(body);
      this.status = init.status === undefined ? 200 : init.status; this.statusText = init.statusText || ''; this.ok = this.status >= 200 && this.status < 300;
      this.headers = new Headers(init.headers); this.url = init.url || ''; this.type = 'basic'; this.redirected = false; this.bodyUsed = false;
    }
    text() { this.bodyUsed = true; return Promise.resolve(this._b); }
    json() { var b = this._b; this.bodyUsed = true; return new Promise(function (res, rej) { try { res(JSON.parse(b)); } catch (e) { rej(e); } }); }
    blob() { return Promise.resolve(new g.Blob([this._b], { type: this.headers.get('content-type') || '' })); }
    arrayBuffer() { var ab = new g.ArrayBuffer(this._b.length); ab._s = this._b; return Promise.resolve(ab); }
    formData() { return Promise.reject(new Error('Response.formData() is not supported')); }
    clone() { return new Response(this._b, { status: this.status, statusText: this.statusText, headers: this.headers, url: this.url }); }
    static json(d, init) { var r = new Response(JSON.stringify(d), init); r.headers.set('content-type', 'application/json'); return r; }
    static error() { var r = new Response('', { status: 0 }); r.type = 'error'; return r; }
  }
  g.Response = Response;
  g.Request = class Request { constructor(i, init) { init = init || {}; if (i instanceof g.Request) { init = Object.assign({ method: i.method, headers: i.headers, body: i.body }, init); i = i.url; } this.url = String(i); this.method = (init.method || 'GET').toUpperCase(); this.headers = new Headers(init.headers); this.body = init.body === undefined ? null : init.body; this.signal = init.signal || null; this.credentials = init.credentials || 'same-origin'; this.mode = init.mode || 'cors'; } clone() { return new g.Request(this); } };
  function doFetch(method, url, body, headers) {
    var h = []; for (var e of new Headers(headers).entries()) h.push(e[0] + ': ' + e[1]);
    return g.__fetch(method, url, body === undefined || body === null ? null : (body instanceof URLSearchParams ? body.toString() : typeof body === 'string' ? body : body._s !== undefined ? body._s : String(body)), h.join('\n'));
  }
  g.fetch = function (input, init) {
    return new Promise(function (resolve, reject) {
      var req = input instanceof g.Request ? input : new g.Request(input, init), sig = (init && init.signal) || req.signal;
      if (sig && sig.aborted) { reject(sig.reason); return; }
      var r = doFetch(req.method, req.url, req.body, req.headers);
      if (!r || r.status === 0) { reject(new TypeError('Failed to fetch: ' + ((r && r.error) || 'network error'))); return; }
      var h = {}; String(r.headers || '').split('\n').forEach(function (l) { var i = l.indexOf(':'); if (i > 0) h[l.slice(0, i).trim().toLowerCase()] = l.slice(i + 1).trim(); });
      if (r.contentType && !h['content-type']) h['content-type'] = r.contentType;
      resolve(new Response(r.body, { status: r.status, statusText: r.statusText || '', headers: h, url: r.url || req.url }));
    });
  };
  g.XMLHttpRequest = class XMLHttpRequest {
    constructor() { this.readyState = 0; this.status = 0; this.statusText = ''; this.responseText = ''; this.response = null; this.responseType = ''; this.responseURL = ''; this.timeout = 0; this.withCredentials = false; this._h = {}; this._rh = {}; this._ev = {}; this.onload = this.onerror = this.onreadystatechange = this.onloadend = this.onabort = this.ontimeout = this.onprogress = this.onloadstart = null; this.upload = { addEventListener: function () {} }; }
    open(m, u) { this._m = String(m).toUpperCase(); this._u = String(u); this.readyState = 1; this._fire('readystatechange'); }
    setRequestHeader(k, v) { this._h[k] = v; }
    getResponseHeader(k) { var v = this._rh[String(k).toLowerCase()]; return v === undefined ? null : v; }
    getAllResponseHeaders() { var o = ''; for (var k of Object.keys(this._rh)) o += k + ': ' + this._rh[k] + '\r\n'; return o; }
    overrideMimeType() {}
    addEventListener(t, f) { (this._ev[t] = this._ev[t] || []).push(f); }
    removeEventListener(t, f) { this._ev[t] = (this._ev[t] || []).filter(function (x) { return x !== f; }); }
    _fire(t) { var e = { type: t, target: this, loaded: this.responseText.length, total: this.responseText.length, lengthComputable: true }, h = this['on' + t]; if (typeof h === 'function') h.call(this, e); for (var f of (this._ev[t] || []).slice()) f.call(this, e); }
    abort() { this._aborted = true; this.readyState = 0; this._fire('abort'); }
    send(body) {
      var self = this, r = doFetch(this._m, this._u, body, this._h);
      setTimeout(function () {
        if (self._aborted) return;
        if (!r || r.status === 0) { self.readyState = 4; self._fire('readystatechange'); self._fire('error'); self._fire('loadend'); return; }
        String(r.headers || '').split('\n').forEach(function (l) { var i = l.indexOf(':'); if (i > 0) self._rh[l.slice(0, i).trim().toLowerCase()] = l.slice(i + 1).trim(); });
        if (r.contentType && !self._rh['content-type']) self._rh['content-type'] = r.contentType;
        self.status = r.status; self.statusText = r.statusText || ''; self.responseText = r.body; self.responseURL = r.url || self._u;
        self.response = self.responseType === 'json' ? (function () { try { return JSON.parse(r.body); } catch (e) { return null; } })() : r.body;
        self.readyState = 2; self._fire('readystatechange'); self.readyState = 3; self._fire('readystatechange'); self._fire('progress'); self.readyState = 4; self._fire('readystatechange');
        self._fire('load'); self._fire('loadend');
      }, 0);
    }
  };
  g.XMLHttpRequest.UNSENT = 0; g.XMLHttpRequest.OPENED = 1; g.XMLHttpRequest.HEADERS_RECEIVED = 2; g.XMLHttpRequest.LOADING = 3; g.XMLHttpRequest.DONE = 4;
})(typeof globalThis !== 'undefined' ? globalThis : this);
