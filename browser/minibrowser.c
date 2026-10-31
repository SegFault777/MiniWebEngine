/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniBrowser v0.2 - portable core, built on MiniWebEngine.
 *
 * NOTE: MiniBrowser is NOT "MiniWeb". MiniWeb is the browser built into the
 * MiniWin operating system. MiniBrowser is a separate, standalone desktop
 * browser that merely shares the "Mini" naming; it is built on MiniWebEngine.
 *
 * This file: URL handling, HTTP/1.1 response parsing, history, UI chrome.
 * No OS calls. File I/O uses stdio only for file:// pages. */
#include <stdio.h>
#include "../mwe.h"
#include "mb.h"

#define PG (1<<20)
#define TB 44
#define SBH 24
#define SBW 12
#define UC 1024

static char pg[PG+1],raw[1<<21];
static char cur[UC],ub[UC],stt[UC],tmp[UC*2],req[UC*2];
static char hist[32][512];static int hsy[32],hn,hi;
static int Wd,Ht,sy,ph,plen,ucur,uf,usel,uoff,hov,ready;

/* ---- tiny string builder ---- */
static char*bd_;static int bn_,bc_;
static void bs(char*d,int cap){bd_=d;bn_=0;bc_=cap;if(cap)d[0]=0;}
static void bn(const char*s,int n){while(n-->0&&*s&&bn_<bc_-1)bd_[bn_++]=*s++;bd_[bn_]=0;}
static void ba(const char*s){bn(s,1<<30);}
static void be(const char*s){ /* HTML-escaped append */
 for(;*s;s++){if(*s=='<')ba("&lt;");else if(*s=='>')ba("&gt;");else if(*s=='&')ba("&amp;");else bn(s,1);}}
static int len(const char*s){int n=0;while(s[n])n++;return n;}
static void cpy(char*d,int cap,const char*s){int i=0;while(s[i]&&i<cap-1){d[i]=s[i];i++;}d[i]=0;}
static int lw(int c){return c>='A'&&c<='Z'?c+32:c;}
static int has(const char*s,const char*p){while(*p){if(lw(*s)!=lw(*p))return 0;s++;p++;}return 1;}
static int hexv(int c){c=lw(c);return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;}
static int dig(int c){return c>='0'&&c<='9';}

/* ---- URLs ---- */
static void dots(char*s){
 char out[UC],tail[UC];int end=0,i=0,o=0,j,k,n;
 while(s[end]&&s[end]!='?'&&s[end]!='#')end++;
 cpy(tail,UC,s+end);
 while(i<end){
  j=i+1;while(j<end&&s[j]!='/')j++;
  n=j-i-1;
  if(n==1&&s[i+1]=='.'){}
  else if(n==2&&s[i+1]=='.'&&s[i+2]=='.'){while(o>0&&out[o-1]!='/')o--;if(o>0)o--;}
  else if(o<UC-2-n){out[o++]='/';for(k=0;k<n;k++)out[o++]=s[i+1+k];}
  i=j;}
 if(!o)out[o++]='/';
 out[o]=0;
 bs(s,UC);ba(out);ba(tail);}
static void resolve(const char*base,const char*ref,char*out,int cap){
 int sch=0,pe,qe,lp,h;char r[UC];
 if(has(ref,"http://")||has(ref,"https://")||has(ref,"file:")||has(ref,"about:")){cpy(out,cap,ref);return;}
 while(base[sch]&&!(base[sch]==':'&&base[sch+1]=='/'&&base[sch+2]=='/'))sch++;
 if(!base[sch]){cpy(out,cap,ref);return;}
 bs(r,sizeof r);
 pe=sch+3;while(base[pe]&&base[pe]!='/'&&base[pe]!='?'&&base[pe]!='#')pe++;
 qe=pe;while(base[qe]&&base[qe]!='?'&&base[qe]!='#')qe++;
 if(ref[0]=='#'){h=0;while(base[h]&&base[h]!='#')h++;bn(base,h);ba(ref);cpy(out,cap,r);return;}
 if(ref[0]=='/'&&ref[1]=='/'){bn(base,sch+1);ba(ref);cpy(out,cap,r);return;}
 bn(base,pe);
 if(ref[0]=='/')ba(ref);
 else if(ref[0]=='?'){bn(base+pe,qe-pe);ba(ref);}
 else{lp=qe;while(lp>pe&&base[lp-1]!='/')lp--;if(lp==pe)ba("/");else bn(base+pe,lp-pe);ba(ref);}
 dots(r+pe);
 cpy(out,cap,r);}
static void norm(const char*in,char*out,int cap){
 char t[UC];int n=0,i;
 while(*in==' ')in++;
 while(in[n]&&n<UC-1){t[n]=in[n];n++;}
 while(n&&t[n-1]==' ')n--;
 t[n]=0;
 if(!n){cpy(out,cap,"about:home");return;}
 if(has(t,"about:")||has(t,"http://")||has(t,"https://")||has(t,"file:")){cpy(out,cap,t);return;}
 bs(out,cap);
 if(((t[0]>='a'&&t[0]<='z')||(t[0]>='A'&&t[0]<='Z'))&&t[1]==':'&&(t[2]=='\\'||t[2]=='/')){
  ba("file:///");for(i=0;t[i];i++){char c=t[i]=='\\'?'/':t[i];bn(&c,1);}return;}
 if(t[0]=='/'||(t[0]=='.'&&(t[1]=='/'||t[1]=='.'))){ba("file://");ba(t);return;}
 ba("http://");ba(t);}
static void fpath(const char*url,char*out,int cap){
 const char*p=url+7;int o=0;
 if(p[0]=='/'&&p[1]&&p[2]==':')p++;
 while(*p&&*p!='?'&&*p!='#'&&o<cap-1){
  if(*p=='%'&&hexv(p[1])>=0&&hexv(p[2])>=0){out[o++]=hexv(p[1])*16+hexv(p[2]);p+=3;}else out[o++]=*p++;}
 out[o]=0;}

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
"<h2>Limits</h2><p class=k>No HTTPS (no TLS, to stay dependency-free), no images, no JavaScript, "
"ASCII/Latin text only. Pages load synchronously.</p>"
"<p><a href=\"about:blank\">about:blank</a></p></body></html>";
static int rdfile(const char*url){
 char p[UC];FILE*f;int n;
 fpath(url,p,sizeof p);
 if(!(f=fopen(p,"rb"))){errpage("File not found","The file could not be opened.",url);return 0;}
 n=(int)fread(pg,1,PG,f);fclose(f);plen=n;pg[n]=0;return 1;}
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
static int http(char*url){
 int rd,n,i,port,st,he,bl,hl;char host[256],path[UC],loc[UC],tc[64],ct[96];const char*p;
 for(rd=0;rd<6;rd++){
  if(has(url,"https://")){errpage("HTTPS is not supported","MiniBrowser v" MB_VERSION " has no TLS (it is dependency-free). Use an http:// address or a local file.",url);return 0;}
  p=url+7;i=0;port=80;
  while(*p&&*p!='/'&&*p!=':'&&*p!='?'&&*p!='#'&&i<255)host[i++]=*p++;
  host[i]=0;
  if(*p==':'){p++;port=0;while(dig(*p)){port=port*10+*p-'0';p++;}}
  if(!host[0]){errpage("Invalid address","The address has no host name.",url);return 0;}
  bs(path,sizeof path);
  if(*p!='/')ba("/");
  while(*p&&*p!='#')bn(p++,1);
  bs(req,sizeof req);
  ba("GET ");ba(path);ba(" HTTP/1.1\r\nHost: ");ba(host);
  ba("\r\nUser-Agent: MiniBrowser/" MB_VERSION " MiniWebEngine/" MWE_VERSION "\r\nAccept: text/html,text/plain;q=0.9,*/*;q=0.5\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n");
  n=mb_plat_http(host,port,req,bn_,raw,(int)sizeof raw-1);
  if(n<=0){errpage("Could not connect","The server did not respond. Check the address and your connection.",url);return 0;}
  raw[n]=0;
  for(he=0;he+3<n;he++)if(raw[he]=='\r'&&raw[he+1]=='\n'&&raw[he+2]=='\r'&&raw[he+3]=='\n')break;
  if(he+3>=n||!has(raw,"HTTP/")){errpage("Bad response","The server sent something that is not HTTP.",url);return 0;}
  hl=he+2;
  st=0;for(i=0;raw[i]&&raw[i]!=' ';i++);
  while(raw[i]==' ')i++;
  while(dig(raw[i]))st=st*10+raw[i++]-'0';
  if(st>=300&&st<400&&hdr(raw,hl,"location:",loc,sizeof loc)&&loc[0]){resolve(url,loc,tmp,UC);cpy(url,UC,tmp);if(has(url,"http"))continue;errpage("Unsupported redirect","The server redirected to an unsupported address.",url);return 0;}
  p=raw+he+4;bl=n-he-4;
  if(hdr(raw,hl,"transfer-encoding:",tc,sizeof tc)&&has(tc,"chunked"))bl=dechunk((char*)p,bl);
  if(hdr(raw,hl,"content-type:",ct,sizeof ct)&&!has(ct,"text/")&&!has(ct,"application/xhtml")){
   errpage("Unsupported content","MiniBrowser can only display HTML and plain text.",url);return 0;}
  if(bl>PG)bl=PG;
  for(i=0;i<bl;i++)pg[i]=p[i];
  pg[bl]=0;plen=bl;
  return 1;}
 errpage("Too many redirects","The server redirected too many times.",url);return 0;}

/* ---- navigation ---- */
static void relayout(void){
 int cw=Wd-SBW,chh=Ht-TB-SBH,mx;
 if(cw<32)cw=32;
 ph=mwe_layout(cw);
 mx=ph-chh;if(mx<0)mx=0;
 if(sy>mx)sy=mx;if(sy<0)sy=0;}
static void settitle(void){
 const char*t;int n;
 t=mwe_title(&n);
 if(t&&n>0)mb_plat_title(t,n);else mb_plat_title(cur,len(cur));}
static void load(char*url){
 int i;
 if(has(url,"about:")){
  if(has(url,"about:blank"))cpy(pg,PG,"<html><head><title>about:blank</title></head><body></body></html>");
  else cpy(pg,PG,HOME);
  plen=len(pg);}
 else if(has(url,"file:"))rdfile(url);
 else if(has(url,"http"))http(url);
 else errpage("Unsupported address","This address type is not supported.",url);
 cpy(cur,UC,url);cpy(ub,UC,cur);ucur=len(ub);usel=0;uoff=0;
 mwe_load(pg,plen);ready=1;
 sy=0;relayout();settitle();
 for(i=0;i<5;i++)stt[i]="Done"[i];}
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
 go(start&&start[0]?start:"about:home",1);}
void mb_resize(int w,int h){Wd=w;Ht=h;if(ready)relayout();}

/* ---- drawing ---- */
static void fillr(unsigned*px,int x,int y,int w,int h,unsigned c){mwe_rect(px,Wd,Wd,Ht,x,y,w,h,c);}
static void seg(unsigned*px,int x0,int y0,int x1,int y1,unsigned c){
 int dx=x1-x0,dy=y1-y0,n=(dx<0?-dx:dx)>(dy<0?-dy:dy)?(dx<0?-dx:dx):(dy<0?-dy:dy),i;
 if(!n)n=1;
 for(i=0;i<=n;i++)fillr(px,x0+dx*i/n,y0+dy*i/n,2,2,c);}
static void icons(unsigned*px){
 unsigned on=0x5F6368,off=0xBDC1C6,c;int cx,cy,dx,dy,d;
 c=hi>0?on:off;cx=23;cy=TB/2;
 seg(px,cx+7,cy,cx-7,cy,c);seg(px,cx-7,cy,cx-1,cy-6,c);seg(px,cx-7,cy,cx-1,cy+6,c);
 c=hi<hn-1?on:off;cx=59;
 seg(px,cx-7,cy,cx+7,cy,c);seg(px,cx+7,cy,cx+1,cy-6,c);seg(px,cx+7,cy,cx+1,cy+6,c);
 cx=95;
 for(dy=-8;dy<=8;dy++)for(dx=-8;dx<=8;dx++){
  d=dx*dx+dy*dy;
  if(d>=30&&d<=52&&!(dx>0&&dy<0&&dx>=-dy*2/3&&dx<=-dy*3))fillr(px,cx+dx,cy+dy,1,1,on);}
 seg(px,cx+3,cy-8,cx+8,cy-8,on);seg(px,cx+8,cy-8,cx+8,cy-3,on);}
static int caretx(int idx){return mwe_textw(ub,idx,14,0);}
void mb_draw(unsigned*px){
 int i,cw=Wd-SBW,chh=Ht-TB-SBH,bx=120,bw=Wd-bx-10,tx,cxp,th,ty,mx;
 if(cw<32)cw=32;
 fillr(px,0,0,Wd,Ht,0xFFFFFF);
 fillr(px,0,0,Wd,TB,0xF1F3F4);fillr(px,0,TB-1,Wd,1,0xDADCE0);
 icons(px);
 /* address bar */
 fillr(px,bx,7,bw,TB-14,uf?0x1A73E8:0xDADCE0);fillr(px,bx+1,8,bw-2,TB-16,0xFFFFFF);
 cxp=caretx(ucur);
 if(cxp-uoff>bw-24)uoff=cxp-(bw-24);
 if(cxp<uoff)uoff=cxp;
 if(uoff<0)uoff=0;
 tx=bx+10-uoff;
 if(uf&&usel)fillr(px,bx+10,11,mwe_textw(ub,len(ub),14,0)<bw-20?mwe_textw(ub,len(ub),14,0):bw-20,TB-22,0xC6DAFC);
 mwe_text(px,Wd,Wd,Ht,tx,TB/2+5,ub,len(ub),14,0,0x202124,bx+10,bx+bw-8);
 if(uf&&!usel)fillr(px,tx+cxp,12,1,TB-24,0x202124);
 /* content */
 if(chh>0){
  mwe_paint(px+TB*Wd,Wd,0,sy,cw,chh);
  fillr(px,cw,TB,SBW,chh,0xF8F9FA);
  if(ph>chh){
   th=chh*chh/ph;if(th<24)th=24;
   mx=ph-chh;ty=TB+(mx?(chh-th)*sy/mx:0);
   fillr(px,cw+2,ty,SBW-4,th,0xBDC1C6);}}
 /* status bar */
 fillr(px,0,Ht-SBH,Wd,SBH,0xF1F3F4);fillr(px,0,Ht-SBH,Wd,1,0xDADCE0);
 mwe_text(px,Wd,Wd,Ht,10,Ht-8,stt,len(stt),12,0,0x5F6368,0,Wd-10);
 i=mwe_textw("MiniWebEngine " MWE_VERSION,sizeof("MiniWebEngine " MWE_VERSION)-1,12,0);
 if(Wd>i+300)mwe_text(px,Wd,Wd,Ht,Wd-i-10,Ht-8,"MiniWebEngine " MWE_VERSION,sizeof("MiniWebEngine " MWE_VERSION)-1,12,0,0x80868B,0,Wd);}

/* ---- input ---- */
static int linkat(int x,int y,char*out){
 const char*h;int n;
 if(x<0||x>=Wd-SBW||y<TB||y>=Ht-SBH)return 0;
 h=mwe_link(x,y-TB+sy,&n);
 if(!h||n<=0||n>=UC)return 0;
 for(y=0;y<n;y++)out[y]=h[y];
 out[n]=0;
 resolve(cur,out,tmp,UC);cpy(out,UC,tmp);return 1;}
void mb_move(int x,int y){
 char u[UC];int h=linkat(x,y,u);
 if(h!=hov){hov=h;mb_plat_cursor(h);}
 if(h)cpy(stt,UC,u);else cpy(stt,UC,"Done");}
void mb_click(int x,int y){
 char u[UC];int chh=Ht-TB-SBH,mx,i,a,b;
 if(y<TB){
  if(y>=8&&y<38){
   if(x>=8&&x<38){hgo(-1);return;}
   if(x>=44&&x<74){hgo(1);return;}
   if(x>=80&&x<110){go(cur,0);return;}}
  if(x>=120&&x<Wd-10){
   if(!uf){uf=1;usel=1;return;}
   usel=0;
   x-=130-uoff;ucur=0;
   for(i=0;ub[i];){
    a=i;i++;while((ub[i]&0xC0)==0x80)i++;
    b=mwe_textw(ub,i,14,0);
    if(b>x+(b-mwe_textw(ub,a,14,0))/2)break;
    ucur=i;}
   return;}
  uf=0;return;}
 uf=0;
 if(x>=Wd-SBW&&y<Ht-SBH&&ph>chh){
  mx=ph-chh;sy=(y-TB)*mx/chh;if(sy<0)sy=0;if(sy>mx)sy=mx;return;}
 if(linkat(x,y,u)){if(u[0]=='#')return;go(u,1);}
 return;}
void mb_wheel(int n){
 int mx=ph-(Ht-TB-SBH);
 sy-=n*60;if(sy>mx)sy=mx;if(sy<0)sy=0;}
static void ins(const char*s,int n){
 int l=len(ub),i;
 if(usel){ub[0]=0;l=0;ucur=0;usel=0;}
 if(l+n>=UC-1)return;
 for(i=l;i>=ucur;i--)ub[i+n]=ub[i];
 for(i=0;i<n;i++)ub[ucur+i]=s[i];
 ucur+=n;}
void mb_char(int c){
 char b[4];int n;
 if(!uf||c<32||c==127)return;
 if(c<0x80){b[0]=c;n=1;}
 else if(c<0x800){b[0]=0xC0|c>>6;b[1]=0x80|(c&63);n=2;}
 else if(c<0x10000){b[0]=0xE0|c>>12;b[1]=0x80|(c>>6&63);b[2]=0x80|(c&63);n=3;}
 else{b[0]=0xF0|c>>18;b[1]=0x80|(c>>12&63);b[2]=0x80|(c>>6&63);b[3]=0x80|(c&63);n=4;}
 ins(b,n);}
void mb_paste(const char*s,int n){
 char b[UC];int i,o=0;
 if(!uf)return;
 for(i=0;i<n&&o<UC-1;i++)if(s[i]!='\r'&&s[i]!='\n')b[o++]=s[i];
 ins(b,o);}
void mb_key(int k){
 int chh=Ht-TB-SBH,mx=ph-chh,l,i;
 if(k==MB_FOCUS_URL){uf=1;usel=1;return;}
 if(k==MB_BACK){hgo(-1);return;}
 if(k==MB_FORWARD){hgo(1);return;}
 if(k==MB_F5){go(cur,0);return;}
 if(uf){
  l=len(ub);
  if(k==MB_ENTER){uf=0;usel=0;go(ub,1);}
  else if(k==MB_ESC){uf=0;usel=0;cpy(ub,UC,cur);ucur=len(ub);}
  else if(k==MB_BACKSPACE){
   if(usel){ub[0]=0;ucur=0;usel=0;}
   else if(ucur>0){i=ucur-1;while(i>0&&(ub[i]&0xC0)==0x80)i--;
    {int j;for(j=ucur;j<=l;j++)ub[i+j-ucur]=ub[j];}ucur=i;}}
  else if(k==MB_DELETE){
   if(usel){ub[0]=0;ucur=0;usel=0;}
   else if(ucur<l){i=ucur+1;while(ub[i]&&(ub[i]&0xC0)==0x80)i++;
    {int j;for(j=i;j<=l;j++)ub[ucur+j-i]=ub[j];}}}
  else if(k==MB_LEFT){usel=0;if(ucur>0){ucur--;while(ucur>0&&(ub[ucur]&0xC0)==0x80)ucur--;}}
  else if(k==MB_RIGHT){usel=0;if(ucur<l){ucur++;while(ub[ucur]&&(ub[ucur]&0xC0)==0x80)ucur++;}}
  else if(k==MB_HOME){usel=0;ucur=0;}
  else if(k==MB_END){usel=0;ucur=l;}
  return;}
 if(k==MB_UP)sy-=48;else if(k==MB_DOWN)sy+=48;
 else if(k==MB_PGUP)sy-=chh-40;else if(k==MB_PGDN)sy+=chh-40;
 else if(k==MB_HOME)sy=0;else if(k==MB_END)sy=mx;
 if(sy>mx)sy=mx;if(sy<0)sy=0;}
