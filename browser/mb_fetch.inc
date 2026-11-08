/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniBrowser module: mb_fetch
 * page sources (file:, HTTP), HSTS, mixed-content rules.
 * Not a standalone translation unit: minibrowser.c includes the modules in order (unity build). */
/* ---- page sources ---- */
static void errpage(const char*head,const char*msg,const char*url){
 bs(pg,PG);
 ba("<html><head><title>");be(head);ba("</title><style>body{max-width:600px;margin:48px auto;color:#202124}h1{font-size:26px}.u{color:#5f6368}</style></head><body><h1>");
 be(head);ba("</h1><p>");be(msg);ba("</p><p class=u>");be(url);ba("</p></body></html>");
 plen=bn_;}
static const char HOME[]=
"<html><head><title>MiniBrowser</title><style>"
"body{max-width:640px;margin:36px auto;color:#202124}h1{font-size:34px;margin-bottom:2px}"
".sub{color:#5f6368;margin-top:0}.note{background:#e8f0fe;border:1px solid #c6dafc;padding:10px 14px}"
"h2{font-size:20px;margin-top:28px}.k{color:#5f6368}</style></head><body>"
"<h1>MiniBrowser</h1><p class=sub>v" MB_VERSION " &middot; powered by MiniWebEngine v" MWE_VERSION "</p>"
"<p class=note><b>MiniBrowser is not MiniWeb.</b> MiniWeb is the browser built into the MiniWin operating system. "
"MiniBrowser is a separate, standalone desktop browser that is built on MiniWebEngine.</p>"
"<h2>Get started</h2><ul>"
"<li>Type an address such as http://example.com/ in the bar above and press Enter.</li>"
"<li>Open a local page with file:///C:/path/to/page.html or a plain path.</li>"
"<li>Ctrl+L focuses the address bar; Alt+Left / Alt+Right go back and forward; F5 reloads.</li></ul>"
"<h2>Supported</h2><p class=k>HTML5 elements, CSS3 selectors, @media, var(), calc(), flexbox, grid, tables, floats, "
"positioning, gradients, rounded corners, shadows, forms (display only), external stylesheets, a JavaScript subset, images (PNG, JPEG, GIF, BMP), "
"Latin, Greek, Cyrillic, Chinese and Korean text in Noto Sans, HTTPS-first with a connection panel (TLS 1.3 and 1.2, "
"verified certificates; no way to bypass an error).</p>"
"<h2>Not yet</h2><p class=k>Web fonts, progressive JPEG, WebP/SVG, animated GIFs (first frame only), legacy text encodings, Traditional-only Chinese characters, and interactive forms. "
"Pages load synchronously.</p>"
"<p><a href=\"about:blank\">about:blank</a> &middot; <a href=\"about:roots\">about:roots</a> &middot; <a href=\"about:licenses\">about:licenses</a> (copyright notices)</p></body></html>";
static int rdfiled(const char*url,char*dst,int cap,int*olen,int page){
 char p[UC];FILE*f;int n;
 fpath(url,p,sizeof p);
 if(!(f=fopen(p,"rb"))){if(page)errpage("File not found","The file could not be opened.",url);return 0;}
 n=(int)fread(dst,1,(size_t)cap-1,f);fclose(f);*olen=n;dst[n]=0;return 1;}
static int hdr(const char*h,int hl,const char*name,char*out,int cap){
 int i=0,n=len(name),o;
 while(i<hl){
  if(has(h+i,name)&&(i==0||h[i-1]=='\n')){
   i+=n;while(i<hl&&(h[i]==' '||h[i]=='\t'))i++;
   for(o=0;i<hl&&h[i]!='\r'&&h[i]!='\n'&&o<cap-1;i++)out[o++]=h[i];
   out[o]=0;return 1;}
  while(i<hl&&h[i]!='\n')i++;
  i++;}
 return 0;}
static int dechunk(char*b,int n){ /* in place */
 int i=0,o=0,sz,k;
 for(;;){
  sz=0;k=0;
  while(i<n&&hexv(b[i])>=0){sz=sz*16+hexv(b[i]);i++;k++;}
  while(i<n&&b[i]!='\n')i++;
  i++;
  if(!k||sz==0||i>=n)break;
  if(i+sz>n)sz=n-i;
  for(k=0;k<sz;k++)b[o++]=b[i+k];
  i+=sz;
  if(i<n&&b[i]=='\r')i++;
  if(i<n&&b[i]=='\n')i++;}
 return o;}
static int netget(const char*host,int port,int tls,const char*rq,int rl,char*err,int ecap){
 int n=0,r,h;
 if(tls){
  netfail=0;
  if(mb_tls_open(host,port)!=0){netfail=mb_tls_net_failed();cpy(err,ecap,mb_tls_error());return -1;}
  if(!mb_tls_send(rq,rl)){mb_tls_close();cpy(err,ecap,"Could not send the request.");return -1;}
  while(n<(int)sizeof raw-1){r=mb_tls_recv(raw+n,(int)sizeof raw-1-n);if(r<=0)break;n+=r;}
  mb_tls_close();
  return n>0?n:-1;}
 h=mb_plat_open(host,port);
 if(h<0){cpy(err,ecap,"Could not connect to the server.");return -1;}
 for(r=0;r<rl;){int k=mb_plat_send(h,rq+r,rl-r);if(k<=0){mb_plat_close(h);cpy(err,ecap,"Could not send the request.");return -1;}r+=k;}
 while(n<(int)sizeof raw-1){r=mb_plat_recv(h,raw+n,(int)sizeof raw-1-n);if(r<=0)break;n+=r;}
 mb_plat_close(h);
 return n>0?n:-1;}
/* ---- HSTS: once a host sent Strict-Transport-Security over verified HTTPS, plain http:// to it is rewritten ---- */
static int hostof(const char*u,char*h,int cap,int*port,const char**rest){
 const char*p=u+(has(u,"https://")?8:7);int i=0;
 while(*p&&*p!='/'&&*p!=':'&&*p!='?'&&*p!='#'&&i<cap-1){h[i]=(*p>='A'&&*p<='Z')?*p+32:*p;i++;p++;}
 h[i]=0;*port=0;
 if(*p==':'){p++;while(dig(*p)){*port=*port*10+*p-'0';p++;}}
 *rest=p;return i>0;}
static int cmpi(const char*a,const char*b){while(*a&&lw(*a)==lw(*b)){a++;b++;}return lw(*a)-lw(*b);}
static const char*findci(const char*s,const char*p){int n=len(p),i;for(;*s;s++){for(i=0;i<n&&s[i]&&lw(s[i])==lw(p[i]);i++){}if(i==n)return s;}return 0;}
static int hsts_match(const char*h){
 int i,hl=len(h),l;
 for(i=0;i<nhsts;i++){
  if(!cmpi(h,hsts[i]))return 1;
  l=len(hsts[i]);
  if(hsub[i]&&hl>l+1&&h[hl-l-1]=='.'&&!cmpi(h+hl-l,hsts[i]))return 1;}
 return 0;}
static void hsts_upgrade(char*url){
 char h[128],t[UC];const char*rest;int port;
 if(!has(url,"http://")||!hostof(url,h,sizeof h,&port,&rest)||!hsts_match(h)||(port&&port!=80))return;
 bs(t,sizeof t);ba("https://");ba(h);ba(rest);cpy(url,UC,t);}
static void hsts_note(const char*host,const char*val){
 const char*m=findci(val,"max-age=");int age=0,i,k;char h[96];
 if(!m)return;
 m+=8;while(dig(*m)){if(age<100000000)age=age*10+*m-'0';m++;}
 cpy(h,sizeof h,host);for(k=0;h[k];k++)if(h[k]>='A'&&h[k]<='Z')h[k]+=32;
 for(i=0;i<nhsts;i++)if(!cmpi(hsts[i],h))break;
 if(!age){if(i<nhsts){for(;i+1<nhsts;i++){cpy(hsts[i],96,hsts[i+1]);hsub[i]=hsub[i+1];}nhsts--;}return;}
 if(i==nhsts){if(nhsts>=16)return;nhsts++;cpy(hsts[i],96,h);}
 hsub[i]=findci(val,"includesubdomains")!=0;}
/* HTTPS-first applies to ordinary public host names only (not localhost, IP literals, single-label or .local names, explicit ports) */
static int upgradable(const char*u){
 char h[128];const char*rest;int port,i,dot=0;
 if(!hostof(u,h,sizeof h,&port,&rest)||port)return 0;
 if(h[0]=='['||(h[0]>='0'&&h[0]<='9'))return 0;
 if(!cmpi(h,"localhost"))return 0;
 for(i=0;h[i];i++)if(h[i]=='.')dot++;
 if(!dot)return 0;
 i=len(h);if(i>6&&!cmpi(h+i-6,".local"))return 0;
 return 1;}
#define EP(a,b,c) do{if(page&&!quiet)errpage(a,b,c);}while(0)
static int httpd(char*url,char*dst,int cap,int*olen,int page){
 int rd,n,i,port,st,he,bl,hl,tls;char host[256],path[UC],loc[UC],tc[64],ct[96],err[256];const char*p;
 for(rd=0;rd<6;rd++){
  hsts_upgrade(url);
  if(!page&&sec==2&&!has(url,"https://"))return 0;   /* mixed content: a secure page may not pull scripts/styles over http */
  tls=has(url,"https://");
  p=url+(tls?8:7);i=0;port=tls?443:80;
  while(*p&&*p!='/'&&*p!=':'&&*p!='?'&&*p!='#'&&i<255)host[i++]=*p++;
  host[i]=0;
  if(*p==':'){p++;port=0;while(dig(*p)){port=port*10+*p-'0';p++;}}
  if(!host[0]||port<=0||port>65535){EP("Invalid address","The address has no valid host name or port.",url);return 0;}
  bs(path,sizeof path);
  if(*p!='/')ba("/");
  while(*p&&*p!='#')bn(p++,1);
  bs(req,sizeof req);
  ba("GET ");ba(path);ba(" HTTP/1.1\r\nHost: ");ba(host);
  if((tls&&port!=443)||(!tls&&port!=80)){char pb[8];int q=port,k=0,j;char t[8];while(q){t[k++]='0'+q%10;q/=10;}for(j=0;j<k;j++)pb[j]=t[k-1-j];pb[k]=0;ba(":");ba(pb);}
  ba("\r\nUser-Agent: MiniBrowser/" MB_VERSION " MiniWebEngine/" MWE_VERSION "\r\nAccept: text/html,text/plain;q=0.9,*/*;q=0.5\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n");
  err[0]=0;
  n=netget(host,port,tls,req,bn_,err,sizeof err);
  if(n<=0){EP(tls?"Secure connection failed":"Could not connect",err[0]?err:"The server did not respond. Check the address and your connection.",url);return 0;}
  raw[n]=0;
  for(he=0;he+3<n;he++)if(raw[he]=='\r'&&raw[he+1]=='\n'&&raw[he+2]=='\r'&&raw[he+3]=='\n')break;
  if(he+3>=n||!has(raw,"HTTP/")){EP("Bad response","The server sent something that is not HTTP.",url);return 0;}
  hl=he+2;
  if(tls&&hdr(raw,hl,"strict-transport-security:",tc,sizeof tc))hsts_note(host,tc);
  st=0;for(i=0;raw[i]&&raw[i]!=' ';i++);
  while(raw[i]==' ')i++;
  while(dig(raw[i]))st=st*10+raw[i++]-'0';
  if(st>=300&&st<400&&hdr(raw,hl,"location:",loc,sizeof loc)&&loc[0]){resolve(url,loc,tmp,UC);cpy(url,UC,tmp);if(has(url,"http"))continue;EP("Unsupported redirect","The server redirected to an unsupported address.",url);return 0;}
  p=raw+he+4;bl=n-he-4;
  if(hdr(raw,hl,"transfer-encoding:",tc,sizeof tc)&&has(tc,"chunked"))bl=dechunk((char*)p,bl);
  if(hdr(raw,hl,"content-encoding:",tc,sizeof tc)&&tc[0]&&!has(tc,"identity")){
   EP("Unsupported content encoding","The server compressed the page, but this browser asked for an uncompressed one and cannot decode it.",url);return 0;}
  if(page&&hdr(raw,hl,"content-type:",ct,sizeof ct)&&!has(ct,"text/")&&!has(ct,"application/xhtml")){
   EP("Unsupported content","MiniBrowser can only display HTML and plain text.",url);return 0;}
  if(bl>cap-1)bl=cap-1;
  for(i=0;i<bl;i++)dst[i]=p[i];
  dst[bl]=0;*olen=bl;if(page){sec=tls?2:1;if(tls){pinfo=*mb_tls_info();cpy(phost,sizeof phost,host);}}
  return 1;}
 EP("Too many redirects","The server redirected too many times.",url);return 0;}

#undef EP
static int http(char*url){return httpd(url,pg,PG,&plen,1);}
