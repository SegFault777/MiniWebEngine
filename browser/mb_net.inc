/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniBrowser module: mb_net  (new in v0.10)
 * Asynchronous HTTP(S) client: request state machine, priority queue, redirects, timeouts, size limits,
 * duplicate-request merging, a small in-memory cache, cancellation. Nothing here ever waits: net_pump() is
 * called from mb_tick() and advances every request a little (within a time budget).
 *
 *   QUEUED -> CONNECTING -> (TLS_HANDSHAKE) -> SENDING -> RECEIVING_HEADERS -> RECEIVING_BODY -> COMPLETE
 *                                                                              (or FAILED / CANCELLED)
 * In the code: queue entries (Qe) are QUEUED; a request that got a slot (Rq) is RS_CONNECTING (plain TCP) or
 * RS_TLS (TCP + handshake), then RS_SENDING, RS_HEADERS, RS_BODY, RS_COMPLETE (with rq.failed set for FAILED);
 * cancelled requests are closed and their slot is freed at once. RS_DELIV marks a slot whose result is being handed over.
 * Not a standalone translation unit: minibrowser.c includes the modules in order (unity build). */

#define NRQ 6                 /* concurrent connections (= MB_NTLS) */
#define NQ 96                 /* queued requests */
#define RBUF (1<<21)          /* response buffer per connection: headers + body */
#define RQMAX 40000           /* request text per connection */
#define RBODY 32768           /* largest request body a script may send */
#ifdef MB_FAST_TIMEOUTS       /* tests only */
#define T_CONN 2000
#define T_HDR 3000
#define T_STALL 2000
#define T_TOTAL 8000
#define T_TOTAL_SUB 6000
#else
#define T_CONN 12000          /* ms: DNS + TCP connect + TLS handshake */
#define T_HDR 20000           /* ms: from sending the request to the end of the response headers */
#define T_STALL 15000         /* ms: no byte arrived while the body was coming */
#define T_TOTAL 60000         /* ms: whole request (pages); subresources get 45000 */
#define T_TOTAL_SUB 45000
#endif
#define MAXREDIR 5
#define TICKBUDGET 10         /* ms of network work per mb_tick */

enum{RS_FREE,RS_CONNECTING,RS_TLS,RS_SENDING,RS_HEADERS,RS_BODY,RS_COMPLETE,RS_DELIV};
enum{K_PAGE,K_CSS,K_JS,K_IMG,K_FETCH,K_JSA};
typedef struct{int kind,tag;}Wt;
typedef struct{
 int used,prio,seq,nw,bo,bl,upgrade;Wt w[4];
 char m[8],url[UC],fb[UC],xh[2048];
}Qe;
typedef struct{
 int st,failed,nw,redir,port,tls,h,ctx,status,netfail,upgrade,clen,chunked,cpos,he,n,sent,rl,bl,nocache,ttl,haveti,prio;
 Wt w[4];
 char m[8],url[UC],host[256],fb[UC],err[200],head[80],xh[2048];
 int pbl;char*bp;
 long long t0,tst,tlast;
 mb_tls_info_t ti;
}Rq;
static Rq rqs[NRQ];
static Qe qs[NQ];
static int qseq,nstarted_,ncancel_,ndone_,nfail_,nredir_,ncache_;
static char rbuf[NRQ][RBUF],rqt[NRQ][RQMAX],pbod[NRQ][RBODY];
static char qbp[1<<16];static int qbn;
static int imgbudget;

static int capof(int k){return k==K_PAGE?PG-1:k==K_CSS?(1<<18):k==K_JS||k==K_JSA?(1<<20):(RBUF-(1<<16));}
static int tot_ms(int k){return k==K_PAGE?T_TOTAL:T_TOTAL_SUB;}
static int strcmp_(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a-*b;}
static int memeq(const char*a,const char*b,int n){int i;for(i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
static void ba_int(long long v){char t[24];int k=0;if(v<0){ba("-");v=-v;}if(!v)t[k++]='0';while(v){t[k++]=(char)('0'+v%10);v/=10;}while(k)bn(&t[--k],1);}
static void* mcopy(void*d,const void*s,int n){char*a=(char*)d;const char*b=(const char*)s;int i;if(a<b){for(i=0;i<n;i++)a[i]=b[i];}else for(i=n-1;i>=0;i--)a[i]=b[i];return d;}

/* ---- cache: successful GET responses of stylesheets, scripts and images (not pages, not script requests) ---- */
#define CE 160
typedef struct{unsigned h;int off,ul,bl;long long exp;}Ce;
static Ce ce[CE];static int nce,cpn;static char cpool[1<<23];
static unsigned hash32(const char*s){unsigned h=2166136261u;while(*s){h^=(unsigned char)*s++;h*=16777619u;}return h;}
static void cache_flush(void){nce=0;cpn=0;}
static int cache_get(const char*url,char*dst,int cap,int*bl){
 unsigned h=hash32(url);int i,ul=len(url)+1;long long now=mb_plat_ms();
 for(i=0;i<nce;i++){
  if(ce[i].h!=h||ce[i].ul!=ul||!memeq(cpool+ce[i].off,url,ul))continue;
  if(ce[i].exp<now||ce[i].bl>=cap)return 0;
  mcopy(dst,cpool+ce[i].off+ul,ce[i].bl);dst[ce[i].bl]=0;*bl=ce[i].bl;return 1;}
 return 0;}
static void cache_put(const char*url,const char*b,int bl,int ttl){
 int ul=len(url)+1;
 if(bl>(1<<20)||ttl<=0)return;
 if(cpn+ul+bl>(int)sizeof cpool||nce>=CE)cache_flush();
 ce[nce].h=hash32(url);ce[nce].off=cpn;ce[nce].ul=ul;ce[nce].bl=bl;ce[nce].exp=mb_plat_ms()+(long long)ttl*1000;
 mcopy(cpool+cpn,url,ul);mcopy(cpool+cpn+ul,b,bl);cpn+=ul+bl;nce++;}

/* ---- connection helpers ---- */
static void rq_close(Rq*r){
 if(r->tls){if(r->ctx>=0)mb_tls_end(r->ctx);r->ctx=-1;}
 else if(r->h>=0){mb_plat_close(r->h);r->h=-1;}}
static void rq_free(Rq*r){rq_close(r);r->st=RS_FREE;r->nw=0;}
static void rq_fail(Rq*r,const char*head,const char*msg){
 cpy(r->head,sizeof r->head,head);cpy(r->err,sizeof r->err,msg);
 rq_close(r);r->failed=1;r->st=RS_COMPLETE;nfail_++;}
static void net_cancel_all(void){
 int i;
 for(i=0;i<NRQ;i++)if(rqs[i].st!=RS_FREE&&rqs[i].st!=RS_DELIV){rq_free(&rqs[i]);ncancel_++;}
 for(i=0;i<NQ;i++)qs[i].used=0;
 qbn=0;}
static int net_busy(void){
 int i,n=0;
 for(i=0;i<NRQ;i++)if(rqs[i].st!=RS_FREE)n++;
 for(i=0;i<NQ;i++)if(qs[i].used)n++;
 return n;}
static int net_fetches(void){   /* requests a script is waiting for */
 int i,k,n=0;
 for(i=0;i<NRQ;i++)if(rqs[i].st!=RS_FREE)for(k=0;k<rqs[i].nw;k++)if(rqs[i].w[k].kind==K_FETCH)n++;
 for(i=0;i<NQ;i++)if(qs[i].used)for(k=0;k<qs[i].nw;k++)if(qs[i].w[k].kind==K_FETCH)n++;
 return n;}

/* ---- starting a request: URL -> host/port, HSTS, mixed content, request text, socket ---- */
static int rq_build(Rq*r,int si){
 char*q=rqt[si];int i;const char*s;
 bs(q,RQMAX);
 ba(r->m);ba(" ");
 {const char*p=r->url+(r->tls?8:7);while(*p&&*p!='/'&&*p!='?'&&*p!='#')p++;   /* skip authority */
  if(*p!='/')ba("/");
  while(*p&&*p!='#')bn(p++,1);}
 ba(" HTTP/1.1\r\nHost: ");ba(r->host);
 if((r->tls&&r->port!=443)||(!r->tls&&r->port!=80)){ba(":");ba_int(r->port);}
 ba("\r\nUser-Agent: MiniBrowser/" MB_VERSION " MiniWebEngine/" MWE_VERSION "\r\nAccept: ");
 ba(r->w[0].kind==K_PAGE?"text/html,application/xhtml+xml;q=0.9,text/plain;q=0.8,*/*;q=0.5":r->w[0].kind==K_CSS?"text/css,*/*;q=0.1":r->w[0].kind==K_IMG?"image/png,image/jpeg,image/gif,image/bmp,*/*;q=0.5":"*/*");
 ba("\r\nAccept-Encoding: identity\r\nConnection: close\r\n");
 s=r->xh;
 while(*s){   /* extra request headers from a script, one "name: value" per line (never Host/Content-Length/Connection) */
  const char*e=s;
  while(*e&&*e!='\n')e++;
  if(e>s&&!has(s,"host:")&&!has(s,"content-length:")&&!has(s,"connection:")&&!has(s,"accept-encoding:")){bn(s,(int)(e-s));ba("\r\n");}
  s=*e?e+1:e;}
 if(r->pbl>0){ba("Content-Length: ");ba_int(r->pbl);ba("\r\n");}
 ba("\r\n");
 if(bn_+r->pbl>=RQMAX-1)return 0;
 for(i=0;i<r->pbl;i++)q[bn_++]=pbod[si][i];
 r->rl=bn_;return 1;}
/* connect (or restart after a redirect / https->http fallback) using r->url; on trouble the request is marked failed */
static void rq_begin(Rq*r){
 int si=(int)(r-rqs),i,port;const char*p;
 r->he=r->n=r->sent=r->cpos=r->bl=r->haveti=0;r->clen=-1;r->chunked=0;r->status=0;r->netfail=0;r->h=-1;r->ctx=-1;r->failed=0;
 r->tst=r->tlast=mb_plat_ms();
 hsts_upgrade(r->url);
 if(has(r->url,"file:")){
  int l=0;r->tls=0;
  if(!rdfiled(r->url,rbuf[si],RBUF,&l,0)){rq_fail(r,"File not found","The file could not be opened.");return;}
  r->bp=rbuf[si];r->bl=l;r->status=200;r->st=RS_COMPLETE;return;}
 if(!has(r->url,"http")){rq_fail(r,"Unsupported address","This address type is not supported.");return;}
 if(r->w[0].kind!=K_PAGE&&sec==2&&!has(r->url,"https://")){rq_fail(r,"Blocked","blocked as mixed content (an https page may not load http resources)");return;}
 r->tls=has(r->url,"https://");
 p=r->url+(r->tls?8:7);i=0;port=r->tls?443:80;
 while(*p&&*p!='/'&&*p!=':'&&*p!='?'&&*p!='#'&&i<255)r->host[i++]=*p++;
 r->host[i]=0;
 if(*p==':'){p++;port=0;while(dig(*p)){port=port*10+*p-'0';p++;}}
 if(!r->host[0]||port<=0||port>65535){rq_fail(r,"Invalid address","The address has no valid host name or port.");return;}
 r->port=port;
 /* cached stylesheets / scripts / images are served from memory */
 if(r->w[0].kind!=K_PAGE&&r->w[0].kind!=K_FETCH&&!cmpi(r->m,"GET")){
  if(cache_get(r->url,rbuf[si],RBUF,&i)){r->bp=rbuf[si];r->bl=i;r->status=200;r->st=RS_COMPLETE;ncache_++;return;}}
 if(!rq_build(r,si)){rq_fail(r,"Request too large","The request is too large for this browser.");return;}
 if(r->tls){
  r->ctx=mb_tls_begin(r->host,port);
  if(r->ctx<0){rq_fail(r,"Secure connection failed","Too many secure connections at once.");return;}
  r->st=RS_TLS;}
 else{
  r->h=mb_plat_connect(r->host,port);
  if(r->h<0){r->netfail=1;rq_fail(r,"Could not connect","Could not connect to the server.");return;}
  r->st=RS_CONNECTING;}
 nstarted_++;}

/* ---- response parsing ---- */
static int chunk_scan(const char*b,int n,int*pos){   /* walk complete chunks from *pos; 1 once the terminating 0-chunk was seen */
 int i=*pos,j,sz,k;
 for(;;){
  sz=0;k=0;j=i;
  while(j<n&&hexv(b[j])>=0){if(sz<(1<<27))sz=sz*16+hexv(b[j]);j++;k++;}
  if(!k)return j<n;                   /* garbage: stop; nothing yet: wait */
  while(j<n&&b[j]!='\n')j++;
  if(j>=n)return 0;
  j++;
  if(sz==0)return j<n&&(b[j]=='\n'||(b[j]=='\r'&&j+1<n));   /* the final CRLF (trailers are not supported) */
  if(j+sz+2>n)return 0;
  i=j+sz+2;*pos=i;}}
static int body_done(Rq*r){
 int si=(int)(r-rqs),bl=r->n-r->he-4;
 if(r->chunked)return chunk_scan(rbuf[si]+r->he+4,bl,&r->cpos);
 return r->clen>=0&&bl>=r->clen;}
static void rq_restart(Rq*r){rq_close(r);rq_begin(r);}
/* headers are complete. 0 = keep reading, 1 = response complete, 2 = restarted (redirect), -1 = failed */
static int rq_headers(Rq*r){
 int si=(int)(r-rqs),i,st=0,hl=r->he+2,kind=r->w[0].kind;char*b=rbuf[si];char tc[256],loc[UC],tmp2[UC];
 b[r->n]=0;
 if(!has(b,"HTTP/")){rq_fail(r,"Bad response","The server sent something that is not HTTP.");return -1;}
 for(i=0;b[i]&&b[i]!=' ';i++){}
 while(b[i]==' ')i++;
 while(dig(b[i]))st=st*10+b[i++]-'0';
 r->status=st;
 if(r->tls&&hdr(b,hl,"strict-transport-security:",tc,sizeof tc))hsts_note(r->host,tc);
 if(st>=300&&st<400&&hdr(b,hl,"location:",loc,sizeof loc)&&loc[0]){
  if(++r->redir>MAXREDIR){rq_fail(r,"Too many redirects","The server redirected too many times.");return -1;}
  if(st==301||st==302||st==303){cpy(r->m,sizeof r->m,"GET");r->pbl=0;}   /* these redirects turn a POST into a GET */
  resolve(r->url,loc,tmp2,UC);
  if(!has(tmp2,"http")){rq_fail(r,"Unsupported redirect","The server redirected to an unsupported address.");return -1;}
  cpy(r->url,UC,tmp2);nredir_++;
  rq_restart(r);return 2;}
 if(hdr(b,hl,"content-encoding:",tc,sizeof tc)&&tc[0]&&!has(tc,"identity")){
  rq_fail(r,"Unsupported content encoding","The server compressed the response, but this browser asked for an uncompressed one and cannot decode it.");return -1;}
 if(kind==K_PAGE&&hdr(b,hl,"content-type:",tc,sizeof tc)&&!has(tc,"text/")&&!has(tc,"application/xhtml")){
  rq_fail(r,"Unsupported content","MiniBrowser can only display HTML and plain text.");return -1;}
 if(kind!=K_PAGE&&kind!=K_FETCH&&(st<200||st>=300)){
  bs(tmp2,sizeof tmp2);ba("HTTP status ");ba_int(st);rq_fail(r,"Failed",tmp2);return -1;}
 if(hdr(b,hl,"transfer-encoding:",tc,sizeof tc)&&has(tc,"chunked"))r->chunked=1;
 else if(hdr(b,hl,"content-length:",tc,sizeof tc)){
  long long v=0;for(i=0;dig(tc[i]);i++){if(v<(1LL<<40))v=v*10+tc[i]-'0';}
  r->clen=v>(1LL<<30)?(1<<30):(int)v;
  if(r->clen>capof(kind)||r->clen>RBUF-1-(r->he+4)){
   bs(tmp2,sizeof tmp2);ba("The response is larger than the limit (");ba_int(r->clen/1024);ba(" KB > ");ba_int(capof(kind)/1024);ba(" KB).");
   rq_fail(r,"Response too large",tmp2);return -1;}}
 if(st==204||st==304||(st>=100&&st<200))r->clen=0;
 r->nocache=0;r->ttl=300;
 if(hdr(b,hl,"cache-control:",tc,sizeof tc)){
  const char*m=findci(tc,"max-age=");
  if(findci(tc,"no-store")||findci(tc,"no-cache")||findci(tc,"private"))r->nocache=1;
  if(m){int a=0;m+=8;while(dig(*m)){if(a<100000)a=a*10+*m-'0';m++;}r->ttl=a>3600?3600:a;}}
 return body_done(r)?1:0;}
/* everything has arrived: dechunk, enforce limits, remember it, close the connection */
static void rq_finish(Rq*r){
 int si=(int)(r-rqs),kind=r->w[0].kind,bl;char*b=rbuf[si];char tmp2[160];
 bl=r->n-r->he-4;
 if(r->chunked)bl=dechunk(b+r->he+4,bl);
 if(r->clen>=0&&bl>r->clen)bl=r->clen;
 if(bl>capof(kind)){bs(tmp2,sizeof tmp2);ba("The response is larger than the limit (");ba_int(capof(kind)/1024);ba(" KB).");rq_fail(r,"Response too large",tmp2);return;}
 b[r->he+4+bl]=0;
 r->bp=b+r->he+4;r->bl=bl;
 if(r->tls){r->ti=*mb_tls_info(r->ctx);r->haveti=1;}
 rq_close(r);
 if(r->status==200&&!r->nocache&&kind!=K_PAGE&&kind!=K_FETCH&&!cmpi(r->m,"GET"))cache_put(r->url,r->bp,bl,r->ttl);
 r->st=RS_COMPLETE;ndone_++;}

static int rq_recv(Rq*r,char*b,int n){   /* >0 bytes, 0 nothing yet, -1 error, -2 closed by the server */
 int k;
 if(r->tls)return mb_tls_read(r->ctx,b,n);
 k=mb_plat_recv(r->h,b,n);
 if(k==-2)return 0;
 if(k==0)return -2;
 return k>0?k:-1;}
/* ---- advancing one request; returns 1 if anything changed ---- */
static int rq_step(Rq*r,long long now){
 int si=(int)(r-rqs),k,prog=0,budget=65536;
 if(now-r->t0>tot_ms(r->w[0].kind)&&r->st<RS_COMPLETE){rq_fail(r,"Timed out","The request took too long.");return 1;}
 switch(r->st){
 case RS_CONNECTING:
  k=mb_plat_connect_poll(r->h);
  if(k>0){r->st=RS_SENDING;r->tst=now;return 1;}
  if(k<0){r->netfail=1;rq_fail(r,"Could not connect","Could not connect to the server.");return 1;}
  if(now-r->tst>T_CONN){r->netfail=1;rq_fail(r,"Could not connect","The server did not accept the connection in time.");return 1;}
  return 0;
 case RS_TLS:
  k=mb_tls_step(r->ctx);
  if(k>0){
   if(!mb_tls_write(r->ctx,rqt[si],r->rl)){rq_fail(r,"Secure connection failed","Could not send the request.");return 1;}
   r->st=RS_HEADERS;r->tst=r->tlast=now;return 1;}
  if(k<0){r->netfail=mb_tls_net_failed(r->ctx);cpy(r->err,sizeof r->err,mb_tls_error(r->ctx));cpy(r->head,sizeof r->head,"Secure connection failed");rq_close(r);r->failed=1;r->st=RS_COMPLETE;nfail_++;return 1;}
  if(now-r->tst>T_CONN){r->netfail=1;rq_fail(r,"Secure connection failed","The secure handshake did not finish in time.");return 1;}
  return 0;
 case RS_SENDING:
  while(r->sent<r->rl){
   k=mb_plat_send(r->h,rqt[si]+r->sent,r->rl-r->sent);
   if(k<0){rq_fail(r,"Could not connect","Could not send the request.");return 1;}
   if(k==0)break;
   r->sent+=k;prog=1;}
  if(r->sent>=r->rl){r->st=RS_HEADERS;r->tst=r->tlast=now;return 1;}
  if(now-r->tst>T_HDR){rq_fail(r,"Timed out","The server did not accept the request in time.");return 1;}
  return prog;
 case RS_HEADERS:case RS_BODY:{
  char*b=rbuf[si];
  while(budget>0&&r->st>=RS_HEADERS&&r->st<RS_COMPLETE){
   int room=RBUF-1-r->n,want;
   if(room<=0){rq_fail(r,"Response too large","The response is larger than this browser can hold.");return 1;}
   want=room>16384?16384:room;
   k=rq_recv(r,b+r->n,want);
   if(k==0)break;
   if(k==-1){rq_fail(r,r->tls?"Secure connection failed":"Connection lost",r->tls&&mb_tls_error(r->ctx)[0]?mb_tls_error(r->ctx):"The connection was interrupted.");return 1;}
   if(k==-2){   /* the server closed the connection */
    if(!r->he){rq_fail(r,"Could not connect",r->n?"The server sent something that is not HTTP.":"The server closed the connection without answering.");return 1;}
    {int have=r->n-r->he-4;
     if(!r->chunked&&r->clen>=0&&have<r->clen&&!(r->w[0].kind==K_PAGE&&have>0)){rq_fail(r,"Truncated","The response was cut off before it was complete.");return 1;}}
    rq_finish(r);return 1;}
   {int from=r->n>3?r->n-3:0;r->n+=k;budget-=k;r->tlast=now;prog=1;
    if(!r->he){
     int j;
     for(j=from;j+3<r->n;j++)if(b[j]=='\r'&&b[j+1]=='\n'&&b[j+2]=='\r'&&b[j+3]=='\n'){r->he=j;break;}
     if(r->he){
      int rc;r->st=RS_BODY;rc=rq_headers(r);
      if(rc==2||rc<0)return 1;
      if(rc==1){rq_finish(r);return 1;}}
     else if(r->n>65536){rq_fail(r,"Bad response","The response headers are too long.");return 1;}}
    else{
     int have=r->n-r->he-4;
     if(have>capof(r->w[0].kind)+(r->chunked?65536:0)){rq_fail(r,"Response too large","The response is larger than the limit.");return 1;}
     if(body_done(r)){rq_finish(r);return 1;}}}}
  if(r->st==RS_HEADERS||(r->st==RS_BODY&&!r->he)){if(now-r->tst>T_HDR){rq_fail(r,"Timed out","The server did not answer in time.");return 1;}}
  else if(r->st==RS_BODY&&now-r->tlast>T_STALL){rq_fail(r,"Timed out","The server stopped sending data.");return 1;}
  return prog;}
 default:return 0;}}

/* ---- handing results over ---- */
static char jsrh[2048];
static void js_headers(Rq*r){   /* "name: value\n" lines of the response, for scripts (r's buffer still holds them) */
 const char*b=rbuf[r-rqs];int a=0,o=0;
 while(a<r->he&&b[a]!='\n')a++;
 a++;
 while(a<r->he&&o<(int)sizeof jsrh-2){jsrh[o++]=b[a]=='\r'?'\n':b[a];a++;}
 jsrh[o]=0;}
static void page_ok(Rq*r);
static void page_fail(Rq*r);
static void css_got(int i,const char*b,int bl);
static void js_got(int kind,int i,const char*b,int bl);
static void img_got(int i,const char*b,int bl);
static void res_failed(int kind,int tag);
/* returns 1 if the page changed. jsonly: hand over script requests only (used while a script awaits) */
static int rq_deliver(Rq*r,int jsonly,int*progress){
 int k,ch=0;
 if(r->failed){
  if(r->w[0].kind==K_PAGE){if(jsonly)return 0;r->st=RS_DELIV;page_fail(r);if(r->st==RS_DELIV)rq_free(r);*progress=1;return 1;}
  r->st=RS_DELIV;
  for(k=0;k<r->nw;k++){
   if(r->w[k].kind==K_FETCH)ch|=mwe_js_fetch_done(r->w[k].tag,0,"","",0,r->err[0]?r->err:"network error","");
   else res_failed(r->w[k].kind,r->w[k].tag);}
  rq_free(r);*progress=1;return ch;}
 if(jsonly){
  for(k=0;k<r->nw;k++)if(r->w[k].kind!=K_FETCH)return 0;}
 if(r->w[0].kind==K_IMG){if(imgbudget<=0)return 0;imgbudget--;}   /* decoding is slow: one image per tick */
 r->st=RS_DELIV;*progress=1;
 for(k=0;k<r->nw;k++){
  Wt w=r->w[k];
  switch(w.kind){
  case K_PAGE:page_ok(r);ch=1;break;
  case K_CSS:css_got(w.tag,r->bp,r->bl);ch=1;break;
  case K_JS:case K_JSA:js_got(w.kind,w.tag,r->bp,r->bl);ch=1;break;
  case K_IMG:img_got(w.tag,r->bp,r->bl);ch=1;break;
  case K_FETCH:
   js_headers(r);
   ch|=mwe_js_fetch_done(w.tag,r->status,jsrh,r->bp,r->bl,"",r->url);break;}}
 if(r->st==RS_DELIV)rq_free(r);
 return ch;}

/* ---- queue ---- */
static void net_enqueue(int kind,int tag,int prio,const char*url,const char*method,const char*xh,const char*body,int blen,int upgrade,const char*fb){
 int i,e=-1;
 if(!cmpi(method,"GET")&&kind!=K_PAGE&&kind!=K_FETCH){   /* merge identical requests */
  for(i=0;i<NRQ;i++)if(rqs[i].st!=RS_FREE&&rqs[i].st!=RS_DELIV&&rqs[i].w[0].kind==kind&&rqs[i].nw<4&&!cmpi(rqs[i].m,"GET")&&!cmpi(rqs[i].url,url)){rqs[i].w[rqs[i].nw].kind=kind;rqs[i].w[rqs[i].nw++].tag=tag;return;}
  for(i=0;i<NQ;i++)if(qs[i].used&&qs[i].w[0].kind==kind&&qs[i].nw<4&&!cmpi(qs[i].m,"GET")&&!cmpi(qs[i].url,url)){qs[i].w[qs[i].nw].kind=kind;qs[i].w[qs[i].nw++].tag=tag;if(prio<qs[i].prio)qs[i].prio=prio;return;}}
 for(i=0;i<NQ;i++)if(!qs[i].used){e=i;break;}
 if(e<0){
  if(kind==K_FETCH)mwe_js_fetch_done(tag,0,"","",0,"too many requests","");
  else res_failed(kind,tag);
  return;}
 qs[e].used=1;qs[e].prio=prio;qs[e].seq=qseq++;qs[e].nw=1;qs[e].w[0].kind=kind;qs[e].w[0].tag=tag;qs[e].upgrade=upgrade;
 cpy(qs[e].m,sizeof qs[e].m,method);cpy(qs[e].url,UC,url);cpy(qs[e].fb,UC,fb?fb:"");cpy(qs[e].xh,sizeof qs[e].xh,xh?xh:"");
 qs[e].bo=-1;qs[e].bl=0;
 if(blen>0){
  if(blen>RBODY||qbn+blen>(int)sizeof qbp){qs[e].used=0;if(kind==K_FETCH)mwe_js_fetch_done(tag,0,"","",0,"request body too large","");return;}
  mcopy(qbp+qbn,body,blen);qs[e].bo=qbn;qs[e].bl=blen;qbn+=blen;}}
static void net_start_queued(int maxstart){
 int s,i,best;
 for(;maxstart>0;maxstart--){
  for(s=0;s<NRQ&&rqs[s].st!=RS_FREE;s++){}
  if(s==NRQ)return;
  best=-1;
  for(i=0;i<NQ;i++)if(qs[i].used&&(best<0||qs[i].prio<qs[best].prio||(qs[i].prio==qs[best].prio&&qs[i].seq<qs[best].seq)))best=i;
  if(best<0){qbn=0;return;}
  {Rq*r=&rqs[s];Qe*q=&qs[best];int k;
   r->nw=q->nw;for(k=0;k<q->nw;k++)r->w[k]=q->w[k];
   r->prio=q->prio;r->redir=0;r->upgrade=q->upgrade;
   cpy(r->m,sizeof r->m,q->m);cpy(r->url,UC,q->url);cpy(r->fb,UC,q->fb);cpy(r->xh,sizeof r->xh,q->xh);
   r->pbl=q->bl;if(q->bl>0)mcopy(pbod[s],qbp+q->bo,q->bl);
   q->used=0;
   r->t0=mb_plat_ms();r->err[0]=0;r->head[0]=0;
   rq_begin(r);}}}

/* advance everything within a time budget; jsonly: only script requests are handed over (see mwe_js_set_poll).
 * Returns 1 if anything happened. */
static int net_pump(int jsonly){
 static int rot;
 int i,j,prog=0,ch=0;long long t0=mb_plat_ms(),now;
 net_start_queued(2);
 for(j=0;j<NRQ;j++){
  Rq*r;
  i=(rot+j)%NRQ;r=&rqs[i];
  if(r->st==RS_FREE||r->st==RS_DELIV)continue;
  now=mb_plat_ms();
  if(r->st<RS_COMPLETE&&(r->st!=RS_TLS||now-t0<=TICKBUDGET)&&rq_step(r,now))prog=1;
  if(r->st==RS_COMPLETE){
   if(rq_deliver(r,jsonly,&prog))ch=1;}
  if(!jsonly&&mb_plat_ms()-t0>TICKBUDGET)break;}
 rot=(rot+1)%NRQ;
 net_start_queued(2);
 if(ch)rdirty_=1;
 return prog||ch;}
