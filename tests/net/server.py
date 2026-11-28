#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Misbehaving HTTP server for the v0.10 network tests: slow, hanging, trickling, chunked, redirecting, huge.
import sys,time,io,threading
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from PIL import Image
def png(c,w=60,h=30):
    b=io.BytesIO();Image.new("RGB",(w,h),c).save(b,"PNG");return b.getvalue()
PAGE=b"""<html><head><title>net test</title><link rel=stylesheet href="/slow.css"><script src="/slow.js"></script></head>
<body><h1 id=h>Hello</h1><p id=p>text before</p><img src="/ok.png"><img src="/slow.png"><img src="/hang.png"><p>tail</p></body></html>"""
class H(BaseHTTPRequestHandler):
    protocol_version="HTTP/1.1"
    def log_message(self,*a): pass
    def send(self,code,body,ct="text/html",extra=()):
        self.send_response(code);self.send_header("Content-Type",ct);self.send_header("Content-Length",str(len(body)))
        for k,v in extra:self.send_header(k,v)
        self.send_header("Connection","close");self.end_headers();self.wfile.write(body)
    def do_GET(self):
        p=self.path.split("?")[0]
        try:
            if p=="/index.html":return self.send(200,PAGE)
            if p=="/slow.css":time.sleep(1.5);return self.send(200,b"h1{color:#c00} body{background:#eef}","text/css",[("Cache-Control","max-age=60")])
            if p=="/slow.js":time.sleep(1.0);return self.send(200,b"document.getElementById('p').textContent='script ran';","application/javascript")
            if p=="/ok.png":return self.send(200,png("green"),"image/png")
            if p=="/slow.png":
                d=png("blue",200,100);self.send_response(200);self.send_header("Content-Type","image/png");self.send_header("Content-Length",str(len(d)));self.send_header("Connection","close");self.end_headers()
                for i in range(0,len(d),200):self.wfile.write(d[i:i+200]);self.wfile.flush();time.sleep(0.2)
                return
            if p=="/hang.png":time.sleep(40);return
            if p=="/hang":time.sleep(60);return
            if p=="/redir":self.send_response(302);self.send_header("Location","/redir");self.send_header("Content-Length","0");self.end_headers();return
            if p=="/redir1":self.send_response(302);self.send_header("Location","/ok.html");self.send_header("Content-Length","0");self.end_headers();return
            if p=="/ok.html":return self.send(200,b"<html><title>redirected</title><body><h1>arrived</h1></body></html>")
            if p=="/chunked":
                self.send_response(200);self.send_header("Content-Type","text/html");self.send_header("Transfer-Encoding","chunked");self.send_header("Connection","close");self.end_headers()
                for part in (b"<html><title>chunked</title><body><h1>chun",b"ked ok</h1></body>",b"</html>"):
                    self.wfile.write(b"%x\r\n%s\r\n"%(len(part),part));self.wfile.flush();time.sleep(0.2)
                self.wfile.write(b"0\r\n\r\n");return
            if p=="/huge.css":return self.send(200,b"a{}"*200000,"text/css")   # 600 KB > 256 KB limit
            if p=="/trickle":   # never finishes the headers
                self.wfile.write(b"HTTP/1.1 200 OK\r\n");self.wfile.flush();time.sleep(60);return
            if p=="/api":time.sleep(0.5);return self.send(200,b'{"ok":true,"n":42}',"application/json")
            if p=="/fetch.html":return self.send(200,b"""<html><body><p id=o>waiting</p><script>
var log=[];
fetch('/api').then(function(r){return r.json()}).then(function(j){document.getElementById('o').textContent='then:'+j.n;});
var ac=new AbortController();fetch('/hang',{signal:ac.signal}).catch(function(e){console.log('aborted:'+e.name)});setTimeout(function(){ac.abort()},50);
</script></body></html>""")
            if p=="/await2.html":return self.send(200,b'''<html><body><script>fetch('/api').then(function(r){console.log('got '+r.status);});(async function(){console.log('A');var r=await fetch('/api');console.log('B '+r.status);})();</script></body></html>''')
            if p=="/slowapi":time.sleep(1.2);return self.send(200,b'{"ok":1}',"application/json")
            if p=="/await_ui.html":return self.send(200,b"""<html><head><title>await ui</title></head><body><p id=o>start</p><script>
var ticks=0;var iv=setInterval(function(){ticks++;document.getElementById('o').textContent='tick '+ticks;},100);
(async function(){var r=await fetch('/slowapi');var j=await r.json();clearInterval(iv);console.log('awaited ok='+j.ok+' ticks-during-wait='+(ticks>=8?'many':ticks));})();
console.log('main script returned');
</script></body></html>""")
            if p=="/await.html":return self.send(200,b"""<html><body><p id=o>waiting</p><script>
(async function(){var r=await fetch('/api');var j=await r.json();document.getElementById('o').textContent='await:'+j.n;console.log('await done '+j.n);})();
console.log('main script end');
</script></body></html>""")
            if p=="/form.html":return self.send(200,b"""<html><head><title>form</title></head><body>
<form action="/echo" method="get"><input name=q id=q placeholder="search"><input type=checkbox name=c value=yes checked><select name=s><option value=a>A</option><option value=b selected>B</option></select><input type=submit value=Go name=go></form>
<form action="/echo" method="post"><textarea name=t>hi</textarea><input name=u value="x y"><button>Post</button></form></body></html>""")
            if p=="/echo":
                q=self.path.partition("?")[2]
                return self.send(200,("<html><head><title>echo GET %s</title></head><body>%s</body></html>"%(q,q)).encode())
            if p=="/setck":
                self.send_response(200);self.send_header("Content-Type","text/html")
                for c in ("sid=abc123; Path=/","pref=dark; Max-Age=3600; Path=/","secret=hid; HttpOnly; Path=/","sec=1; Secure; Path=/","evil=1; Domain=example.com; Path=/","gone=1; Path=/","narrow=x; Path=/only"):
                    self.send_header("Set-Cookie",c)
                body=b"<html><head><title>cookies set</title></head><body>ok</body></html>"
                self.send_header("Content-Length",str(len(body)));self.send_header("Connection","close");self.end_headers();self.wfile.write(body);return
            if p=="/delck":
                self.send_response(200);self.send_header("Set-Cookie","gone=; Max-Age=0; Path=/");body=b"<html><title>del</title></html>"
                self.send_header("Content-Length",str(len(body)));self.send_header("Connection","close");self.end_headers();self.wfile.write(body);return
            if p=="/ck":return self.send(200,("<html><head><title>Cookie: %s</title></head></html>"%self.headers.get("Cookie","(none)")).encode())
            if p=="/only/ck":return self.send(200,("<html><head><title>Cookie: %s</title></head></html>"%self.headers.get("Cookie","(none)")).encode())
            if p=="/jsck.html":return self.send(200,b"""<html><head><title>jsck</title></head><body><script>
console.log('doc.cookie='+document.cookie);
document.cookie='fromjs=7; Path=/'; document.cookie='bad=1; HttpOnly';
console.log('after='+document.cookie);
fetch('/ck').then(function(r){return r.text()}).then(function(t){console.log('fetch saw '+t.match(/<title>(.*)<\/title>/)[1]);});
</script></body></html>""")
            self.send(404,b"nope")
        except (BrokenPipeError,ConnectionResetError):pass
    def do_POST(self):
        n=int(self.headers.get("Content-Length","0"));b=self.rfile.read(n).decode()
        ct=self.headers.get("Content-Type","")
        self.send(200,("<html><head><title>echo POST %s | %s</title></head><body>x</body></html>"%(b,ct)).encode())
if __name__=="__main__":
    ThreadingHTTPServer(("127.0.0.1",int(sys.argv[1])),H).serve_forever()
