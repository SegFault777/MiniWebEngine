/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine v0.1
 * Zero-dependency mini HTML+CSS engine: HTML parse -> CSS cascade -> block/inline
 * layout -> band rasterizer (built-in 5x7 font) -> streamed 24-bit BMP.
 * libc used: fopen fread fwrite fclose exit stderr. No malloc (static arena), no string.h.
 * Build: gcc -Os -s -ffunction-sections -fdata-sections -Wl,--gc-sections mwe.c -o mwe
 * Use:   mwe in.html out.bmp [width]
 */
#include <stdio.h>
void exit(int);

typedef unsigned char u8;
typedef unsigned int u32;
#define AU (-32768)
#define MAXW 2048
#define BH 32
#define MC 20000
#define MR 512
#define IN (1<<20)
#define SC(f) ((((f)+4)/8)<1?1:(((f)+4)/8))

typedef struct{char*p;int n;}S;
typedef struct{int disp,col,bg,bc,fs,bold,ul,al,m[4],p[4],bw,w,wp,h,mw;}St;
typedef struct N N;
struct N{N*par,*kid,*last,*nx;S tag,txt,id,cls,sty;St s;};
typedef struct{S tag,id,cls[2];int nc;}Cp;
typedef struct{Cp c[3];int ns,spec;S d;}Rule;
typedef struct{int k,x,y,w,h,c,bg,sc,b,u,n;char*t;}Cmd;

/* ---- arena ---- */
static long long ar[1<<19];
static int ap;
static void*al(int n){n=(n+7)&~7;if(ap+n>(int)sizeof ar)exit(2);ap+=n;return(char*)ar+ap-n;}

/* ---- strings ---- */
static int lc(int c){return c>='A'&&c<='Z'?c+32:c;}
static int isws(int c){return c==' '||c=='\n'||c=='\t'||c=='\r'||c=='\f';}
static int isa(int c){c=lc(c);return c>='a'&&c<='z';}
static int isal(int c){c=lc(c);return isa(c)||(c>='0'&&c<='9')||c=='-'||c=='_';}
static int sl(const char*s){int n=0;while(s[n])n++;return n;}
static int seq(S a,const char*b){int i=0;for(;i<a.n;i++)if(!b[i]||lc(a.p[i])!=lc(b[i]))return 0;return!b[i];}
static int seqs(S a,S b){int i;if(a.n!=b.n)return 0;for(i=0;i<a.n;i++)if(lc(a.p[i])!=lc(b.p[i]))return 0;return 1;}
static S trim(S s){while(s.n&&isws(*s.p)){s.p++;s.n--;}while(s.n&&isws(s.p[s.n-1]))s.n--;return s;}
static int tk(S v,S*t,int mx){int n=0,i=0,a,pd;while(i<v.n&&n<mx){while(i<v.n&&isws(v.p[i]))i++;if(i>=v.n)break;a=i;pd=0;while(i<v.n&&(pd||!isws(v.p[i]))){if(v.p[i]=='(')pd++;else if(v.p[i]==')')pd--;i++;}t[n].p=v.p+a;t[n].n=i-a;n++;}return n;}

/* ---- CSS values ---- */
static const struct{const char*n;int c;}CN[]={{"black",0},{"white",0xffffff},{"red",0xff0000},{"green",0x008000},{"blue",0x0000ff},{"gray",0x808080},{"grey",0x808080},{"yellow",0xffff00},{"orange",0xffa500},{"purple",0x800080},{"silver",0xc0c0c0},{"navy",0x000080},{"teal",0x008080},{"maroon",0x800000},{"lime",0x00ff00},{"aqua",0x00ffff},{"fuchsia",0xff00ff},{"olive",0x808000},{0,0}};
static int hx(int c){c=lc(c);return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;}
/* returns 0xRRGGBB, -1 transparent, -2 invalid */
static int clr(S v){
 int i,r=0,d,k,g[3];
 if(v.n&&v.p[0]=='#'){
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
/* length -> px; AU if not a number; percent goes to *pc */
static int ln(S v,int fs,int*pc){
 int i=0,ng=0,iv=0,fr=0,fd=1,dg=0,r;
 if(v.n&&(v.p[0]=='-'||v.p[0]=='+')){ng=v.p[0]=='-';i++;}
 for(;i<v.n&&v.p[i]>='0'&&v.p[i]<='9';i++,dg=1)if(iv<100000)iv=iv*10+v.p[i]-'0';
 if(i<v.n&&v.p[i]=='.'){i++;for(;i<v.n&&v.p[i]>='0'&&v.p[i]<='9';i++,dg=1)if(fd<1000){fr=fr*10+v.p[i]-'0';fd*=10;}}
 if(!dg)return AU;
 if(i<v.n&&v.p[i]=='%'){if(pc)*pc=iv;return 0;}
 r=(i+1<v.n&&lc(v.p[i])=='e'&&lc(v.p[i+1])=='m')?(iv*fd+fr)*fs/fd:iv;
 return ng?-r:r;}
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
 else if(seq(k,"font-size")){x=ln(t[0],s->fs,&pc);if(pc>=0)x=s->fs*pc/100;if(x>0)s->fs=x;}
 else if(seq(k,"font-weight"))s->bold=seq(t[0],"bold")||seq(t[0],"bolder")||(t[0].p[0]>='6'&&t[0].p[0]<='9');
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
static N*nn(S tag){N*n=al(sizeof(N));n->tag=tag;return n;}
static void add(N*p,N*n){n->par=p;if(p->last)p->last->nx=n;else p->kid=n;p->last=n;}
static int isvoid(S t){static const char*v[]={"br","hr","img","meta","link","input",0};int i;for(i=0;v[i];i++)if(seq(t,v[i]))return 1;return 0;}
static int blkt(S t){static const char*v[]={"p","div","ul","ol","li","hr","pre","blockquote","table","section","article","header","footer","nav","main","aside","form","h1","h2","h3","h4","h5","h6",0};int i;for(i=0;v[i];i++)if(seq(t,v[i]))return 1;return 0;}
static int istag(const char*s,int i,int n){return s[i]=='<'&&i+1<n&&(isa(s[i+1])||s[i+1]=='/'||s[i+1]=='!'||s[i+1]=='?');}
static S dec(S t){ /* in-place entity decode */
 char*p=t.p;int o=0,i=0,j,c,v;S e;
 while(i<t.n){
  if(p[i]=='&'){
   j=i+1;while(j<t.n&&j-i<9&&p[j]!=';'&&!isws(p[j]))j++;
   if(j<t.n&&p[j]==';'){
    e=(S){p+i+1,j-i-1};c=0;
    if(seq(e,"amp"))c='&';else if(seq(e,"lt"))c='<';else if(seq(e,"gt"))c='>';else if(seq(e,"quot"))c='"';else if(seq(e,"apos"))c='\'';else if(seq(e,"nbsp"))c=' ';
    else if(e.n>1&&e.p[0]=='#'){v=0;if(lc(e.p[1])=='x'){for(c=2;c<e.n&&hx(e.p[c])>=0;c++)v=v*16+hx(e.p[c]);}else for(c=1;c<e.n&&e.p[c]>='0'&&e.p[c]<='9'&&v<99999;c++)v=v*10+e.p[c]-'0';c=(v>0&&v<128)?v:'?';}
    if(c){p[o++]=c;i=j+1;continue;}}}
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
  /* split selector list on ',' */
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
 N*root=nn((S){"#",1}),*cur=root,*e,*x;
 int i=0,dp=0,a,j,k,sc,ra,va;S nm,key,v,t;char q;N*p;
 while(i<n){
  if(!istag(s,i,n)){
   a=i++;while(i<n&&!istag(s,i,n))i++;
   t=dec((S){s+a,i-a});
   if(t.n){x=nn((S){0,0});x->txt=t;add(cur,x);}
   continue;}
  if(s[i+1]=='!'||s[i+1]=='?'){
   if(i+3<n&&s[i+2]=='-'&&s[i+3]=='-'){i+=4;while(i+2<n&&!(s[i]=='-'&&s[i+1]=='-'&&s[i+2]=='>'))i++;i+=3;}
   else{while(i<n&&s[i]!='>')i++;i++;}
   continue;}
  if(s[i+1]=='/'){
   a=i+2;j=a;while(j<n&&isal(s[j]))j++;
   nm=(S){s+a,j-a};while(j<n&&s[j]!='>')j++;i=j+1;
   p=cur;k=0;while(p!=root&&!seqs(p->tag,nm)){p=p->par;k++;}
   if(p!=root){cur=p->par;dp-=k+1;}
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
   if(key.n){if(seq(key,"id"))e->id=v;else if(seq(key,"class"))e->cls=v;else if(seq(key,"style"))e->sty=v;}}
  if(cur!=root&&((seq(cur->tag,"p")&&blkt(nm))||(seq(nm,"li")&&seq(cur->tag,"li")))){cur=cur->par;dp--;}
  add(cur,e);
  if(seq(nm,"style")||seq(nm,"script")){
   ra=j;while(j<n&&!(s[j]=='<'&&j+2+nm.n<=n&&s[j+1]=='/'&&seqs((S){s+j+2,nm.n},nm)))j++;
   if(seq(nm,"style"))css((S){s+ra,j-ra});
   while(j<n&&s[j]!='>')j++;j++;}
  else if(!sc&&!isvoid(nm)&&dp<200){cur=e;dp++;}
  i=j;}
 return root;}

/* ---- layout ---- */
static Cmd cm[MC],dm;static int nc;
static int Y,pend,lx,cx,cw,lh,lf,ca,sp;static Cmd*bulp;
static Cmd*cadd(int k,int x,int y,int w,int h,int c){
 Cmd*p=nc<MC?&cm[nc++]:&dm;
 p->k=k;p->x=x;p->y=y;p->w=w;p->h=h;p->c=c;p->bg=-1;p->sc=1;p->b=p->u=p->n=0;p->t=0;return p;}
static void nl(void){
 int i,dx,sl_;
 if(!lh){lx=cx;return;}
 sl_=cw-(lx-cx);dx=ca==1?sl_/2:ca==2?sl_:0;if(dx<0)dx=0;
 for(i=lf;i<nc;i++){cm[i].x+=dx;cm[i].y=Y+lh-cm[i].h;}
 Y+=lh;lh=0;lx=cx;lf=nc;sp=0;}
static void ls(void){if(!lh){Y+=pend;pend=0;if(bulp){bulp->y=Y+3*bulp->sc;bulp=0;}}}
static void word(char*p,int n,int cells,N*e){
 St*s=&e->s;int sc=SC(s->fs),c1=6*sc,ww=cells*c1,spw=(sp&&lh)?c1:0;Cmd*c;
 if(lh&&lx+spw+ww>cx+cw){nl();spw=0;}
 ls();lx+=spw;
 c=cadd(1,lx,0,ww,9*sc,s->col);
 c->bg=s->disp==2?s->bg:-1;c->sc=sc;c->b=s->bold;c->u=s->ul;c->t=p;c->n=n;
 lx+=ww;if(9*sc>lh)lh=9*sc;sp=0;}
static void txt(N*n){
 S t=n->txt;int i=0,j,c;
 while(i<t.n){
  if(isws(t.p[i])){sp=1;i++;continue;}
  j=i;c=0;while(j<t.n&&!isws(t.p[j])){if(((u8)t.p[j]&0xC0)!=0x80)c++;j++;}
  word(t.p+i,j-i,c,n->par);i=j;}}
static void blk(N*n,int ax,int aw);
static void walk(N*n){
 N*c;St*s;int sc;
 for(c=n->kid;c;c=c->nx){
  if(!c->tag.n){txt(c);continue;}
  s=&c->s;if(!s->disp)continue;
  if(seq(c->tag,"br")){sc=SC(s->fs);ls();if(9*sc>lh)lh=9*sc;nl();continue;}
  if(s->disp==1)blk(c,cx,cw);else walk(c);}}
static void blk(N*n,int ax,int aw){
 St*s=&n->s;int b=s->bw,ml=s->m[3],mr=s->m[1],mt=s->m[0],mb=s->m[2];
 int eh=s->p[1]+s->p[3]+2*b,w,fr,box,tt,bt,top,x0,tw,ys,ocx=cx,ocw=cw,oca=ca,sc=SC(s->fs),bc;Cmd*bp=0;
 if(mt==AU)mt=0;if(mb==AU)mb=0;
 if(s->wp>=0)w=aw*s->wp/100;else if(s->w>=0)w=s->w;else w=aw-(ml==AU?0:ml)-(mr==AU?0:mr)-eh;
 if(s->mw>=0&&w>s->mw)w=s->mw;
 if(w<0)w=0;
 fr=aw-w-eh;
 if(ml==AU){ml=(mr==AU)?fr/2:fr-mr;if(ml<0)ml=0;}
 nl();
 box=s->bg>=0||b>0;tt=!box&&!s->p[0]&&s->h<0;bt=!box&&!s->p[2]&&s->h<0;
 if(tt){if(mt>pend)pend=mt;}else{Y+=mt>pend?mt:pend;pend=0;}
 top=Y;x0=ax+ml;tw=w+eh;
 if(s->bg>=0)bp=cadd(0,x0,top,tw,0,s->bg);
 if(seq(n->tag,"li")){bulp=cadd(0,ax+ml+b+s->p[3]-5*sc,0,2*sc,2*sc,s->col);bulp->sc=sc;}
 if(!tt)Y+=b+s->p[0];
 cx=ax+ml+b+s->p[3];cw=w;ca=s->al;lx=cx;lh=0;sp=0;lf=nc;ys=Y;
 walk(n);nl();
 if(s->h>=0){Y=ys+s->h;pend=0;}
 if(bt){if(mb>pend)pend=mb;}else{Y+=pend+s->p[2]+b;pend=mb;}
 if(bp)bp->h=Y-top;
 if(b>0){bc=s->bc<0?s->col:s->bc;
  cadd(0,x0,top,tw,b,bc);cadd(0,x0,Y-b,tw,b,bc);cadd(0,x0,top,b,Y-top,bc);cadd(0,x0+tw-b,top,b,Y-top,bc);}
 cx=ocx;cw=ocw;ca=oca;lx=cx;lh=0;sp=0;lf=nc;}

/* ---- raster ---- */
static const u8 F[475]={
0,0,0,0,0,0,0,0x5F,0,0,0,7,0,7,0,0x14,0x7F,0x14,0x7F,0x14,
0x24,0x2A,0x7F,0x2A,0x12,0x23,0x13,8,0x64,0x62,0x36,0x49,0x55,0x22,0x50,0,5,3,0,0,
0,0x1C,0x22,0x41,0,0,0x41,0x22,0x1C,0,0x14,8,0x3E,8,0x14,8,8,0x3E,8,8,
0,0x50,0x30,0,0,8,8,8,8,8,0,0x60,0x60,0,0,0x20,0x10,8,4,2,
0x3E,0x51,0x49,0x45,0x3E,0,0x42,0x7F,0x40,0,0x42,0x61,0x51,0x49,0x46,0x21,0x41,0x45,0x4B,0x31,
0x18,0x14,0x12,0x7F,0x10,0x27,0x45,0x45,0x45,0x39,0x3C,0x4A,0x49,0x49,0x30,1,0x71,9,5,3,
0x36,0x49,0x49,0x49,0x36,6,0x49,0x49,0x29,0x1E,0,0x36,0x36,0,0,0,0x56,0x36,0,0,
8,0x14,0x22,0x41,0,0x14,0x14,0x14,0x14,0x14,0,0x41,0x22,0x14,8,2,1,0x51,9,6,
0x32,0x49,0x79,0x41,0x3E,0x7E,0x11,0x11,0x11,0x7E,0x7F,0x49,0x49,0x49,0x36,0x3E,0x41,0x41,0x41,0x22,
0x7F,0x41,0x41,0x22,0x1C,0x7F,0x49,0x49,0x49,0x41,0x7F,9,9,9,1,0x3E,0x41,0x49,0x49,0x7A,
0x7F,8,8,8,0x7F,0,0x41,0x7F,0x41,0,0x20,0x40,0x41,0x3F,1,0x7F,8,0x14,0x22,0x41,
0x7F,0x40,0x40,0x40,0x40,0x7F,2,0x0C,2,0x7F,0x7F,4,8,0x10,0x7F,0x3E,0x41,0x41,0x41,0x3E,
0x7F,9,9,9,6,0x3E,0x41,0x51,0x21,0x5E,0x7F,9,0x19,0x29,0x46,0x46,0x49,0x49,0x49,0x31,
1,1,0x7F,1,1,0x3F,0x40,0x40,0x40,0x3F,0x1F,0x20,0x40,0x20,0x1F,0x3F,0x40,0x38,0x40,0x3F,
0x63,0x14,8,0x14,0x63,7,8,0x70,8,7,0x61,0x51,0x49,0x45,0x43,0,0x7F,0x41,0x41,0,
2,4,8,0x10,0x20,0,0x41,0x41,0x7F,0,4,2,1,2,4,0x40,0x40,0x40,0x40,0x40,
0,1,2,4,0,0x20,0x54,0x54,0x54,0x78,0x7F,0x48,0x44,0x44,0x38,0x38,0x44,0x44,0x44,0x20,
0x38,0x44,0x44,0x48,0x7F,0x38,0x54,0x54,0x54,0x18,8,0x7E,9,1,2,0x0C,0x52,0x52,0x52,0x3E,
0x7F,8,4,4,0x78,0,0x44,0x7D,0x40,0,0x20,0x40,0x44,0x3D,0,0x7F,0x10,0x28,0x44,0,
0,0x41,0x7F,0x40,0,0x7C,4,0x18,4,0x78,0x7C,8,4,4,0x78,0x38,0x44,0x44,0x44,0x38,
0x7C,0x14,0x14,0x14,8,8,0x14,0x14,0x18,0x7C,0x7C,8,4,4,8,0x48,0x54,0x54,0x54,0x20,
4,0x3F,0x44,0x40,0x20,0x3C,0x40,0x40,0x20,0x7C,0x1C,0x20,0x40,0x20,0x1C,0x3C,0x40,0x30,0x40,0x3C,
0x44,0x28,0x10,0x28,0x44,0x0C,0x50,0x50,0x50,0x3C,0x44,0x64,0x54,0x4C,0x44,0,8,0x36,0x41,0,
0,0,0x7F,0,0,0,0x41,0x36,8,0,0x10,8,8,0x10,8};
static u32 bd[MAXW*BH];
static u8 rb[MAXW*3+4];
static int by,bh,W;
static void fr(int x,int y,int w,int h,int c){
 int x1=x+w,y1=y+h,xx;
 if(c<0)return;
 if(x<0)x=0;if(x1>W)x1=W;if(y<by)y=by;if(y1>by+bh)y1=by+bh;
 for(;y<y1;y++)for(xx=x;xx<x1;xx++)bd[(y-by)*W+xx]=c;}
static void gl(int x,int y,int sc,int ch,int c){
 const u8*g=F+(ch-32)*5;int i,r;
 if(ch<32||ch>126)return;
 for(i=0;i<5;i++)for(r=0;r<7;r++)if(g[i]>>r&1)fr(x+i*sc,y+r*sc,sc,sc,c);}
static void pt(Cmd*c){
 int i,x=c->x,ch;
 if(c->bg>=0)fr(c->x,c->y,c->w,c->h,c->bg);
 for(i=0;i<c->n;i++){
  ch=(u8)c->t[i];
  if(ch>=128){if((ch&0xC0)==0x80)continue;ch='?';}
  if(ch<32||ch>126){x+=6*c->sc;continue;}
  gl(x,c->y,c->sc,ch,c->c);
  if(c->b)gl(x+(c->sc+1)/2,c->y,c->sc,ch,c->c);
  x+=6*c->sc;}
 if(c->u)fr(c->x,c->y+8*c->sc-1,c->w,c->sc>1?c->sc/2:1,c->c);}
static void pl(u8*p,int v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}

static const char UA[]=
"html,body,div,p,h1,h2,h3,h4,h5,h6,ul,ol,li,hr,pre,blockquote,section,article,header,footer,nav,main,aside,form,table,tr,center{display:block}"
"head,style,script,title,meta,link{display:none}"
"body{margin:8px}p{margin:1em 0}"
"h1{font-size:32px;font-weight:bold;margin:.67em 0}h2{font-size:24px;font-weight:bold;margin:.83em 0}"
"h3{font-size:18px;font-weight:bold;margin:1em 0}h4,h5,h6{font-size:16px;font-weight:bold;margin:1em 0}"
"b,strong,th{font-weight:bold}u,ins{text-decoration:underline}a{color:#00e;text-decoration:underline}"
"ul,ol{margin:1em 0;padding-left:24px}hr{height:2px;background:#888;margin:8px 0}"
"blockquote{margin:1em 40px}center{text-align:center}";

int main(int ac,char**av){
 FILE*f;char*buf;int n,i,r,x,H,rs,cv=0xffffff;St base;N*root,*c,*d;u8 h[54]={'B','M'};Rule t;
 if(ac<3){static const char u[]="MiniWebEngine v0.1\nusage: mwe in.html out.bmp [width]\n";fwrite(u,1,sizeof u-1,stderr);return 1;}
 W=800;
 if(ac>3){for(i=0,x=0;av[3][i]>='0'&&av[3][i]<='9'&&x<9999;i++)x=x*10+av[3][i]-'0';if(x>=64&&x<=MAXW)W=x;}
 if(!(f=fopen(av[1],"rb")))return 2;
 buf=al(IN+1);n=(int)fread(buf,1,IN,f);fclose(f);
 base=(St){1,0,-1,-1,16,0,0,0,{0,0,0,0},{0,0,0,0},0,-1,-1,-1,-1};
 css((S){(char*)UA,sl(UA)});
 root=parse(buf,n);
 for(i=1;i<nr;i++){t=rl[i];r=i-1;while(r>=0&&rl[r].spec>t.spec){rl[r+1]=rl[r];r--;}rl[r+1]=t;}
 root->s=base;stl(root,&base);
 cx=0;cw=W;lx=0;lf=0;
 walk(root);nl();
 H=Y+pend;if(H<1)H=1;if(H>30000)H=30000;
 for(c=root->kid;c;c=c->nx){
  if(seq(c->tag,"html")){if(c->s.bg>=0)cv=c->s.bg;for(d=c->kid;d;d=d->nx)if(seq(d->tag,"body")&&d->s.bg>=0)cv=d->s.bg;}
  else if(seq(c->tag,"body")&&c->s.bg>=0)cv=c->s.bg;}
 if(!(f=fopen(av[2],"wb")))return 3;
 rs=(W*3+3)&~3;
 pl(h+2,54+rs*H);pl(h+10,54);pl(h+14,40);pl(h+18,W);pl(h+22,-H);h[26]=1;h[28]=24;pl(h+34,rs*H);pl(h+38,2835);pl(h+42,2835);
 fwrite(h,1,54,f);
 for(by=0;by<H;by+=BH){
  bh=H-by<BH?H-by:BH;
  for(i=0;i<W*bh;i++)bd[i]=cv;
  for(i=0;i<nc;i++){Cmd*k=&cm[i];if(k->y>=by+bh||k->y+k->h<=by)continue;if(k->k)pt(k);else fr(k->x,k->y,k->w,k->h,k->c);}
  for(r=0;r<bh;r++){u32*q=bd+r*W;for(x=0;x<W;x++){rb[x*3]=q[x];rb[x*3+1]=q[x]>>8;rb[x*3+2]=q[x]>>16;}fwrite(rb,1,rs,f);}}
 fclose(f);
 return 0;}
