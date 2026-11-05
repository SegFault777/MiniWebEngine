function P(v){console.log(String(v))}
class T{constructor(){this._v=1}get v(){return this._v*2}set v(x){this._v=x}static get s(){return "st"}m(){return this.v}}
var t=new T();P(t.v);t.v=5;P(t.v);P(T.s);P(t.m());
var o={_a:3,get a(){return this._a+1},set a(x){this._a=x},get:1,set:2,b:5};P(o.a);o.a=10;P(o.a);P(o.get+o.set);P(JSON.stringify(o));P(Object.keys(o));
class U extends T{get v(){return super.v+100}}P(new U().v);
var d={};Object.defineProperty(d,"q",{get(){return 42}});P(d.q);
P({...o}.a);
