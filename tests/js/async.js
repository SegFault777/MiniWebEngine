function P(v){console.log(typeof v==='object'&&v!==null?JSON.stringify(v):String(v))}
function sleep(ms){return new Promise(function(r){setTimeout(r,ms)})}
/* Everything is sequential: await does not suspend in this engine, so no output may depend on how an
 * async function interleaves with the code after its call. */
async function main(){
  P('start');
  Promise.resolve().then(function(){P('a1')}).then(function(){P('a2')});
  Promise.resolve().then(function(){P('b1')}).then(function(){P('b2')});
  await sleep(5);
  P(await Promise.all([1,Promise.resolve(2),new Promise(function(r){setTimeout(function(){r(3)},5)})]));
  var st=await Promise.allSettled([Promise.reject(new Error('x')),4]);
  P(st.map(function(x){return x.status+':'+(x.value!==undefined?x.value:x.reason.message)}));
  P(await Promise.race([sleep(30).then(function(){return 'slow'}),sleep(2).then(function(){return 'fast'})]));
  P(await Promise.any([Promise.reject(1),Promise.resolve(2)]));
  try{await Promise.any([Promise.reject(1)])}catch(e){P(e.constructor.name==='AggregateError'||e.message)}
  var fin=[];var r=await Promise.reject(new Error('boom')).catch(function(e){return e.message}).finally(function(){fin.push('fin')});P(r);P(fin);
  P(await new Promise(function(res,rej){res(1);rej(2);res(3)}));
  P(await Promise.resolve({then:function(r){r('thenable')}}));
  var v=await 5;P(v);
  P(await new Promise(function(r){setTimeout(function(){r('timer')},3)}));
  try{await Promise.reject(new TypeError('bad'))}catch(e){P(e instanceof TypeError);P(e.message)}
  for await(var x of [Promise.resolve('p'),'q'])P(x);
  var arrow=async function(n){return n*2};P(await arrow(21));
  var o={async m(){return 'method'}};P(await o.m());
  class K{static async s(){return 'static'}}P(await K.s());
  async function thrower(){await sleep(1);throw new RangeError('late')}
  try{await thrower()}catch(e){P(e.name+':'+e.message)}
  var ac=new AbortController();setTimeout(function(){ac.abort()},2);var abortedAt=await new Promise(function(res){ac.signal.addEventListener('abort',function(){res('aborted')})});P(abortedAt);
  queueMicrotask(function(){P('micro')});await sleep(1);
  return 'main done';
}
main().then(P);
