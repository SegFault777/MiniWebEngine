/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine v0.2
 * Zero-dependency mini HTML+CSS engine: HTML parse -> CSS cascade -> block/inline
 * layout -> anti-aliased vector-font rasterizer (Inter outlines embedded as
 * compact data, see tools/genfont.py) -> pixels.
 *
 * The engine itself calls no libc function (static arena, own string code).
 * Only the optional CLI (default build) uses stdio: fopen fread fwrite fclose.
 *
 * CLI build : cc -Os mwe.c -o mwe              usage: mwe in.html out.bmp [width]
 * Lib build : cc -Os -DMWE_LIB -c mwe.c        API in mwe.h (used by browser/)
 */
#include "mwe.h"
#include "mwe_font.h"

typedef unsigned char u8;
typedef unsigned int u32;
#define AU (-32768)
#define MC 40000
#define MR 512
#define IN (1<<20)

typedef struct{char*p;int n;}S;
typedef struct{int disp,col,bg,bc,fs,bold,ul,al,m[4],p[4],bw,w,wp,h,mw,lh;}St;
typedef struct N N;
struct N{N*par,*kid,*last,*nx;S tag,txt,id,cls,sty,href;St s;};
typedef struct{S tag,id,cls[2];int nc;}Cp;
typedef struct{Cp c[3];int ns,spec;S d;}Rule;
typedef struct{int k,x,y,w,h,a,c,bg,fs,b,u,n,hn;char*t,*hp;}Cmd;

/* ---- arena (no malloc) ---- */
static long long ar[(1<<22)/8],dmy[64];
static int ap,full;
static void*al(int n){
 char*p;int i;
 n=(n+7)&~7;
 if(ap+n>(int)sizeof ar||n>(int)sizeof dmy){full=1;p=(char*)dmy;n=(int)sizeof dmy;}
 else{p=(char*)ar+ap;ap+=n;}
 for(i=0;i<n;i++)p[i]=0;
 return p;}

/* ---- strings / utf-8 ---- */
static int lc(int c){return c>='A'&&c<='Z'?c+32:c;}
static int isws(int c){return c==' '||c=='\n'||c=='\t'||c=='\r'||c=='\f';}
static int isa(int c){c=lc(c);return c>='a'&&c<='z';}
static int isal(int c){c=lc(c);return isa(c)||(c>='0'&&c<='9')||c=='-'||c=='_';}
static int sl(const char*s){int n=0;while(s[n])n++;return n;}
static int seq(S a,const char*b){int i=0;for(;i<a.n;i++)if(!b[i]||lc(a.p[i])!=lc(b[i]))return 0;return!b[i];}
static int seqs(S a,S b){int i;if(a.n!=b.n)return 0;for(i=0;i<a.n;i++)if(lc(a.p[i])!=lc(b.p[i]))return 0;return 1;}
static S trim(S s){while(s.n&&isws(*s.p)){s.p++;s.n--;}while(s.n&&isws(s.p[s.n-1]))s.n--;return s;}
static int tk(S v,S*t,int mx){int n=0,i=0,a,pd;while(i<v.n&&n<mx){while(i<v.n&&isws(v.p[i]))i++;if(i>=v.n)break;a=i;pd=0;while(i<v.n&&(pd||!isws(v.p[i]))){if(v.p[i]=='(')pd++;else if(v.p[i]==')')pd--;i++;}t[n].p=v.p+a;t[n].n=i-a;n++;}return n;}
static int u8d(const char*p,int n,int*i){
 int c=(u8)p[*i],k;
 (*i)++;
 if(c<0x80)return c;
 if(c>=0xF0){k=3;c&=7;}else if(c>=0xE0){k=2;c&=15;}else if(c>=0xC0){k=1;c&=31;}else return 0xFFFD;
 while(k--&&*i<n&&(p[*i]&0xC0)==0x80)c=c<<6|(p[(*i)++]&63);
 return c;}
static int put8(char*p,int c){
 if(c<0x80){p[0]=c;return 1;}
 if(c<0x800){p[0]=0xC0|c>>6;p[1]=0x80|(c&63);return 2;}
 if(c<0x10000){p[0]=0xE0|c>>12;p[1]=0x80|(c>>6&63);p[2]=0x80|(c&63);return 3;}
 p[0]=0xF0|c>>18;p[1]=0x80|(c>>12&63);p[2]=0x80|(c>>6&63);p[3]=0x80|(c&63);return 4;}

/* ---- font metrics ---- */
static int gi(int cp){int i;if(cp>=32&&cp<=126)return cp-32;for(i=95;i<MF_N;i++)if(MF_CP[i]==cp)return i;return '?'-32;}
static int wid(const char*p,int n,int fs,int b){int i=0;long long s=0;while(i<n)s+=MF_ADV[b][gi(u8d(p,n,&i))];s=(s*fs+512)>>10;return s>100000000?100000000:(int)s;}

/* ---- CSS values ---- */
static const struct{const char*n;int c;}CN[]={{"black",0},{"white",0xffffff},{"red",0xff0000},{"green",0x008000},{"blue",0x0000ff},{"gray",0x808080},{"grey",0x808080},{"yellow",0xffff00},{"orange",0xffa500},{"purple",0x800080},{"silver",0xc0c0c0},{"navy",0x000080},{"teal",0x008080},{"maroon",0x800000},{"lime",0x00ff00},{"aqua",0x00ffff},{"fuchsia",0xff00ff},{"olive",0x808000},{0,0}};
static int hx(int c){c=lc(c);return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;}
static int clr(S v){ /* 0xRRGGBB, -1 transparent, -2 invalid */
 int i,r=0,d,k,g[3];
 if(v.n&&v.p[0]=='#'){
  if(v.n!=4&&v.n!=7)return-2;
  for(i=1;i<v.n;i++){if((d=hx(v.p[i]))<0)return-2;r=r*16+d;}
  if(v.n==7)return r;
  if(v.n==4)return(((r>>8)&15)*17<<16)|(((r>>4)&15)*17<<8)|((r&15)*17);
  return-2;}
 if(v.n>4&&lc(v.p[0])=='r'&&lc(v.p[1])=='g'&&lc(v.p[2])=='b'){
  i=0;while(i<v.n&&v.p[i]!='(')i++;
  for(k=0;k<3;k++){while(i<v.n&&!(v.p[i]>='0'&&v.p[i]<='9'))i++;g[k]=0;while(i<v.n&&v.p[i]>='0'&&v.p[i]<='9'&&g[k]<999)g[k]=g[k]*10+v.p[i++]-'0';if(g[k]>255)g[k]=255;}
  return(g[0]<<16)|(g[1]<<8)|g[2];}
 if(seq(v,"transparent")||seq(v,"none"))return-1;
 for(i=0;CN[i].n;i++)if(seq(v,CN[i].n))return CN[i].c;
 return-2;}
static int ln(S v,int fs,int*pc){ /* px; AU if not numeric; percent -> *pc */
 int i=0,ng=0,iv=0,fr=0,fd=1,dg=0,r;
 if(v.n&&(v.p[0]=='-'||v.p[0]=='+')){ng=v.p[0]=='-';i++;}
 for(;i<v.n&&v.p[i]>='0'&&v.p[i]<='9';i++,dg=1)if(iv<100000)iv=iv*10+v.p[i]-'0';
 if(i<v.n&&v.p[i]=='.'){i++;for(;i<v.n&&v.p[i]>='0'&&v.p[i]<='9';i++,dg=1)if(fd<1000){fr=fr*10+v.p[i]-'0';fd*=10;}}
 if(!dg)return AU;
 if(i<v.n&&v.p[i]=='%'){if(pc)*pc=iv;return 0;}
 r=(i+1<v.n&&lc(v.p[i])=='e'&&lc(v.p[i+1])=='m')?(int)(((long long)iv*fd+fr)*fs/fd):iv;
 if(r>100000)r=100000;
 return ng?-r:r;}
static int num100(S v){ /* value*100, -1 if not numeric */
 int i=0,iv=0,fr=0,fd=1,dg=0;
 for(;i<v.n&&v.p[i]>='0'&&v.p[i]<='9';i++,dg=1)if(iv<100000)iv=iv*10+v.p[i]-'0';
 if(i<v.n&&v.p[i]=='.'){i++;for(;i<v.n&&v.p[i]>='0'&&v.p[i]<='9'&&fd<100;i++,dg=1){fr=fr*10+v.p[i]-'0';fd*=10;}}
 return dg?iv*100+fr*100/fd:-1;}
static void edge(int*a,S v,int fs,int pad){
 S t[4];int n=tk(v,t,4),x[4],i;
 if(!n)return;
 for(i=0;i<n;i++){x[i]=ln(t[i],fs,0);if(pad&&x[i]<0)x[i]=0;}
 a[0]=x[0];a[1]=n>1?x[1]:x[0];a[2]=n>2?x[2]:x[0];a[3]=n>3?x[3]:a[1];}
static void prop(St*s,S k,S v){
 static const char*mp[]={"margin-top","margin-right","margin-bottom","margin-left"};
 static const char*pp[]={"padding-top","padding-right","padding-bottom","padding-left"};
 S t[4];int n=tk(v,t,4),i,c,pc=-1,x;
 if(!n)return;
 if(seq(k,"color")){c=clr(t[0]);if(c!=-2)s->col=c;}
 else if(seq(k,"background")||seq(k,"background-color")){for(i=0;i<n;i++)if((c=clr(t[i]))!=-2){s->bg=c;break;}}
 else if(seq(k,"font-size")){x=ln(t[0],s->fs,&pc);if(pc>=0)x=s->fs*pc/100;if(x>0)s->fs=x>180?180:x;}
 else if(seq(k,"font-weight"))s->bold=seq(t[0],"bold")||seq(t[0],"bolder")||(t[0].p[0]>='6'&&t[0].p[0]<='9');
 else if(seq(k,"line-height")){x=num100(t[0]);s->lh=x<0?0:t[0].p[t[0].n-1]=='x'?x/s->fs:x;}
 else if(seq(k,"text-decoration")){for(i=0;i<n;i++){if(seq(t[i],"underline"))s->ul=1;else if(seq(t[i],"none"))s->ul=0;}}
 else if(seq(k,"text-align"))s->al=seq(t[0],"center")?1:seq(t[0],"right")?2:0;
 else if(seq(k,"display"))s->disp=seq(t[0],"none")?0:(seq(t[0],"inline")||seq(t[0],"inline-block"))?2:1;
 else if(seq(k,"margin"))edge(s->m,v,s->fs,0);
 else if(seq(k,"padding"))edge(s->p,v,s->fs,1);
 else if(seq(k,"width")){x=ln(t[0],s->fs,&pc);s->wp=pc;s->w=(pc>=0||x==AU)?-1:x;}
 else if(seq(k,"height")){x=ln(t[0],s->fs,0);s->h=(x==AU||x<0)?-1:x;}
 else if(seq(k,"max-width")){x=ln(t[0],s->fs,&pc);s->mw=(pc>=0||x==AU)?-1:x;}
 else if(seq(k,"border")){for(i=0;i<n;i++){if(seq(t[i],"none")||seq(t[i],"hidden"))s->bw=0;else if((x=ln(t[i],s->fs,0))!=AU)s->bw=x<0?0:x;else if((c=clr(t[i]))!=-2)s->bc=c;}}
 else if(seq(k,"border-width")){x=ln(t[0],s->fs,0);s->bw=x<0?0:x;}
 else if(seq(k,"border-color")){c=clr(t[0]);if(c!=-2)s->bc=c;}
 else for(i=0;i<4;i++){
  if(seq(k,mp[i]))s->m[i]=ln(t[0],s->fs,0);
  else if(seq(k,pp[i])){x=ln(t[0],s->fs,0);s->p[i]=x<0?0:x;}}}
static void decl(St*s,S d){
 int i=0,a,b,pd,e;S k,v;
 while(i<d.n){
  a=i;while(i<d.n&&d.p[i]!=':'&&d.p[i]!=';')i++;
  if(i>=d.n||d.p[i]==';'){i++;continue;}
  k=trim((S){d.p+a,i-a});i++;
  b=i;pd=0;while(i<d.n&&(d.p[i]!=';'||pd)){if(d.p[i]=='(')pd++;else if(d.p[i]==')')pd--;i++;}
  v=(S){d.p+b,i-b};i++;
  for(e=0;e<v.n;e++)if(v.p[e]=='!'){v.n=e;break;}
  prop(s,k,trim(v));}}

/* ---- DOM + HTML parser ---- */
static N*root;static S ttl;
static N*nn(S tag){N*n=al(sizeof(N));n->tag=tag;return n;}
static void add(N*p,N*n){n->par=p;if(p->last)p->last->nx=n;else p->kid=n;p->last=n;}
static int isvoid(S t){static const char*v[]={"br","hr","img","meta","link","input",0};int i;for(i=0;v[i];i++)if(seq(t,v[i]))return 1;return 0;}
static int blkt(S t){static const char*v[]={"p","div","ul","ol","li","hr","pre","blockquote","table","section","article","header","footer","nav","main","aside","form","h1","h2","h3","h4","h5","h6",0};int i;for(i=0;v[i];i++)if(seq(t,v[i]))return 1;return 0;}
static int istag(const char*s,int i,int n){return s[i]=='<'&&i+1<n&&(isa(s[i+1])||s[i+1]=='/'||s[i+1]=='!'||s[i+1]=='?');}
static const struct{const char*n;int c;}EN[]={{"amp",'&'},{"lt",'<'},{"gt",'>'},{"quot",'"'},{"apos",'\''},{"nbsp",0xA0},{"copy",0xA9},{"reg",0xAE},{"deg",0xB0},{"middot",0xB7},{"times",0xD7},{"laquo",0xAB},{"raquo",0xBB},{"ndash",0x2013},{"mdash",0x2014},{"lsquo",0x2018},{"rsquo",0x2019},{"ldquo",0x201C},{"rdquo",0x201D},{"bull",0x2022},{"hellip",0x2026},{"rarr",0x2192},{"euro",0x20AC},{0,0}};
static S dec(S t){ /* in-place entity decode; output never longer than input */
 char*p=t.p;int o=0,i=0,j,c,v,k;S e;
 while(i<t.n){
  if(p[i]=='&'){
   j=i+1;while(j<t.n&&j-i<10&&p[j]!=';'&&!isws(p[j]))j++;
   if(j<t.n&&p[j]==';'){
    e=(S){p+i+1,j-i-1};c=0;
    for(k=0;EN[k].n;k++)if(seq(e,EN[k].n)){c=EN[k].c;break;}
    if(!c&&e.n>1&&e.p[0]=='#'){
     v=0;
     if(lc(e.p[1])=='x'){for(k=2;k<e.n&&hx(e.p[k])>=0&&v<0x110000;k++)v=v*16+hx(e.p[k]);}
     else for(k=1;k<e.n&&e.p[k]>='0'&&e.p[k]<='9'&&v<0x110000;k++)v=v*10+e.p[k]-'0';
     c=(v>0&&v<0x110000&&!(v>=0xD800&&v<0xE000))?v:0xFFFD;}
    if(c){o+=put8(p+o,c);i=j+1;continue;}}}
  p[o++]=p[i++];}
 t.n=o;return t;}

/* ---- CSS rules ---- */
static Rule rl[MR];static int nr;
static void addsel(S s,S d){
 Rule*r;Cp*c;int i=0,j,st;char k;S v;
 if(nr>=MR)return;
 r=&rl[nr];r->ns=0;r->spec=0;r->d=d;
 while(i<s.n){
  while(i<s.n&&(isws(s.p[i])||s.p[i]=='>'||s.p[i]=='+'||s.p[i]=='~'))i++;
  if(i>=s.n)break;
  if(r->ns>=3)return;
  c=&r->c[r->ns++];c->tag.n=c->id.n=c->nc=0;
  while(i<s.n&&!isws(s.p[i])&&s.p[i]!='>'&&s.p[i]!='+'&&s.p[i]!='~'){
   k=s.p[i];
   if(k=='*'){i++;continue;}
   st=i+(k=='#'||k=='.');j=st;while(j<s.n&&isal(s.p[j]))j++;
   if(j==st)return; /* pseudo/attr/unknown -> drop rule */
   v=(S){s.p+st,j-st};
   if(k=='#'){c->id=v;r->spec+=10000;}else if(k=='.'){if(c->nc<2)c->cls[c->nc++]=v;r->spec+=100;}else{c->tag=v;r->spec+=1;}
   i=j;}}
 if(r->ns)nr++;}
static void css(S t){
 int i=0,a,b,d,be,p0,q;
 while(i<t.n){
  while(i<t.n&&isws(t.p[i]))i++;
  if(i+1<t.n&&t.p[i]=='/'&&t.p[i+1]=='*'){i+=2;while(i+1<t.n&&!(t.p[i]=='*'&&t.p[i+1]=='/'))i++;i+=2;continue;}
  if(i>=t.n)break;
  a=i;while(i<t.n&&t.p[i]!='{'&&t.p[i]!=';')i++;
  if(i>=t.n)break;
  if(t.p[i]==';'){i++;continue;}
  be=i;i++;b=i;d=1;
  while(i<t.n&&d){if(t.p[i]=='{')d++;else if(t.p[i]=='}')d--;i++;}
  if(t.p[a]=='@')continue;
  d=i-(d?0:1);
  p0=a;
  for(q=a;q<=be;q++)if(q==be||t.p[q]==','){addsel(trim((S){t.p+p0,q-p0}),(S){t.p+b,d-b});p0=q+1;}}}
static int hc(S cl,S k){int i=0,a;while(i<cl.n){while(i<cl.n&&isws(cl.p[i]))i++;a=i;while(i<cl.n&&!isws(cl.p[i]))i++;if(i>a&&seqs((S){cl.p+a,i-a},k))return 1;}return 0;}
static int cp1(N*n,Cp*c){int k;if(c->tag.n&&!seqs(n->tag,c->tag))return 0;if(c->id.n&&!seqs(n->id,c->id))return 0;for(k=0;k<c->nc;k++)if(!hc(n->cls,c->cls[k]))return 0;return 1;}
static int mtch(N*n,Rule*r){int k=r->ns-1;N*a;if(!cp1(n,&r->c[k]))return 0;a=n->par;for(k--;k>=0;k--){while(a&&!cp1(a,&r->c[k]))a=a->par;if(!a)return 0;a=a->par;}return 1;}
static void stl(N*n,St*pa){
 N*c;St*s;int i;
 for(c=n->kid;c;c=c->nx){
  if(!c->tag.n)continue;
  s=&c->s;
  *s=*pa;s->disp=2;s->bg=-1;s->bc=-1;
  for(i=0;i<4;i++)s->m[i]=s->p[i]=0;
  s->bw=0;s->w=s->wp=s->h=s->mw=-1;
  for(i=0;i<nr;i++)if(mtch(c,&rl[i]))decl(s,rl[i].d);
  if(c->sty.n)decl(s,c->sty);
  stl(c,s);}}
static N*parse(char*s,int n){
 N*rt=nn((S){"#",1}),*cur=rt,*e,*x;
 int i=0,dp=0,a,j,k,sc,ra,va;S nm,key,v,t;char q;N*p;
 while(i<n&&!full){
  if(!istag(s,i,n)){
   a=i++;while(i<n&&!istag(s,i,n))i++;
   t=dec((S){s+a,i-a});
   if(t.n){x=nn((S){0,0});x->txt=t;add(cur,x);if(cur!=rt&&seq(cur->tag,"title"))ttl=t;}
   continue;}
  if(s[i+1]=='!'||s[i+1]=='?'){
   if(i+3<n&&s[i+2]=='-'&&s[i+3]=='-'){i+=4;while(i+2<n&&!(s[i]=='-'&&s[i+1]=='-'&&s[i+2]=='>'))i++;i+=3;}
   else{while(i<n&&s[i]!='>')i++;i++;}
   continue;}
  if(s[i+1]=='/'){
   a=i+2;j=a;while(j<n&&isal(s[j]))j++;
   nm=(S){s+a,j-a};while(j<n&&s[j]!='>')j++;i=j+1;
   p=cur;k=0;while(p!=rt&&!seqs(p->tag,nm)){p=p->par;k++;}
   if(p!=rt){cur=p->par;dp-=k+1;}
   continue;}
  a=i+1;j=a;while(j<n&&isal(s[j]))j++;
  nm=(S){s+a,j-a};e=nn(nm);sc=0;
  for(;;){
   while(j<n&&isws(s[j]))j++;
   if(j>=n)break;
   if(s[j]=='>'){j++;break;}
   if(s[j]=='/'){sc=1;j++;continue;}
   a=j;while(j<n&&!isws(s[j])&&s[j]!='='&&s[j]!='>'&&s[j]!='/')j++;
   key=(S){s+a,j-a};
   while(j<n&&isws(s[j]))j++;
   v=(S){0,0};
   if(j<n&&s[j]=='='){
    j++;while(j<n&&isws(s[j]))j++;
    if(j<n&&(s[j]=='"'||s[j]=='\'')){q=s[j++];va=j;while(j<n&&s[j]!=q)j++;v=(S){s+va,j-va};j++;}
    else{va=j;while(j<n&&!isws(s[j])&&s[j]!='>')j++;v=(S){s+va,j-va};}}
   if(key.n){if(seq(key,"id"))e->id=v;else if(seq(key,"class"))e->cls=v;else if(seq(key,"style"))e->sty=v;else if(seq(key,"href"))e->href=dec(v);}}
  if(cur!=rt&&((seq(cur->tag,"p")&&blkt(nm))||(seq(nm,"li")&&seq(cur->tag,"li")))){cur=cur->par;dp--;}
  add(cur,e);
  if(seq(nm,"style")||seq(nm,"script")){
   ra=j;while(j<n&&!(s[j]=='<'&&j+2+nm.n<=n&&s[j+1]=='/'&&seqs((S){s+j+2,nm.n},nm)))j++;
   if(seq(nm,"style"))css((S){s+ra,j-ra});
   while(j<n&&s[j]!='>')j++;j++;}
  else if(!sc&&!isvoid(nm)&&dp<200){cur=e;dp++;}
  i=j;}
 return rt;}

/* ---- paint target + vector font rasterizer ---- */
static unsigned*pb;
static int pst,px0,py0,kx0,ky0,kx1,ky1;
static void tgt(unsigned*b,int st,int x,int y,int w,int h){pb=b;pst=st;px0=x;py0=y;kx0=x;ky0=y;kx1=x+w;ky1=y+h;}
static void fr(int x,int y,int w,int h,int c){
 int x1=x+w,y1=y+h,xx;
 if(c<0)return;
 if(x<kx0)x=kx0;if(y<ky0)y=ky0;if(x1>kx1)x1=kx1;if(y1>ky1)y1=ky1;
 for(;y<y1;y++)for(xx=x;xx<x1;xx++)pb[(y-py0)*pst+(xx-px0)]=c;}
static void bl(int x,int y,int c,int a){
 unsigned*d,o;int r,g,b;
 if(x<kx0||x>=kx1||y<ky0||y>=ky1)return;
 d=&pb[(y-py0)*pst+(x-px0)];o=*d;
 r=((o>>16&255)*(255-a)+(c>>16&255)*a+127)/255;
 g=((o>>8&255)*(255-a)+(c>>8&255)*a+127)/255;
 b=((o&255)*(255-a)+(c&255)*a+127)/255;
 *d=(r<<16)|(g<<8)|b;}

#define MS 700
typedef struct{short g,fs,ox,oy;u8 b,w,h,used;int off;}GC;
static GC gc[1024];
static u8 pool[1<<20];
static int pp;
static float acc[65536],sg[MS][6];
static u8 sgq[MS];
static int nsg;
static float mnx,mny,mxx,mxy;
static int fl(float v){int i=(int)v;return v<(float)i?i-1:i;}
static int ce(float v){return-fl(-v);}
static void ext(float x,float y){if(x<mnx)mnx=x;if(x>mxx)mxx=x;if(y<mny)mny=y;if(y>mxy)mxy=y;}
static void sadd(int q,float a,float b,float c,float d,float e,float f){
 if(nsg>=MS)return;
 sgq[nsg]=q;sg[nsg][0]=a;sg[nsg][1]=b;sg[nsg][2]=c;sg[nsg][3]=d;sg[nsg][4]=e;sg[nsg][5]=f;nsg++;
 ext(a,b);ext(c,d);if(q)ext(e,f);}
static int rv(const u8**p){int z=0,sh=0,c;do{c=*(*p)++;z|=(c&127)<<sh;sh+=7;}while(c&128);return(z&1)?-(z>>1)-1:(z>>1);}
/* signed-area accumulation (coverage) line */
static void dl(float x0,float y0,float x1,float y1,int w,int h){
 float dir=1,dxdy,x,dy,xn,d,xa,xb,fa,cb,xm,s,x0f,a0,x1f,am,a1,a2,lo,*r;
 int y,ys,ye,x0i,x1i,xi;
 if(y0==y1)return;
 if(y0>y1){float t;t=x0;x0=x1;x1=t;t=y0;y0=y1;y1=t;dir=-1;}
 dxdy=(x1-x0)/(y1-y0);x=x0;
 ys=y0<0?0:(int)y0;
 if(y0<0)x-=y0*dxdy;
 ye=(int)y1;if((float)ye<y1)ye++;if(ye>h)ye=h;
 for(y=ys;y<ye;y++){
  r=acc+y*(w+2);
  dy=(float)(y+1);if(dy>y1)dy=y1;
  lo=(float)y;if(lo<y0)lo=y0;
  dy-=lo;xn=x+dxdy*dy;d=dy*dir;
  if(x<xn){xa=x;xb=xn;}else{xa=xn;xb=x;}
  if(xa<0)xa=0;if(xb<0)xb=0;if(xa>(float)w)xa=(float)w;if(xb>(float)w)xb=(float)w;
  x0i=(int)xa;fa=(float)x0i;
  x1i=(int)xb;cb=(float)x1i;if(cb<xb){cb+=1;x1i++;}
  if(x1i<=x0i+1){xm=0.5f*(x+xn)-fa;if(xm<0)xm=0;if(xm>1)xm=1;r[x0i]+=d-d*xm;r[x0i+1]+=d*xm;}
  else{
   s=1.0f/(xb-xa);x0f=xa-fa;a0=0.5f*s*(1-x0f)*(1-x0f);x1f=xb-cb+1;am=0.5f*s*x1f*x1f;
   r[x0i]+=d*a0;
   if(x1i==x0i+2)r[x0i+1]+=d*(1-a0-am);
   else{a1=s*(1.5f-x0f);r[x0i+1]+=d*(a1-a0);for(xi=x0i+2;xi<x1i-1;xi++)r[xi]+=d*s;a2=a1+(float)(x1i-x0i-3)*s;r[x1i-1]+=d*(1-a2-am);}
   r[x1i]+=d*am;}
  x=xn;}}
static void dq(float x0,float y0,float cx,float cy,float x1,float y1,int w,int h){
 float dv,t,u,px,py,qx,qy;int n=1,i;
 dv=x0-2*cx+x1;if(dv<0)dv=-dv;t=y0-2*cy+y1;if(t<0)t=-t;dv+=t;
 while((float)(n*n)*0.2f<dv&&n<16)n++;
 px=x0;py=y0;
 for(i=1;i<=n;i++){t=(float)i/(float)n;u=1-t;qx=u*u*x0+2*u*t*cx+t*t*x1;qy=u*u*y0+2*u*t*cy+t*t*y1;dl(px,py,qx,qy,w,h);px=qx;py=qy;}}
static GC*glyph(int g,int fs,int b){
 GC*e=&gc[(g*131+fs*7+b*3)&1023];
 const u8*p;float k=(float)fs/(float)MF_UPM;
 int cx=0,cy=0,sx=0,sy=0,c,qx,qy,x,y,i,j,w,h,ox,oy;float s,v;u8*o;
 if(e->used&&e->g==g&&e->fs==fs&&e->b==b)return e;
 nsg=0;mnx=mny=1e9f;mxx=mxy=-1e9f;
 p=(b?MF_D1:MF_D0)+MF_OFF[b][g];
 while((c=*p++)){
  if(c==1){cx+=rv(&p);cy+=rv(&p);sx=cx;sy=cy;}
  else if(c==2){x=cx+rv(&p);y=cy+rv(&p);sadd(0,cx*k,-cy*k,x*k,-y*k,0,0);cx=x;cy=y;}
  else if(c==3){qx=cx+rv(&p);qy=cy+rv(&p);x=qx+rv(&p);y=qy+rv(&p);sadd(1,cx*k,-cy*k,qx*k,-qy*k,x*k,-y*k);cx=x;cy=y;}
  else{if(cx!=sx||cy!=sy)sadd(0,cx*k,-cy*k,sx*k,-sy*k,0,0);cx=sx;cy=sy;}}
 w=h=0;ox=oy=0;
 if(nsg){ox=fl(mnx);oy=fl(mny);w=ce(mxx)-ox;h=ce(mxy)-oy;if(w<=0||h<=0||w>250||h>250)w=h=0;}
 if(pp+w*h>(int)sizeof pool){for(i=0;i<1024;i++)gc[i].used=0;pp=0;}
 e->used=1;e->g=g;e->fs=fs;e->b=b;e->w=w;e->h=h;e->ox=ox;e->oy=oy;e->off=pp;
 if(!w)return e;
 for(i=0;i<(w+2)*h;i++)acc[i]=0;
 for(i=0;i<nsg;i++){
  if(sgq[i])dq(sg[i][0]-ox,sg[i][1]-oy,sg[i][2]-ox,sg[i][3]-oy,sg[i][4]-ox,sg[i][5]-oy,w,h);
  else dl(sg[i][0]-ox,sg[i][1]-oy,sg[i][2]-ox,sg[i][3]-oy,w,h);}
 o=pool+pp;pp+=w*h;
 for(j=0;j<h;j++){s=0;for(i=0;i<w;i++){s+=acc[j*(w+2)+i];v=s<0?-s:s;if(v>1)v=1;o[j*w+i]=(u8)(v*255+0.5f);}}
 return e;}
static void gl(int x,int base,int g,int fs,int b,int col){
 GC*e=glyph(g,fs,b);u8*bm;int i,j,a;
 if(!e->w)return;
 bm=pool+e->off;
 for(j=0;j<e->h;j++)for(i=0;i<e->w;i++)if((a=bm[j*e->w+i]))bl(x+e->ox+i,base+e->oy+j,col,a);}
static void run(int x,int base,const char*s,int n,int fs,int b,int col,int ul,int w){
 int i=0,pre=0,g;
 while(i<n){g=gi(u8d(s,n,&i));gl(x+((pre*fs+512)>>10),base,g,fs,b,col);pre+=MF_ADV[b][g];}
 if(ul)fr(x,base+fs/10+1,w,fs/14+1,col);}

/* ---- layout ---- */
static Cmd cm[MC],dm;
static int nc,W,H,cv=0xffffff;
static int Y,pend,lx,cx,cw,LH,LA,LD,lf,ca,sp;
static Cmd*bulp;static S*curh;static St base;
static Cmd*cadd(int k,int x,int y,int w,int h,int c){
 Cmd*p=nc<MC?&cm[nc++]:&dm;
 p->k=k;p->x=x;p->y=y;p->w=w;p->h=h;p->a=0;p->c=c;p->bg=-1;p->fs=16;p->b=p->u=p->n=p->hn=0;p->t=p->hp=0;return p;}
static void nl(void){
 int i,dx,sl_;
 if(!LH){lx=cx;return;}
 sl_=cw-(lx-cx);dx=ca==1?sl_/2:ca==2?sl_:0;if(dx<0)dx=0;
 for(i=lf;i<nc;i++){cm[i].x+=dx;cm[i].y=Y+LA-cm[i].a;}
 Y+=LH;LH=LA=LD=0;lx=cx;lf=nc;sp=0;}
static void ls(void){if(!LH){Y+=pend;pend=0;if(bulp){bulp->y=Y;bulp=0;}}}
static void mt0(St*s,int*as,int*de){
 int fs=s->fs,L,hf;
 *as=(fs*MF_ASC+512)>>10;*de=(fs*MF_DESC+512)>>10;
 if(s->lh>0){L=fs*s->lh/100;hf=(L-*as-*de)/2;*as+=hf;*de=L-*as;if(*de<0)*de=0;}}
static void met(St*s,int*a,int*h){
 int as,de;
 mt0(s,&as,&de);*a=as;*h=as+de;
 if(as>LA)LA=as;if(de>LD)LD=de;LH=LA+LD;}
static void word(char*p,int n,N*e){
 St*s=&e->s;int fs=s->fs,b=s->bold,ww=wid(p,n,fs,b),spw=(sp&&LH)?(MF_ADV[b][0]*fs+512)>>10:0,a,h;Cmd*c;
 if(LH&&lx+spw+ww>cx+cw){nl();spw=0;}
 ls();lx+=spw;
 met(s,&a,&h);
 c=cadd(1,lx,0,ww,h,s->col);
 c->a=a;c->bg=s->disp==2?s->bg:-1;c->fs=fs;c->b=b;c->u=s->ul;c->t=p;c->n=n;
 if(curh){c->hp=curh->p;c->hn=curh->n;}
 lx+=ww;sp=0;}
static void txt(N*n){
 S t=n->txt;int i=0,j,k,m,q,a,fs=n->par->s.fs,b=n->par->s.bold;long long acc;
 while(i<t.n){
  if(isws(t.p[i])){sp=1;i++;continue;}
  j=i;while(j<t.n&&!isws(t.p[j]))j++;
  while(i<j){ /* one word; hard-break it only if wider than the line */
   acc=0;k=m=i;
   while(k<j){q=k;a=MF_ADV[b][gi(u8d(t.p,j,&q))];if(m>i&&cw>0&&(((acc+a)*fs+512)>>10)>cw)break;acc+=a;k=q;m=q;}
   word(t.p+i,m-i,n->par);i=m;}}}
static void blk(N*n,int ax,int aw);
static void walk(N*n){
 N*c;St*s;int a,h;S*old;
 for(c=n->kid;c;c=c->nx){
  if(!c->tag.n){txt(c);continue;}
  s=&c->s;if(!s->disp)continue;
  old=curh;if(c->href.n&&seq(c->tag,"a"))curh=&c->href;
  if(seq(c->tag,"br")){ls();met(s,&a,&h);nl();}
  else if(s->disp==1)blk(c,cx,cw);else walk(c);
  curh=old;}}
static void blk(N*n,int ax,int aw){
 St*s=&n->s;int b=s->bw,ml=s->m[3],mr=s->m[1],mt=s->m[0],mb=s->m[2];
 int eh=s->p[1]+s->p[3]+2*b,w,fr_,box,tt,bt,top,x0,tw,ys,ocx=cx,ocw=cw,oca=ca,bc,as,de;Cmd*bp=0;
 if(mt==AU)mt=0;
 if(mb==AU)mb=0;
 if(s->wp>=0)w=aw*s->wp/100;else if(s->w>=0)w=s->w;else w=aw-(ml==AU?0:ml)-(mr==AU?0:mr)-eh;
 if(s->mw>=0&&w>s->mw)w=s->mw;
 if(w<0)w=0;
 fr_=aw-w-eh;
 if(ml==AU){ml=(mr==AU)?fr_/2:fr_-mr;if(ml<0)ml=0;}
 nl();
 box=s->bg>=0||b>0;tt=!box&&!s->p[0]&&s->h<0;bt=!box&&!s->p[2]&&s->h<0;
 if(tt){if(mt>pend)pend=mt;}else{Y+=mt>pend?mt:pend;pend=0;}
 top=Y;x0=ax+ml;tw=w+eh;
 if(s->bg>=0)bp=cadd(0,x0,top,tw,0,s->bg);
 if(seq(n->tag,"li")){ /* bullet: U+2022 glyph, placed when the first line starts */
  mt0(s,&as,&de);
  bulp=cadd(1,0,0,wid("\xE2\x80\xA2",3,s->fs,0),as+de,s->col);
  bulp->a=as;bulp->fs=s->fs;bulp->t=(char*)"\xE2\x80\xA2";bulp->n=3;
  bulp->x=ax+ml+b+s->p[3]-bulp->w-s->fs/3;}
 if(!tt)Y+=b+s->p[0];
 cx=ax+ml+b+s->p[3];cw=w;ca=s->al;lx=cx;LH=LA=LD=0;sp=0;lf=nc;ys=Y;
 walk(n);nl();
 if(s->h>=0){Y=ys+s->h;pend=0;}
 if(bt){if(mb>pend)pend=mb;}else{Y+=pend+s->p[2]+b;pend=mb;}
 if(bp)bp->h=Y-top;
 if(b>0){bc=s->bc<0?s->col:s->bc;
  cadd(0,x0,top,tw,b,bc);cadd(0,x0,Y-b,tw,b,bc);cadd(0,x0,top,b,Y-top,bc);cadd(0,x0+tw-b,top,b,Y-top,bc);}
 cx=ocx;cw=ocw;ca=oca;lx=cx;LH=LA=LD=0;sp=0;lf=nc;}

/* ---- UA stylesheet ---- */
static const char UA[]=
"html,body,div,p,h1,h2,h3,h4,h5,h6,ul,ol,li,hr,pre,blockquote,section,article,header,footer,nav,main,aside,form,table,tr,center{display:block}"
"head,style,script,title,meta,link{display:none}"
"body{margin:8px}p{margin:1em 0}"
"h1{font-size:32px;font-weight:bold;margin:.67em 0}h2{font-size:24px;font-weight:bold;margin:.83em 0}"
"h3{font-size:19px;font-weight:bold;margin:1em 0}h4,h5,h6{font-size:16px;font-weight:bold;margin:1em 0}"
"b,strong,th{font-weight:bold}u,ins{text-decoration:underline}a{color:#0b57d0;text-decoration:underline}"
"ul,ol{margin:1em 0;padding-left:28px}hr{height:1px;background:#bbb;margin:12px 0}"
"blockquote{margin:1em 40px}center{text-align:center}";

/* ---- public API ---- */
int mwe_load(char*html,int len){
 int i,r;Rule t;
 ap=0;full=0;nr=0;ttl.n=0;ttl.p=0;nc=0;
 css((S){(char*)UA,sl(UA)});
 root=parse(html,len);
 for(i=1;i<nr;i++){t=rl[i];r=i-1;while(r>=0&&rl[r].spec>t.spec){rl[r+1]=rl[r];r--;}rl[r+1]=t;}
 base=(St){1,0,-1,-1,16,0,0,0,{0,0,0,0},{0,0,0,0},0,-1,-1,-1,-1,0};
 root->s=base;stl(root,&base);
 return full;}
int mwe_layout(int width){
 N*c,*d;
 if(width<16)width=16;
 W=width;nc=0;Y=0;pend=0;LH=LA=LD=0;lx=cx=0;cw=W;lf=0;ca=0;sp=0;bulp=0;curh=0;
 walk(root);nl();
 H=Y+pend;if(H<1)H=1;if(H>400000)H=400000;
 cv=0xffffff;
 for(c=root->kid;c;c=c->nx){
  if(seq(c->tag,"html")){if(c->s.bg>=0)cv=c->s.bg;for(d=c->kid;d;d=d->nx)if(seq(d->tag,"body")&&d->s.bg>=0)cv=d->s.bg;}
  else if(seq(c->tag,"body")&&c->s.bg>=0)cv=c->s.bg;}
 return H;}
void mwe_paint(unsigned*buf,int stride,int x,int y,int w,int h){
 int i,j;Cmd*k;
 tgt(buf,stride,x,y,w,h);
 for(j=0;j<h;j++)for(i=0;i<w;i++)buf[j*stride+i]=cv;
 for(i=0;i<nc;i++){
  k=&cm[i];
  if(k->y>=y+h||k->y+k->h<=y||k->x>=x+w||k->x+k->w+8<=x)continue;
  if(k->k)fr(k->x,k->y,k->w,k->h,k->bg),run(k->x,k->y+k->a,k->t,k->n,k->fs,k->b,k->c,k->u,k->w);
  else fr(k->x,k->y,k->w,k->h,k->c);}}
const char*mwe_link(int x,int y,int*len){
 int i;Cmd*k;
 for(i=nc-1;i>=0;i--){
  k=&cm[i];
  if(k->hn&&x>=k->x&&x<k->x+k->w&&y>=k->y&&y<k->y+k->h){*len=k->hn;return k->hp;}}
 return 0;}
const char*mwe_title(int*len){*len=ttl.n;return ttl.p;}
unsigned mwe_bg(void){return(unsigned)cv;}
const char*mwe_font_name(void){return MF_NAME;}
int mwe_textw(const char*s,int n,int fs,int b){return wid(s,n,fs,b);}
void mwe_text(unsigned*buf,int st,int bw,int bh,int x,int base_,const char*s,int n,int fs,int b,unsigned col,int clx0,int clx1){
 tgt(buf,st,0,0,bw,bh);
 if(clx0>kx0)kx0=clx0;
 if(clx1<kx1)kx1=clx1;
 run(x,base_,s,n,fs,b,(int)col,0,0);}
void mwe_rect(unsigned*buf,int st,int bw,int bh,int x,int y,int w,int h,unsigned col){
 tgt(buf,st,0,0,bw,bh);fr(x,y,w,h,(int)col);}

#ifndef MWE_LIB
/* ---- CLI: html -> BMP (streamed in 32-row bands) ---- */
#include <stdio.h>
#define MAXW 2048
#define BH 32
static char inbuf[IN+1];
static unsigned bd[MAXW*BH];
static u8 rb[MAXW*3+4];
static void pl(u8*p,int v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
int main(int ac,char**av){
 FILE*f;int n,i,r,x,w=800,rs,by,bh;u8 h[54]={'B','M'};
 if(ac<3){static const char u[]="MiniWebEngine v0.2 (font: " MF_NAME ")\nusage: mwe in.html out.bmp [width]\n";fwrite(u,1,sizeof u-1,stderr);return 1;}
 if(ac>3){for(i=0,x=0;av[3][i]>='0'&&av[3][i]<='9'&&x<9999;i++)x=x*10+av[3][i]-'0';if(x>=64&&x<=MAXW)w=x;}
 if(!(f=fopen(av[1],"rb")))return 2;
 n=(int)fread(inbuf,1,IN,f);fclose(f);
 mwe_load(inbuf,n);
 n=mwe_layout(w);
 rs=(w*3+3)&~3;
 if((long long)rs*n>2000000000LL)n=(int)(2000000000LL/rs); /* BMP size field is 32-bit */
 if(!(f=fopen(av[2],"wb")))return 3;
 pl(h+2,54+rs*n);pl(h+10,54);pl(h+14,40);pl(h+18,w);pl(h+22,-n);h[26]=1;h[28]=24;pl(h+34,rs*n);pl(h+38,2835);pl(h+42,2835);
 fwrite(h,1,54,f);
 for(by=0;by<n;by+=BH){
  bh=n-by<BH?n-by:BH;
  mwe_paint(bd,w,0,by,w,bh);
  for(r=0;r<bh;r++){unsigned*q=bd+r*w;for(x=0;x<w;x++){rb[x*3]=q[x];rb[x*3+1]=q[x]>>8;rb[x*3+2]=q[x]>>16;}fwrite(rb,1,rs,f);}}
 fclose(f);
 return 0;}
#endif
