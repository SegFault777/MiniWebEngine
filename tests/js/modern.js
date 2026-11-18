function P(v){console.log(typeof v==='object'&&v!==null?JSON.stringify(v):String(v))}
/* --- generators --- */
function* g(){var a=yield 1;yield* [2,3];return 4}
P([...g()]);var it=g();P(it.next());P(it.next());P(it.next());P(it.next());P(it.next());
function* range(a,b){for(var i=a;i<b;i++)yield i}
var s=0;for(var n of range(0,5))s+=n;P(s);P(Array.from(range(1,4)));var [p,q]=range(10,20);P(p+q);
/* --- Symbol and iterables --- */
var sy=Symbol('d');var so={[sy]:1,a:2};P(typeof sy);P(Object.keys(so));P(Symbol('x')===Symbol('x'));P(Symbol.for('k')===Symbol.for('k'));
var iter={from:1,to:4,[Symbol.iterator](){var c=this.from,t=this.to;return{next:function(){return c<=t?{value:c++,done:false}:{value:undefined,done:true}}}}};
P([...iter]);P(Array.from(iter));var [i1,i2]=iter;P(i1+i2);for(var z of iter)s+=z;P(s);
class Money{constructor(v){this.v=v}[Symbol.toPrimitive](h){return h==='string'?'$'+this.v:this.v}}P(+new Money(5));P(`${new Money(7)}`);
/* --- classes --- */
class A{x=1;#p=2;static z=3;static #cnt=0;static{A.init='ready'}#m(){return this.#p*2}get p(){return this.#m()}static inc(){return ++A.#cnt}}
var a=new A();P(a.x);P(a.p);P(A.z);P(A.init);P(A.inc());P(A.inc());P(Object.keys(a));
/* --- tagged templates --- */
function tag(strs,...vals){return strs.join('|')+'#'+vals.join(',')+'#'+strs.raw.length}
P(tag`a${1}b${2}c`);P(String.raw`x${1}y`);
/* --- Proxy / Reflect --- */
var log=[];var px=new Proxy({a:1},{get:function(t,k){log.push('get '+String(k));return k in t?t[k]:'?'},set:function(t,k,v){log.push('set '+k);t[k]=v;return true},has:function(t,k){return k==='magic'||k in t},deleteProperty:function(t,k){log.push('del '+k);delete t[k];return true}});
P(px.a);P(px.zz);px.b=2;P('magic' in px);P('b' in px);delete px.a;P(Object.keys(px));P(log);P(JSON.stringify(px));P(Array.isArray(new Proxy([],{})));
P(Reflect.ownKeys({q:1,w:2}));P(Reflect.has({a:1},'a'));
/* --- builtins --- */
P(Object.fromEntries([['a',1],['b',2]]));P([[1],[2,3]].flatMap(function(x){return x}));P([1,2,3,4].findLast(function(x){return x%2}));P([3,1,2].toSorted());P([1,2,3].toReversed());P([1,2,3].with(1,9));
P(structuredClone({a:[1,{b:2}],d:new Date(0)}).a[1].b);P(btoa('hello world'));P(atob('aGVsbG8gd29ybGQ='));P(new TextEncoder().encode('é€').length);
P(new Error('e',{cause:'c'}).cause);P(String.fromCodePoint(128512).length);
P((1234567.891).toLocaleString('en-US'));P(new Intl.NumberFormat('en-US',{style:'currency',currency:'USD'}).format(1234.5));
/* --- URL / URLSearchParams --- */
var u=new URL('https://user:pw@Example.com:8080/a/../b/./c?x=1&y=a%20b#h');
P([u.protocol,u.username,u.password,u.hostname,u.port,u.host,u.pathname,u.search,u.hash,u.origin]);P(u.searchParams.get('y'));
P(new URL('../d?z=9','http://h.com/a/b/c').href);P(new URL('/root','http://h.com/a/b').href);P(new URL('//cdn.x.org/f.js','https://h.com').href);
var sp=new URLSearchParams({q:'a b',r:'1&2'});sp.append('q','z');P(sp.toString());P(sp.getAll('q'));sp.delete('q');P([...sp].length);
u.searchParams.set('n','5');P(u.search);
P(new AbortController().signal.aborted);var ac=new AbortController();ac.abort();P(ac.signal.aborted);
