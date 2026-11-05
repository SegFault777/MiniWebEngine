function P(v){console.log(typeof v==='object'&&v!==null?JSON.stringify(v):String(v))}
var s=new Set([3,1,3,2]);var out=[];for(var x of s)out.push(x);P(out);P([...s]);P(Array.from(s).length);
var m=new Map([["a",1],["b",2]]);var ks=[];for(var [k,v] of m)ks.push(k+v);P(ks);P([...m].length);P([...m.keys()]);P([...m.values()]);
var [p,q]=new Set([7,8]);P(p+q);P(Math.max(...new Set([4,9,2])));
for(const ch of "héy")out.push(ch);P(out);
const [a,...rest]="xyz";P(a+rest.join("-"));
try{for(var z of 5){}}catch(e){P(e instanceof TypeError)}
m.forEach((v,k)=>P(k+v));s.forEach(v=>P(v));P(new Set("hello").size);P([...new Set([1,2,2,3])].join());
