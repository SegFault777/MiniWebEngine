/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniBrowser module: mb_cookie  (new in v0.11)
 * A session cookie jar. Set-Cookie from every response (pages, subresources, fetch/XHR) is stored; matching cookies are sent
 * in a Cookie header; document.cookie reads/writes it (HttpOnly cookies are invisible to scripts).
 * Supported: Domain, Path, Expires, Max-Age, Secure, HttpOnly, SameSite (Lax/Strict cookies are not sent on cross-site
 * subresource requests; third-party cookies are only accepted with SameSite=None; Secure). Limits: 160 cookies, name+value
 * under ~700 bytes, no public-suffix list (a Domain= attribute needs a dot and must match the host), cookies are NOT saved to
 * disk (they end with the browser session), no cookie prompts or settings UI.
 * Not a standalone translation unit: minibrowser.c includes the modules in order (unity build). */

static int isalpha_(int c);static int hostdot(const char*d);
#define NCK 160
typedef struct{char n[96],v[640],dom[128],path[200];long long exp;int hostonly,secure,httponly,ss;}Ck;   /* ss: 0 unset, 1 lax/strict, 2 none */
static Ck jar[NCK];static int njar;

static long long days_civil(long long y,int m,int d){   /* days since 1970-01-01 */
 long long era,yoe,doy,doe;
 y-=m<=2;era=(y>=0?y:y-399)/400;yoe=y-era*400;
 doy=(153*(m+(m>2?-3:9))+2)/5+d-1;doe=yoe*365+yoe/4-yoe/100+doy;
 return era*146097+doe-719468;}
static long long parse_date(const char*s){   /* "Wdy, DD Mon YYYY HH:MM:SS GMT" (also "DD-Mon-YY"); 0 if unparsable */
 static const char*mon[]={"jan","feb","mar","apr","may","jun","jul","aug","sep","oct","nov","dec"};
 int d=0,m=-1,y=0,hh=0,mm=0,ss=0,i;
 while(*s&&!dig(*s))s++;
 while(dig(*s))d=d*10+*s++-'0';
 while(*s&&!isalpha_(*s))s++;
 for(i=0;i<12;i++)if(lw(s[0])==mon[i][0]&&lw(s[1])==mon[i][1]&&lw(s[2])==mon[i][2])m=i;
 if(m<0)return 0;
 while(*s&&!dig(*s))s++;
 while(dig(*s))y=y*10+*s++-'0';
 if(y<100)y+=y<70?2000:1900;
 while(*s&&!dig(*s))s++;
 while(dig(*s))hh=hh*10+*s++-'0';
 if(*s==':')s++;
 while(dig(*s))mm=mm*10+*s++-'0';
 if(*s==':')s++;
 while(dig(*s))ss=ss*10+*s++-'0';
 return days_civil(y,m+1,d)*86400+hh*3600+mm*60+ss;}
static int isalpha_(int c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z');}

/* host / path / scheme of a URL */
static void urlparts(const char*u,char*host,int hc,char*path,int pc,int*https){
 const char*p=u;int i=0;
 *https=has(u,"https://");
 p+=has(u,"https://")?8:has(u,"http://")?7:0;
 while(*p&&*p!='/'&&*p!=':'&&*p!='?'&&*p!='#'&&i<hc-1){host[i]=(char)lw(*p);i++;p++;}
 host[i]=0;
 while(*p&&*p!='/'&&*p!='?'&&*p!='#')p++;   /* skip :port */
 i=0;
 if(*p!='/'){path[i++]='/';}
 else while(*p&&*p!='?'&&*p!='#'&&i<pc-1)path[i++]=*p++;
 path[i]=0;}
static int host_in(const char*host,const char*dom){   /* host equals dom or is a subdomain of it */
 int hl=len(host),dl=len(dom);
 if(hl==dl)return !cmpi(host,dom);
 return hl>dl&&host[hl-dl-1]=='.'&&!cmpi(host+hl-dl,dom);}
static void regdom(const char*h,char*out,int cap){   /* last two labels (no public-suffix list) */
 int l=len(h),i,dots=0;
 for(i=l-1;i>=0;i--){if(h[i]=='.'&&++dots==2){i++;break;}}
 if(i<0)i=0;
 cpy(out,cap,h+i);}
static int samesite(const char*a,const char*b){char x[128],y[128];regdom(a,x,128);regdom(b,y,128);return !cmpi(x,y);}
static int path_match(const char*req,const char*cp){
 int l=len(cp);
 if(!has(req,cp)||(l>0&&req[l]&&cp[l-1]!='/'&&req[l]!='/'))return 0;   /* has() is a case-insensitive prefix test */
 return 1;}

/* store one cookie. line = "name=value; attr; attr"; url = the request (or page) it came from.
 * fromjs: set by a script (HttpOnly not allowed). cross: a cross-site subresource response. */
static void cookie_store(const char*line,const char*url,int fromjs,int cross){
 Ck c;char host[128],rpath[200];int https,i=0,l=len(line);const char*p=line;long long now=mb_plat_time(),maxage=-1,expd=0;int hasmax=0;
 urlparts(url,host,sizeof host,rpath,sizeof rpath,&https);
 for(i=0;i<(int)sizeof c;i++)((char*)&c)[i]=0;
 /* name=value */
 i=0;while(*p&&*p!='='&&*p!=';'&&i<(int)sizeof c.n-1)c.n[i++]=*p++;
 if(*p!='=')return;
 p++;
 {int k=0;char*e;while(*p&&*p!=';'&&k<(int)sizeof c.v-1)c.v[k++]=*p++;c.v[k]=0;
  if(*p&&*p!=';')return;   /* value too long */
  for(e=c.n+i-1;e>=c.n&&*e==' ';e--)*e=0;
  {char*s=c.v+k-1;while(s>=c.v&&*s==' ')*s--=0;}}
 {char*s=c.n;int sh=0;while(*s==' '){s++;sh++;}if(sh){int q;for(q=0;s[q];q++)c.n[q]=s[q];c.n[q]=0;}}
 if(!c.n[0]||l>4096)return;
 c.hostonly=1;
 cpy(c.dom,sizeof c.dom,host);
 {int pl=len(rpath),q=pl;while(q>1&&rpath[q-1]!='/')q--;   /* default path: directory of the request path */
  if(q<=1)q=1;
  {int k;for(k=0;k<q&&k<(int)sizeof c.path-1;k++)c.path[k]=rpath[k];c.path[k]=0;}}
 while(*p==';'){
  char an[32],av[200];int a=0,b=0;
  p++;while(*p==' ')p++;
  while(*p&&*p!='='&&*p!=';'&&a<31)an[a++]=*p++;
  while(a>0&&an[a-1]==' ')a--;
  an[a]=0;
  if(*p=='='){p++;while(*p==' ')p++;while(*p&&*p!=';'&&b<199)av[b++]=*p++;}
  while(b>0&&av[b-1]==' ')b--;
  av[b]=0;
  if(!cmpi(an,"domain")&&av[0]){
   const char*d=av[0]=='.'?av+1:av;
   if(!host_in(host,d)||(!hostdot(d)&&cmpi(host,d)))return;   /* must cover the host; a bare label like "com" is refused */
   cpy(c.dom,sizeof c.dom,d);c.hostonly=0;}
  else if(!cmpi(an,"path")&&av[0]=='/')cpy(c.path,sizeof c.path,av);
  else if(!cmpi(an,"max-age")){int neg=0,k=0;long long v=0;if(av[0]=='-'){neg=1;k=1;}while(dig(av[k])){if(v<1000000000LL)v=v*10+av[k]-'0';k++;}maxage=neg?-1:v;hasmax=1;}
  else if(!cmpi(an,"expires"))expd=parse_date(av);
  else if(!cmpi(an,"secure"))c.secure=1;
  else if(!cmpi(an,"httponly"))c.httponly=1;
  else if(!cmpi(an,"samesite"))c.ss=!cmpi(av,"none")?2:1;}
 if(fromjs&&c.httponly)return;
 if(c.secure&&!https)return;                       /* a Secure cookie only comes over https */
 if(cross&&!(c.ss==2&&c.secure))return;           /* third-party cookies need SameSite=None; Secure */
 c.exp=hasmax?(maxage<=0?1:now+maxage):expd?expd:0;   /* 0 = session; 1 = delete */
 if(c.exp&&c.exp<=now)c.exp=1;
 /* replace an existing cookie with the same name, domain and path */
 for(i=0;i<njar;i++)if(!cmpi(jar[i].n,c.n)&&!cmpi(jar[i].dom,c.dom)&&!strcmp_(jar[i].path,c.path)){
  if(c.exp==1){jar[i]=jar[--njar];return;}
  jar[i]=c;return;}
 if(c.exp==1)return;
 if(njar>=NCK){int k;for(k=1;k<njar;k++)jar[k-1]=jar[k];njar--;}   /* full: drop the oldest */
 jar[njar++]=c;}
static int hostdot(const char*d){while(*d){if(*d=='.')return 1;d++;}return 0;}

/* the Cookie header value for a request to url; cross: a cross-site subresource. js: for document.cookie (no HttpOnly) */
static int cookie_header(const char*url,char*out,int cap,int cross,int js){
 char host[128],path[200];int https,i,o=0;long long now=mb_plat_time();
 urlparts(url,host,sizeof host,path,sizeof path,&https);
 out[0]=0;
 for(i=0;i<njar;i++){
  Ck*c=&jar[i];int l,k;
  if(c->exp&&c->exp<=now){jar[i--]=jar[--njar];continue;}
  if(c->hostonly?cmpi(host,c->dom):!host_in(host,c->dom))continue;
  if(!path_match(path,c->path))continue;
  if(c->secure&&!https)continue;
  if(js&&c->httponly)continue;
  if(cross&&c->ss!=2)continue;
  l=len(c->n)+1+len(c->v);
  if(o+l+3>=cap)break;
  if(o){out[o++]=';';out[o++]=' ';}
  for(k=0;c->n[k];k++)out[o++]=c->n[k];
  out[o++]='=';
  for(k=0;c->v[k];k++)out[o++]=c->v[k];
  out[o]=0;}
 return o;}

/* every Set-Cookie line in the header block b[0..hl) */
static void cookies_from_headers(const char*b,int hl,const char*url,int cross){
 int i=0;
 while(i<hl){
  int e=i;char line[4200];int k=0;
  while(e<hl&&b[e]!='\n')e++;
  if(e-i>11&&has(b+i,"set-cookie:")){
   int s=i+11;
   while(s<e&&b[s]==' ')s++;
   while(s<e&&b[s]!='\r'&&k<(int)sizeof line-1)line[k++]=b[s++];
   line[k]=0;
   cookie_store(line,url,0,cross);}
  i=e+1;}}

/* document.cookie for the current page */
static const char*js_cookie_get(void){
 static char o[4096];
 cookie_header(cur,o,sizeof o,0,1);
 return o;}
static void js_cookie_set(const char*line){cookie_store(line,cur,1,0);}
