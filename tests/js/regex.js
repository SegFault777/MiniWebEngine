function P(v){console.log(typeof v==='object'&&v!==null?JSON.stringify(v):String(v))}
P(/a+b/.test("xaaab"));P(/^\d+$/.test("12345"));P(/^\d+$/.test("12a45"));P("a1b22c333".match(/\d+/g));P("a1b22".match(/(\w)(\d+)/));
P("2024-01-15".replace(/(\d+)-(\d+)-(\d+)/,"$3/$2/$1"));P("hello world".replace(/o/g,"0"));P("Hello".replace(/l/gi,function(m,i){return i}));
P("a, b,c ,d".split(/\s*,\s*/));P("abc".split(/(b)/));P("a1b2c3".split(/\d/));P("abc".split(/(?:)/));P("x".split(/y/));
P(/colou?r/i.test("COLOR"));P(/^(?:a|b)+$/.test("abba"));P("aaa".match(/a*?/)[0].length);P("foo.bar".replace(/\./g,"-"));
P(/(\d{3})-(\d{4})/.exec("call 555-1234 now"));var m=/(\d{3})-(\d{4})/.exec("call 555-1234 now");P(m.index);P(m[2]);
var re=/a/g;var s="aXaXa";var c=0;while(re.exec(s))c++;P(c);P(re.lastIndex);
P("  trim me  ".replace(/^\s+|\s+$/g,""));P("camelCaseString".replace(/([A-Z])/g," $1").toLowerCase());
P("a-b_c d".split(/[-_ ]/));P(/[^abc]/.test("abc"));P(/[a-c]+/.exec("zzabcabz")[0]);P(/\bcat\b/.test("a cat sat"));P(/\bcat\b/.test("concat"));
P(/(a)\1/.test("aa"));P(/(a)\1/.test("ab"));P(/x(?=y)/.test("xy"));P(/x(?!y)/.test("xy"));P("price: $5.50".match(/\$(\d+)\.(\d+)/).slice(1));
P("line1\nline2".match(/^line\d$/gm));P(/a.c/s.test("a\nc"));P(/a.c/.test("a\nc"));
P("The Quick Brown".match(/[A-Z]/g).join(""));P("a  b   c".split(/\s+/));P("abc".search(/c/));P("abc".search(/z/));
P([..."a1b2".matchAll(/[a-z](\d)/g)].map(x=>x[1]));P(/^[\w.+-]+@[\w-]+\.[\w.]+$/.test("user.name+tag@example.co.uk"));
P("aaa".replace(/a/g,"$&$&"));P("abc".replace(/(?<first>a)/,"[$1]"));P("x".replace(/(y)?x/,"[$1]"));
P(/\u0041/.test("A"));P(/[\u4e00-\u9fa5]+/.test("한자中文"));P("가나다".replace(/나/,"X"));P(String(/a\/b/gi));P(/a{2,3}/.exec("aaaa")[0]);P(/a{2}/.exec("aaaa")[0]);
P("abcabc".replace(/b/g,"$'"));P("abc".replace(/b/,"$`"));P(new RegExp("a.c","i").test("ABC"));P(new RegExp("\\d+").exec("ab12")[0]);
P(/(\d+)\s*(px|em)/.exec("width: 12 px")[2]);P("1,234,567".replace(/,/g,""));P("a.b.c".split(".").length);P("tel: 010-1234-5678".replace(/\D/g,""));
P(/^$/.test(""));P(/^a*$/.test("aaaa"));P("ab".match(/(a)|(b)/g));P(/(a)|(b)/.exec("b"));
