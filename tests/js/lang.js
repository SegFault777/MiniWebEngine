function P(v){console.log(typeof v==='object'&&v!==null?JSON.stringify(v):String(v))}
P(123456789012345680000);P(1e21);P(1.5e300);P(-1.5e-10);P(0.000001);P(0.0000001);P(123456.789);P(1/3);P(2/3);P(10/3);P(100);P(1e5);P(12345678901234567890);
P(0.1+0.7);P(4.35*100);P(1.005*1000);P(9007199254740993);P(5e-324);P(1.7976931348623157e308);
P(Math.floor(-1.5));P(Math.ceil(-1.5));P(Math.round(-1.5));P(Math.round(2.5));P(Math.round(-0.4));P(Math.trunc(-3.9));P(Math.sign(-3));P(Math.abs(-0));
P(Math.sqrt(2));P(Math.pow(2,0.5));P(Math.pow(2,-2));P(Math.max());P(Math.min());P(Math.hypot(3,4));
P(Math.sin(1).toFixed(6));P(Math.cos(1).toFixed(6));P(Math.atan2(1,2).toFixed(6));P(Math.log(10).toFixed(6));P(Math.exp(2).toFixed(5));P(Math.atan(0.5).toFixed(6));P(Math.tan(0.5).toFixed(6));
P((1.45).toFixed(1));P((8.345).toFixed(2));P((0).toFixed(2));P((-1.5).toFixed(0));P((1e21).toFixed(2));P((123.456).toFixed(10));
P(parseInt("0x1f"));P(parseInt("12abc"));P(parseInt("abc"));P(parseInt("-7.9"));P(parseInt("101",2));P(parseFloat(".5"));P(parseFloat("1e3x"));P(Number("0b101"));P(Number(""));P(Number(null));P(Number(undefined));P(Number(true));P(Number([5]));P(Number("1,2"));
P(5%0);P(-0===0);P(1/-0);P(2**53+1);P(7/2|0);P(-7/2|0);P(1<<32);P(1<<33);P(2147483647+1|0);P(4294967295>>>0);P(-1>>>0);P(5&3);P(5|3);P(5^3);
P("5"+3);P("5"-3);P("5"*"2");P(true+1);P(null+1);P(undefined+1);P([]+1);P([2]*[3]);P({}+1);P("b">"a");P("10"<"9");P(10<"9");P("a"<1);P(null>=0);P(undefined==0);P(null==0);
P("abc".charAt(1));P("abc".charAt(5)==="");P("abc"[1]);P("abc"[9]);P("abc".charCodeAt(1));P("abc".at(-1));
P("hello world".indexOf("o"));P("hello world".lastIndexOf("o"));P("hello".indexOf("z"));P("hello".includes("ell"));P("hello".startsWith("he"));P("hello".endsWith("lo"));
P("hello".slice(-3));P("hello".slice(1,-1));P("hello".slice(3,1));P("hello".substring(3,1));P("hello".substring(-2,2));P("hello".substr(1,2));P("hello".substr(-3,2));
P("a-b-c".split("-"));P("abc".split(""));P("a,b".split(",",1));P("".split(","));P("abc".split());P("a--b".split("-"));
P("  x ".trim());P("x".repeat(3));P("5".padStart(3,"0"));P("5".padEnd(3,"ab"));P("Abc".toLowerCase());P("abc".toUpperCase());P("a".concat("b",1));
P("abc".replace("b",function(m){return m+m}));P("aaa".replace("a","b"));P("aaa".replaceAll("a","b"));P("a.b".replace(".","-"));
P("b".localeCompare("a"));P("a".localeCompare("a"));
P([1,2,3].join("-"));P([1,[2,[3]]].join(";"));P([null,undefined,1].join());P([].join());P([1,2,3].toString());P(String([1,[2,3]]));P(String({}));P(String(null));P(String(undefined));P(String(()=>1).length>0);
P([1,2,3].indexOf(2));P([1,2,3].lastIndexOf(3));P([NaN].includes(NaN));P([NaN].indexOf(NaN));P([1,2,3,4].slice(1,3));P([1,2,3,4].slice(-2));P([1,2,3].concat(4,[5,[6]]));
var a=[1,2,3,4,5];P(a.splice(1,2));P(a);a.splice(1,0,"x","y");P(a);a.splice(-1);P(a);P(a.length);
var b=[3,1,10,2];P(b.sort());P(b.sort((x,y)=>x-y));P(b.reverse());P(["b","a","C"].sort());P([true,false].sort());
P([1,2,3].map((x,i)=>x*i));P([1,2,3,4].filter(x=>x%2));P([1,2,3].find(x=>x>1));P([1,2,3].find(x=>x>5));P([[1,2],[3]].flat());P([1,[2,[3,[4]]]].flat(Infinity));
P([1,2,3].reduce((a,b)=>a+b));P([[1,2],[3,4]].reduce((a,b)=>a.concat(b),[]));P([1,2,3].reduceRight===undefined);
P([1,2,3].shift());P([1,2,3].pop());var c=[1];c.unshift(0,0.5);P(c);c.push(9,8);P(c);P(c.length);c.length=2;P(c);c[5]=1;P(c.length);P(c);
P(Array(3).join("x"));P(new Array(2,3));P(Array.of(7,8));P(Array.from("abc"));P(Array.from(new Set([1,2,2])));P(Array.from({length:2}));
P(Object.keys({a:1,b:2}));P(Object.values({a:1,b:2}));P(Object.entries({a:1}));P(Object.assign({a:1},{b:2},{a:3}));P(Object.keys([5,6]));P(Object.keys("ab"));
P({a:1,b:{c:[1,2]},d:undefined,e:function(){},f:null});P(JSON.stringify([undefined,function(){},NaN]));P(JSON.stringify("q\"\\\n"));P(JSON.stringify({a:1,b:[1,2]},null,2));P(JSON.stringify({a:[]},null,"--"));
P(JSON.parse("[1,2,{\"a\":\"b\\n\"}]"));P(JSON.parse(" 12 "));P(JSON.parse('"x"'));P(JSON.parse("true"));P(JSON.parse("null"));P(JSON.parse('{"a":{"b":[]}}'));
try{JSON.parse("{bad")}catch(e){P(e instanceof SyntaxError)}
var o={x:1,y:2,z:3};var ks=[];for(var k in o)ks.push(k);P(ks);var vs=[];for(var v of [7,8])vs.push(v);P(vs);delete o.y;P(o);P("x" in o);P("y" in o);P(o.hasOwnProperty("z"));
function F(a,b=2,...r){return [a,b,r,arguments.length]}P(F(1));P(F(1,undefined,3,4));
var cnt=(function(){var n=0;return {inc:()=>++n,get:()=>n}})();cnt.inc();cnt.inc();P(cnt.get());
function Pt(x,y){this.x=x;this.y=y}Pt.prototype.sum=function(){return this.x+this.y};var pt=new Pt(1,2);P(pt.sum());P(pt instanceof Pt);P(pt.constructor===Pt);P(Object.getPrototypeOf(pt)===Pt.prototype);
var i=0;do{i++}while(i<5);P(i);var w=0;while(true){if(++w>3)break}P(w);
var x1=1;x1+=2;x1*=3;x1-=1;x1/=4;x1**=2;x1%=3;P(x1);var s1="a";s1+="b";P(s1);var n1=null;n1??=5;P(n1);var n2=0;n2||=7;P(n2);var n3=1;n3&&=9;P(n3);
var u=1;P(u++ + ++u);P(u--);P(u);P(-u);P(+"3");P(-"3");P(!u);P(void 0);P((1,2,3));
P(((a,b)=>a+b)(1,2));P((function(){return typeof this})());P([..."ab"].map(c=>c+c));
var date=new Date(Date.UTC?0:0);P(typeof date.getTime());
P([1,2,3].at(-1));P("x".at(0));P(isNaN("a"));P(isNaN("1"));P(isFinite("1"));P(Number.isInteger(5.0));P(Number.isInteger(5.5));P(Number.isNaN("a"));
P(encodeURIComponent("a b&c/d?é"));P(decodeURIComponent("a%20b%C3%A9"));
P(0.1+0.2===0.3);P(Math.abs(0.1+0.2-0.3)<Number.EPSILON);P(Number.MAX_SAFE_INTEGER);P(1000000*1000000);P(123e-20);P(0.00001234);P(1e-6);P(15e-7);
P((25).toString(2));P((-255).toString(16));P((3.75).toString(2));P((1e21).toString());P(String(-0));P(`${-0}`);P((12345.6789).toLocaleString());
P(new Map([[1,2]]).get(1));P(new Set("aab").size);var mm=new Map();mm.set("a",1).set("b",2);var ms=[];mm.forEach((v,k)=>ms.push(k+v));P(ms);P(mm.has("a"));mm.delete("a");P(mm.size);
P([3,2,1].map(String));P(["1","2","3"].map(Number));P(["1","2","3"].map(parseInt));P([0,1,"",null,"a"].filter(Boolean));
label2:{P("in");break label2;}
P(typeof globalThis)
