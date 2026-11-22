/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniBrowser module: mb_nav
 * navigation (v0.10: asynchronous): page commit, resource discovery and ordering, history, mb_tick.
 * Order of events for a page: the HTML request goes first (priority 0); when it arrives the page is parsed and shown at
 * once; then stylesheets (1), scripts (2/3), visible images (4), the rest (5) and async scripts (6) are requested, and
 * each result is applied as it arrives (stylesheets in document order, then all scripts run together, images re-layout
 * the page, at most once every 30 ms).
 * Not a standalone translation unit: minibrowser.c includes the modules in order (unity build). */
static void relayout(void){
 int cw=Wd-SBW,chh=Ht-TB-SBH,mx;
 if(cw<32)cw=32;
 mwe_viewport_h(chh>100?chh:100);
 ph=mwe_layout(cw);
 mx=ph-chh;if(mx<0)mx=0;
 if(want_sy>=0){sy=want_sy;if(sy<=mx)want_sy=-1;}   /* back/forward: keep trying to restore the scroll position while the page grows */
 if(sy>mx)sy=mx;
 if(sy<0)sy=0;}
static void settitle(void){
 static char lt[256];const char*t;int n,i,same=1;
 t=mwe_title(&n);
 if(!(t&&n>0)){t=cur;n=len(cur);}
 if(n>255)n=255;
 for(i=0;i<n;i++)if(lt[i]!=t[i])same=0;
 if(same&&!lt[n])return;
 for(i=0;i<n;i++)lt[i]=t[i];
 lt[n]=0;
 mb_plat_title(t,n);}
static void go(const char*in,int push);
static void finish_commit(const char*url,int secv,const mb_tls_info_t*ti,const char*host);

/* ---- per-page resource bookkeeping ---- */
static char csspool[1<<18],jspool[1<<20];
static int cssn,cssst[8],cssoff[8],csslen[8],cssapp,cssused;     /* st: 1 pending, 2 arrived, 3 failed/skipped */
static int jsn,jsst[16],jsoff[16],jslen[16],jsasyn[16],jsdone[16],jsran,jsused;
static unsigned char imreq[64];
static int scansy=-1;
static void res_reset(void){
 int i;
 cssn=jsn=cssapp=cssused=jsused=jsran=0;scansy=-1;
 for(i=0;i<8;i++)cssst[i]=0;
 for(i=0;i<16;i++){jsst[i]=0;jsasyn[i]=0;jsdone[i]=0;}
 for(i=0;i<64;i++)imreq[i]=0;}
static void css_got(int i,const char*b,int bl){
 if(i<0||i>=8||cssst[i]!=1)return;
 if(cssused+bl+1>(int)sizeof csspool){cssst[i]=3;return;}
 mcopy(csspool+cssused,b,bl);csspool[cssused+bl]=0;
 cssoff[i]=cssused;csslen[i]=bl;cssused+=bl+1;cssst[i]=2;}
static void js_got(int kind,int i,const char*b,int bl){
 (void)kind;
 if(i<0||i>=16||jsst[i]!=1)return;
 if(jsused+bl+1>(int)sizeof jspool){jsst[i]=3;return;}
 mcopy(jspool+jsused,b,bl);jspool[jsused+bl]=0;
 jsoff[i]=jsused;jslen[i]=bl;jsused+=bl+1;jsst[i]=2;}
static void img_got(int i,const char*b,int bl){
 if(i<0||i>=64)return;
 if(mwe_image_state(i)==0&&mwe_image_set(i,(const unsigned char*)b,bl)){}
 else if(mwe_image_state(i)==0)mwe_image_fail(i);}
static void res_failed(int kind,int tag){
 if(kind==K_CSS&&tag>=0&&tag<8&&cssst[tag]==1)cssst[tag]=3;
 else if((kind==K_JS||kind==K_JSA)&&tag>=0&&tag<16&&jsst[tag]==1)jsst[tag]=3;
 else if(kind==K_IMG&&tag>=0&&tag<64&&mwe_image_state(tag)==0)mwe_image_fail(tag);}
static int copyurl(const char*h,int n,char*full){
 char u[UC];int i;
 if(!h||n<=0||n>=UC)return 0;
 for(i=0;i<n;i++)u[i]=h[i];
 u[n]=0;resolve(cur,u,full,UC);
 return has(full,"file:")||has(full,"http");}
static void res_discover(void){   /* stylesheets and scripts named in the document (images: img_scan) */
 int i,n;const char*h;char full[UC];
 for(i=0;i<8;i++){
  h=mwe_stylesheet(i,&n);
  if(!h)break;
  cssn=i+1;
  if(!copyurl(h,n,full)){cssst[i]=3;continue;}
  cssst[i]=1;net_enqueue(K_CSS,i,1,full,"GET",0,0,0,0,0);}
 for(i=0;i<16;i++){
  h=mwe_script(i,&n);
  if(!h)break;
  jsn=i+1;jsasyn[i]=mwe_script_async(i);
  if(!copyurl(h,n,full)){jsst[i]=3;continue;}
  jsst[i]=1;net_enqueue(jsasyn[i]?K_JSA:K_JS,i,jsasyn[i]?6:3,full,"GET",0,0,0,0,0);}}
static void img_scan(void){   /* request images that have a box (visible ones first); hidden ones wait until they are shown */
 int i,n,cnt=mwe_image_count(),chh=Ht-TB-SBH,y,pr;const char*h;char full[UC];
 if(!ready)return;
 for(i=0;i<cnt&&i<64;i++){
  if(imreq[i]||mwe_image_state(i)!=0)continue;
  y=mwe_image_y(i);
  if(y<0)continue;
  if(y>sy+chh*2+200)continue;                 /* far below the fold: lazy */
  pr=y<=sy+chh+100?4:5;
  h=mwe_image_src(i,&n);
  imreq[i]=1;
  if(!copyurl(h,n,full)){mwe_image_fail(i);continue;}
  net_enqueue(K_IMG,i,pr,full,"GET",0,0,0,0,0);}}
/* apply what has arrived; returns 1 if the page changed */
static int res_settle(void){
 int i,ch=0,ok;
 if(!ready)return 0;
 while(cssapp<cssn&&cssst[cssapp]>=2){
  if(cssst[cssapp]==2)mwe_add_css(csspool+cssoff[cssapp],csslen[cssapp]);
  cssapp++;ch=1;}
 if(!jsran&&cssapp>=cssn){
  ok=1;
  for(i=0;i<jsn;i++)if(!jsasyn[i]&&jsst[i]==1)ok=0;
  if(ok){
   for(i=0;i<jsn;i++)if(!jsasyn[i]&&jsst[i]==2)mwe_add_js(i,jspool+jsoff[i],jslen[i]);
   mwe_js_time(mb_plat_time()*1000LL,mb_plat_ms());mwe_js_url(cur);
   jsran=1;mwe_js_run();ch=1;}}
 if(jsran)for(i=0;i<jsn;i++)if(jsasyn[i]&&jsst[i]==2&&!jsdone[i]){jsdone[i]=1;mwe_js_eval(jspool+jsoff[i],jslen[i]);ch=1;}
 return ch;}
static int res_pending(void){   /* anything that still blocks "Done" */
 int i;
 if(net_busy())return 1;
 if(ready&&!jsran&&cssapp<cssn)return 1;
 for(i=0;i<jsn;i++)if(!jsasyn[i]&&jsst[i]==1)return 1;
 return 0;}


static void start_nav(char*url){
 char su[UC];int up=0;
 loading=1;tnav=mb_plat_ms();tcommit=0;tsettle=0;
 cpy(ub,UC,url);ucur=len(ub);usel=0;uoff=0;
 if(has(url,"about:")){
  if(has(url,"about:licenses")){
   int ln;
   cpy(pg,PG,"<html><head><meta charset=\"utf-8\"><title>Licenses</title><style>body{margin:24px;max-width:760px}pre{white-space:pre-wrap}</style></head><body><h1>Licenses</h1><pre>");
   ln=len(pg);cpy(pg+ln,PG-ln,mwe_licenses());
   ln=len(pg);cpy(pg+ln,PG-ln,"</pre><p>Full licence texts: fonts/OFL-NotoSansCJK.txt and NOTICE.md in the source distribution.</p></body></html>");}
  else if(has(url,"about:roots")){   /* every trusted root, its tier and SHA-256 */
   int rn=mb_root_count(),i,lg,k,cn=0,ln;const char*sha,*nm;char num[12];
   for(i=0;i<rn;i++){mb_root_name(i,&lg,&sha);cn+=lg;}
   bs(pg,PG);
   ba("<html><head><meta charset=\"utf-8\"><title>Trusted roots</title><style>body{margin:24px;max-width:960px}td,th{padding:2px 12px 2px 0;text-align:left;font-size:13px}.l{color:#b3261e}.s{color:#5f6368}</style></head><body><h1>Trusted root certificates</h1><p>");
   for(k=0,ln=rn;ln>0||!k;k++){num[k]=(char)('0'+ln%10);ln/=10;if(!ln){k++;break;}}
   while(k>0)bn(&num[--k],1);
   ba(" roots compiled in (");
   for(k=0,ln=rn-cn;ln>0||!k;k++){num[k]=(char)('0'+ln%10);ln/=10;if(!ln){k++;break;}}
   while(k>0)bn(&num[--k],1);
   ba(" core from the current Mozilla program + ");
   for(k=0,ln=cn;ln>0||!k;k++){num[k]=(char)('0'+ln%10);ln/=10;if(!ln){k++;break;}}
   while(k>0)bn(&num[--k],1);
   ba(" <span class=l>legacy</span>: roots from an older Mozilla bundle that Mozilla has since removed; build with -DMB_NO_LEGACY_ROOTS to drop them).</p><table><tr><th>Name</th><th>Tier</th><th>SHA-256</th></tr>");
   for(i=0;i<rn;i++){
    nm=mb_root_name(i,&lg,&sha);
    ba("<tr><td>");
    for(k=0;nm[k];k++){if(nm[k]=='<')ba("&lt;");else if(nm[k]=='>')ba("&gt;");else if(nm[k]=='&')ba("&amp;");else bn(&nm[k],1);}
    ba(lg?"</td><td class=l>legacy</td><td class=s>":"</td><td>core</td><td class=s>");
    bn(sha,16);ba("...</td></tr>");}
   ba("</table></body></html>");}
  else if(has(url,"about:blank"))cpy(pg,PG,"<html><head><title>about:blank</title></head><body></body></html>");
  else cpy(pg,PG,HOME);
  plen=len(pg);}

 if(has(url,"about:")){finish_commit(url,0,0,"");return;}
 if(has(url,"file:")){rdfiled(url,pg,PG,&plen,1);finish_commit(url,0,0,"");return;}
 if(has(url,"http")){
  if(ufirst&&has(url,"http://")&&upgradable(url)){bs(su,sizeof su);ba("https://");ba(url+7);up=1;}
  else cpy(su,sizeof su,url);
  net_enqueue(K_PAGE,0,0,su,"GET",0,0,0,up,up?url:0);
  cpy(stt,UC,"Connecting...");
  return;}
 errpage("Unsupported address","This address type is not supported.",url);
 finish_commit(url,0,0,"");}

/* ---- JavaScript side effects: console, alert(), location changes ---- */
static int navdepth;
static void pump(void){
 const char*t;int n;char u[UC],full[UC];
 t=mwe_js_log(&n);
 if(t&&n>0){mb_plat_log(t,n);mwe_js_log_clear();}
 while((t=mwe_js_alert(&n))!=0)mb_plat_alert(t,n);
 t=mwe_js_nav(&n);
 if(t&&n>0&&n<UC&&navdepth<4){
  int i;for(i=0;i<n;i++)u[i]=t[i];u[n]=0;
  resolve(cur,u,full,UC);
  navdepth++;go(full,1);navdepth--;}}

/* ---- fetch() / XMLHttpRequest from scripts: asynchronous; same rules as page subresources (HSTS, mixed content
 * blocked, certificates always verified). file: URLs are readable only from file: pages. CORS is not enforced. ---- */
static int js_fetch_start(int id,const char*method,const char*url,const char*body,int blen,const char*hdrs){
 char u[UC],full[UC];int i;
 for(i=0;url[i]&&i<UC-1;i++)u[i]=url[i];
 u[i]=0;resolve(cur,u,full,UC);
 if(has(full,"file:")&&!has(cur,"file:"))return -1;
 if(!has(full,"file:")&&!has(full,"http"))return -1;
 net_enqueue(K_FETCH,id,2,full,method,hdrs,body,blen,0,0);
 return 0;}
static void js_fetch_abort(int id){   /* the script gave up on request id (AbortController, xhr.abort(), timeout) */
 int i,k,j;
 for(i=0;i<NRQ;i++)if(rqs[i].st!=RS_FREE&&rqs[i].st!=RS_DELIV)for(k=0;k<rqs[i].nw;k++)if(rqs[i].w[k].kind==K_FETCH&&rqs[i].w[k].tag==id){
  for(j=k;j+1<rqs[i].nw;j++)rqs[i].w[j]=rqs[i].w[j+1];
  if(--rqs[i].nw==0){rq_free(&rqs[i]);ncancel_++;}
  return;}
 for(i=0;i<NQ;i++)if(qs[i].used)for(k=0;k<qs[i].nw;k++)if(qs[i].w[k].kind==K_FETCH&&qs[i].w[k].tag==id){
  for(j=k;j+1<qs[i].nw;j++)qs[i].w[j]=qs[i].w[j+1];
  if(--qs[i].nw==0){qs[i].used=0;ncancel_++;}
  return;}}
static int js_poll(long long*now){   /* a script is awaiting: move the network along, nothing else */
 if(!net_pump(1))mb_plat_idle(1);
 *now=mb_plat_ms();
 return net_fetches();}

/* ---- committing a page ---- */
static void hist_commit(void){
 if(pendpush){
  hsy[hi]=sy;
  if(hn){hi++;if(hi>31){for(hn=1;hn<32;hn++){cpy(hist[hn-1],512,hist[hn]);hsy[hn-1]=hsy[hn];}hi=31;}}
  hn=hi+1;
  cpy(hist[hi],512,cur);hsy[hi]=0;}}
/* pg/plen hold the document; url is its final address; secv: 0 local, 1 http, 2 https */
static void finish_commit(const char*url,int secv,const mb_tls_info_t*ti,const char*host){
 net_cancel_all();
 cpy(cur,UC,url);cpy(ub,UC,cur);ucur=len(ub);usel=0;uoff=0;
 sec=secv;secpanel=0;
 if(ti){pinfo=*ti;cpy(phost,sizeof phost,host);}
 hist_commit();
 mwe_load(pg,plen);ready=1;loading=0;
 res_reset();
 if(want_sy<0)sy=0;
 tcommit=mb_plat_ms();
 relayout();settitle();
 res_discover();img_scan();
 res_settle();relayout();
 pump();}
static void page_ok(Rq*r){
 int n=r->bl;
 if(n>PG-1)n=PG-1;
 mcopy(pg,r->bp,n);pg[n]=0;plen=n;
 finish_commit(r->url,r->tls?2:1,r->haveti?&r->ti:0,r->host);}
static void page_fail(Rq*r){
 if(r->upgrade&&r->netfail&&r->fb[0]){   /* HTTPS-first: nothing answered on 443 (or no TLS there) - use http:// instead */
  cpy(r->url,UC,r->fb);r->fb[0]=0;r->upgrade=0;r->redir=0;r->failed=0;r->err[0]=0;
  r->st=RS_FREE;rq_begin(r);return;}
 errpage(r->head[0]?r->head:"Could not load the page",r->err[0]?r->err:"The server did not respond. Check the address and your connection.",r->url);
 finish_commit(r->url,0,0,"");}

static void go(const char*in,int push){
 char u[UC];
 norm(in,u,sizeof u);
 net_cancel_all();
 navgen++;pendpush=push;want_sy=-1;
 if(!push&&!cmpi(u,cur))cache_flush();      /* reload: fetch everything again */
 start_nav(u);}
/* a form was submitted (Enter in a field, or a submit button): GET -> address with ?query, POST -> request with a body */
static void post_nav(const char*url,const char*body,int bl){
 net_cancel_all();navgen++;pendpush=1;want_sy=-1;
 loading=1;tnav=mb_plat_ms();tcommit=0;tsettle=0;
 cpy(ub,UC,url);ucur=len(ub);usel=0;uoff=0;
 net_enqueue(K_PAGE,0,0,url,"POST","Content-Type: application/x-www-form-urlencoded",body,bl,0,0);
 cpy(stt,UC,"Sending form...");}
static void formpoll(void){
 const char*m,*a,*d;int dl,i;char act[UC],full[UC];
 if(!mwe_form_submit(&m,&a,&d,&dl))return;
 for(i=0;a[i]&&i<UC-1;i++)act[i]=a[i];
 act[i]=0;
 if(!act[0])cpy(full,UC,cur);else resolve(cur,act,full,UC);
 for(i=0;full[i];i++)if(full[i]=='#'){full[i]=0;break;}
 if(m[0]=='G'){
  for(i=0;full[i];i++)if(full[i]=='?'){full[i]=0;break;}   /* the form's fields replace the query */
  if(dl>0&&len(full)+dl+2<UC){bs(act,UC);ba(full);ba("?");bn(d,dl);cpy(full,UC,act);}
  go(full,1);}
 else if(has(full,"http"))post_nav(full,d,dl);
 else go(full,1);}
static void hgo(int d){
 int t=hi+d;
 if(t<0||t>=hn)return;
 hsy[hi]=sy;hi=t;
 net_cancel_all();navgen++;pendpush=0;
 start_nav(hist[hi]);
 want_sy=hsy[hi];}   /* set after start_nav: a local page commits at once and relayout() consumes it */

/* status line: progress while loading, the link under the mouse otherwise */
static void statline(char*out){
 int i;Rq*p=0;
 bs(out,UC);
 if(loading){
  for(i=0;i<NRQ;i++)if(rqs[i].st!=RS_FREE&&rqs[i].w[0].kind==K_PAGE)p=&rqs[i];
  if(p){
   if(p->st==RS_CONNECTING)ba("Connecting to ");
   else if(p->st==RS_TLS)ba("Secure handshake with ");
   else if(p->st==RS_BODY){ba("Receiving ");ba_int((p->n-p->he)/1024);ba(" KB from ");}
   else ba("Waiting for ");
   ba(p->host);ba("...");}
  else ba("Loading...");
  return;}
 if(res_pending()){
  int q=net_busy();
  ba("Loading resources (");ba_int(q);ba(q==1?" request left)...":" requests left)...");
  return;}
 ba("Done");}
int mb_busy(void){return loading||res_pending()||rdirty_;}
const char *mb_summary(void){   /* one line of timing/statistics for tests (milliseconds since navigation start) */
 static char o[256];
 bs(o,sizeof o);
 ba("first-render=");ba_int(tcommit?tcommit-tnav:-1);ba("ms settled=");ba_int(tsettle?tsettle-tnav:-1);
 ba("ms started=");ba_int(nstarted_);ba(" done=");ba_int(ndone_);ba(" failed=");ba_int(nfail_);ba(" cancelled=");ba_int(ncancel_);
 ba(" redirects=");ba_int(nredir_);ba(" cache-hits=");ba_int(ncache_);
 return o;}
int mb_tick(void){ /* called by the platform every ~16 ms; 1 = redraw needed */
 int r=0;long long now;char st2[UC];
 if(inpump)return 0;
 inpump=1;
 now=mb_plat_ms();
 imgbudget=1;
 if(net_pump(0))r=1;
 if(ready){
  if(res_settle())rdirty_=1;
  if(mwe_js_active()&&mwe_js_tick(now)){rdirty_=1;}
  pump();
  if(sy!=scansy){scansy=sy;img_scan();}
  if(rdirty_&&now-lastrl>=30){rdirty_=0;lastrl=now;relayout();settitle();img_scan();r=1;}}
 statline(st2);
 if(!hov&&strcmp_(st2,stt)){cpy(stt,UC,st2);r=1;}
 if(!tsettle&&!loading&&ready&&!res_pending()&&!rdirty_)tsettle=now;
 inpump=0;
 return r;}
void mb_init(const char*start,int w,int h){
 mwe_js_set_fetch_async(js_fetch_start,js_fetch_abort);
 mwe_js_set_poll(js_poll);
 mwe_js_set_cookies(js_cookie_get,js_cookie_set);
 Wd=w;Ht=h;sy=0;hn=hi=0;uf=0;
 ufirst=1;go(start&&start[0]?start:"about:home",1);ufirst=0;}
void mb_resize(int w,int h){Wd=w;Ht=h;if(ready)relayout();}
