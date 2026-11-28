// v0.13: real coroutines. Expected output comes from Node (same program).
var out = [];
function log() { var s = []; for (var i = 0; i < arguments.length; i++) s.push(String(arguments[i])); out.push(s.join(' ')); console.log(s.join(' ')); }

// 1. async function runs synchronously until its first await; the rest is a microtask
async function a1() { log('a1 start'); await null; log('a1 after await'); return 'a1 done'; }
log('before'); var p1 = a1(); log('after call'); p1.then(function (v) { log('then', v); });
Promise.resolve().then(function () { log('micro 1'); }).then(function () { log('micro 2'); });
log('sync end');

// 2. tick counting: await of a promise vs a non-promise vs a thenable
async function a2() { await 1; log('a2 1'); await new Promise(function (r) { r(); }); log('a2 2'); await { then: function (r) { r(); } }; log('a2 3'); }
a2(); Promise.resolve().then(function () { log('t1'); }).then(function () { log('t2'); }).then(function () { log('t3'); }).then(function () { log('t4'); }).then(function () { log('t5'); });

// 3. exceptions across await, try/catch/finally
async function a3() { try { await Promise.reject(new Error('boom')); log('not reached'); } catch (e) { log('caught', e.message); } finally { log('finally'); } return 7; }
a3().then(function (v) { log('a3 ->', v); });
async function a4() { await null; throw new TypeError('late'); }
a4().catch(function (e) { log('a4 rejected', e.name); });

// 4. sequential loops with awaits, closures kept across suspension
async function a5() { var fns = []; for (let i = 0; i < 3; i++) { await null; fns.push(function () { return i; }); } log('a5', fns.map(function (f) { return f(); }).join(',')); }
a5();

// 5. nested async calls and Promise.all interleaving
async function leaf(n, d) { for (var i = 0; i < d; i++) await null; log('leaf', n); return n * 2; }
async function a6() { var r = await Promise.all([leaf(1, 3), leaf(2, 1), leaf(3, 2)]); log('a6', r.join(',')); }
a6();

// 6. timers interleave with awaits (virtual time in the engine: only ordering matters)
function sleep(ms) { return new Promise(function (r) { setTimeout(r, ms); }); }
async function a7() { log('a7 s'); await sleep(20); log('a7 20'); await sleep(10); log('a7 30'); }
async function a8() { log('a8 s'); await sleep(25); log('a8 25'); }
a7(); a8(); setTimeout(function () { log('timer 15'); }, 15);

// 7. generators are lazy; next(v) sends values; infinite generator
function* nat() { var i = 0; while (true) { var s = yield i++; if (s) i = s; } }
var g = nat(); log('g', g.next().value, g.next().value, g.next(10).value, g.next().value);
function* two() { try { var x = yield 1; log('got', x); yield 2; } finally { log('cleanup'); } }
var t = two(); log(t.next().value, t.next('hi').value, JSON.stringify(t.return(9)), JSON.stringify(t.next()));
var t2 = two(); t2.next(); try { t2.throw(new Error('inj')); } catch (e) { log('throw ->', e.message); }
function* inner() { var a = yield 'i1'; log('inner got', a); return 'iret'; }
function* outer() { var r = yield* inner(); log('outer r', r); yield* [10, 20]; }
var o = outer(); log(o.next().value, o.next('X').value, o.next().value, JSON.stringify(o.next()));
// for-of over a lazy generator with break closes it
function* closing() { try { yield 1; yield 2; yield 3; } finally { log('closed'); } }
for (var v of closing()) { log('v', v); if (v == 2) break; }
function* take(n, it) { var i = 0; for (var x of it) { if (i++ >= n) return; yield x; } }
log([...take(5, nat())].join(','));
var spreadGen = (function* () { yield 1; yield 2; })(); log(Array.from(spreadGen).join('-'));
var m = new Map(); function* kv() { yield ['a', 1]; yield ['b', 2]; } m = new Map(kv()); log(m.size);

// 8. async generators and for await
async function* ag() { for (var i = 0; i < 3; i++) { await null; yield i; } return 'end'; }
(async function () { var s = []; for await (var x of ag()) s.push(x); log('for await', s.join(',')); var it = ag(); log(JSON.stringify(await it.next()), JSON.stringify(await it.return('r')), JSON.stringify(await it.next())); })();

// 9. many pending coroutines, deep recursion inside one
async function many(i) { await null; return i; }
(async function () { var ps = []; for (var i = 0; i < 100; i++) ps.push(many(i)); var r = await Promise.all(ps); log('many', r.length, r[99]); })();
async function deep(n) { return n == 0 ? 0 : 1 + await deep(n - 1); }
deep(60).then(function (v) { log('deep', v); });
log('script end');
