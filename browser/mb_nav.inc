/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniBrowser module: mb_nav
 * navigation: load pipeline, redirects, HTTPS-first, stylesheets/scripts/images, history.
 * Not a standalone translation unit: minibrowser.c includes the modules in order (unity build). */
/* ---- navigation ---- */
static void relayout(void){
 int cw=Wd-SBW,chh=Ht-TB-SBH,mx;
 if(cw<32)cw=32;
 mwe_viewport_h(chh>100?chh:100);
 ph=mwe_layout(cw);
 mx=ph-chh;if(mx<0)mx=0;
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
static char csspool[1<<18];
static void loadcss(void){ /* external stylesheets: <link rel=stylesheet href=...> (at most 8, 256 KB in total) */
 int i,n,used=0,l2;const char*h;char u[UC],full[UC];
 for(i=0;i<8;i++){
  h=mwe_stylesheet(i,&n);
  if(!h)break;
  if(n<=0||n>=UC)continue;
  for(l2=0;l2<n;l2++)u[l2]=h[l2];
  u[n]=0;
  resolve(cur,u,full,UC);
  if(used>=(int)sizeof csspool-2)break;
  l2=0;
  if(has(full,"file:")){if(!rdfiled(full,csspool+used,(int)sizeof csspool-used,&l2,0))continue;}
  else if(has(full,"http")){if(!httpd(full,csspool+used,(int)sizeof csspool-used,&l2,0))continue;}
  else continue;
  mwe_add_css(csspool+used,l2);
  used+=l2+1;}}
static unsigned char imgbuf[1<<22];
static int loadimg(void){ /* <img src>: fetched one by one (at most 32, 4 MB each) and decoded by the engine */
 int i,n,got=0,l2,ok;const char*h;char u[UC],full[UC];
 for(i=0;i<mwe_image_count()&&i<32;i++){
  if(mwe_image_state(i)!=0)continue;
  h=mwe_image_src(i,&n);
  if(!h||n<=0||n>=UC){mwe_image_fail(i);continue;}
  for(l2=0;l2<n;l2++)u[l2]=h[l2];
  u[n]=0;
  resolve(cur,u,full,UC);
  l2=0;ok=0;
  if(has(full,"file:"))ok=rdfiled(full,(char*)imgbuf,(int)sizeof imgbuf,&l2,0);
  else if(has(full,"http"))ok=httpd(full,(char*)imgbuf,(int)sizeof imgbuf,&l2,0);
  if(ok&&l2>0&&mwe_image_set(i,imgbuf,l2))got=1;
  else mwe_image_fail(i);}
 return got;}
static char jspool[1<<20];
static void go(const char*in,int push);
static void loadjs(void){ /* external scripts: <script src=...> (at most 16, 1 MB in total) */
 int i,n,used=0,l2;const char*h;char u[UC],full[UC];
 for(i=0;i<16;i++){
  h=mwe_script(i,&n);
  if(!h)break;
  if(n<=0||n>=UC)continue;
  for(l2=0;l2<n;l2++)u[l2]=h[l2];
  u[n]=0;
  resolve(cur,u,full,UC);
  if(used>=(int)sizeof jspool-2)break;
  l2=0;
  if(has(full,"file:")){if(!rdfiled(full,jspool+used,(int)sizeof jspool-used,&l2,0))continue;}
  else if(has(full,"http")){if(!httpd(full,jspool+used,(int)sizeof jspool-used,&l2,0))continue;}
  else continue;
  mwe_add_js(i,jspool+used,l2);
  used+=l2+1;}}
static int navdepth,nloads;
static void pump(void){ /* console output, alert() and location changes requested by scripts */
 const char*t;int n;char u[UC],full[UC];
 t=mwe_js_log(&n);
 if(t&&n>0){mb_plat_log(t,n);mwe_js_log_clear();}
 while((t=mwe_js_alert(&n))!=0)mb_plat_alert(t,n);
 t=mwe_js_nav(&n);
 if(t&&n>0&&n<UC&&navdepth<4){
  int i;for(i=0;i<n;i++)u[i]=t[i];u[n]=0;
  resolve(cur,u,full,UC);
  navdepth++;go(full,1);navdepth--;}}
static void load(char*url){
 int i;
 nloads++;
 sec=0;secpanel=0;ufirst&=1;
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
 else if(has(url,"file:"))rdfiled(url,pg,PG,&plen,1);
 else if(has(url,"http")){
  if(ufirst&&has(url,"http://")&&upgradable(url)){       /* HTTPS-first for typed addresses: fall back to http only if nothing answers on 443 */
   char su[UC];int ok;
   bs(su,sizeof su);ba("https://");ba(url+7);
   quiet=1;ok=http(su);quiet=0;
   if(ok)cpy(url,UC,su);
   else if(netfail)http(url);
   else{bs(su,sizeof su);ba("https://");ba(url+7);http(su);cpy(url,UC,su);}}
  else http(url);}
 else errpage("Unsupported address","This address type is not supported.",url);
 cpy(cur,UC,url);cpy(ub,UC,cur);ucur=len(ub);usel=0;uoff=0;
 mwe_load(pg,plen);ready=1;
 loadcss();
 loadjs();
 mwe_js_time(mb_plat_time()*1000LL,mb_plat_ms());mwe_js_url(cur);
 mwe_js_run();
 sy=0;relayout();settitle();
 for(i=0;i<2&&loadimg();i++)relayout();   /* images found by the first layout (also those added by scripts) */
 for(i=0;i<5;i++)stt[i]="Done"[i];
 pump();}
int mb_tick(void){ /* called by the platform every ~16 ms; 1 = redraw needed */
 int r=0;
 if(!ready||!mwe_js_active())return 0;
 if(mwe_js_tick(mb_plat_ms())){relayout();settitle();r=1;}
 pump();
 return r;}
static void go(const char*in,int push){
 char u[UC];
 norm(in,u,sizeof u);
 if(push){
  hsy[hi]=sy;
  if(hn){hi++;if(hi>31){for(hn=1;hn<32;hn++){cpy(hist[hn-1],512,hist[hn]);hsy[hn-1]=hsy[hn];}hi=31;}}
  hn=hi+1;
  load(u);cpy(hist[hi],512,cur);hsy[hi]=0;}
 else load(u);}
static void hgo(int d){
 int t=hi+d;
 if(t<0||t>=hn)return;
 hsy[hi]=sy;hi=t;
 load(hist[hi]);
 sy=hsy[hi];relayout();}
void mb_init(const char*start,int w,int h){
 Wd=w;Ht=h;sy=0;hn=hi=0;uf=0;
 ufirst=1;go(start&&start[0]?start:"about:home",1);ufirst=0;}
void mb_resize(int w,int h){Wd=w;Ht=h;if(ready)relayout();}
