function P(v){console.log(String(v))}
var xs=[0.1,0.2,0.3,1.1,2.675,1e22,1e23,123.456,9.995,5e-5,0.000123,3.14159265358979,2.718281828459045,1/7,22/7,100/7,1e100,1.23e-100,4.9e-324,2.2250738585072014e-308,1.7976931348623157e308,0.5,255/256,1e15+0.3,123456789.123456789,0.1+0.2,0.3-0.1,1.15*100,9.7*100];
for(var i=0;i<xs.length;i++){P(xs[i]);P(xs[i].toFixed(3))}
for(var i=1;i<60;i++){P(Math.sqrt(i*1.37));P(Math.pow(i,0.3).toFixed(10));P(Math.log(i*0.7).toFixed(10));P(Math.exp(i/9).toPrecision?Math.exp(i/9).toFixed(6):0);P(Math.sin(i).toFixed(10));P(Math.cos(i*0.3).toFixed(10));P(Math.atan(i/7).toFixed(10))}
P(Number("1.7976931348623157e308"));P(Number("1.7976931348623159e308"));P(Number("5e-324"));P(Number("2e-324"));P(Number("3e-324"));P(Number("1e-400"));P(Number("12345678901234567890123"));P(Number("0.1e1"));P(Number("1e22"));P(Number("1.5e300"));
