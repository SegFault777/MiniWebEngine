/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine v0.3-dev2
 * Zero-dependency mini HTML5 + CSS3 engine: HTML parse -> selectors/cascade (@media, var(),
 * calc()) -> block / inline / flex / grid / table layout -> anti-aliased vector-font
 * rasterizer (outlines embedded as compact data, see tools/genfont.py) -> pixels.
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
#define MC 80000
#define MR 1024
#define IN (1<<20)

typedef struct{char*p;int n;}S;
typedef struct{int px,pc,k;}Len; /* px + pc% ; k: 0 length, 1 auto, 2 invalid, 3 plain number (px = value*1000) */
enum{D_NONE,D_BLOCK,D_INLINE,D_IB,D_X4,D_TABLE,D_ROW,D_CELL,D_LI,D_RG};
typedef struct{
 int disp,fx,gr,col,bg,fs,bold,it,ul,al,tt,ws,ls,va,vaa,lh,lst,vis,op,bs,ov,fl,clr,pos,bsp,bcol,fdir,fwrap,jc,ai,as,fgrow,fshr,ord,hascont,gtn,gtauto,gtmin,gcs,
     shx,shy,shb,shs,shc,gk,ga,gn;
 Len m[4],p[4],w,h,mnw,mxw,mnh,mxh,top,rt,bot,lf,rad[4],fb,gx,gy,gt[12];
 int gtk[12],bw[4],bc[4],bst[4],gcol[6],gpos[6],gpx[6];
 S cont;
}St;
typedef struct N N;
struct N{N*par,*kid,*last,*nx,*pv,*lel;S tag,txt,id,cls,sty,href,at;St*s;int det,gen,x0,y0,x1,y1;};

/* ---- arena (no malloc) ---- */
static long long ar[(1<<24)/8],dmy[64];
static int ap,full;
static void*al(int n){
 char*p;int i;
 n=(n+7)&~7;
 if(ap+n>(int)sizeof ar||n>(int)sizeof dmy*0+(1<<22)){full=1;p=(char*)dmy;n=(int)sizeof dmy;}
 else{p=(char*)ar+ap;ap+=n;}
 for(i=0;i<n;i++)p[i]=0;
 return p;}
static char scr[1<<17],cpool[1<<15];static int scp,cpp; /* per-layout / per-restyle scratch */
static char*sal(int n){char*p;if(scp+n>(int)sizeof scr)return scr;p=scr+scp;scp+=n;return p;}
static int VW=800,VH=800,rootfs=16;

/* ---- strings / utf-8 ---- */
static int lc(int c){return c>='A'&&c<='Z'?c+32:c;}
static int isws(int c){return c==' '||c=='\n'||c=='\t'||c=='\r'||c=='\f';}
static int isa(int c){c=lc(c);return c>='a'&&c<='z';}
static int isal(int c){c=lc(c);return isa(c)||(c>='0'&&c<='9')||c=='-'||c=='_';}
static int isdg(int c){return c>='0'&&c<='9';}
static int sl(const char*s){int n=0;while(s[n])n++;return n;}
static int seq(S a,const char*b){int i=0;for(;i<a.n;i++)if(!b[i]||lc(a.p[i])!=lc(b[i]))return 0;return!b[i];}
static int seqs(S a,S b){int i;if(a.n!=b.n)return 0;for(i=0;i<a.n;i++)if(lc(a.p[i])!=lc(b.p[i]))return 0;return 1;}
static int starts(S a,const char*b){int i=0;for(;b[i];i++)if(i>=a.n||lc(a.p[i])!=lc(b[i]))return 0;return 1;}
static S trim(S s){while(s.n&&isws(*s.p)){s.p++;s.n--;}while(s.n&&isws(s.p[s.n-1]))s.n--;return s;}
static int tk(S v,S*t,int mx){int n=0,i=0,a,pd;while(i<v.n&&n<mx){while(i<v.n&&isws(v.p[i]))i++;if(i>=v.n)break;a=i;pd=0;while(i<v.n&&(pd||!isws(v.p[i]))){if(v.p[i]=='(')pd++;else if(v.p[i]==')')pd--;i++;}t[n].p=v.p+a;t[n].n=i-a;n++;}return n;}
static int splitc(S v,S*t,int mx){int n=0,i=0,a=0,pd=0;if(!v.n)return 0;for(;i<=v.n&&n<mx;i++){if(i<v.n&&v.p[i]=='(')pd++;else if(i<v.n&&v.p[i]==')')pd--;else if(i==v.n||(v.p[i]==','&&!pd)){t[n++]=trim((S){v.p+a,i-a});a=i+1;}}return n;}
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
static int hx(int c){c=lc(c);return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;}

/* ---- font metrics: tf bit0 bold, bit1 italic, bits2-3 text-transform ---- */
static int gi(int cp){int i;if(cp>=32&&cp<=126)return cp-32;for(i=95;i<MF_N;i++)if(MF_CP[i]==cp)return i;return '?'-32;}
static int tfm(int cp,int tt,int first){
 if(tt==1)return cp>='a'&&cp<='z'?cp-32:cp>=0xE0&&cp<=0xFE&&cp!=0xF7?cp-32:cp;
 if(tt==2)return cp>='A'&&cp<='Z'?cp+32:cp>=0xC0&&cp<=0xDE&&cp!=0xD7?cp+32:cp;
 if(tt==3&&first)return cp>='a'&&cp<='z'?cp-32:cp;
 return cp;}
static int wid(const char*p,int n,int fs,int tf,int ls){
 int i=0,k=0;long long s=0;
 while(i<n){s+=MF_ADV[tf&1][gi(tfm(u8d(p,n,&i),tf>>2,k==0))];k++;}
 s=(s*fs+512)>>10;s+=(long long)ls*k;
 return s>100000000?100000000:s<0?0:(int)s;}

/* ---- lengths, calc(), units ---- */
static Len mk(int px,int pc,int k){Len l;l.px=px;l.pc=pc;l.k=k;return l;}
static S es;static int ei,efs,eerr;
static void esk(void){while(ei<es.n&&isws(es.p[ei]))ei++;}
static Len eadd(void);
static int unitmul(const char*u,int un,int*num,int*den){
 static const struct{const char*n;int a,b;}U[]={{"px",1,1},{"pt",4,3},{"pc",16,1},{"in",96,1},{"cm",378,10},{"mm",378,100},{"q",378,400},{"ch",-1,2},{"ex",-1,2},{"em",-1,1},{"rem",-2,1},{"vw",-3,100},{"vh",-4,100},{"vmin",-5,100},{"vmax",-6,100},{0,0,0}};
 int i,k;S s;s.p=(char*)u;s.n=un;
 for(i=0;U[i].n;i++)if(seq(s,U[i].n)){k=U[i].a;*den=U[i].b;
  *num=k==-1?efs:k==-2?rootfs:k==-3?VW:k==-4?VH:k==-5?(VW<VH?VW:VH):k==-6?(VW>VH?VW:VH):k;return 1;}
 return 0;}
static Len efac(void){
 Len r=mk(0,0,2),v[8];int a,neg=0,n,num,den,dg=0;long long iv=0,fr=0,fd=1,v1;S nm;
 esk();
 if(ei>=es.n){eerr=1;return r;}
 if(es.p[ei]=='('){ei++;r=eadd();esk();if(ei<es.n&&es.p[ei]==')')ei++;else eerr=1;return r;}
 if(isa(es.p[ei])){
  a=ei;while(ei<es.n&&isal(es.p[ei]))ei++;
  nm=(S){es.p+a,ei-a};esk();
  if(ei>=es.n||es.p[ei]!='('){eerr=1;return r;}
  ei++;n=0;
  for(;;){v[n++]=eadd();esk();if(ei<es.n&&es.p[ei]==','&&n<8){ei++;continue;}break;}
  if(ei<es.n&&es.p[ei]==')')ei++;else eerr=1;
  if(seq(nm,"calc"))return v[0];
  if(seq(nm,"min")||seq(nm,"max")){
   r=v[0];for(a=1;a<n;a++){if(v[a].pc||r.pc)continue;if(seq(nm,"min")?v[a].px<r.px:v[a].px>r.px)r=v[a];}return r;}
  if(seq(nm,"clamp")&&n==3){
   r=v[1];if(!v[0].pc&&!v[1].pc&&!v[2].pc){if(r.px<v[0].px)r=v[0];if(r.px>v[2].px)r=v[2];}return r;}
  eerr=1;return mk(0,0,2);}
 if(es.p[ei]=='-'){neg=1;ei++;}else if(es.p[ei]=='+')ei++;
 iv=0;
 while(ei<es.n&&isdg(es.p[ei])){if(iv<1000000)iv=iv*10+es.p[ei]-'0';ei++;dg=1;}
 if(ei<es.n&&es.p[ei]=='.'){ei++;while(ei<es.n&&isdg(es.p[ei])){if(fd<1000){fr=fr*10+es.p[ei]-'0';fd*=10;}ei++;dg=1;}}
 if(!dg){eerr=1;return r;}
 v1=iv*1000+fr*1000/fd;if(neg)v1=-v1;
 if(ei<es.n&&es.p[ei]=='%'){ei++;return mk(0,(int)(v1/1000),0);}
 a=ei;while(ei<es.n&&isa(es.p[ei]))ei++;
 if(ei==a)return mk((int)v1,0,3);
 if(!unitmul(es.p+a,ei-a,&num,&den)){eerr=1;return r;}
 v1=v1*num/(den*1000LL);
 if(v1>100000)v1=100000;
 v1=v1<-100000?-100000:v1;
 return mk((int)v1,0,0);}
static Len eterm(void){
 Len a=efac(),b;int op;
 for(;;){
  esk();
  if(ei>=es.n||(es.p[ei]!='*'&&es.p[ei]!='/'))return a;
  op=es.p[ei++];b=efac();
  if(a.k==2||b.k==2){a.k=2;continue;}
  if(op=='*'){
   if(a.k==3&&b.k==3)a.px=(int)((long long)a.px*b.px/1000);
   else if(a.k==3){b.px=(int)((long long)b.px*a.px/1000);b.pc=(int)((long long)b.pc*a.px/1000);a=b;}
   else if(b.k==3){a.px=(int)((long long)a.px*b.px/1000);a.pc=(int)((long long)a.pc*b.px/1000);}
   else a.k=2;}
  else{
   if(b.k!=3||b.px==0)a.k=2;
   else{a.px=(int)((long long)a.px*1000/b.px);a.pc=(int)((long long)a.pc*1000/b.px);}}}}
static Len eadd(void){
 Len a=eterm(),b;int op;
 for(;;){
  esk();
  if(ei>=es.n||(es.p[ei]!='+'&&es.p[ei]!='-'))return a;
  if(es.p[ei]=='-'&&ei+1<es.n&&!isws(es.p[ei+1])&&ei>0&&!isws(es.p[ei-1]))return a; /* "1px-2px" is not a subtraction */
  op=es.p[ei++];b=eterm();
  if(a.k==2||b.k==2||(a.k==3)!=(b.k==3)){a.k=2;continue;}
  a.px+=op=='+'?b.px:-b.px;a.pc+=op=='+'?b.pc:-b.pc;}}
static Len lenv(S v,int fs){
 Len r;
 v=trim(v);
 if(seq(v,"auto"))return mk(0,0,1);
 es=v;ei=0;efs=fs;eerr=0;
 r=eadd();esk();
 if(ei<es.n||eerr)r.k=2;
 return r;}
static int rl(Len l,int base){return l.k==0?l.px+(int)((long long)base*l.pc/100):0;}

/* ---- colors: 0xRRGGBB opaque, ((127-A/2)<<24)|RGB translucent, -1 transparent, -2 invalid, -3 currentColor ---- */
static const char CNN[]="aliceblue\0antiquewhite\0aqua\0aquamarine\0azure\0beige\0bisque\0black\0blanchedalmond\0blue\0blueviolet\0brown\0burlywood\0cadetblue\0chartreuse\0chocolate\0coral\0cornflowerblue\0cornsilk\0crimson\0cyan\0darkblue\0darkcyan\0darkgoldenrod\0darkgray\0darkgreen\0darkgrey\0darkkhaki\0darkmagenta\0darkolivegreen\0darkorange\0darkorchid\0darkred\0darksalmon\0darkseagreen\0darkslateblue\0darkslategray\0darkslategrey\0darkturquoise\0darkviolet\0deeppink\0deepskyblue\0dimgray\0dimgrey\0dodgerblue\0firebrick\0floralwhite\0forestgreen\0fuchsia\0gainsboro\0ghostwhite\0gold\0goldenrod\0gray\0green\0greenyellow\0grey\0honeydew\0hotpink\0indianred\0indigo\0ivory\0khaki\0lavender\0lavenderblush\0lawngreen\0lemonchiffon\0lightblue\0lightcoral\0lightcyan\0lightgoldenrodyellow\0lightgray\0lightgreen\0lightgrey\0lightpink\0lightsalmon\0lightseagreen\0lightskyblue\0lightslategray\0lightslategrey\0lightsteelblue\0lightyellow\0lime\0limegreen\0linen\0magenta\0maroon\0mediumaquamarine\0mediumblue\0mediumorchid\0mediumpurple\0mediumseagreen\0mediumslateblue\0mediumspringgreen\0mediumturquoise\0mediumvioletred\0midnightblue\0mintcream\0mistyrose\0moccasin\0navajowhite\0navy\0oldlace\0olive\0olivedrab\0orange\0orangered\0orchid\0palegoldenrod\0palegreen\0paleturquoise\0palevioletred\0papayawhip\0peachpuff\0peru\0pink\0plum\0powderblue\0purple\0rebeccapurple\0red\0rosybrown\0royalblue\0saddlebrown\0salmon\0sandybrown\0seagreen\0seashell\0sienna\0silver\0skyblue\0slateblue\0slategray\0slategrey\0snow\0springgreen\0steelblue\0tan\0teal\0thistle\0tomato\0turquoise\0violet\0wheat\0white\0whitesmoke\0yellow\0yellowgreen\0";
static const int CNV[]={0xF0F8FF,0xFAEBD7,0x00FFFF,0x7FFFD4,0xF0FFFF,0xF5F5DC,0xFFE4C4,0x000000,0xFFEBCD,0x0000FF,0x8A2BE2,0xA52A2A,0xDEB887,0x5F9EA0,0x7FFF00,0xD2691E,0xFF7F50,0x6495ED,0xFFF8DC,0xDC143C,0x00FFFF,0x00008B,0x008B8B,0xB8860B,0xA9A9A9,0x006400,0xA9A9A9,0xBDB76B,0x8B008B,0x556B2F,0xFF8C00,0x9932CC,0x8B0000,0xE9967A,0x8FBC8F,0x483D8B,0x2F4F4F,0x2F4F4F,0x00CED1,0x9400D3,0xFF1493,0x00BFFF,0x696969,0x696969,0x1E90FF,0xB22222,0xFFFAF0,0x228B22,0xFF00FF,0xDCDCDC,0xF8F8FF,0xFFD700,0xDAA520,0x808080,0x008000,0xADFF2F,0x808080,0xF0FFF0,0xFF69B4,0xCD5C5C,0x4B0082,0xFFFFF0,0xF0E68C,0xE6E6FA,0xFFF0F5,0x7CFC00,0xFFFACD,0xADD8E6,0xF08080,0xE0FFFF,0xFAFAD2,0xD3D3D3,0x90EE90,0xD3D3D3,0xFFB6C1,0xFFA07A,0x20B2AA,0x87CEFA,0x778899,0x778899,0xB0C4DE,0xFFFFE0,0x00FF00,0x32CD32,0xFAF0E6,0xFF00FF,0x800000,0x66CDAA,0x0000CD,0xBA55D3,0x9370DB,0x3CB371,0x7B68EE,0x00FA9A,0x48D1CC,0xC71585,0x191970,0xF5FFFA,0xFFE4E1,0xFFE4B5,0xFFDEAD,0x000080,0xFDF5E6,0x808000,0x6B8E23,0xFFA500,0xFF4500,0xDA70D6,0xEEE8AA,0x98FB98,0xAFEEEE,0xDB7093,0xFFEFD5,0xFFDAB9,0xCD853F,0xFFC0CB,0xDDA0DD,0xB0E0E6,0x800080,0x663399,0xFF0000,0xBC8F8F,0x4169E1,0x8B4513,0xFA8072,0xF4A460,0x2E8B57,0xFFF5EE,0xA0522D,0xC0C0C0,0x87CEEB,0x6A5ACD,0x708090,0x708090,0xFFFAFA,0x00FF7F,0x4682B4,0xD2B48C,0x008080,0xD8BFD8,0xFF6347,0x40E0D0,0xEE82EE,0xF5DEB3,0xFFFFFF,0xF5F5F5,0xFFFF00,0x9ACD32};
#define NCN 148
static int mkc(int r,int g,int b,int a){
 r=r<0?0:r>255?255:r;g=g<0?0:g>255?255:g;b=b<0?0:b>255?255:b;
 if(a<=0)return-1;
 if(a>=254)return(r<<16)|(g<<8)|b;
 return((127-(a>>1))<<24)|(r<<16)|(g<<8)|b; /* 7-bit alpha keeps the value non-negative */}
static int cargs(S v,int*o,int*pcf){ /* numbers in "(...)": o[0..3] value*1000, pcf bit i = percent */
 int i=0,n=0,neg,dg;long long iv,fr,fd;
 while(i<v.n&&v.p[i]!='(')i++;
 i++;*pcf=0;
 while(i<v.n&&n<4){
  while(i<v.n&&(isws(v.p[i])||v.p[i]==','||v.p[i]=='/'))i++;
  if(i>=v.n||v.p[i]==')')break;
  neg=0;iv=fr=0;fd=1;dg=0;
  if(v.p[i]=='-'){neg=1;i++;}else if(v.p[i]=='+')i++;
  while(i<v.n&&isdg(v.p[i])){if(iv<100000)iv=iv*10+v.p[i]-'0';i++;dg=1;}
  if(i<v.n&&v.p[i]=='.'){i++;while(i<v.n&&isdg(v.p[i])){if(fd<1000){fr=fr*10+v.p[i]-'0';fd*=10;}i++;dg=1;}}
  if(!dg)return-1;
  o[n]=(int)((iv*1000+fr*1000/fd)*(neg?-1:1));
  if(i<v.n&&v.p[i]=='%'){*pcf|=1<<n;i++;}
  else while(i<v.n&&isa(v.p[i]))i++; /* deg etc. */
  n++;}
 return n;}
static float fabs_(float x){return x<0?-x:x;}
static float hue(float p,float q,float t){if(t<0)t+=1;if(t>1)t-=1;if(t<1.0f/6)return p+(q-p)*6*t;if(t<0.5f)return q;if(t<2.0f/3)return p+(q-p)*(2.0f/3-t)*6;return p;}
static int clr(S v){
 int i,k,d,r=0,o[4],pf,n;const char*q;
 v=trim(v);
 if(!v.n)return-2;
 if(v.p[0]=='#'){
  for(i=1;i<v.n;i++)if(hx(v.p[i])<0)return-2;
  if(v.n==4||v.n==5){r=0;for(i=1;i<v.n;i++)r=r*16+hx(v.p[i]);
   d=v.n==5?(r&15)*17:255;if(v.n==5)r>>=4;
   return mkc(((r>>8)&15)*17,((r>>4)&15)*17,(r&15)*17,d);}
  if(v.n==7||v.n==9){for(i=1;i<7;i++)r=r*16+hx(v.p[i]);
   d=255;if(v.n==9)d=hx(v.p[7])*16+hx(v.p[8]);
   return mkc(r>>16,(r>>8)&255,r&255,d);}
  return-2;}
 if(starts(v,"rgb")){
  n=cargs(v,o,&pf);if(n<3)return-2;
  for(i=0;i<3;i++)o[i]=(int)((pf>>i&1)?(long long)o[i]*255/100000:o[i]/1000);
  d=255;if(n>3)d=(int)((pf>>3&1)?(long long)o[3]*255/100000:(long long)o[3]*255/1000);
  return mkc(o[0],o[1],o[2],d);}
 if(starts(v,"hsl")){
  float h,s,l,p,qq;
  n=cargs(v,o,&pf);if(n<3)return-2;
  h=(float)o[0]/1000/360;h-=(float)(int)h;if(h<0)h+=1;
  s=(float)o[1]/100000;l=(float)o[2]/100000;if(s>1)s=1;if(s<0)s=0;if(l>1)l=1;if(l<0)l=0;
  d=255;if(n>3)d=(int)((pf>>3&1)?(long long)o[3]*255/100000:(long long)o[3]*255/1000);
  qq=l<0.5f?l*(1+s):l+s-l*s;p=2*l-qq;
  return mkc((int)(hue(p,qq,h+1.0f/3)*255+0.5f),(int)(hue(p,qq,h)*255+0.5f),(int)(hue(p,qq,h-1.0f/3)*255+0.5f),d);}
 if(seq(v,"transparent"))return-1;
 if(seq(v,"currentcolor"))return-3;
 q=CNN;
 for(k=0;k<NCN;k++){if(seq(v,q))return CNV[k];q+=sl(q)+1;}
 (void)fabs_;
 return-2;}
static int calpha(int c){int h;if(c<0)return 0;h=(c>>24)&127;return h?(127-h)*2:255;}

/* ---- style defaults, custom properties, var() ---- */
static St*cpa; /* parent style while a declaration block is applied (for "inherit") */
static void defst(St*s,St*pa){
 int i;char*b=(char*)s;
 for(i=0;i<(int)sizeof(St);i++)b[i]=0;
 s->disp=D_INLINE;s->bg=-1;s->op=255;s->fshr=1000;s->as=-1;
 s->w=s->h=s->mnw=s->mxw=s->mnh=s->mxh=s->top=s->rt=s->bot=s->lf=s->fb=mk(0,0,1);
 s->shc=-1;
 for(i=0;i<4;i++){s->bw[i]=3;s->bc[i]=-3;}
 for(i=0;i<12;i++)s->gtk[i]=2;
 if(pa){s->col=pa->col;s->fs=pa->fs;s->bold=pa->bold;s->it=pa->it;s->ul=pa->ul;s->al=pa->al;s->tt=pa->tt;s->ws=pa->ws;s->ls=pa->ls;s->lh=pa->lh;s->lst=pa->lst;s->vis=pa->vis;s->bsp=pa->bsp;s->bcol=pa->bcol;}
 else{s->fs=16;s->lst=1;s->bsp=2;}}
#define NV 512
static S vnm[NV],vvl[NV];static int nvar;
static int vfind(S nm){int i,j;for(i=nvar-1;i>=0;i--){if(vnm[i].n!=nm.n)continue;for(j=0;j<nm.n&&vnm[i].p[j]==nm.p[j];j++);if(j==nm.n)return i;}return-1;}
static char wb[2048];
static S subvar(S v){
 int i,j,d,len,k,loops,c;S nm,fb,val,in;
 for(i=0;i+3<v.n;i++)if(v.p[i]=='v'&&v.p[i+1]=='a'&&v.p[i+2]=='r'&&v.p[i+3]=='(')break;
 if(i+3>=v.n)return v;
 if(v.n>=(int)sizeof wb-1)return(S){0,0};
 for(i=0;i<v.n;i++)wb[i]=v.p[i];
 len=v.n;
 for(loops=0;loops<32;loops++){
  for(i=len-4;i>=0;i--)if(wb[i]=='v'&&wb[i+1]=='a'&&wb[i+2]=='r'&&wb[i+3]=='(')break;
  if(i<0)break;
  d=1;j=i+4;while(j<len&&d){if(wb[j]=='(')d++;else if(wb[j]==')')d--;j++;}
  if(d)return(S){0,0};
  in=(S){wb+i+4,j-1-(i+4)};
  nm=in;fb=(S){0,0};
  for(k=0;k<in.n;k++)if(in.p[k]==','){nm=(S){in.p,k};fb=trim((S){in.p+k+1,in.n-k-1});break;}
  nm=trim(nm);
  c=vfind(nm);val=c>=0?vvl[c]:fb;
  if(len-(j-i)+val.n>=(int)sizeof wb-1)return(S){0,0};
  { char tmp[2048];int tl=len-j,x;
    for(x=0;x<tl;x++)tmp[x]=wb[j+x];
    for(x=0;x<val.n;x++)wb[i+x]=val.p[x];
    for(x=0;x<tl;x++)wb[i+val.n+x]=tmp[x];
    len=i+val.n+tl;}}
 return(S){wb,len};}

/* ---- declarations ---- */
static int kw(S t,const char*a){return seq(t,a);}
static int bstyle(S t){
 if(kw(t,"none")||kw(t,"hidden"))return 0;
 if(kw(t,"dashed"))return 2;
 if(kw(t,"dotted"))return 3;
 if(kw(t,"solid")||kw(t,"double")||kw(t,"groove")||kw(t,"ridge")||kw(t,"inset")||kw(t,"outset"))return 1;
 return-1;}
static int bwidth(S t,int fs){
 Len l;
 if(kw(t,"thin"))return 1;
 if(kw(t,"medium"))return 3;
 if(kw(t,"thick"))return 5;
 l=lenv(t,fs);
 return l.k==0&&l.pc==0&&l.px>=0?l.px:-1;}
static void edge4(Len*a,S v,int fs,int pad){
 S t[4];int n=tk(v,t,4),i;Len x[4];
 if(!n)return;
 for(i=0;i<n;i++){x[i]=lenv(t[i],fs);if(x[i].k==2)return;if(pad&&(x[i].k==1||x[i].px<0||x[i].pc<0))x[i]=mk(0,0,0);}
 a[0]=x[0];a[1]=n>1?x[1]:x[0];a[2]=n>2?x[2]:x[0];a[3]=n>3?x[3]:a[1];}
static int angle(S t){ /* degrees, or -1000 */
 int i=0,dg=0;long long iv=0,fr=0,fd=1;int neg=0;
 if(i<t.n&&t.p[i]=='-'){neg=1;i++;}
 while(i<t.n&&isdg(t.p[i])){iv=iv*10+t.p[i]-'0';i++;dg=1;if(iv>100000)break;}
 if(i<t.n&&t.p[i]=='.'){i++;while(i<t.n&&isdg(t.p[i])){if(fd<1000){fr=fr*10+t.p[i]-'0';fd*=10;}i++;dg=1;}}
 if(!dg)return-1000;
 t.p+=i;t.n-=i;
 iv=iv*1000+fr*1000/fd;if(neg)iv=-iv;
 if(seq(t,"deg"))return(int)(iv/1000);
 if(seq(t,"turn"))return(int)(iv*360/1000);
 if(seq(t,"grad"))return(int)(iv*9/10/1000);
 if(seq(t,"rad"))return(int)(iv*573/10000);
 return-1000;}
static void grad(St*s,S in){
 S a[10],t[3];int n,i,j,m,c,k=0,po,lo;
 for(i=0;i<in.n&&in.p[i]!='(';i++);
 if(i>=in.n)return;
 in=(S){in.p+i+1,in.n-i-1};
 while(in.n&&in.p[in.n-1]!=')')in.n--;
 if(in.n)in.n--;
 n=splitc(in,a,10);s->gn=0;s->ga=180;i=0;
 if(n&&starts(a[0],"to ")){
  m=tk(a[0],t,3);
  if(m>=2){j=0;
   if(kw(t[1],"top"))j=0;else if(kw(t[1],"right"))j=90;else if(kw(t[1],"bottom"))j=180;else if(kw(t[1],"left"))j=270;
   if(m>=3){if((kw(t[1],"top")||kw(t[1],"bottom"))&&(kw(t[2],"right")||kw(t[2],"left")))j=kw(t[1],"top")?(kw(t[2],"right")?45:315):(kw(t[2],"right")?135:225);
    else if(kw(t[2],"top"))j=kw(t[1],"right")?45:315;else j=kw(t[1],"right")?135:225;}
   s->ga=j;}
  i=1;}
 else if(n&&(c=angle(a[0]))!=-1000){s->ga=((c%360)+360)%360;i=1;}
 for(;i<n&&s->gn<6;i++){
  m=tk(a[i],t,3);
  if(!m||(c=clr(t[0]))==-2)continue;
  k=s->gn;s->gcol[k]=c;s->gpos[k]=-1;s->gpx[k]=0;
  if(m>1){Len l=lenv(t[1],16);if(l.k==0){if(l.pc||!l.px){s->gpos[k]=l.pc;}else{s->gpos[k]=l.px;s->gpx[k]=1;}}}
  s->gn++;
  if(m>2&&s->gn<6){Len l=lenv(t[2],16);if(l.k==0){s->gcol[s->gn]=c;s->gpos[s->gn]=l.pc?l.pc:l.px;s->gpx[s->gn]=!l.pc&&l.px;s->gn++;}}}
 if(s->gn<2){s->gn=0;s->gk=0;return;}
 s->gk=1;
 if(s->gpos[0]<0)s->gpos[0]=0;
 if(s->gpos[s->gn-1]<0){s->gpos[s->gn-1]=100;s->gpx[s->gn-1]=0;}
 for(i=1;i<s->gn-1;i++)if(s->gpos[i]<0){
  lo=i-1;po=i+1;while(po<s->gn-1&&s->gpos[po]<0)po++;
  s->gpos[i]=s->gpos[lo]+(s->gpos[po]-s->gpos[lo])*(i-lo)/(po-lo);s->gpx[i]=s->gpx[lo];}}
static int fsize(S t,int pfs,int*ok){
 static const struct{const char*n;int v;}K[]={{"xx-small",9},{"x-small",10},{"small",13},{"medium",16},{"large",18},{"x-large",24},{"xx-large",32},{"xxx-large",48},{0,0}};
 int i;Len l;
 *ok=1;
 for(i=0;K[i].n;i++)if(kw(t,K[i].n))return K[i].v;
 if(kw(t,"smaller"))return pfs*5/6;
 if(kw(t,"larger"))return pfs*6/5;
 l=lenv(t,pfs);
 if(l.k==0){int v=l.px+pfs*l.pc/100;if(v>0)return v>180?180:v;}
 *ok=0;return pfs;}
static void setcont(St*s,S v){
 int i=0,o=cpp,q,c,k;char*p;
 v=trim(v);
 if(kw(v,"none")||kw(v,"normal")){s->hascont=0;return;}
 s->cont.p=cpool+cpp;
 while(i<v.n){
  if(v.p[i]=='"'||v.p[i]=='\''){q=v.p[i++];
   while(i<v.n&&v.p[i]!=q){
    if(cpp>(int)sizeof cpool-8)break;
    if(v.p[i]=='\\'&&i+1<v.n){i++;
     if(hx(v.p[i])>=0){c=0;k=0;while(i<v.n&&k<6&&hx(v.p[i])>=0){c=c*16+hx(v.p[i]);i++;k++;}if(i<v.n&&v.p[i]==' ')i++;cpp+=put8(cpool+cpp,c);}
     else cpool[cpp++]=v.p[i++];}
    else cpool[cpp++]=v.p[i++];}
   i++;}
  else i++;}
 (void)p;
 s->cont.n=cpp-o;s->hascont=1;}
static void prop(St*s,S k,S v){
 static const char*mp[]={"margin-top","margin-right","margin-bottom","margin-left"};
 static const char*pp[]={"padding-top","padding-right","padding-bottom","padding-left"};
 static const char*bs_[]={"border-top","border-right","border-bottom","border-left"};
 static const char*rn[]={"border-top-left-radius","border-top-right-radius","border-bottom-right-radius","border-bottom-left-radius"};
 S t[8];int n=tk(v,t,8),i,j,c,x,ok;Len l;
 if(!n)return;
 if(kw(t[0],"inherit")&&cpa){
  if(kw(k,"color"))s->col=cpa->col;else if(kw(k,"font-size"))s->fs=cpa->fs;else if(kw(k,"font-weight"))s->bold=cpa->bold;
  else if(kw(k,"font-style"))s->it=cpa->it;else if(kw(k,"line-height"))s->lh=cpa->lh;else if(kw(k,"text-align"))s->al=cpa->al;
  else if(kw(k,"font")){s->fs=cpa->fs;s->bold=cpa->bold;s->it=cpa->it;s->lh=cpa->lh;}
  else if(kw(k,"text-decoration")||kw(k,"text-decoration-line"))s->ul=cpa->ul;
  return;}
 if(kw(k,"color")){c=clr(t[0]);if(c!=-2)s->col=c==-3?(cpa?cpa->col:0):c;}
 else if(kw(k,"background")||kw(k,"background-color")||kw(k,"background-image")){
  if(kw(k,"background")||kw(k,"background-image")){s->gk=0;s->gn=0;}
  for(i=0;i<n;i++){
   if(starts(t[i],"linear-gradient")||starts(t[i],"repeating-linear-gradient"))grad(s,t[i]);
   else if(kw(k,"background-image"))continue;
   else if((c=clr(t[i]))!=-2)s->bg=c==-3?s->col:c;}
  if(kw(k,"background")&&kw(t[0],"none"))s->bg=-1;}
 else if(kw(k,"font-size")){x=fsize(t[0],cpa?cpa->fs:16,&ok);if(ok)s->fs=x;}
 else if(kw(k,"font-weight")){
  if(kw(t[0],"bold")||kw(t[0],"bolder"))s->bold=1;
  else if(kw(t[0],"normal")||kw(t[0],"lighter"))s->bold=0;
  else if(isdg(t[0].p[0]))s->bold=t[0].p[0]>='6'||t[0].n>3;}
 else if(kw(k,"font-style"))s->it=kw(t[0],"italic")||kw(t[0],"oblique");
 else if(kw(k,"font")){
  for(i=0;i<n;i++){
   if(kw(t[i],"italic")||kw(t[i],"oblique"))s->it=1;
   else if(kw(t[i],"bold")||kw(t[i],"bolder"))s->bold=1;
   else if(kw(t[i],"normal"))continue;
   else if(isdg(t[i].p[0])&&t[i].n==3&&t[i].p[1]=='0'&&t[i].p[2]=='0'&&t[i].p[0]!='0')s->bold=t[i].p[0]>='6';
   else{S sz=t[i];for(j=0;j<sz.n;j++)if(sz.p[j]=='/'){S lh=(S){sz.p+j+1,sz.n-j-1};int c2=0;{Len q=lenv(lh,16);c2=q.k==3?q.px/10:0;}if(c2)s->lh=c2;sz.n=j;break;}
    x=fsize(sz,cpa?cpa->fs:16,&ok);if(ok){s->fs=x;break;}}}}
 else if(kw(k,"line-height")){
  if(kw(t[0],"normal"))s->lh=0;
  else{l=lenv(t[0],s->fs);
   if(l.k==3)s->lh=l.px/10;else if(l.k==0)s->lh=l.pc?l.pc:(s->fs?l.px*100/s->fs:0);
   s->lh=s->lh>2000?2000:s->lh<0?0:s->lh;}}
 else if(kw(k,"text-decoration")||kw(k,"text-decoration-line")){
  s->ul=0;
  for(i=0;i<n;i++){if(kw(t[i],"underline"))s->ul|=1;else if(kw(t[i],"line-through"))s->ul|=2;}}
 else if(kw(k,"text-align"))s->al=kw(t[0],"center")?1:(kw(t[0],"right")||kw(t[0],"end"))?2:0;
 else if(kw(k,"text-transform"))s->tt=kw(t[0],"uppercase")?1:kw(t[0],"lowercase")?2:kw(t[0],"capitalize")?3:0;
 else if(kw(k,"letter-spacing")){l=lenv(t[0],s->fs);s->ls=l.k==0?l.px:0;}
 else if(kw(k,"white-space"))s->ws=kw(t[0],"nowrap")?1:kw(t[0],"pre")?2:(kw(t[0],"pre-wrap")||kw(t[0],"break-spaces"))?3:kw(t[0],"pre-line")?4:0;
 else if(kw(k,"vertical-align")){
  s->va=0;s->vaa=0;
  if(kw(t[0],"sub"))s->va=-(s->fs/5);else if(kw(t[0],"super"))s->va=s->fs*2/5;
  else if(kw(t[0],"top")||kw(t[0],"text-top"))s->vaa=1;else if(kw(t[0],"middle"))s->vaa=2;else if(kw(t[0],"bottom")||kw(t[0],"text-bottom"))s->vaa=3;
  else{l=lenv(t[0],s->fs);if(l.k==0)s->va=l.px;}}
 else if(kw(k,"display")){
  s->fx=s->gr=0;
  if(kw(t[0],"none")||kw(t[0],"table-column")||kw(t[0],"table-column-group"))s->disp=D_NONE;
  else if(kw(t[0],"inline"))s->disp=D_INLINE;
  else if(kw(t[0],"inline-block"))s->disp=D_IB;
  else if(kw(t[0],"flex")){s->disp=D_BLOCK;s->fx=1;}
  else if(kw(t[0],"inline-flex")){s->disp=D_IB;s->fx=1;}
  else if(kw(t[0],"grid")){s->disp=D_BLOCK;s->gr=1;}
  else if(kw(t[0],"inline-grid")){s->disp=D_IB;s->gr=1;}
  else if(kw(t[0],"table")||kw(t[0],"inline-table"))s->disp=D_TABLE;
  else if(kw(t[0],"table-row"))s->disp=D_ROW;
  else if(kw(t[0],"table-cell"))s->disp=D_CELL;
  else if(kw(t[0],"table-row-group")||kw(t[0],"table-header-group")||kw(t[0],"table-footer-group"))s->disp=D_RG;
  else if(kw(t[0],"list-item"))s->disp=D_LI;
  else if(kw(t[0],"contents"))s->disp=D_INLINE;
  else s->disp=D_BLOCK;}
 else if(kw(k,"visibility"))s->vis=kw(t[0],"hidden")||kw(t[0],"collapse");
 else if(kw(k,"opacity")){l=lenv(t[0],16);x=l.k==3?l.px*255/1000:l.k==0&&l.pc?l.pc*255/100:-1;if(x>=0)s->op=x>255?255:x;}
 else if(kw(k,"overflow")||kw(k,"overflow-x")||kw(k,"overflow-y"))s->ov=(kw(t[0],"hidden")||kw(t[0],"clip")||kw(t[0],"scroll"))?1:kw(t[0],"auto")?2:0;
 else if(kw(k,"position"))s->pos=kw(t[0],"relative")||kw(t[0],"sticky")?1:kw(t[0],"absolute")?2:kw(t[0],"fixed")?3:0;
 else if(kw(k,"float"))s->fl=kw(t[0],"left")?1:kw(t[0],"right")?2:0;
 else if(kw(k,"clear"))s->clr=kw(t[0],"left")?1:kw(t[0],"right")?2:kw(t[0],"both")?3:0;
 else if(kw(k,"top")){l=lenv(t[0],s->fs);if(l.k!=2)s->top=l;}
 else if(kw(k,"right")){l=lenv(t[0],s->fs);if(l.k!=2)s->rt=l;}
 else if(kw(k,"bottom")){l=lenv(t[0],s->fs);if(l.k!=2)s->bot=l;}
 else if(kw(k,"left")){l=lenv(t[0],s->fs);if(l.k!=2)s->lf=l;}
 else if(kw(k,"inset")){Len a[4];edge4(a,v,s->fs,0);s->top=a[0];s->rt=a[1];s->bot=a[2];s->lf=a[3];}
 else if(kw(k,"width")||kw(k,"height")||kw(k,"min-width")||kw(k,"max-width")||kw(k,"min-height")||kw(k,"max-height")){
  l=lenv(t[0],s->fs);
  if(l.k==2||(l.k==3&&l.px))return;
  if(l.k==3)l=mk(0,0,0);
  if(kw(t[0],"none")||kw(t[0],"auto")||kw(t[0],"fit-content")||kw(t[0],"max-content")||kw(t[0],"min-content"))l=mk(0,0,1);
  if(kw(k,"width"))s->w=l;else if(kw(k,"height"))s->h=l;else if(kw(k,"min-width"))s->mnw=l;else if(kw(k,"max-width"))s->mxw=l;
  else if(kw(k,"min-height"))s->mnh=l;else s->mxh=l;}
 else if(kw(k,"box-sizing"))s->bs=kw(t[0],"border-box");
 else if(kw(k,"margin"))edge4(s->m,v,s->fs,0);
 else if(kw(k,"padding"))edge4(s->p,v,s->fs,1);
 else if(kw(k,"margin-inline")||kw(k,"margin-block")||kw(k,"padding-inline")||kw(k,"padding-block")){
  int pd=k.p[0]=='p'||k.p[0]=='P',inl=k.p[k.n-1]=='e';
  Len a=lenv(t[0],s->fs),b2=n>1?lenv(t[1],s->fs):a;
  if(a.k==2||b2.k==2)return;
  if(pd){if(a.px<0||a.pc<0||a.k)a=mk(0,0,0);if(b2.px<0||b2.pc<0||b2.k)b2=mk(0,0,0);}
  if(inl){(pd?s->p:s->m)[3]=a;(pd?s->p:s->m)[1]=b2;}else{(pd?s->p:s->m)[0]=a;(pd?s->p:s->m)[2]=b2;}}
 else if(kw(k,"border")){
  int hs=0;
  for(i=0;i<n;i++)if(bstyle(t[i])>=0)hs=1;
  for(j=0;j<4;j++){
   for(i=0;i<n;i++){
    if((x=bstyle(t[i]))>=0)s->bst[j]=x;
    else if((x=bwidth(t[i],s->fs))>=0)s->bw[j]=x;
    else if((c=clr(t[i]))!=-2)s->bc[j]=c;}
   if(!hs)s->bst[j]=0;}}
 else{
  for(j=0;j<4;j++)if(kw(k,bs_[j])){
   int hs=0;
   for(i=0;i<n;i++){
    if((x=bstyle(t[i]))>=0){s->bst[j]=x;hs=1;}
    else if((x=bwidth(t[i],s->fs))>=0)s->bw[j]=x;
    else if((c=clr(t[i]))!=-2)s->bc[j]=c;}
   if(!hs)s->bst[j]=0;
   return;}
 if(kw(k,"border-width")||kw(k,"border-style")||kw(k,"border-color")){
   static const char M[5][4]={{0,0,0,0},{0,0,0,0},{0,1,0,1},{0,1,2,1},{0,1,2,3}};
   for(j=0;j<4;j++){
    i=M[n>4?4:n][j];
    if(kw(k,"border-width")){x=bwidth(t[i],s->fs);if(x>=0)s->bw[j]=x;}
    else if(kw(k,"border-style")){x=bstyle(t[i]);if(x>=0)s->bst[j]=x;}
    else{c=clr(t[i]);if(c!=-2)s->bc[j]=c;}}
   return;}
  for(j=0;j<4;j++){
   if(seq(k,mp[j])){l=lenv(t[0],s->fs);if(l.k!=2)s->m[j]=l;return;}
   if(seq(k,pp[j])){l=lenv(t[0],s->fs);if(l.k==0&&l.px>=0&&l.pc>=0)s->p[j]=l;return;}
   if(seq(k,rn[j])){l=lenv(t[0],s->fs);if(l.k==0)s->rad[j]=l;return;}}
  if(kw(k,"border-radius")){
   Len a[4];S h=v;int sl_;
   for(sl_=0;sl_<h.n;sl_++)if(h.p[sl_]=='/'){h.n=sl_;break;}
   edge4(a,h,s->fs,1);
   s->rad[0]=a[0];s->rad[1]=a[1];s->rad[2]=a[2];s->rad[3]=a[3];}
  else if(kw(k,"box-shadow")){
   S a[4],q[8];int m,nn;
   s->shc=-1;s->shb=s->shx=s->shy=s->shs=0;
   if(kw(t[0],"none"))return;
   splitc(v,a,4);
   m=tk(a[0],q,8);nn=0;
   for(i=0;i<m;i++){
    if(kw(q[i],"inset"))return;
    if((c=clr(q[i]))!=-2)s->shc=c==-3?s->col:c;
    else{l=lenv(q[i],s->fs);if(l.k==0||l.k==3){x=l.px;if(x>400)x=400;if(x<-400)x=-400;if(nn==0)s->shx=x;else if(nn==1)s->shy=x;else if(nn==2)s->shb=x<0?0:x;else if(nn==3)s->shs=x;nn++;}}}
   if(s->shc==-2||(s->shc==-1&&nn==0))s->shc=-1;
   else if(s->shc==-1&&nn>=2&&0)s->shc=0x000000;}
  else if(kw(k,"flex")){
   s->fgrow=0;s->fshr=1000;s->fb=mk(0,0,1);
   if(kw(t[0],"none")){s->fshr=0;return;}
   if(kw(t[0],"auto")){s->fgrow=1000;return;}
   x=0;
   for(i=0;i<n;i++){l=lenv(t[i],s->fs);
    if(l.k==3&&x==0){s->fgrow=l.px;x=1;if(n==1)s->fb=mk(0,0,0);}
    else if(l.k==3&&x==1){s->fshr=l.px;x=2;}
    else if(l.k==0)s->fb=l;}}
  else if(kw(k,"flex-grow")){l=lenv(t[0],16);if(l.k==3)s->fgrow=l.px;}
  else if(kw(k,"flex-shrink")){l=lenv(t[0],16);if(l.k==3)s->fshr=l.px;}
  else if(kw(k,"flex-basis")){l=lenv(t[0],s->fs);if(l.k!=2)s->fb=l;if(kw(t[0],"content"))s->fb=mk(0,0,1);}
  else if(kw(k,"flex-direction")||kw(k,"flex-flow")){
   for(i=0;i<n;i++){
    if(kw(t[i],"row"))s->fdir=0;else if(kw(t[i],"column"))s->fdir=1;else if(kw(t[i],"row-reverse"))s->fdir=2;else if(kw(t[i],"column-reverse"))s->fdir=3;
    else if(kw(t[i],"wrap")||kw(t[i],"wrap-reverse"))s->fwrap=1;else if(kw(t[i],"nowrap"))s->fwrap=0;}}
  else if(kw(k,"flex-wrap"))s->fwrap=kw(t[0],"wrap")||kw(t[0],"wrap-reverse");
  else if(kw(k,"justify-content")){
   s->jc=kw(t[0],"center")?2:(kw(t[0],"flex-end")||kw(t[0],"end")||kw(t[0],"right"))?1:kw(t[0],"space-between")?3:kw(t[0],"space-around")?4:kw(t[0],"space-evenly")?5:0;}
  else if(kw(k,"align-items")||kw(k,"align-self")){
   c=kw(t[0],"center")?3:(kw(t[0],"flex-end")||kw(t[0],"end"))?2:(kw(t[0],"flex-start")||kw(t[0],"start")||kw(t[0],"baseline"))?1:0;
   if(kw(k,"align-items"))s->ai=c;else s->as=kw(t[0],"auto")?-1:c;}
  else if(kw(k,"gap")||kw(k,"grid-gap")){l=lenv(t[0],s->fs);if(l.k==0){s->gy=l;s->gx=n>1?lenv(t[1],s->fs):l;if(s->gx.k!=0)s->gx=l;}}
  else if(kw(k,"row-gap")||kw(k,"grid-row-gap")){l=lenv(t[0],s->fs);if(l.k==0)s->gy=l;}
  else if(kw(k,"column-gap")||kw(k,"grid-column-gap")){l=lenv(t[0],s->fs);if(l.k==0)s->gx=l;}
  else if(kw(k,"order")){l=lenv(t[0],16);if(l.k==3)s->ord=l.px/1000;}
  else if(kw(k,"list-style")||kw(k,"list-style-type")){
   static const char*L[]={"none","disc","circle","square","decimal","decimal-leading-zero","lower-alpha","upper-alpha","lower-latin","upper-latin","lower-roman","upper-roman",0};
   for(i=0;i<n;i++)for(j=0;L[j];j++)if(kw(t[i],L[j]))s->lst=j==8?6:j==9?7:j;}
  else if(kw(k,"border-collapse"))s->bcol=kw(t[0],"collapse");
  else if(kw(k,"border-spacing")){l=lenv(t[0],s->fs);if(l.k==0)s->bsp=l.px;}
  else if(kw(k,"content"))setcont(s,v);
  else if(kw(k,"grid-template-columns")){
   s->gtn=0;s->gtauto=0;
   for(i=0;i<n&&s->gtn<12;i++){
    if(starts(t[i],"repeat(")){
     S in=(S){t[i].p+7,t[i].n-8},a[2],tr[4];int cnt,m,q;
     if(splitc(in,a,2)<2)continue;
     m=tk(a[1],tr,4);
     if(kw(a[0],"auto-fill")||kw(a[0],"auto-fit")){
      s->gtauto=1;s->gtmin=0;
      if(m&&starts(tr[0],"minmax(")){S mi[2];if(splitc((S){tr[0].p+7,tr[0].n-8},mi,2)==2){Len ml=lenv(mi[0],s->fs);if(ml.k==0)s->gtmin=ml.px;}}
      else if(m){Len ml=lenv(tr[0],s->fs);if(ml.k==0)s->gtmin=ml.px;}
      continue;}
     {Len cl=lenv(a[0],16);cnt=cl.k==3?cl.px/1000:0;}
     if(cnt>12)cnt=12;
     for(q=0;q<cnt;q++)for(j=0;j<m&&s->gtn<12;j++){
      S tt=tr[j];
      if(starts(tt,"minmax(")){S mi[2];if(splitc((S){tt.p+7,tt.n-8},mi,2)==2)tt=mi[1];}
      Len tl=lenv(tt,s->fs);
      if(tt.n>2&&seq((S){tt.p+tt.n-2,2},"fr")){s->gtk[s->gtn]=1;s->gt[s->gtn]=mk(0,0,3);{S nn_=(S){tt.p,tt.n-2};Len f=lenv(nn_,16);s->gt[s->gtn].px=f.k==3?f.px:1000;}}
      else if(tl.k==0){s->gtk[s->gtn]=0;s->gt[s->gtn]=tl;}
      else s->gtk[s->gtn]=2;
      s->gtn++;}
     continue;}
    {S tt=t[i];Len tl;
     if(starts(tt,"minmax(")){S mi[2];if(splitc((S){tt.p+7,tt.n-8},mi,2)==2)tt=mi[1];}
     tl=lenv(tt,s->fs);
     if(tt.n>2&&seq((S){tt.p+tt.n-2,2},"fr")){Len f=lenv((S){tt.p,tt.n-2},16);s->gtk[s->gtn]=1;s->gt[s->gtn]=mk(f.k==3?f.px:1000,0,3);}
     else if(tl.k==0){s->gtk[s->gtn]=0;s->gt[s->gtn]=tl;}
     else s->gtk[s->gtn]=2;
     s->gtn++;}}}
  else if(kw(k,"grid-column")||kw(k,"grid-column-end")||kw(k,"grid-column-start")){
   for(i=0;i+1<n;i++)if(kw(t[i],"span")){l=lenv(t[i+1],16);if(l.k==3){s->gcs=l.px/1000;if(s->gcs>12)s->gcs=12;}}}}}
static void decl(St*s,S d,int pass){
 int i=0,a,b,pd,e,imp,j;S k,v;
 while(i<d.n){
  a=i;while(i<d.n&&d.p[i]!=':'&&d.p[i]!=';')i++;
  if(i>=d.n||d.p[i]==';'){i++;continue;}
  k=trim((S){d.p+a,i-a});i++;
  b=i;pd=0;while(i<d.n&&(d.p[i]!=';'||pd)){if(d.p[i]=='(')pd++;else if(d.p[i]==')')pd--;i++;}
  v=trim((S){d.p+b,i-b});i++;
  imp=0;
  for(e=v.n-1;e>=0;e--)if(v.p[e]=='!'){imp=1;v.n=e;v=trim(v);break;}
  if(k.n>2&&k.p[0]=='-'&&k.p[1]=='-'){
   if(pass==1&&nvar<NV){vnm[nvar]=k;vvl[nvar]=v;nvar++;}
   continue;}
  if(pass==1)continue;
  if(imp!=(pass==3))continue;
  v=subvar(v);
  if(!v.n)continue;
  (void)j;
  prop(s,k,v);}}

/* ---- DOM + HTML parser ---- */
static N*root;static S ttl;static S sshref[16];static int nss;
static N*nn(S tag){N*n=al(sizeof(N));n->tag=tag;return n;}
static void add(N*p,N*n){
 n->par=p;
 if(p->last)p->last->nx=n;else p->kid=n;
 p->last=n;
 if(n->tag.n){n->pv=p->lel;p->lel=n;}}
static int isvoid(S t){static const char*v[]={"br","hr","img","meta","link","input","wbr","area","base","col","embed","param","source","track",0};int i;for(i=0;v[i];i++)if(seq(t,v[i]))return 1;return 0;}
static int blkt(S t){static const char*v[]={"p","div","ul","ol","li","hr","pre","blockquote","table","section","article","header","footer","nav","main","aside","form","h1","h2","h3","h4","h5","h6","dl","dt","dd","figure","fieldset","details","address",0};int i;for(i=0;v[i];i++)if(seq(t,v[i]))return 1;return 0;}
static int one(S t,const char*a,const char*b,const char*c,const char*d){return seq(t,a)||(b&&seq(t,b))||(c&&seq(t,c))||(d&&seq(t,d));}
static int autoclose(S nm,S cur){
 if(seq(cur,"p"))return blkt(nm);
 if(seq(cur,"li"))return seq(nm,"li");
 if(one(cur,"dt","dd",0,0))return one(nm,"dt","dd",0,0);
 if(one(cur,"td","th",0,0))return one(nm,"td","th","tr","thead")||one(nm,"tbody","tfoot",0,0);
 if(seq(cur,"tr"))return one(nm,"tr","thead","tbody","tfoot");
 if(one(cur,"thead","tbody","tfoot",0))return one(nm,"thead","tbody","tfoot",0);
 if(seq(cur,"option"))return one(nm,"option","optgroup",0,0);
 if(seq(cur,"optgroup"))return seq(nm,"optgroup");
 return 0;}
static int istag(const char*s,int i,int n){return s[i]=='<'&&i+1<n&&(isa(s[i+1])||s[i+1]=='/'||s[i+1]=='!'||s[i+1]=='?');}
static const struct{const char*n;int c;}EN[]={{"AElig",198},{"Aacute",193},{"Acirc",194},{"Agrave",192},{"Aring",197},{"Atilde",195},{"Auml",196},{"Ccedil",199},{"ETH",208},{"Eacute",201},{"Ecirc",202},{"Egrave",200},{"Euml",203},{"Iacute",205},{"Icirc",206},{"Igrave",204},{"Iuml",207},{"Ntilde",209},{"Oacute",211},{"Ocirc",212},{"Ograve",210},{"Oslash",216},{"Otilde",213},{"Ouml",214},{"THORN",222},{"Uacute",218},{"Ucirc",219},{"Ugrave",217},{"Uuml",220},{"Yacute",221},{"aacute",225},{"acirc",226},{"acute",180},{"aelig",230},{"agrave",224},{"amp",38},{"apos",39},{"aring",229},{"atilde",227},{"auml",228},{"bdquo",8222},{"bull",8226},{"ccedil",231},{"cedil",184},{"cent",162},{"copy",169},{"darr",8595},{"deg",176},{"divide",247},{"eacute",233},{"ecirc",234},{"egrave",232},{"eth",240},{"euml",235},{"euro",8364},{"gt",62},{"hellip",8230},{"iacute",237},{"icirc",238},{"iexcl",161},{"igrave",236},{"iquest",191},{"iuml",239},{"laquo",171},{"larr",8592},{"ldquo",8220},{"lsaquo",8249},{"lsquo",8216},{"lt",60},{"macr",175},{"mdash",8212},{"middot",183},{"minus",8722},{"nbsp",160},{"ndash",8211},{"ntilde",241},{"oacute",243},{"ocirc",244},{"ograve",242},{"ordf",170},{"ordm",186},{"oslash",248},{"otilde",245},{"ouml",246},{"para",182},{"permil",8240},{"pound",163},{"quot",34},{"raquo",187},{"rarr",8594},{"rdquo",8221},{"reg",174},{"rsaquo",8250},{"rsquo",8217},{"sbquo",8218},{"sect",167},{"szlig",223},{"thorn",254},{"times",215},{"trade",8482},{"uacute",250},{"uarr",8593},{"ucirc",251},{"ugrave",249},{"uml",168},{"uuml",252},{"yacute",253},{"yen",165},{"yuml",255},{0,0}};
static S dec(S t){ /* in-place entity decode; output never longer than input */
 char*p=t.p;int o=0,i=0,j,c,v,k;S e;
 while(i<t.n){
  if(p[i]=='&'){
   j=i+1;while(j<t.n&&j-i<12&&p[j]!=';'&&!isws(p[j]))j++;
   if(j<t.n&&p[j]==';'){
    e=(S){p+i+1,j-i-1};c=0;
    for(k=0;EN[k].n;k++)if(e.n&&seq(e,EN[k].n)){c=EN[k].c;break;}
    if(!c&&e.n>1&&e.p[0]=='#'){
     v=0;
     if(lc(e.p[1])=='x'){for(k=2;k<e.n&&hx(e.p[k])>=0&&v<0x110000;k++)v=v*16+hx(e.p[k]);}
     else for(k=1;k<e.n&&isdg(e.p[k])&&v<0x110000;k++)v=v*10+e.p[k]-'0';
     c=(v>0&&v<0x110000&&!(v>=0xD800&&v<0xE000))?v:0xFFFD;}
    if(c){o+=put8(p+o,c);i=j+1;continue;}}}
  p[o++]=p[i++];}
 t.n=o;return t;}
/* attribute lookup: returns S with p==0 when absent; p!=0 (maybe n==0) when present */
static S getattr(N*n,const char*name){
 S a=n->at,r={0,0};int i=0,j,ka,q;
 while(i<a.n){
  while(i<a.n&&(isws(a.p[i])||a.p[i]=='/'))i++;
  if(i>=a.n)break;
  ka=i;while(i<a.n&&!isws(a.p[i])&&a.p[i]!='='&&a.p[i]!='/')i++;
  j=i;
  while(i<a.n&&isws(a.p[i]))i++;
  r=(S){0,0};
  if(i<a.n&&a.p[i]=='='){
   i++;while(i<a.n&&isws(a.p[i]))i++;
   if(i<a.n&&(a.p[i]=='"'||a.p[i]=='\'')){q=a.p[i++];r.p=a.p+i;while(i<a.n&&a.p[i]!=q)i++;r.n=(int)(a.p+i-r.p);i++;}
   else{r.p=a.p+i;while(i<a.n&&!isws(a.p[i]))i++;r.n=(int)(a.p+i-r.p);}}
  else{r.p=(char*)"";r.n=0;}
  if(seq((S){a.p+ka,j-ka},name))return r;}
 return(S){0,0};}
static int numat(N*n,const char*name,int*pc){ /* "12" or "50%" -> value; -1 absent */
 S v=getattr(n,name);int i=0,x=0;
 if(!v.p||!v.n)return-1;
 v=trim(v);
 if(!isdg(v.p[0]))return-1;
 while(i<v.n&&isdg(v.p[i])){if(x<100000)x=x*10+v.p[i]-'0';i++;}
 if(pc)*pc=i<v.n&&v.p[i]=='%';
 return x;}
static void css(S t);
static N*parse(char*s,int n){
 N*rt=nn((S){"#",1}),*cur=rt,*e,*x;
 int i=0,dp=0,a,j,k,sc,ra,va,j0;S nm,key,v,t;char q;N*p;
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
  nm=(S){s+a,j-a};e=nn(nm);sc=0;j0=j;
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
  e->at=(S){s+j0,(j>j0?j-1:j)-j0};
  if(e->at.n<0)e->at.n=0;
  while(cur!=rt&&autoclose(nm,cur->tag)){cur=cur->par;dp--;}
  add(cur,e);
  if(seq(nm,"link")&&nss<16){S rel=getattr(e,"rel"),hr=getattr(e,"href");int k2;
   for(k2=0;k2+10<=rel.n;k2++)if(seq((S){rel.p+k2,10},"stylesheet")){if(hr.p&&hr.n)sshref[nss++]=dec(hr);break;}}
  if(seq(nm,"style")||seq(nm,"script")||seq(nm,"textarea")){
   ra=j;while(j<n&&!(s[j]=='<'&&j+2+nm.n<=n&&s[j+1]=='/'&&seqs((S){s+j+2,nm.n},nm)))j++;
   if(seq(nm,"style"))css((S){s+ra,j-ra});
   else if(seq(nm,"textarea")){
    if(ra<j&&s[ra]=='\n')ra++;
    t=dec((S){s+ra,j-ra});
    if(t.n){x=nn((S){0,0});x->txt=t;add(e,x);}}
   while(j<n&&s[j]!='>')j++;
   j++;}
  else if(!sc&&!isvoid(nm)&&dp<200){cur=e;dp++;}
  i=j;}
 return rt;}

/* ---- selectors, rules, @media ---- */
typedef struct{S n,v;int op;}At;
typedef struct{int t,a,b;}Ps;
typedef struct{S tag,id,cls[3];int nc;At at[3];int na;Ps ps[3];int np;int nt;}Cp;
typedef struct{Cp c[4];char comb[4];int ns,spec,pe,mnw,mxw,mnh,mxh;S d;}Rule;
static Rule rls[MR];static int nr,rdirty;
static Cp nots[96];static int nnot;
static int qb[4]={0,1<<30,0,1<<30};
enum{PS_FIRST=1,PS_LAST,PS_ONLY,PS_NTH,PS_NTHL,PS_FIRSTT,PS_LASTT,PS_ONLYT,PS_NTHT,PS_EMPTY,PS_ROOT,PS_LINK,PS_CHECKED,PS_DISABLED,PS_ENABLED};
static int nthp(S a,int*A,int*B){
 char b[32];int n=0,i=0,sg=1,v=0,dg=0,hn=0;
 for(;a.n&&n<31;a.p++,a.n--)if(!isws(*a.p))b[n++]=(char)lc(*a.p);
 b[n]=0;
 if(!sl(b))return 0;
 if(b[0]=='o'&&b[1]=='d'&&b[2]=='d'&&!b[3]){*A=2;*B=1;return 1;}
 if(b[0]=='e'&&b[1]=='v'&&b[2]=='e'&&b[3]=='n'&&!b[4]){*A=2;*B=0;return 1;}
 *A=0;*B=0;
 if(b[i]=='-'){sg=-1;i++;}else if(b[i]=='+')i++;
 while(isdg(b[i])){v=v*10+b[i]-'0';i++;dg=1;if(v>100000)return 0;}
 if(b[i]=='n'){hn=1;*A=(dg?v:1)*sg;i++;
  if(b[i]=='+'||b[i]=='-'){int s2=b[i]=='-'?-1:1;i++;v=0;dg=0;while(isdg(b[i])){v=v*10+b[i]-'0';i++;dg=1;if(v>100000)return 0;}if(!dg)return 0;*B=v*s2;}}
 else{if(!dg)return 0;*B=v*sg;}
 (void)hn;
 return b[i]==0;}
static int pcomp(S s,int*pi,Cp*c,int*spec,int*pe,int depth){
 int i=*pi,j,st,k,q,cl_=0;char ch;S v,nm,arg;
 c->tag.n=c->id.n=0;c->nc=c->na=c->np=c->nt=0;
 while(i<s.n&&!isws(s.p[i])&&s.p[i]!='>'&&s.p[i]!='+'&&s.p[i]!='~'&&s.p[i]!=','){
  ch=s.p[i];
  if(ch=='*'){i++;continue;}
  if(ch=='#'||ch=='.'){
   st=i+1;j=st;while(j<s.n&&isal(s.p[j]))j++;
   if(j==st)return 0;
   v=(S){s.p+st,j-st};
   if(ch=='#'){c->id=v;*spec+=10000;}else{if(c->nc>=3)return 0;c->cls[c->nc++]=v;*spec+=100;}
   i=j;continue;}
  if(ch=='['){
   At*a;
   if(c->na>=3)return 0;
   st=i+1;k=st;q=0;
   while(k<s.n&&(q||s.p[k]!=']')){if(s.p[k]=='"'||s.p[k]=='\''){if(q==s.p[k])q=0;else if(!q)q=s.p[k];}k++;}
   if(k>=s.n)return 0;
   arg=trim((S){s.p+st,k-st});
   cl_=k;
   a=&c->at[c->na++];
   j=0;while(j<arg.n&&!isws(arg.p[j])&&!(arg.p[j]=='='||((arg.p[j]=='~'||arg.p[j]=='|'||arg.p[j]=='^'||arg.p[j]=='$'||arg.p[j]=='*')&&j+1<arg.n&&arg.p[j+1]=='=')))j++;
   a->n=(S){arg.p,j};a->op=0;a->v=(S){0,0};
   while(j<arg.n&&isws(arg.p[j]))j++;
   if(j<arg.n){
    a->op=arg.p[j]=='='?1:arg.p[j]=='~'?2:arg.p[j]=='^'?3:arg.p[j]=='$'?4:arg.p[j]=='*'?5:arg.p[j]=='|'?6:0;
    if(!a->op)return 0;
    j+=a->op==1?1:2;
    while(j<arg.n&&isws(arg.p[j]))j++;
    v=(S){arg.p+j,arg.n-j};
    if(v.n&&(v.p[0]=='"'||v.p[0]=='\'')){q=v.p[0];v.p++;v.n--;for(k=0;k<v.n&&v.p[k]!=q;k++);v.n=k;}
    else{for(k=0;k<v.n&&!isws(v.p[k]);k++);v.n=k;}
    a->v=v;}
   *spec+=100;i=cl_+1;continue;}
  if(ch==':'){
   if(i+1<s.n&&s.p[i+1]==':'){
    st=i+2;j=st;while(j<s.n&&isal(s.p[j]))j++;
    nm=(S){s.p+st,j-st};
    if(seq(nm,"before"))*pe=1;else if(seq(nm,"after"))*pe=2;else return 0;
    *spec+=1;i=j;continue;}
   st=i+1;j=st;while(j<s.n&&isal(s.p[j]))j++;
   nm=(S){s.p+st,j-st};arg=(S){0,0};
   if(j<s.n&&s.p[j]=='('){
    int d=1;k=j+1;while(k<s.n&&d){if(s.p[k]=='(')d++;else if(s.p[k]==')')d--;k++;}
    if(d)return 0;
    arg=(S){s.p+j+1,k-j-2};j=k;}
   if(seq(nm,"before")||seq(nm,"after")){*pe=seq(nm,"before")?1:2;*spec+=1;i=j;continue;}
   if(seq(nm,"not")){
    int sp2=0,pe2=0,ii=0;Cp*nc_;
    if(depth||nnot>=96||c->nt)return 0;
    arg=trim(arg);nc_=&nots[nnot];c->nt=++nnot;
    if(!pcomp(arg,&ii,nc_,&sp2,&pe2,1)||ii<arg.n||pe2)return 0;
    *spec+=sp2;i=j;continue;}
   {Ps*p;int t=0,A=0,B=0;
    if(c->np>=3)return 0;
    if(seq(nm,"first-child"))t=PS_FIRST;else if(seq(nm,"last-child"))t=PS_LAST;else if(seq(nm,"only-child"))t=PS_ONLY;
    else if(seq(nm,"first-of-type"))t=PS_FIRSTT;else if(seq(nm,"last-of-type"))t=PS_LASTT;else if(seq(nm,"only-of-type"))t=PS_ONLYT;
    else if(seq(nm,"empty"))t=PS_EMPTY;else if(seq(nm,"root"))t=PS_ROOT;
    else if(seq(nm,"link")||seq(nm,"any-link"))t=PS_LINK;
    else if(seq(nm,"checked"))t=PS_CHECKED;else if(seq(nm,"disabled"))t=PS_DISABLED;else if(seq(nm,"enabled"))t=PS_ENABLED;
    else if(seq(nm,"nth-child")||seq(nm,"nth-last-child")||seq(nm,"nth-of-type")){
     if(!nthp(arg,&A,&B))return 0;
     t=seq(nm,"nth-child")?PS_NTH:seq(nm,"nth-last-child")?PS_NTHL:PS_NTHT;}
    else return 0; /* :hover :focus :active :visited ... cannot apply to a static page */
    p=&c->ps[c->np++];p->t=t;p->a=A;p->b=B;*spec+=100;i=j;continue;}}
  if(isa(ch)){st=i;j=i;while(j<s.n&&isal(s.p[j]))j++;c->tag=(S){s.p+st,j-st};*spec+=1;i=j;continue;}
  return 0;}
 *pi=i;return 1;}
static void addsel(S s,S d){
 Rule*r;int i=0,cb,pe=0,sp,k;
 if(nr>=MR)return;
 r=&rls[nr];r->ns=0;r->spec=0;r->pe=0;r->d=d;
 r->mnw=qb[0];r->mxw=qb[1];r->mnh=qb[2];r->mxh=qb[3];
 s=trim(s);
 while(i<s.n){
  cb=0;
  while(i<s.n&&isws(s.p[i]))i++;
  if(i>=s.n)break;
  if(s.p[i]=='>'||s.p[i]=='+'||s.p[i]=='~'){
   if(!r->ns)return;
   cb=s.p[i]=='>'?1:s.p[i]=='+'?2:3;i++;
   while(i<s.n&&isws(s.p[i]))i++;}
  if(r->ns>=4||pe)return;
  sp=0;
  if(!pcomp(s,&i,&r->c[r->ns],&sp,&pe,0))return;
  if(i==0)return;
  r->comb[r->ns]=(char)cb;r->ns++;r->spec+=sp;
  (void)k;}
 r->pe=pe;
 if(r->ns){nr++;rdirty=1;}}
static int mq1(S q,int*b){
 S t[16],nm,val;int n=tk(q,t,16),i,j,l,ok=1;
 for(i=0;i<n;i++){
  if(seq(t[i],"print")||seq(t[i],"speech")||seq(t[i],"not"))return 0;
  if(t[i].p[0]!='(')continue;
  nm=trim((S){t[i].p+1,t[i].n-2});val=(S){0,0};
  for(j=0;j<nm.n;j++)if(nm.p[j]==':'){val=trim((S){nm.p+j+1,nm.n-j-1});nm=trim((S){nm.p,j});break;}
  {Len ln_=lenv(val,16);l=ln_.k==0?ln_.px:-1;}
  if(seq(nm,"min-width")&&l>=0){if(l>b[0])b[0]=l;}
  else if(seq(nm,"max-width")&&l>=0){if(l<b[1])b[1]=l;}
  else if(seq(nm,"min-height")&&l>=0){if(l>b[2])b[2]=l;}
  else if(seq(nm,"max-height")&&l>=0){if(l<b[3])b[3]=l;}
  else if(seq(nm,"width")&&l>=0){b[0]=b[1]=l;}
  else if(seq(nm,"height")&&l>=0){b[2]=b[3]=l;}
  else if(seq(nm,"prefers-color-scheme")&&seq(val,"dark"))return 0;
  else if(seq(nm,"prefers-reduced-motion")&&seq(val,"reduce"))return 0;
  else if((seq(nm,"hover")&&seq(val,"none"))||(seq(nm,"pointer")&&(seq(val,"coarse")||seq(val,"none")))||(seq(nm,"forced-colors")&&seq(val,"active")))return 0;}
 (void)ok;return 1;}
static void css(S t){
 int i=0,a,b,d,be,p0,q,sv[4],k,m;S qs[6],pre;
 while(i<t.n){
  while(i<t.n&&isws(t.p[i]))i++;
  if(i+1<t.n&&t.p[i]=='/'&&t.p[i+1]=='*'){i+=2;while(i+1<t.n&&!(t.p[i]=='*'&&t.p[i+1]=='/'))i++;i+=2;continue;}
  if(i>=t.n)break;
  a=i;while(i<t.n&&t.p[i]!='{'&&t.p[i]!=';')i++;
  if(i>=t.n)break;
  if(t.p[i]==';'){i++;continue;}
  be=i;i++;b=i;d=1;
  while(i<t.n&&d){if(t.p[i]=='{')d++;else if(t.p[i]=='}')d--;i++;}
  d=i-(d?0:1);
  if(t.p[a]=='@'){
   pre=trim((S){t.p+a,be-a});
   if(starts(pre,"@media")){
    for(k=0;k<4;k++)sv[k]=qb[k];
    m=splitc((S){pre.p+6,pre.n-6},qs,6);
    for(k=0;k<m;k++){int tb[4]={sv[0],sv[1],sv[2],sv[3]};
     if(mq1(qs[k],tb)){qb[0]=tb[0];qb[1]=tb[1];qb[2]=tb[2];qb[3]=tb[3];css((S){t.p+b,d-b});break;}}
    for(k=0;k<4;k++)qb[k]=sv[k];}
   else if((starts(pre,"@supports")&&!(pre.n>10&&(pre.p[10]=='n'||pre.p[10]=='N')))||starts(pre,"@layer"))css((S){t.p+b,d-b});
   continue;}
  p0=a;
  for(q=a;q<=be;q++)if(q==be||t.p[q]==','){addsel((S){t.p+p0,q-p0},(S){t.p+b,d-b});p0=q+1;}}}
/* ---- matching ---- */
static int hc(S cl,S k){int i=0,a;while(i<cl.n){while(i<cl.n&&isws(cl.p[i]))i++;a=i;while(i<cl.n&&!isws(cl.p[i]))i++;if(i>a&&seqs((S){cl.p+a,i-a},k))return 1;}return 0;}
static N*nxel(N*n){N*c=n->nx;while(c&&(!c->tag.n||c->gen))c=c->nx;return c;}
static int eidx(N*n,int last,int type){
 int k=1;N*c;
 if(!last){for(c=n->pv;c;c=c->pv)if(!type||seqs(c->tag,n->tag))k++;}
 else for(c=nxel(n);c;c=nxel(c))if(!type||seqs(c->tag,n->tag))k++;
 return k;}
static int nthok(int idx,int A,int B){
 if(A==0)return idx==B;
 return((idx-B)%A==0)&&((idx-B)/A>=0);}
static int cp1(N*n,Cp*c){
 int k,i;S v;
 if(c->tag.n&&!seqs(n->tag,c->tag))return 0;
 if(c->id.n&&!seqs(n->id,c->id))return 0;
 for(k=0;k<c->nc;k++)if(!hc(n->cls,c->cls[k]))return 0;
 for(k=0;k<c->na;k++){
  At*a=&c->at[k];
  char nb[64];
  for(i=0;i<a->n.n&&i<63;i++)nb[i]=a->n.p[i];
  nb[i]=0;
  v=getattr(n,nb);
  if(!v.p)return 0;
  switch(a->op){
   case 1:if(!seqs(v,a->v))return 0;break;
   case 2:if(!hc(v,a->v))return 0;break;
   case 3:if(!a->v.n||v.n<a->v.n||!seqs((S){v.p,a->v.n},a->v))return 0;break;
   case 4:if(!a->v.n||v.n<a->v.n||!seqs((S){v.p+v.n-a->v.n,a->v.n},a->v))return 0;break;
   case 5:{int f=0;if(!a->v.n)return 0;for(i=0;i+a->v.n<=v.n;i++)if(seqs((S){v.p+i,a->v.n},a->v)){f=1;break;}if(!f)return 0;break;}
   case 6:if(!(seqs(v,a->v)||(v.n>a->v.n&&seqs((S){v.p,a->v.n},a->v)&&v.p[a->v.n]=='-')))return 0;break;}}
 for(k=0;k<c->np;k++){
  Ps*p=&c->ps[k];
  switch(p->t){
   case PS_FIRST:if(n->pv)return 0;break;
   case PS_LAST:if(nxel(n))return 0;break;
   case PS_ONLY:if(n->pv||nxel(n))return 0;break;
   case PS_NTH:if(!nthok(eidx(n,0,0),p->a,p->b))return 0;break;
   case PS_NTHL:if(!nthok(eidx(n,1,0),p->a,p->b))return 0;break;
   case PS_FIRSTT:if(eidx(n,0,1)!=1)return 0;break;
   case PS_LASTT:if(eidx(n,1,1)!=1)return 0;break;
   case PS_ONLYT:if(eidx(n,0,1)!=1||eidx(n,1,1)!=1)return 0;break;
   case PS_NTHT:if(!nthok(eidx(n,0,1),p->a,p->b))return 0;break;
   case PS_EMPTY:if(n->kid)return 0;break;
   case PS_ROOT:if(n->par!=root)return 0;break;
   case PS_LINK:if(!(seq(n->tag,"a")&&n->href.n))return 0;break;
   case PS_CHECKED:if(!getattr(n,"checked").p)return 0;break;
   case PS_DISABLED:if(!getattr(n,"disabled").p)return 0;break;
   case PS_ENABLED:if(getattr(n,"disabled").p)return 0;break;}}
 if(c->nt&&cp1(n,&nots[c->nt-1]))return 0;
 return 1;}
static int mtchk(N*n,Rule*r,int k){
 N*a;
 if(!cp1(n,&r->c[k]))return 0;
 if(!k)return 1;
 switch(r->comb[k]){
  case 0:for(a=n->par;a&&a!=root;a=a->par)if(mtchk(a,r,k-1))return 1;return 0;
  case 1:a=n->par;return a&&a!=root&&mtchk(a,r,k-1);
  case 2:return n->pv&&mtchk(n->pv,r,k-1);
  default:for(a=n->pv;a;a=a->pv)if(mtchk(a,r,k-1))return 1;return 0;}}
static int rmatch(N*n,Rule*r,int pe){
 return r->pe==pe&&VW>=r->mnw&&VW<=r->mxw&&VH>=r->mnh&&VH<=r->mxh&&mtchk(n,r,r->ns-1);}
static void sortrules(void){
 int i,j;Rule t;
 if(!rdirty)return;
 for(i=1;i<nr;i++){t=rls[i];j=i-1;while(j>=0&&rls[j].spec>t.spec){rls[j+1]=rls[j];j--;}rls[j+1]=t;}
 rdirty=0;}

/* ---- user-agent stylesheet ---- */
static const char UA[]=
"html,body,div,p,h1,h2,h3,h4,h5,h6,ul,ol,li,hr,pre,blockquote,section,article,header,footer,nav,main,aside,form,fieldset,legend,figure,figcaption,address,dl,dt,dd,details,summary,hgroup,menu,dir,center,optgroup,caption,search{display:block}"
"head,style,script,title,meta,link,base,template,noembed,area,datalist,param,source,track,svg,option,audio,colgroup,col,[hidden],dialog:not([open]),input[type=hidden]{display:none}"
"li{display:list-item}table{display:table;border-spacing:2px}tr{display:table-row}td,th{display:table-cell;padding:1px}thead,tbody,tfoot{display:table-row-group}"
"body{margin:8px}p,dl,blockquote,figure,pre,ul,ol,menu,hgroup,fieldset{margin:1em 0}"
"h1{font-size:2em;margin:.67em 0}h2{font-size:1.5em;margin:.83em 0}h3{font-size:1.17em;margin:1em 0}h4{margin:1.33em 0}h5{font-size:.83em;margin:1.67em 0}h6{font-size:.67em;margin:2.33em 0}"
"h1,h2,h3,h4,h5,h6,b,strong,th,dt{font-weight:bold}dt{font-weight:normal}"
"i,em,cite,dfn,var,address{font-style:italic}u,ins{text-decoration:underline}s,strike,del{text-decoration:line-through}"
"a{color:#0b57d0;text-decoration:underline}mark{background:#ff0;color:#000}small{font-size:smaller}big{font-size:larger}"
"sub{vertical-align:sub;font-size:smaller}sup{vertical-align:super;font-size:smaller}"
"q::before{content:\"\\201C\"}q::after{content:\"\\201D\"}"
"ul,ol,menu{padding-left:28px}ul,menu{list-style-type:disc}ol{list-style-type:decimal}ul ul,ol ul{list-style-type:circle}ul ul ul,ol ul ul,ul ol ul{list-style-type:square}"
"dd{margin-left:40px}blockquote{margin:1em 40px}hr{height:1px;background:#bbb;margin:12px 0}"
"pre{white-space:pre}textarea{white-space:pre-wrap}center{text-align:center}th{text-align:center}caption{text-align:center}"
"fieldset{margin:0 2px;padding:.35em .75em .625em;border:1px solid #c0c0c0}legend{padding:0 2px}"
"img{display:inline-block;min-width:16px;min-height:16px;background:#f1f1f1;border:1px solid #d0d0d0;color:#666;box-sizing:border-box}"
"iframe,video,canvas,object,embed{display:inline-block;width:300px;height:150px;background:#eee;border:1px solid #ccc;box-sizing:border-box}"
"input,button,select,textarea{display:inline-block;font-size:13px;color:#000;box-sizing:border-box}"
"input,select,textarea{background:#fff;border:1px solid #767676;border-radius:2px;padding:2px 4px}input{width:170px}"
"input[type=checkbox],input[type=radio]{width:13px;height:13px;padding:0;margin:3px 3px 3px 4px}input[type=radio]{border-radius:50%}"
"input[type=checkbox][checked],input[type=radio][checked]{background:#0b57d0;border-color:#0b57d0}"
"button,input[type=button],input[type=submit],input[type=reset]{width:auto;background:#efefef;border:1px solid #767676;border-radius:3px;padding:2px 10px;text-align:center}"
"select{padding:2px 22px 2px 6px;min-width:60px}textarea{width:200px;height:56px;vertical-align:top}"
"label{display:inline}details>summary{display:block;padding-left:18px}";

/* ---- presentational attributes (HTML4-style hints) ---- */
static int hcolor(S a){
 int c;char b[8];int i;
 a=trim(a);
 if(!a.n)return-2;
 c=clr(a);
 if(c!=-2)return c;
 if(a.n==6){for(i=0;i<6;i++)if(hx(a.p[i])<0)return-2;b[0]='#';for(i=0;i<6;i++)b[i+1]=a.p[i];return clr((S){b,7});}
 return-2;}
static N*up(N*c,const char*t){N*p=c->par;while(p&&p!=root&&!seq(p->tag,t))p=p->par;return p&&p!=root?p:0;}
static void hints(N*c,St*s){
 int v,pc=0,i,k;S a;N*t;
 a=getattr(c,"bgcolor");
 if(a.p&&(k=hcolor(a))!=-2)s->bg=k;
 if(seq(c->tag,"table")){
  v=numat(c,"border",0);
  if(v>0)for(i=0;i<4;i++){s->bw[i]=v;s->bst[i]=1;s->bc[i]=0x808080;}
  v=numat(c,"cellspacing",0);if(v>=0)s->bsp=v;
  a=getattr(c,"align");if(a.p&&seq(a,"center")){s->m[1]=s->m[3]=mk(0,0,1);}}
 if(seq(c->tag,"td")||seq(c->tag,"th")){
  t=up(c,"table");
  if(t){
   v=numat(t,"cellpadding",0);if(v>=0)for(i=0;i<4;i++)s->p[i]=mk(v,0,0);
   v=numat(t,"border",0);if(v>0)for(i=0;i<4;i++){s->bw[i]=1;s->bst[i]=1;s->bc[i]=0xa0a0a0;}}
  a=getattr(c,"valign");if(a.p)s->vaa=seq(a,"top")?1:seq(a,"bottom")?3:seq(a,"middle")?2:0;
  if(getattr(c,"nowrap").p)s->ws=1;}
 if(seq(c->tag,"table")||seq(c->tag,"td")||seq(c->tag,"th")||seq(c->tag,"img")||seq(c->tag,"iframe")||seq(c->tag,"canvas")||seq(c->tag,"video")||seq(c->tag,"embed")||seq(c->tag,"object")||seq(c->tag,"hr")||seq(c->tag,"col")){
  v=numat(c,"width",&pc);if(v>=0)s->w=pc?mk(0,v,0):mk(v,0,0);
  v=numat(c,"height",&pc);if(v>=0&&!pc)s->h=mk(v,0,0);}
 if(seq(c->tag,"div")||seq(c->tag,"p")||seq(c->tag,"td")||seq(c->tag,"th")||seq(c->tag,"tr")||(c->tag.n==2&&lc(c->tag.p[0])=='h'&&isdg(c->tag.p[1]))){
  a=getattr(c,"align");if(a.p)s->al=seq(a,"center")?1:seq(a,"right")?2:0;}
 if(seq(c->tag,"font")){
  a=getattr(c,"color");if(a.p&&(k=hcolor(a))>=0)s->col=k;
  v=numat(c,"size",0);
  if(v>=1){static const int F[]={10,13,16,18,24,32,48};s->fs=F[v>7?6:v-1];}}
 if(seq(c->tag,"body")){a=getattr(c,"text");if(a.p&&(k=hcolor(a))>=0)s->col=k;}
 if(seq(c->tag,"ol")||seq(c->tag,"ul")){
  a=getattr(c,"type");
  if(a.p&&a.n==1){s->lst=a.p[0]=='1'?4:a.p[0]=='a'?6:a.p[0]=='A'?7:a.p[0]=='i'?10:a.p[0]=='I'?11:s->lst;}
  else if(a.p&&seq(a,"circle"))s->lst=2;else if(a.p&&seq(a,"square"))s->lst=3;else if(a.p&&seq(a,"disc"))s->lst=1;}
 if(seq(c->tag,"input")){
  v=numat(c,"size",0);
  if(v>0&&getattr(c,"type").n==0)s->w=mk(v*7+10,0,0);}
 if(seq(c->tag,"textarea")){
  v=numat(c,"cols",0);if(v>0)s->w=mk(v*8+12,0,0);
  v=numat(c,"rows",0);if(v>0)s->h=mk(v*16+8,0,0);}}

/* ---- generated boxes: ::before / ::after, form-control text ---- */
static N*findgen(N*c,int slot){N*g;for(g=c->kid;g;g=g->nx)if(g->gen==slot)return g;return 0;}
static N*mkgen(N*c,int slot){
 N*g=nn((S){slot==1?(char*)"::before":slot==2?(char*)"::after":(char*)"::value",slot==1?8:slot==2?7:7});
 g->gen=slot;g->par=c;g->s=al(sizeof(St));
 if(slot==1){g->nx=c->kid;c->kid=g;if(!c->last)c->last=g;}
 else{if(c->last)c->last->nx=g;else c->kid=g;c->last=g;}
 return g;}
static void gentext(N*g,S text){
 N*tn=g->kid;
 if(!tn){tn=nn((S){0,0});tn->par=g;g->kid=g->last=tn;}
 tn->txt=text;}
static void decls(N*c,St*s,int pe,int*mi,int nm,int pass){
 int i;
 for(i=0;i<nm;i++)decl(s,rls[mi[i]].d,pass);
 if(!pe&&c->sty.n)decl(s,c->sty,pass);}
static int collect(N*c,int pe,int*mi,int max){
 int i,nm=0;
 for(i=0;i<nr&&nm<max;i++)if(rmatch(c,&rls[i],pe))mi[nm++]=i;
 return nm;}
static N*findopt(N*n){
 N*c,*r,*first=0;
 for(c=n->kid;c;c=c->nx){
  if(!c->tag.n)continue;
  if(seq(c->tag,"option")){if(getattr(c,"selected").p)return c;if(!first)first=c;}
  else if(seq(c->tag,"optgroup")){r=findopt(c);if(r&&!first)first=r;}}
 return first;}
static void gens(N*c,St*s){
 int mi[256],nm,slot;N*g;St*gs;S txt;int col=-9;
 for(slot=1;slot<=2;slot++){
  nm=collect(c,slot,mi,256);
  g=findgen(c,slot);
  if(!nm){if(g){g->s->disp=D_NONE;}continue;}
  if(!g)g=mkgen(c,slot);
  gs=g->s;defst(gs,s);gs->disp=D_INLINE;cpa=s;
  decls(c,gs,1,mi,nm,2);decls(c,gs,1,mi,nm,3);
  gs->bg=gs->bg;
  if(!gs->hascont||!gs->cont.n)gs->disp=D_NONE;
  gentext(g,gs->hascont?gs->cont:(S){0,0});}
 /* text shown inside replaced / form elements */
 txt=(S){0,0};
 if(seq(c->tag,"input")){
  S ty=getattr(c,"type"),va=getattr(c,"value"),ph=getattr(c,"placeholder");
  int btn=seq(ty,"submit")||seq(ty,"button")||seq(ty,"reset");
  int tx=!ty.p||!ty.n||seq(ty,"text")||seq(ty,"search")||seq(ty,"email")||seq(ty,"url")||seq(ty,"tel")||seq(ty,"number")||seq(ty,"password");
  if(btn){txt=va.p&&va.n?va:seq(ty,"submit")?(S){(char*)"Submit",6}:seq(ty,"reset")?(S){(char*)"Reset",5}:(S){0,0};}
  else if(tx){
   if(va.p&&va.n){
    if(seq(ty,"password")){int k,o=cpp;for(k=0;k<va.n&&k<48&&cpp<(int)sizeof cpool-8;k++)cpp+=put8(cpool+cpp,0x2022);txt=(S){cpool+o,cpp-o};}
    else txt=va;}
   else if(ph.p&&ph.n){txt=ph;col=0x757575;}}}
 else if(seq(c->tag,"img")){S al_=getattr(c,"alt");if(al_.p&&al_.n)txt=al_;}
 else if(seq(c->tag,"select")){N*o=findopt(c);if(o){N*t;for(t=o->kid;t;t=t->nx)if(!t->tag.n){txt=trim(t->txt);break;}}}
 g=findgen(c,3);
 if(!txt.n){if(g)g->s->disp=D_NONE;return;}
 if(!g)g=mkgen(c,3);
 gs=g->s;defst(gs,s);gs->disp=D_INLINE;
 if(col!=-9)gs->col=col;
 gentext(g,txt);}

/* ---- cascade ---- */
static void styleone(N*c,St*s,St*pa){
 int mi[256],nm,i;
 cpa=pa;
 hints(c,s);
 nm=collect(c,0,mi,256);
 decls(c,s,0,mi,nm,1);
 decls(c,s,0,mi,nm,2);
 decls(c,s,0,mi,nm,3);
 for(i=0;i<4;i++)if(!s->bst[i])s->bw[i]=0;
 if((s->fl||s->pos>=2)&&s->disp==D_INLINE)s->disp=D_BLOCK;
 if(s->bs==0&&0)s->bs=0;
 if(c->par==root&&seq(c->tag,"html"))rootfs=s->fs;}
static void stl(N*n,St*pa){
 N*c;St*s;int base;
 for(c=n->kid;c;c=c->nx){
  if(!c->tag.n||c->gen)continue;
  if(!c->s)c->s=al(sizeof(St));
  s=c->s;base=nvar;
  defst(s,pa);
  styleone(c,s,pa);
  gens(c,s);
  stl(c,s);
  nvar=base;}}
static St rootst;
static void restyle(void){
 sortrules();
 cpp=0;nvar=0;rootfs=16;
 defst(&rootst,0);rootst.disp=D_BLOCK;
 root->s=&rootst;
 stl(root,&rootst);}

/* ---- paint target, blending, vector font rasterizer ---- */
static unsigned*pb;
static int pst,px0,py0,wx0,wy0,wx1,wy1,kx0,ky0,kx1,ky1,gop=255;
static void tgt(unsigned*b,int st,int x,int y,int w,int h){pb=b;pst=st;px0=x;py0=y;wx0=kx0=x;wy0=ky0=y;wx1=kx1=x+w;wy1=ky1=y+h;gop=255;}
static void bp(int x,int y,int c,int a){ /* blend color c at coverage a (0..255) */
 unsigned*d,o;int r,g,b;
 if(x<kx0||x>=kx1||y<ky0||y>=ky1||c<0)return;
 a=a*calpha(c)/255;a=a*gop/255;
 if(a<=0)return;
 d=&pb[(y-py0)*pst+(x-px0)];
 if(a>=255){*d=(unsigned)c&0xFFFFFF;return;}
 o=*d;
 r=((o>>16&255)*(255-a)+(c>>16&255)*a+127)/255;
 g=((o>>8&255)*(255-a)+(c>>8&255)*a+127)/255;
 b=((o&255)*(255-a)+(c&255)*a+127)/255;
 *d=(r<<16)|(g<<8)|b;}
static void fr(int x,int y,int w,int h,int c){
 int x1=x+w,y1=y+h,xx,a;
 if(c<0)return;
 a=calpha(c)*gop/255;
 if(a<=0)return;
 if(x<kx0)x=kx0;
 if(y<ky0)y=ky0;
 if(x1>kx1)x1=kx1;
 if(y1>ky1)y1=ky1;
 if(a>=255){for(;y<y1;y++)for(xx=x;xx<x1;xx++)pb[(y-py0)*pst+(xx-px0)]=(unsigned)c&0xFFFFFF;}
 else for(;y<y1;y++)for(xx=x;xx<x1;xx++)bp(xx,y,c,255);}

#define MS 800
typedef struct{short g,fs,ox,oy;u8 b,w,h,used;int off;}GC;
static GC gc[2048];
static u8 pool[1<<21];
static int pp;
static float acc[65536],sg[MS][6];
static u8 sgq[MS];
static int nsg;
static float mnx,mny,mxx,mxy;
static int fl_(float v){int i=(int)v;return v<(float)i?i-1:i;}
static int ce(float v){return-fl_(-v);}
static void ext(float x,float y){if(x<mnx)mnx=x;if(x>mxx)mxx=x;if(y<mny)mny=y;if(y>mxy)mxy=y;}
static void sadd(int q,float a,float b,float c,float d,float e,float f){
 if(nsg>=MS)return;
 sgq[nsg]=q;sg[nsg][0]=a;sg[nsg][1]=b;sg[nsg][2]=c;sg[nsg][3]=d;sg[nsg][4]=e;sg[nsg][5]=f;nsg++;
 ext(a,b);ext(c,d);if(q)ext(e,f);}
static int rv(const u8**p){int z=0,sh=0,c;do{c=*(*p)++;z|=(c&127)<<sh;sh+=7;}while(c&128);return(z&1)?-(z>>1)-1:(z>>1);}
static void dl(float x0,float y0,float x1,float y1,int w,int h){ /* signed-area coverage accumulation */
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
  if(xa<0)xa=0;
  if(xb<0)xb=0;
  if(xa>(float)w)xa=(float)w;
  if(xb>(float)w)xb=(float)w;
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
static GC*glyph(int g,int fs,int tf){
 int b=tf&1,it=(tf>>1)&1;
 GC*e=&gc[(g*131+fs*7+tf*3)&2047];
 const u8*p;float k=(float)fs/(float)MF_UPM,sk=it?0.2126f:0;
 int cx=0,cy=0,sx=0,sy=0,c,qx,qy,x,y,i,j,w,h,ox,oy;float s,v;u8*o;
 if(e->used&&e->g==g&&e->fs==fs&&e->b==tf)return e;
 nsg=0;mnx=mny=1e9f;mxx=mxy=-1e9f;
 p=(b?MF_D1:MF_D0)+MF_OFF[b][g];
 while((c=*p++)){
  if(c==1){cx+=rv(&p);cy+=rv(&p);sx=cx;sy=cy;}
  else if(c==2){x=cx+rv(&p);y=cy+rv(&p);sadd(0,cx*k+cy*k*sk,-cy*k,x*k+y*k*sk,-y*k,0,0);cx=x;cy=y;}
  else if(c==3){qx=cx+rv(&p);qy=cy+rv(&p);x=qx+rv(&p);y=qy+rv(&p);sadd(1,cx*k+cy*k*sk,-cy*k,qx*k+qy*k*sk,-qy*k,x*k+y*k*sk,-y*k);cx=x;cy=y;}
  else{if(cx!=sx||cy!=sy)sadd(0,cx*k+cy*k*sk,-cy*k,sx*k+sy*k*sk,-sy*k,0,0);cx=sx;cy=sy;}}
 w=h=0;ox=oy=0;
 if(nsg){ox=fl_(mnx);oy=fl_(mny);w=ce(mxx)-ox;h=ce(mxy)-oy;if(w<=0||h<=0||w>250||h>250)w=h=0;}
 if(pp+w*h>(int)sizeof pool){for(i=0;i<2048;i++)gc[i].used=0;pp=0;}
 e->used=1;e->g=g;e->fs=fs;e->b=tf;e->w=w;e->h=h;e->ox=ox;e->oy=oy;e->off=pp;
 if(!w)return e;
 for(i=0;i<(w+2)*h;i++)acc[i]=0;
 for(i=0;i<nsg;i++){
  if(sgq[i])dq(sg[i][0]-ox,sg[i][1]-oy,sg[i][2]-ox,sg[i][3]-oy,sg[i][4]-ox,sg[i][5]-oy,w,h);
  else dl(sg[i][0]-ox,sg[i][1]-oy,sg[i][2]-ox,sg[i][3]-oy,w,h);}
 o=pool+pp;pp+=w*h;
 for(j=0;j<h;j++){s=0;for(i=0;i<w;i++){s+=acc[j*(w+2)+i];v=s<0?-s:s;if(v>1)v=1;o[j*w+i]=(u8)(v*255+0.5f);}}
 return e;}
static void gl(int x,int base,int g,int fs,int tf,int col){
 GC*e=glyph(g,fs,tf);u8*bm;int i,j,a;
 if(!e->w)return;
 bm=pool+e->off;
 for(j=0;j<e->h;j++)for(i=0;i<e->w;i++)if((a=bm[j*e->w+i]))bp(x+e->ox+i,base+e->oy+j,col,a);}
static void run(int x,int base,const char*s,int n,int fs,int tf,int ls,int col,int ul,int w){
 int i=0,pre=0,g,k=0;
 while(i<n){
  g=gi(tfm(u8d(s,n,&i),tf>>2,k==0));
  gl(x+((pre*fs+512)>>10)+k*ls,base,g,fs,tf&3,col);
  pre+=MF_ADV[tf&1][g];k++;}
 if(ul&1)fr(x,base+fs/10+1,w,fs/14+1,col);
 if(ul&2)fr(x,base-fs*3/10,w,fs/14+1,col);}

/* ---- rounded boxes, gradients, shadows ---- */
typedef struct{int r[4],bw[4],bc[4],bg,gk,ga,gn,gc[6],gp[6],gpx[6],shx,shy,shb,shs,shc,x,y,w,h;}Bx;
static Bx bxs[8192];static int nbx;
static float sq(float v){float x;int i;if(v<=0)return 0;x=v>1?v*0.5f:1;for(i=0;i<8;i++)x=0.5f*(x+v/x);return x;}
static float sn(float a){
 float t,r,a2;
 while(a>3.14159265f)a-=6.2831853f;
 while(a<-3.14159265f)a+=6.2831853f;
 a2=a*a;t=a;r=t;
 t*=-a2/6;r+=t;t*=-a2/20;r+=t;t*=-a2/42;r+=t;t*=-a2/72;r+=t;t*=-a2/110;r+=t;
 return r;}
static float sdrr(float fx,float fy,float x,float y,float w,float h,const float*r){
 float cx=x+w*0.5f,cy=y+h*0.5f,hx=w*0.5f,hy=h*0.5f,rr,qx,qy,dx,dy,m;
 rr=r[fy<cy?(fx<cx?0:1):(fx<cx?3:2)];
 qx=(fx>cx?fx-cx:cx-fx)-(hx-rr);qy=(fy>cy?fy-cy:cy-fy)-(hy-rr);
 dx=qx>0?qx:0;dy=qy>0?qy:0;m=qx>qy?qx:qy;if(m>0)m=0;
 return sq(dx*dx+dy*dy)+m-rr;}
static int cov255(float sd){float c=0.5f-sd;return c<=0?0:c>=1?255:(int)(c*255);}
static int lerpc(int a,int b,float t){
 int ar=a>>16&255,ag=a>>8&255,ab=a&255,aa=calpha(a),br=b>>16&255,bg=b>>8&255,bb=b&255,ba=calpha(b);
 if(a<0){ar=br;ag=bg;ab=bb;aa=0;}
 if(b<0){br=ar;bg=ag;bb=ab;ba=0;}
 return mkc((int)(ar+(br-ar)*t),(int)(ag+(bg-ag)*t),(int)(ab+(bb-ab)*t),(int)(aa+(ba-aa)*t));}
static int gradcol(Bx*b,float fx,float fy){
 float ang=(float)b->ga*0.0174533f,dx=sn(ang),dy=-sn(ang+1.5707963f),len,t,p[6];
 float ax=(float)b->w*dx,ay=(float)b->h*dy;int i;
 len=(ax<0?-ax:ax)+(ay<0?-ay:ay);if(len<1)len=1;
 t=((fx-(b->x+b->w*0.5f))*dx+(fy-(b->y+b->h*0.5f))*dy)/len+0.5f;
 for(i=0;i<b->gn;i++){p[i]=b->gpx[i]?(float)b->gp[i]/len:(float)b->gp[i]/100;if(i&&p[i]<p[i-1])p[i]=p[i-1];}
 if(t<=p[0])return b->gc[0];
 for(i=1;i<b->gn;i++)if(t<=p[i])return lerpc(b->gc[i-1],b->gc[i],p[i]>p[i-1]?(t-p[i-1])/(p[i]-p[i-1]):1);
 return b->gc[b->gn-1];}
static void pbox(Bx*b){
 float r[4],ri[4],fx,fy,ro[4];int x,y,i,c,ob,ib,bgc,sd0;
 int x0=b->x,y0=b->y,w=b->w,h=b->h,hasb=b->bw[0]||b->bw[1]||b->bw[2]||b->bw[3];
 int bx0=x0,by0=y0,bx1=x0+w,by1=y0+h;
 float avg=(float)(b->bw[0]+b->bw[1]+b->bw[2]+b->bw[3])/4;
 {float mr_=(float)(w<h?w:h)*0.5f;for(i=0;i<4;i++){r[i]=(float)b->r[i];if(r[i]>mr_)r[i]=mr_;ri[i]=r[i]-avg;if(ri[i]<0)ri[i]=0;}}
 if(b->shc!=-1&&(b->shb||b->shx||b->shy||b->shs)){
  float sx=(float)(x0+b->shx-b->shs),sy=(float)(y0+b->shy-b->shs),sw=(float)(w+2*b->shs),sh=(float)(h+2*b->shs),bl=(float)(b->shb>0?b->shb:1);
  int ex=b->shs+b->shb+1;
  for(i=0;i<4;i++)ro[i]=r[i]+(float)b->shs;
  {int ya=(y0+b->shy<y0?y0+b->shy:y0)-ex,yb=(y0+b->shy>y0?y0+b->shy:y0)+h+ex;
   int xa=(x0+b->shx<x0?x0+b->shx:x0)-ex,xb=(x0+b->shx>x0?x0+b->shx:x0)+w+ex;
   if(ya<ky0)ya=ky0;
   if(yb>ky1)yb=ky1;
   if(xa<kx0)xa=kx0;
   if(xb>kx1)xb=kx1;
   for(y=ya;y<yb;y++)
   for(x=xa;x<xb;x++){
    if(x<kx0||x>=kx1||y<ky0||y>=ky1)continue;
    fx=(float)x+0.5f;fy=(float)y+0.5f;
    if(sdrr(fx,fy,(float)x0,(float)y0,(float)w,(float)h,r)<-0.5f)continue;
    {float s2=0.5f-sdrr(fx,fy,sx,sy,sw,sh,ro)/bl;sd0=s2<=0?0:s2>=1?255:(int)(s2*255);}
    if(sd0)bp(x,y,b->shc,sd0);}}}
 for(y=by0;y<by1;y++){
  if(y<ky0||y>=ky1)continue;
  for(x=bx0;x<bx1;x++){
   if(x<kx0||x>=kx1)continue;
   fx=(float)x+0.5f;fy=(float)y+0.5f;
   ob=cov255(sdrr(fx,fy,(float)x0,(float)y0,(float)w,(float)h,r));
   if(!ob)continue;
   bgc=b->gk?gradcol(b,fx,fy):b->bg;
   if(bgc>=0)bp(x,y,bgc,ob);
   if(hasb){
    int iw=w-b->bw[1]-b->bw[3],ih=h-b->bw[0]-b->bw[2];
    ib=iw>0&&ih>0?cov255(sdrr(fx,fy,(float)(x0+b->bw[3]),(float)(y0+b->bw[0]),(float)iw,(float)ih,ri)):0;
    if(ob>ib){
     float dt=fy-y0,db=y0+h-fy,dlf=fx-x0,dr=x0+w-fx,best=1e9f,q;int side=0;
     q=b->bw[0]?dt/b->bw[0]:1e9f;if(q<best){best=q;side=0;}
     q=b->bw[1]?dr/b->bw[1]:1e9f;if(q<best){best=q;side=1;}
     q=b->bw[2]?db/b->bw[2]:1e9f;if(q<best){best=q;side=2;}
     q=b->bw[3]?dlf/b->bw[3]:1e9f;if(q<best){best=q;side=3;}
     c=b->bc[side];
     if(c>=0)bp(x,y,c,ob-ib);}}}}}
static void ptri(int x,int y,int s,int dir,int c){ /* small solid triangle: dir 0 points right, 1 points down */
 int i,d,w;
 if(dir==0){for(i=0;i<s;i++){d=2*i-(s-1);if(d<0)d=-d;w=((s+1)-d)/2;if(w>0)fr(x,y+i,w,1,c);}}
 else{for(i=0;i<=s/2;i++){w=s-2*i;if(w>0)fr(x+i,y+i,w,1,c);}}}

/* ---- layout state and paint list ---- */
typedef struct{int k,x,y,w,h,a,c,bg,fs,tf,u,n,hn,vs,ls,op,cl,bi,grp,ex;char*t,*hp;N*nd;}Cmd;
static Cmd cm[MC],dm;
static int nc,W,H,cv=0xffffff;
static int Y,pend,lx,lsx,cx,cw,LH,LA,LD,lf,ca,sp,lend,padp;
static Cmd*bulp;static S*curh;
static int curop=255,curcl=0,isol,fw=-1,fh=-1,fva,fch=-1,ng,ibA[64];
typedef struct{int x,y,w,h,side;}Fl;static Fl fls[64];static int nf;
typedef struct{int x0,y0,x1,y1,par;}Clp;static Clp clps[512];static int nclp;
typedef struct{N*n,*cb;int sx,sy;}Ab;static Ab abq[64];static int nab;
static Cmd*cadd(int k,int x,int y,int w,int h,int c){
 Cmd*p=nc<MC?&cm[nc++]:&dm;
 p->k=k;p->x=x;p->y=y;p->w=w;p->h=h;p->a=0;p->c=c;p->bg=-1;p->fs=16;p->tf=p->u=p->n=p->hn=p->vs=p->ls=p->bi=p->grp=p->ex=0;
 p->op=curop;p->cl=curcl;p->t=p->hp=0;p->nd=0;return p;}
static int rc(int c,St*s){return c==-3?s->col:c;}
static void fbounds(int y,int h,int*l,int*r){
 int i;
 *l=cx;*r=cx+cw;
 for(i=0;i<nf;i++)if(y<fls[i].y+fls[i].h&&y+(h>0?h:1)>fls[i].y){
  if(fls[i].side==1){if(fls[i].x+fls[i].w>*l)*l=fls[i].x+fls[i].w;}
  else if(fls[i].x<*r)*r=fls[i].x;}}
static int fbottom(int mask,int from){
 int i,b=0;
 for(i=from;i<nf;i++)if((mask>>(fls[i].side-1))&1&&fls[i].y+fls[i].h>b)b=fls[i].y+fls[i].h;
 return b;}
static void nl(void){
 int i,dx,sl_;
 if(!LH){lx=cx;return;}
 sl_=lend-lx;dx=ca==1?sl_/2:ca==2?sl_:0;if(dx<0)dx=0;
 for(i=lf;i<nc;i++){
  if(cm[i].grp<0)continue;
  cm[i].x+=dx;
  if(cm[i].grp>0)cm[i].y+=LA-ibA[cm[i].grp-1];
  else cm[i].y=Y+LA-cm[i].a-cm[i].vs;}
 Y+=LH;LH=LA=LD=0;lx=cx;lf=nc;sp=0;ng=0;}
static void ls(int need){
 int l,r,i,nb,tries;
 if(LH)return;
 Y+=pend;pend=0;
 for(tries=0;tries<24;tries++){
  fbounds(Y,1,&l,&r);
  if(r-l>=need||!nf)break;
  nb=0;
  for(i=0;i<nf;i++)if(Y<fls[i].y+fls[i].h&&(!nb||fls[i].y+fls[i].h<nb))nb=fls[i].y+fls[i].h;
  if(nb<=Y)break;
  Y=nb;}
 lx=lsx=l+padp;padp=0;lend=r;
 if(bulp){bulp->y=bulp->k==1?Y:Y+bulp->a-bulp->h-bulp->fs/10;bulp=0;}}
static void mt0(St*s,int*as,int*de){
 int fs=s->fs,L,hf;
 *as=(fs*MF_ASC+512)>>10;*de=(fs*MF_DESC+512)>>10;
 if(s->lh>0){L=fs*s->lh/100;hf=(L-*as-*de)/2;*as+=hf;*de=L-*as;if(*de<0)*de=0;}}
static void met(St*s,int*a,int*h){
 int as,de;
 mt0(s,&as,&de);*a=as;*h=as+de;
 if(as>LA)LA=as;
 if(de>LD)LD=de;
 LH=LA+LD;}
static int ttf(St*s){return s->bold|(s->it<<1)|(s->tt<<2);}
static void word(char*p,int n,N*e){
 St*s=e->s;int fs=s->fs,tf=ttf(s),ww=wid(p,n,fs,tf,s->ls),a,h,spw;Cmd*c;
 spw=(sp&&LH)?((MF_ADV[tf&1][0]*fs+512)>>10)+s->ls:0;
 if(LH&&s->ws!=1&&lx+spw+ww>lend){nl();spw=0;}
 ls(ww);lx+=spw;
 met(s,&a,&h);
 c=cadd(1,lx,0,ww,h,s->vis?-1:rc(s->col,s));
 c->a=a;c->bg=s->disp==D_INLINE&&!s->vis?rc(s->bg,s):-1;c->fs=fs;c->tf=tf;c->u=s->ul;c->t=p;c->n=n;c->vs=s->va;c->ls=s->ls;
 if(curh){c->hp=curh->p;c->hn=curh->n;}
 lx+=ww;sp=0;}
static void txtc(N*n,char*p,int len){ /* collapsed-whitespace text */
 St*s=n->par->s;int i=0,j,k,m,q,a,fs=s->fs,tf=ttf(s);long long acc;
 while(i<len){
  if(isws(p[i])){sp=1;i++;continue;}
  j=i;while(j<len&&!isws(p[j]))j++;
  while(i<j){
   acc=0;k=m=i;
   while(k<j){q=k;a=MF_ADV[tf&1][gi(tfm(u8d(p,j,&q),tf>>2,k==i))];if(m>i&&cw>0&&s->ws!=1&&(((acc+a)*fs+512)>>10)>cw)break;acc+=a;k=q;m=q;}
   word(p+i,m-i,n->par);i=m;}}}
static void brk(St*s){int a,h;ls(0);met(s,&a,&h);nl();}
static void txt(N*n){
 S t=n->txt;St*s=n->par->s;int i=0,j;
 if(s->ws>=2){ /* pre, pre-wrap, pre-line: newlines are line breaks */
  while(i<=t.n){
   j=i;while(j<t.n&&t.p[j]!='\n')j++;
   if(j>i){
    if(s->ws==4)txtc(n,t.p+i,j-i);
    else{sp=0;word(t.p+i,j-i,n->par);}}
   if(j<t.n)brk(s);
   i=j+1;}}
 else txtc(n,t.p,t.n);}

/* ---- measuring: min-content / max-content widths (border-box, no margins) ---- */
static void blk(N*n,int ax,int aw);
static void walk(N*n);
static void flexc(N*n);
static void gridc(N*n);
static void tablec(N*n);
static int lay_at(N*c,int ax,int aw,int y,int fwv,int fhv,int fvv,int roll);
static void mmbox(N*c,int*mn,int*mx);
static void mm(N*n,int*mn,int*mx);
static int ex_(St*s,int aw){
 int a=rl(s->p[1],aw),b=rl(s->p[3],aw);
 if(a<0)a=0;
 if(b<0)b=0;
 return a+b+s->bw[1]+s->bw[3];}
static int mlr(St*s,int aw){return rl(s->m[3],aw)+rl(s->m[1],aw);}
typedef struct{N*n;int r,c,cs,rs,mn,mx,w,h;}Tc;
typedef struct{N*rows[128];int nrows;Tc cl[256];int ncl;int ncols;int cmn[16],cmx[16],cwd[16],rh[128];}Tb;
static void tbuild(N*n,Tb*t){
 N*c,*g;int r,col,cs,rs,i,j,v;unsigned char occ[128][16];
 t->nrows=t->ncl=t->ncols=0;
 for(c=n->kid;c;c=c->nx){
  if(!c->tag.n||!c->s)continue;
  if(c->s->disp==D_ROW&&t->nrows<128)t->rows[t->nrows++]=c;
  else if(c->s->disp==D_RG)for(g=c->kid;g;g=g->nx)if(g->tag.n&&g->s&&g->s->disp==D_ROW&&t->nrows<128)t->rows[t->nrows++]=g;}
 for(i=0;i<t->nrows;i++)for(j=0;j<16;j++)occ[i][j]=0;
 for(r=0;r<t->nrows;r++){
  col=0;
  for(c=t->rows[r]->kid;c;c=c->nx){
   if(!c->tag.n||!c->s||c->s->disp!=D_CELL||t->ncl>=256)continue;
   while(col<16&&occ[r][col])col++;
   if(col>=16)break;
   cs=numat(c,"colspan",0);if(cs<1)cs=1;
   rs=numat(c,"rowspan",0);if(rs<1)rs=1;
   if(col+cs>16)cs=16-col;
   if(r+rs>t->nrows)rs=t->nrows-r;
   for(i=0;i<rs;i++)for(j=0;j<cs;j++)occ[r+i][col+j]=1;
   t->cl[t->ncl].n=c;t->cl[t->ncl].r=r;t->cl[t->ncl].c=col;t->cl[t->ncl].cs=cs;t->cl[t->ncl].rs=rs;
   t->ncl++;
   if(col+cs>t->ncols)t->ncols=col+cs;
   col+=cs;}}
 (void)v;}
static void tmeas(Tb*t){
 int i,j,sm,sx;Tc*c;
 for(i=0;i<16;i++)t->cmn[i]=t->cmx[i]=0;
 for(i=0;i<t->ncl;i++){c=&t->cl[i];mmbox(c->n,&c->mn,&c->mx);
  if(c->cs==1){if(c->mn>t->cmn[c->c])t->cmn[c->c]=c->mn;if(c->mx>t->cmx[c->c])t->cmx[c->c]=c->mx;}}
 for(i=0;i<t->ncl;i++){c=&t->cl[i];
  if(c->cs>1){
   sm=sx=0;for(j=0;j<c->cs;j++){sm+=t->cmn[c->c+j];sx+=t->cmx[c->c+j];}
   if(sm<c->mn)t->cmn[c->c+c->cs-1]+=c->mn-sm;
   if(sx<c->mx)t->cmx[c->c+c->cs-1]+=c->mx-sx;}}}
static void tdist(Tb*t,int avail,int fixed){
 int i,sm=0,sx=0,x,ex;
 for(i=0;i<t->ncols;i++){sm+=t->cmn[i];sx+=t->cmx[i];}
 for(i=0;i<t->ncols;i++){
  if(sx<=avail){ex=fixed&&sx>0?(int)((long long)(avail-sx)*t->cmx[i]/sx):0;t->cwd[i]=t->cmx[i]+ex;}
  else if(sm>=avail||sx<=sm)t->cwd[i]=t->cmn[i];
  else t->cwd[i]=t->cmn[i]+(int)((long long)(t->cmx[i]-t->cmn[i])*(avail-sm)/(sx-sm));}
 if(fixed&&sx<=avail&&t->ncols){x=0;for(i=0;i<t->ncols;i++)x+=t->cwd[i];t->cwd[t->ncols-1]+=avail-x;}}
static void mmtable(N*n,int*mn,int*mx){
 Tb t;int i,sp_=n->s->bcol?0:n->s->bsp,a=0,b=0;
 tbuild(n,&t);tmeas(&t);
 for(i=0;i<t.ncols;i++){a+=t.cmn[i];b+=t.cmx[i];}
 *mn=a+sp_*(t.ncols+1);*mx=b+sp_*(t.ncols+1);}
static void mmflex(N*n,int*mn,int*mx){
 St*s=n->s;N*c;int a,b,row=(s->fdir&1)==0,sa=0,sb=0,ma=0,mb=0,cnt=0,gm=rl(s->gx,0);
 for(c=n->kid;c;c=c->nx){
  if(!c->tag.n||!c->s||c->s->disp==D_NONE||c->s->pos>=2)continue;
  mmbox(c,&a,&b);a+=mlr(c->s,0);b+=mlr(c->s,0);
  sa+=a;sb+=b;if(a>ma)ma=a;if(b>mb)mb=b;cnt++;}
 if(row){*mx=sb+gm*(cnt>1?cnt-1:0);*mn=s->fwrap?ma:sa+gm*(cnt>1?cnt-1:0);}
 else{*mx=mb;*mn=ma;}}
static void mmgrid(N*n,int*mn,int*mx){
 St*s=n->s;N*c;int a,b,ma=0,mb=0,ncol=s->gtn>0?s->gtn:1,gx=rl(s->gx,0),fixed=0,i;
 for(c=n->kid;c;c=c->nx){
  if(!c->tag.n||!c->s||c->s->disp==D_NONE||c->s->pos>=2)continue;
  mmbox(c,&a,&b);if(a>ma)ma=a;if(b>mb)mb=b;}
 if(s->gtauto){*mn=s->gtmin;*mx=mb;return;}
 for(i=0;i<s->gtn;i++)if(s->gtk[i]==0)fixed+=rl(s->gt[i],0);
 *mn=ma*ncol+gx*(ncol-1);*mx=(s->gtn&&fixed)?fixed+gx*(ncol-1):mb*ncol+gx*(ncol-1);}
static void mm(N*n,int*mn,int*mx){
 int lmn=0,lmx=0,bmn=0,bmx=0,spp=0,i,j,w,a,b,ex,spw,tf,fs;N*c;St*s,*ps=n->s;S t;
 if(!ps){*mn=*mx=0;return;}
 fs=ps->fs;tf=ttf(ps);spw=((MF_ADV[tf&1][0]*fs+512)>>10)+ps->ls;
 for(c=n->kid;c;c=c->nx){
  if(!c->tag.n){
   t=c->txt;i=0;
   while(i<t.n){
    if(isws(t.p[i])){
     if(ps->ws>=2&&ps->ws!=4&&t.p[i]=='\n'){if(lmx>bmx)bmx=lmx;if(lmn>bmn)bmn=lmn;lmx=lmn=0;}
     else spp=1;
     i++;continue;}
    j=i;while(j<t.n&&!isws(t.p[j]))j++;
    w=wid(t.p+i,j-i,fs,tf,ps->ls);
    lmx+=(spp&&lmx?spw:0)+w;if(w>lmn)lmn=w;spp=0;i=j;}
   continue;}
  s=c->s;
  if(!s||s->disp==D_NONE||s->pos>=2)continue;
  if(seq(c->tag,"br")){if(lmx>bmx)bmx=lmx;if(lmn>bmn)bmn=lmn;lmx=lmn=0;spp=0;continue;}
  if(s->disp==D_INLINE&&!s->fl){
   mm(c,&a,&b);ex=mlr(s,0)+rl(s->p[1],0)+rl(s->p[3],0)+s->bw[1]+s->bw[3];
   lmx+=b+ex;if(a+ex>lmn)lmn=a+ex;continue;}
  mmbox(c,&a,&b);ex=mlr(s,0);
  if(s->disp==D_IB||s->fl){lmx+=b+ex;if(a+ex>lmn)lmn=a+ex;}
  else{
   if(lmx>bmx)bmx=lmx;
   if(lmn>bmn)bmn=lmn;
   lmx=lmn=0;
   if(b+ex>bmx)bmx=b+ex;
   if(a+ex>bmn)bmn=a+ex;}}
 *mn=bmn>lmn?bmn:lmn;*mx=bmx>lmx?bmx:lmx;
 if(ps->ws==1||(ps->ws>=2&&ps->ws!=4))*mn=*mx;}
static void mmbox(N*c,int*mn,int*mx){
 St*s=c->s;int a,b,eh=ex_(s,0),w;
 if(s->disp==D_TABLE)mmtable(c,&a,&b);
 else if(s->fx)mmflex(c,&a,&b);
 else if(s->gr)mmgrid(c,&a,&b);
 else mm(c,&a,&b);
 if(s->w.k==0&&!s->w.pc){w=s->w.px-(s->bs?eh:0);if(w<0)w=0;a=b=w;}
 *mn=a+eh;*mx=b+eh;}

/* ---- laying a box out at a given place (used by flex, grid, table, inline-block, float) ---- */
static int lay_at(N*c,int ax,int aw,int y,int fwv,int fhv,int fvv,int roll){
 int sY=Y,sP=pend,scx=cx,scw=cw,sca=ca,slx=lx,sLH=LH,sLA=LA,sLD=LD,slf=lf,ssp=sp,snc=nc,snb=nbx,snl=nclp,snf=nf,sab=nab,sng=ng,slend=lend,slsx=lsx,sop=curop,scl=curcl,spad=padp,h;
 Cmd*sb=bulp;S*sh=curh;S hr;
 Y=y;pend=0;bulp=0;padp=0;LH=LA=LD=0;
 if(seq(c->tag,"a")&&c->href.n)curh=&c->href;
 fw=fwv;fh=fhv;fva=fvv;isol=1;
 blk(c,ax,aw);
 h=Y+pend-y;
 (void)hr;
 Y=sY;pend=sP;cx=scx;cw=scw;ca=sca;lx=slx;LH=sLH;LA=sLA;LD=sLD;lf=slf;sp=ssp;bulp=sb;curh=sh;nf=snf;ng=sng;lend=slend;lsx=slsx;curop=sop;curcl=scl;padp=spad;
 if(roll){nc=snc;nbx=snb;nclp=snl;nab=sab;}
 return h;}
static void ibox(N*c){
 St*s=c->s;int mn,mx,eh,wb,hh,ml,mr,mb,pb,bb,spw,outer,c0,a,d,i,g,tf;
 mmbox(c,&mn,&mx);
 eh=ex_(s,cw);ml=rl(s->m[3],cw);mr=rl(s->m[1],cw);mb=rl(s->m[2],cw);pb=rl(s->p[2],cw);bb=s->bw[2];
 if(s->w.k==0)wb=rl(s->w,cw)+(s->bs?0:eh);
 else{wb=cw-ml-mr;if(wb>mx)wb=mx;if(wb<mn)wb=mn;}
 if(s->mxw.k==0&&!s->mxw.pc){i=s->mxw.px+(s->bs?0:eh);if(wb>i)wb=i;}
 if(s->mnw.k==0&&!s->mnw.pc){i=s->mnw.px+(s->bs?0:eh);if(wb<i)wb=i;}
 if(wb<eh)wb=eh;
 outer=wb+ml+mr;
 hh=lay_at(c,0,outer,0,wb,-1,0,1);
 tf=ttf(s);
 spw=(sp&&LH)?((MF_ADV[tf&1][0]*s->fs+512)>>10):0;
 if(LH&&s->ws!=1&&lx+spw+outer>lend){nl();spw=0;}
 ls(outer);lx+=spw;
 c0=nc;
 lay_at(c,lx,outer,Y,wb,-1,0,0);
 a=hh-mb-pb-bb-((s->fs*MF_DESC)>>10);
 if(a<0)a=0;
 if(a>hh)a=hh;
 if(ng<63){ng++;ibA[ng-1]=a;g=ng;}else g=0;
 for(i=c0;i<nc;i++)if(cm[i].grp>=0)cm[i].grp=g;
 d=hh-a;if(a>LA)LA=a;if(d>LD)LD=d;LH=LA+LD;
 lx+=outer;sp=0;}
static void place_float(N*c){
 St*s=c->s;int mn,mx,ml,mr,eh,wb,hh,x,y,l,r,outer,i,tries,nb,c0;
 mmbox(c,&mn,&mx);
 eh=ex_(s,cw);ml=rl(s->m[3],cw);mr=rl(s->m[1],cw);
 if(s->w.k==0)wb=rl(s->w,cw)+(s->bs?0:eh);
 else{wb=cw-ml-mr;if(wb>mx)wb=mx;if(wb<mn)wb=mn;}
 if(wb<eh)wb=eh;
 outer=wb+ml+mr;
 hh=lay_at(c,0,outer,0,wb,-1,0,1);
 if(!LH){Y+=pend;pend=0;}
 y=Y;
 if(s->clr){i=fbottom(s->clr,0);if(i>y)y=i;}
 for(tries=0;tries<30;tries++){
  fbounds(y,hh,&l,&r);
  if(r-l>=outer||!nf)break;
  nb=0;
  for(i=0;i<nf;i++)if(y<fls[i].y+fls[i].h&&(!nb||fls[i].y+fls[i].h<nb))nb=fls[i].y+fls[i].h;
  if(nb<=y)break;
  y=nb;}
 x=s->fl==1?l:r-outer;
 if(x<cx&&s->fl==1)x=cx;
 c0=nc;
 lay_at(c,x,outer,y,wb,-1,0,0);
 for(i=c0;i<nc;i++)cm[i].grp=-1;
 if(nf<64){fls[nf].x=x;fls[nf].y=y;fls[nf].w=outer;fls[nf].h=hh;fls[nf].side=s->fl;nf++;}}
static void walk(N*n){
 N*c;St*s;int a,h,op0,open,closed=0,lpx,rpx;S*old;
 if(seq(n->tag,"details")){open=(getattr(n,"open").p!=0)^(n->det&1);closed=!open;}
 for(c=n->kid;c;c=c->nx){
  if(!c->tag.n){if(!closed)txt(c);continue;}
  s=c->s;
  if(!s||s->disp==D_NONE)continue;
  if(closed&&!seq(c->tag,"summary"))continue;
  if(s->pos>=2){
   if(nab<64){N*p=c->par,*cb=0;
    while(p&&p!=root){if(p->s&&p->s->pos>=1&&p->s->disp!=D_INLINE){cb=p;break;}p=p->par;}
    if(s->pos==3)cb=0;
    abq[nab].n=c;abq[nab].cb=cb;abq[nab].sx=cx;abq[nab].sy=Y+(LH?0:pend);nab++;}
   continue;}
  old=curh;if(c->href.n&&seq(c->tag,"a"))curh=&c->href;
  op0=curop;if(s->op<255)curop=curop*s->op/255;
  if(seq(c->tag,"br")){brk(s);}
  else if(s->fl)place_float(c);
  else if(s->disp==D_IB)ibox(c);
  else if(s->disp==D_INLINE){
   lpx=rl(s->m[3],cw)+rl(s->p[3],cw)+s->bw[3];rpx=rl(s->m[1],cw)+rl(s->p[1],cw)+s->bw[1];
   if(lpx>0){if(LH)lx+=lpx;else padp+=lpx;}
   walk(c);
   if(rpx>0&&LH)lx+=rpx;}
  else blk(c,cx,cw);
  curh=old;curop=op0;}
 (void)a;(void)h;}

/* ---- list markers ---- */
static char*mkroman(int n,int up,int*len){
 static const int V[]={1000,900,500,400,100,90,50,40,10,9,5,4,1};
 static const char*R[]={"m","cm","d","cd","c","xc","l","xl","x","ix","v","iv","i"};
 char*o=sal(24);int i,k=0,j;
 if(n<1||n>3999){o[0]='?';*len=1;return o;}
 for(i=0;i<13;i++)while(n>=V[i]&&k<18){for(j=0;R[i][j];j++)o[k++]=up?(char)(R[i][j]-32):R[i][j];n-=V[i];}
 *len=k;return o;}
static char*mktxt(N*li,int lst,int*len){
 int idx=1,i,k=0;N*p=li->par,*c;S v;char*o,tmp[16];
 if(p&&p!=root){v=getattr(p,"start");if(v.p&&v.n&&isdg(v.p[0])){idx=0;for(i=0;i<v.n&&isdg(v.p[i]);i++)if(idx<1000000)idx=idx*10+v.p[i]-'0';}}
 for(c=li->pv;c;c=c->pv)if(c->s&&c->s->disp==D_LI)idx++;
 v=getattr(li,"value");
 if(v.p&&v.n&&isdg(v.p[0])){idx=0;for(i=0;i<v.n&&isdg(v.p[i]);i++)if(idx<1000000)idx=idx*10+v.p[i]-'0';}
 if(lst==10||lst==11){o=mkroman(idx,lst==11,len);o[*len]='.';*len+=1;return o;}
 o=sal(24);
 if(lst==6||lst==7){
  int n=idx>0?idx:1,t=0;char b[8];
  while(n>0&&t<7){n--;b[t++]=(char)((lst==6?'a':'A')+n%26);n/=26;}
  while(t>0)o[k++]=b[--t];
  o[k++]='.';*len=k;return o;}
 i=0;{int n=idx<0?0:idx;do{tmp[i++]=(char)('0'+n%10);n/=10;}while(n&&i<12);}
 if(lst==5&&i<2)tmp[i++]='0';
 while(i>0)o[k++]=tmp[--i];
 o[k++]='.';*len=k;return o;}

/* ---- block layout ---- */
static void bside(int st,int x,int y,int w,int h,int c,int horiz){
 int th=horiz?h:w,len=horiz?w:h,seg,p,gap;
 if(c<0||w<=0||h<=0)return;
 if(st==1||st==0){cadd(0,x,y,w,h,c);return;}
 seg=st==2?th*3:th;gap=st==2?th*2:th;if(seg<1)seg=1;if(gap<1)gap=1;
 for(p=0;p<len;p+=seg+gap){int l=len-p<seg?len-p:seg;if(horiz)cadd(0,x+p,y,l,h,c);else cadd(0,x,y+p,w,l,c);}}
static void blk(N*n,int ax,int aw){
 St*s=n->s;int fwv=fw,fhv=fh,fvv=fva,iso=isol,d=s->disp;
 int ml,mr,mt,mb,pl,pr,pt,pb,bl,br,bt,bb,eh,ev,w,box,cplx,bfc,tt,bt_,top,x0,tw,ys,i,nf0=nf,c0=nc,c1,hcnt=-1,autoh,fin,bh;
 int ocx=cx,ocw=cw,oca=ca,oop=curop,ocl=curcl,as,de,mlA=s->m[3].k==1,mrA=s->m[1].k==1,rad=0,shd=0,cli=-1,lim,fr_;
 Cmd*bp=0;Bx*bx=0;
 fw=fh=-1;fva=0;isol=0;
 mt=rl(s->m[0],aw);mb=rl(s->m[2],aw);ml=rl(s->m[3],aw);mr=rl(s->m[1],aw);
 pl=rl(s->p[3],aw);pr=rl(s->p[1],aw);pt=rl(s->p[0],aw);pb=rl(s->p[2],aw);
 pl=pl<0?0:pl;pr=pr<0?0:pr;pt=pt<0?0:pt;pb=pb<0?0:pb;
 bl=s->bw[3];br=s->bw[1];bt=s->bw[0];bb=s->bw[2];
 eh=pl+pr+bl+br;ev=pt+pb+bt+bb;
 bfc=s->ov||s->fl||d==D_IB||d==D_CELL||d==D_TABLE||iso||s->fx||s->gr||s->pos>=2;
 nl();
 if(s->clr&&!iso){i=fbottom(s->clr,0);if(i>Y+pend){Y=i;pend=0;}}
 if(bfc&&nf&&!s->fl&&fwv<0&&s->pos<2&&!iso){int l,r;fbounds(Y,1,&l,&r);if(l>ax){aw-=l-ax;ax=l;}if(r<ax+aw)aw=r-ax;ml=rl(s->m[3],aw);mr=rl(s->m[1],aw);}
 if(fwv>=0){w=fwv-eh;mlA=mrA=0;}
 else if(s->w.k==0){w=rl(s->w,aw);if(s->bs)w-=eh;}
 else{
  w=aw-ml-mr-eh;
  if(d==D_TABLE){int a,b;mmtable(n,&a,&b);if(b<w)w=b;}}
 if(fwv<0){
  if(s->mxw.k==0&&!s->mxw.pc){lim=s->mxw.px-(s->bs?eh:0);if(w>lim)w=lim;}
  if(s->mnw.k==0&&!s->mnw.pc){lim=s->mnw.px-(s->bs?eh:0);if(w<lim)w=lim;}}
 if(w<0)w=0;
 fr_=aw-w-eh-ml-mr;
 if(mlA&&mrA){ml=mr=fr_>0?fr_/2:0;}else if(mlA)ml=fr_>0?fr_:0;
 for(i=0;i<4;i++)if(s->rad[i].k==0&&(s->rad[i].px>0||s->rad[i].pc>0))rad=1;
 if(s->shc!=-1&&(s->shx||s->shy||s->shb||s->shs))shd=1;
 box=s->bg>=0||s->gk||bt||bb||bl||br||rad||shd;
 cplx=(rad||s->gk||shd)&&nbx<8192;
 tt=!box&&!pt&&!bfc&&s->h.k==1&&fhv<0;
 bt_=!box&&!pb&&!bfc&&s->h.k==1&&fhv<0;
 if(tt){if(mt>pend)pend=mt;}else{Y+=mt>pend?mt:pend;pend=0;}
 top=Y;x0=ax+ml;tw=w+eh;
 if(s->op<255)curop=curop*s->op/255;
 if(cplx){
  bx=&bxs[nbx];
  for(i=0;i<(int)sizeof(Bx);i++)((char*)bx)[i]=0;
  for(i=0;i<4;i++){bx->r[i]=rl(s->rad[i],tw);if(bx->r[i]<0)bx->r[i]=0;bx->bw[i]=s->bw[i];bx->bc[i]=rc(s->bc[i],s);}
  bx->bg=rc(s->bg,s);bx->gk=s->gk;bx->ga=s->ga;bx->gn=s->gn;
  for(i=0;i<s->gn;i++){bx->gc[i]=rc(s->gcol[i],s);bx->gp[i]=s->gpos[i];bx->gpx[i]=s->gpx[i];}
  bx->shx=s->shx;bx->shy=s->shy;bx->shb=s->shb;bx->shs=s->shs;bx->shc=shd?rc(s->shc,s):-1;
  bp=cadd(2,x0,top,tw,0,0);bp->bi=nbx++;bp->ex=shd?(s->shb+s->shs+(s->shx<0?-s->shx:s->shx)+(s->shy<0?-s->shy:s->shy)+2):0;}
 else if(s->bg>=0)bp=cadd(0,x0,top,tw,0,rc(s->bg,s));
 if(!tt)Y+=bt+pt;
 cx=ax+ml+bl+pl;cw=w;ca=s->al;lx=cx;LH=LA=LD=0;sp=0;lf=nc;ys=Y;ng=0;padp=0;
 if(fhv>=0)hcnt=fhv-ev;
 else if(s->h.k==0&&!s->h.pc)hcnt=s->h.px-(s->bs?ev:0);
 if(hcnt<-1)hcnt=0;
 if(s->ov==1&&nclp<512){cli=nclp++;clps[cli].par=curcl;clps[cli].x0=x0+bl;clps[cli].y0=top+bt;clps[cli].x1=x0+tw-br;clps[cli].y1=top+bt+(hcnt>=0?hcnt+pt+pb:100000);curcl=cli+1;}
 fch=hcnt;
 if(d==D_LI&&s->lst>0){
  int lst=s->lst,sz=s->fs*3/8,len_;char*tx;
  if(sz<4)sz=4;
  mt0(s,&as,&de);
  if(lst>=4){tx=mktxt(n,lst,&len_);bulp=cadd(1,0,0,wid(tx,len_,s->fs,0,0),as+de,rc(s->col,s));bulp->t=tx;bulp->n=len_;}
  else if(lst==1){bulp=cadd(1,0,0,wid("\xE2\x80\xA2",3,s->fs,0,0),as+de,rc(s->col,s));bulp->t=(char*)"\xE2\x80\xA2";bulp->n=3;}
  else if(lst==3){bulp=cadd(0,0,0,sz,sz,rc(s->col,s));bulp->a=as-s->fs/4+sz/2;}
  else{
   bulp=cadd(2,0,0,sz,sz,0);
   if(nbx<8192){Bx*q=&bxs[nbx];for(i=0;i<(int)sizeof(Bx);i++)((char*)q)[i]=0;for(i=0;i<4;i++){q->r[i]=sz;q->bw[i]=1;q->bc[i]=rc(s->col,s);}q->bg=-1;q->shc=-1;bulp->bi=nbx++;}
   bulp->a=as-s->fs/4+sz/2;}
  bulp->fs=s->fs;if(bulp->k==1)bulp->a=as;
  bulp->x=cx-bulp->w-s->fs/3;bulp->grp=-1;}
 if(seq(n->tag,"summary")&&n->par&&seq(n->par->tag,"details")){
  int sz=s->fs*2/5;if(sz<5)sz=5;
  mt0(s,&as,&de);
  bulp=cadd(4,cx-sz-s->fs/3-2,0,sz,sz,rc(s->col,s));
  bulp->u=((getattr(n->par,"open").p!=0)^(n->par->det&1));bulp->a=as;bulp->fs=s->fs;bulp->grp=-1;}
 c1=nc;
 if(d==D_TABLE)tablec(n);
 else if(s->fx)flexc(n);
 else if(s->gr)gridc(n);
 else walk(n);
 nl();
 if(bfc){i=fbottom(3,nf0);if(i>Y)Y=i;}
 autoh=Y-ys;fin=hcnt>=0?hcnt:autoh;
 if(s->mnh.k==0&&!s->mnh.pc){lim=s->mnh.px-(s->bs?ev:0);if(fin<lim)fin=lim;}
 if(s->mxh.k==0&&!s->mxh.pc){lim=s->mxh.px-(s->bs?ev:0);if(fin>lim)fin=lim;}
 if(fin<0)fin=0;
 if(fin!=autoh){
  if(fhv>=0&&fvv&&fin>autoh){int dy=fvv==1?(fin-autoh)/2:fin-autoh;for(i=c1;i<nc;i++)cm[i].y+=dy;}
  Y=ys+fin;pend=0;}
 if(bt_){if(mb>pend)pend=mb;}else{Y+=pend+pb+bb;pend=mb;}
 bh=Y-top;
 if(cli>=0){clps[cli].y1=top+bh-bb;}
 curcl=ocl;
 if(bp)bp->h=bh;
 if(!cplx&&(bt||bb||bl||br)){
  bside(s->bst[0],x0,top,tw,bt,rc(s->bc[0],s),1);
  bside(s->bst[2],x0,Y-bb,tw,bb,rc(s->bc[2],s),1);
  bside(s->bst[3],x0,top+bt,bl,bh-bt-bb,rc(s->bc[3],s),0);
  bside(s->bst[1],x0+tw-br,top+bt,br,bh-bt-bb,rc(s->bc[1],s),0);}
 if(seq(n->tag,"select")){Cmd*t2=cadd(4,x0+tw-bl-14,top+(bh-5)/2,9,5,0x555555);t2->u=1;}
 if(seq(n->tag,"summary")&&n->par&&seq(n->par->tag,"details")){Cmd*h2=cadd(3,x0,top,tw,bh,0);h2->nd=n->par;}
 if(s->pos==1){
  int dx=0,dy=0;
  if(s->lf.k==0)dx=rl(s->lf,aw);else if(s->rt.k==0)dx=-rl(s->rt,aw);
  if(s->top.k==0)dy=rl(s->top,0);else if(s->bot.k==0)dy=-rl(s->bot,0);
  if(dx||dy)for(i=c0;i<nc;i++){cm[i].x+=dx;cm[i].y+=dy;}
  n->x0=x0+bl+dx;n->y0=top+bt+dy;n->x1=x0+tw-br+dx;n->y1=Y-bb+dy;}
 else if(s->pos>=2){n->x0=x0+bl;n->y0=top+bt;n->x1=x0+tw-br;n->y1=Y-bb;}
 cx=ocx;cw=ocw;ca=oca;curop=oop;
 lx=cx;LH=LA=LD=0;sp=0;lf=nc;ng=0;}

/* ---- flexbox ---- */
typedef struct{N*n;St*s;int ord,bas,mnf,w,h,mlA,mrA,ml,mr,mt,mb,xb,eh;}Fi;
static void flexc(N*n){
 St*s=n->s;Fi it[64];int ni=0,row=(s->fdir&1)==0,rev=s->fdir>=2,gm=rl(row?s->gx:s->gy,cw),gc_=rl(row?s->gy:s->gx,cw),i,j,a,b,lim,nln=0,ls_[66],li;
 N*c;Fi t;
 for(c=n->kid;c&&ni<64;c=c->nx){
  if(!c->tag.n||!c->s||c->s->disp==D_NONE||c->s->pos>=2)continue;
  it[ni].n=c;it[ni].s=c->s;it[ni].ord=c->s->ord;ni++;}
 if(!ni){walk(n);return;}
 for(i=1;i<ni;i++){t=it[i];j=i-1;while(j>=0&&it[j].ord>t.ord){it[j+1]=it[j];j--;}it[j+1]=t;}
 for(i=0;i<ni;i++){
  St*is=it[i].s;
  it[i].mlA=is->m[3].k==1;it[i].mrA=is->m[1].k==1;
  it[i].ml=rl(is->m[3],cw);it[i].mr=rl(is->m[1],cw);it[i].mt=rl(is->m[0],cw);it[i].mb=rl(is->m[2],cw);
  it[i].eh=ex_(is,cw);}
 if(row){
  for(i=0;i<ni;i++){
   St*is=it[i].s;int bas;
   mmbox(it[i].n,&a,&b);
   if(is->fb.k==0)bas=rl(is->fb,cw)+(is->bs?0:it[i].eh);
   else if(is->w.k==0)bas=rl(is->w,cw)+(is->bs?0:it[i].eh);
   else bas=b;
   if(is->mxw.k==0&&!is->mxw.pc){lim=is->mxw.px+(is->bs?0:it[i].eh);if(bas>lim)bas=lim;}
   it[i].mnf=a<bas?a:bas;
   if(is->mnw.k==0&&!is->mnw.pc){lim=is->mnw.px+(is->bs?0:it[i].eh);if(bas<lim)bas=lim;it[i].mnf=lim;}
   if(bas<it[i].eh)bas=it[i].eh;
   it[i].bas=bas;}
  ls_[0]=0;nln=1;
  {int sum=0;
   for(i=0;i<ni;i++){
    int o=it[i].bas+it[i].ml+it[i].mr;
    if(s->fwrap&&i>ls_[nln-1]&&sum+gm+o>cw){ls_[nln++]=i;sum=0;}
    sum+=(i>ls_[nln-1]?gm:0)+o;}}
  ls_[nln]=ni;
  for(li=0;li<nln;li++){
   int i0=ls_[li],i1=ls_[li+1],cnt=i1-i0,sum=0,tg=0,tsh=0,nauto=0,free,rem,start=0,gap2=gm,x,lineH=0,tot=0;
   for(i=i0;i<i1;i++){sum+=it[i].bas+it[i].ml+it[i].mr;tg+=it[i].s->fgrow;tsh+=(int)((long long)it[i].s->fshr*it[i].bas/1000);nauto+=it[i].mlA+it[i].mrA;it[i].w=it[i].bas;}
   sum+=gm*(cnt-1);free=cw-sum;
   if(free>0&&tg>0){for(i=i0;i<i1;i++)if(it[i].s->fgrow>0)it[i].w+=(int)((long long)free*it[i].s->fgrow/tg);}
   else if(free<0&&tsh>0){
    for(i=i0;i<i1;i++){
     int sh=(int)((long long)it[i].s->fshr*it[i].bas/1000);
     it[i].w+=(int)((long long)free*sh/tsh);
     if(it[i].w<it[i].mnf)it[i].w=it[i].mnf;
     if(it[i].w<it[i].eh)it[i].w=it[i].eh;}}
   for(i=i0;i<i1;i++)tot+=it[i].w+it[i].ml+it[i].mr;
   tot+=gm*(cnt-1);rem=cw-tot;
   if(rem>0&&nauto){int per=rem/nauto;for(i=i0;i<i1;i++){if(it[i].mlA)it[i].ml+=per;if(it[i].mrA)it[i].mr+=per;}rem=0;}
   if(rem>0){
    switch(s->jc){
     case 1:start=rem;break;
     case 2:start=rem/2;break;
     case 3:if(cnt>1)gap2=gm+rem/(cnt-1);break;
     case 4:{int u=rem/cnt;start=u/2;gap2=gm+u;}break;
     case 5:{int u=rem/(cnt+1);start=u;gap2=gm+u;}break;}}
   x=cx+start;
   for(i=i0;i<i1;i++){it[i].xb=x+it[i].ml;x+=it[i].ml+it[i].w+it[i].mr+gap2;}
   if(rev)for(i=i0;i<i1;i++)it[i].xb=cx+cw-(it[i].xb-cx)-it[i].w;
   for(i=i0;i<i1;i++){
    it[i].h=lay_at(it[i].n,cx,it[i].w+it[i].ml+it[i].mr,Y,it[i].w,-1,0,1);
    if(it[i].h>lineH)lineH=it[i].h;}
   if(nln==1&&fch>=0&&lineH<fch)lineH=fch;
   for(i=i0;i<i1;i++){
    St*is=it[i].s;int ai=is->as>=0?is->as:s->ai,yoff=0,fhv=-1;
    if(ai==0&&is->h.k==1)fhv=lineH-it[i].mt-it[i].mb;
    else if(ai==2)yoff=lineH-it[i].h;
    else if(ai==3)yoff=(lineH-it[i].h)/2;
    if(yoff<0)yoff=0;
    lay_at(it[i].n,it[i].xb-it[i].ml,it[i].w+it[i].ml+it[i].mr,Y+yoff,it[i].w,fhv,0,0);}
   Y+=lineH+(li<nln-1?gc_:0);}
  return;}
 /* column */
 {int tot=0,tg=0,free,grow[64];
  for(i=0;i<ni;i++){
   St*is=it[i].s;int ai=is->as>=0?is->as:s->ai;
   if(ai==0)it[i].w=-1;
   else{mmbox(it[i].n,&a,&b);it[i].w=b;if(it[i].w>cw-it[i].ml-it[i].mr)it[i].w=cw-it[i].ml-it[i].mr;}
   it[i].h=lay_at(it[i].n,cx,it[i].w>=0?it[i].w+it[i].ml+it[i].mr:cw,Y,it[i].w,-1,0,1);
   tot+=it[i].h;tg+=is->fgrow;grow[i]=0;}
  tot+=gm*(ni-1);
  free=fch>=0?fch-tot:0;
  if(free>0&&tg>0)for(i=0;i<ni;i++)grow[i]=(int)((long long)free*it[i].s->fgrow/tg);
  for(j=0;j<ni;j++){
   i=rev?ni-1-j:j;
   {St*is=it[i].s;int ai=is->as>=0?is->as:s->ai,ax=cx,aw=cw,fhv=-1,hh;
    if(it[i].w>=0){aw=it[i].w+it[i].ml+it[i].mr;ax=ai==2?cx+cw-aw:ai==3?cx+(cw-aw)/2:cx;}
    if(grow[i]>0&&is->h.k==1)fhv=it[i].h-it[i].mt-it[i].mb+grow[i];
    hh=lay_at(it[i].n,ax,aw,Y,it[i].w,fhv,0,0);
    Y+=(fhv>=0?it[i].h+grow[i]:hh)+gm;}}
  if(ni)Y-=gm;}}

/* ---- grid (auto-placement, fixed / fr / auto tracks, span) ---- */
static void gridc(N*n){
 St*s=n->s;N*c,*items[64];int ni=0,i,j,ncol,gx=rl(s->gx,cw),gy=rl(s->gy,cw),tw_[12],tx_[12],fixedsum=0,frsum=0,rem,span[64],col[64],row[64],r,maxr=0;
 int kind[12];long long fr[12];
 for(c=n->kid;c&&ni<64;c=c->nx)if(c->tag.n&&c->s&&c->s->disp!=D_NONE&&c->s->pos<2)items[ni++]=c;
 if(!ni)return;
 if(s->gtauto){int mw_=s->gtmin>0?s->gtmin:1;ncol=(cw+gx)/(mw_+gx);if(ncol<1)ncol=1;if(ncol>12)ncol=12;for(i=0;i<ncol;i++){kind[i]=1;fr[i]=1000;}}
 else if(s->gtn>0){ncol=s->gtn;for(i=0;i<ncol;i++){kind[i]=s->gtk[i];fr[i]=s->gtk[i]==1?s->gt[i].px:1000;if(kind[i]==2)kind[i]=1;}}
 else{ncol=1;kind[0]=1;fr[0]=1000;}
 for(i=0;i<ncol;i++){if(kind[i]==0){tw_[i]=rl(s->gt[i],cw);fixedsum+=tw_[i];}else frsum+=(int)fr[i];}
 rem=cw-fixedsum-gx*(ncol-1);if(rem<0)rem=0;
 for(i=0;i<ncol;i++)if(kind[i]!=0)tw_[i]=frsum?(int)((long long)rem*fr[i]/frsum):0;
 tx_[0]=cx;for(i=1;i<ncol;i++)tx_[i]=tx_[i-1]+tw_[i-1]+gx;
 {int cc=0,rr=0;
  for(i=0;i<ni;i++){
   int sp_=items[i]->s->gcs>1?items[i]->s->gcs:1;
   if(sp_>ncol)sp_=ncol;
   if(cc+sp_>ncol){cc=0;rr++;}
   col[i]=cc;span[i]=sp_;row[i]=rr;cc+=sp_;if(cc>=ncol){cc=0;rr++;}
   if(row[i]>maxr)maxr=row[i];}}
 for(r=0;r<=maxr;r++){
  int rh=0,hh[64];
  for(i=0;i<ni;i++)if(row[i]==r){
   St*is=items[i]->s;int w=0,ml=rl(is->m[3],cw),mr=rl(is->m[1],cw);
   for(j=0;j<span[i];j++)w+=tw_[col[i]+j]+(j?gx:0);
   hh[i]=lay_at(items[i],tx_[col[i]],w,Y,is->w.k==0?-1:w-ml-mr,-1,0,1);
   if(hh[i]>rh)rh=hh[i];}
  for(i=0;i<ni;i++)if(row[i]==r){
   St*is=items[i]->s;int w=0,ml=rl(is->m[3],cw),mr=rl(is->m[1],cw),mt=rl(is->m[0],cw),mb=rl(is->m[2],cw),ai=is->as>=0?is->as:s->ai,yoff=0,fhv=-1;
   for(j=0;j<span[i];j++)w+=tw_[col[i]+j]+(j?gx:0);
   if(ai==0&&is->h.k==1)fhv=rh-mt-mb;else if(ai==2)yoff=rh-hh[i];else if(ai==3)yoff=(rh-hh[i])/2;
   if(yoff<0)yoff=0;
   lay_at(items[i],tx_[col[i]],w,Y+yoff,is->w.k==0?-1:w-ml-mr,fhv,0,0);}
  Y+=rh+(r<maxr?gy:0);}}

/* ---- tables ---- */
static void tablec(N*n){
 St*s=n->s;Tb t;int sp_=s->bcol?0:s->bsp,i,j,r,avail,fixedw,colx[17],yr[129],x,tot=0,h,ov;N*c;
 for(c=n->kid;c;c=c->nx)if(c->tag.n&&c->s&&seq(c->tag,"caption")&&c->s->disp!=D_NONE){h=lay_at(c,cx,cw,Y,-1,-1,0,0);Y+=h;}
 tbuild(n,&t);
 if(!t.ncl)return;
 tmeas(&t);
 avail=cw-sp_*(t.ncols+1);if(avail<0)avail=0;
 fixedw=s->w.k==0;
 tdist(&t,avail,fixedw);
 colx[0]=cx+sp_;
 for(i=0;i<t.ncols;i++){
  ov=0;
  if(s->bcol&&i+1<t.ncols)for(j=0;j<t.ncl;j++)if(t.cl[j].c==i+1){ov=t.cl[j].n->s->bw[3];break;}
  colx[i+1]=colx[i]+t.cwd[i]+sp_-ov;
  tot+=t.cwd[i];}
 for(i=0;i<t.ncl;i++){
  Tc*k=&t.cl[i];int w=0;
  for(j=0;j<k->cs;j++)w+=t.cwd[k->c+j]+(j?sp_:0);
  k->w=w;
  k->h=lay_at(k->n,colx[k->c],w,Y,w,-1,0,1);}
 for(i=0;i<t.nrows;i++)t.rh[i]=0;
 for(i=0;i<t.ncl;i++)if(t.cl[i].rs==1&&t.cl[i].h>t.rh[t.cl[i].r])t.rh[t.cl[i].r]=t.cl[i].h;
 for(i=0;i<t.ncl;i++)if(t.cl[i].rs>1){
  Tc*k=&t.cl[i];int have=0;
  for(j=0;j<k->rs;j++)have+=t.rh[k->r+j]+(j?sp_:0);
  if(have<k->h)t.rh[k->r+k->rs-1]+=k->h-have;}
 yr[0]=Y+sp_;
 for(r=0;r<t.nrows;r++){
  ov=0;
  if(s->bcol&&r+1<t.nrows)for(j=0;j<t.ncl;j++)if(t.cl[j].r==r+1){ov=t.cl[j].n->s->bw[0];break;}
  yr[r+1]=yr[r]+t.rh[r]+sp_-ov;}
 for(r=0;r<t.nrows;r++)if(t.rows[r]->s->bg>=0)cadd(0,cx+sp_,yr[r],colx[t.ncols]-colx[0]-sp_+(s->bcol?0:0),t.rh[r],rc(t.rows[r]->s->bg,t.rows[r]->s));
 for(i=0;i<t.ncl;i++){
  Tc*k=&t.cl[i];int hh=0,va=k->n->s->vaa;
  for(j=0;j<k->rs;j++)hh+=t.rh[k->r+j]+(j?sp_:0);
  lay_at(k->n,colx[k->c],k->w,yr[k->r],k->w,hh,va==1?0:va==3?2:1,0);}
 Y=yr[t.nrows]-(s->bcol?0:0);
 if(sp_&&0)Y+=0;
 (void)x;(void)tot;}

/* ---- absolutely positioned boxes ---- */
static void doabs(void){
 int i,bx,by,bw,bh,mn,mx,eh,wb,l,r,t,b,x,y,hh;N*c;St*s;
 for(i=0;i<nab;i++){
  c=abq[i].n;s=c->s;
  if(abq[i].cb&&abq[i].cb->x1>abq[i].cb->x0){bx=abq[i].cb->x0;by=abq[i].cb->y0;bw=abq[i].cb->x1-bx;bh=abq[i].cb->y1-by;}
  else{bx=0;by=0;bw=W;bh=VH;}
  cx=bx;cw=bw;
  mmbox(c,&mn,&mx);eh=ex_(s,bw);
  l=s->lf.k==0?rl(s->lf,bw):AU;r=s->rt.k==0?rl(s->rt,bw):AU;t=s->top.k==0?rl(s->top,bh):AU;b=s->bot.k==0?rl(s->bot,bh):AU;
  if(s->w.k==0)wb=rl(s->w,bw)+(s->bs?0:eh);
  else if(l!=AU&&r!=AU)wb=bw-l-r-mlr(s,bw);
  else{wb=bw-(l!=AU?l:0)-(r!=AU?r:0);if(wb>mx)wb=mx;if(wb<mn)wb=mn;}
  if(wb<eh)wb=eh;
  hh=lay_at(c,0,wb+mlr(s,bw),0,wb,-1,0,1);
  x=l!=AU?bx+l:r!=AU?bx+bw-r-wb-mlr(s,bw):abq[i].sx;
  y=t!=AU?by+t:b!=AU?by+bh-b-hh:abq[i].sy;
  Y=y;pend=0;
  lay_at(c,x,wb+mlr(s,bw),y,wb,-1,0,0);}}

/* ---- public API ---- */
int mwe_load(char*html,int len){
 int i;
 ap=0;full=0;nr=0;nnot=0;rdirty=0;ttl.n=0;ttl.p=0;nss=0;nc=0;nbx=0;nclp=0;
 for(i=0;i<4;i++)qb[i]=(i&1)?(1<<30):0;
 css((S){(char*)UA,sl(UA)});
 root=parse(html,len);
 return full;}
void mwe_viewport_h(int h){VH=h<100?100:h;}
void mwe_add_css(char*text,int len){css((S){text,len});}
const char*mwe_stylesheet(int i,int*len){if(i<0||i>=nss)return 0;*len=sshref[i].n;return sshref[i].p;}
int mwe_layout(int width){
 N*c,*d;
 if(width<16)width=16;
 W=VW=width;
 restyle();
 nc=0;nbx=0;nclp=0;nf=0;nab=0;scp=0;Y=0;pend=0;LH=LA=LD=0;lx=lsx=cx=0;cw=W;lend=W;lf=0;ca=0;sp=0;bulp=0;curh=0;curop=255;curcl=0;isol=0;fw=fh=-1;fva=0;fch=-1;ng=0;padp=0;
 walk(root);nl();
 {int ey=Y+pend;doabs();Y=ey;pend=0;}
 H=Y;if(H<1)H=1;if(H>400000)H=400000;
 for(c=root->kid;c;c=c->nx)if(c->tag.n&&c->s){if(c->s->disp==D_NONE)continue;
  if(seq(c->tag,"html")){if(c->s->bg>=0)cv=c->s->bg;for(d=c->kid;d;d=d->nx)if(d->tag.n&&seq(d->tag,"body")&&d->s&&d->s->bg>=0)cv=d->s->bg;}
  else if(seq(c->tag,"body")&&c->s->bg>=0)cv=c->s->bg;}
 if(cv<0)cv=0xffffff;
 cv&=0xFFFFFF;
 return H;}
void mwe_paint(unsigned*buf,int stride,int x,int y,int w,int h){
 int i,j,m;Cmd*k;Bx*b;
 tgt(buf,stride,x,y,w,h);
 for(j=0;j<h;j++)for(i=0;i<w;i++)buf[j*stride+i]=(unsigned)cv;
 for(i=0;i<nc;i++){
  k=&cm[i];m=k->ex+8;
  if(k->k==3||k->y>=y+h+m||k->y+k->h<=y-m||k->x>=x+w+m||k->x+k->w+m<=x)continue;
  kx0=wx0;ky0=wy0;kx1=wx1;ky1=wy1;
  for(j=k->cl;j;j=clps[j-1].par){
   if(clps[j-1].x0>kx0)kx0=clps[j-1].x0;
   if(clps[j-1].y0>ky0)ky0=clps[j-1].y0;
   if(clps[j-1].x1<kx1)kx1=clps[j-1].x1;
   if(clps[j-1].y1<ky1)ky1=clps[j-1].y1;}
  gop=k->op;
  switch(k->k){
   case 0:fr(k->x,k->y,k->w,k->h,k->c);break;
   case 1:
    if(k->bg>=0)fr(k->x,k->y,k->w,k->h,k->bg);
    run(k->x,k->y+k->a,k->t,k->n,k->fs,k->tf,k->ls,k->c,k->u,k->w);break;
   case 2:b=&bxs[k->bi];b->x=k->x;b->y=k->y;b->w=k->w;b->h=k->h;pbox(b);break;
   case 4:ptri(k->x,k->y,k->w,k->u,k->c);break;}}
 gop=255;}
int mwe_click(int x,int y){
 int i;Cmd*k;
 for(i=nc-1;i>=0;i--){
  k=&cm[i];
  if(k->k==3&&k->nd&&x>=k->x&&x<k->x+k->w&&y>=k->y&&y<k->y+k->h){k->nd->det^=1;return 1;}}
 return 0;}
const char*mwe_link(int x,int y,int*len){
 int i;Cmd*k;
 for(i=nc-1;i>=0;i--){
  k=&cm[i];
  if(k->hn&&x>=k->x&&x<k->x+k->w&&y>=k->y&&y<k->y+k->h){*len=k->hn;return k->hp;}}
 return 0;}
const char*mwe_title(int*len){*len=ttl.n;return ttl.p;}
unsigned mwe_bg(void){return(unsigned)cv;}
const char*mwe_font_name(void){return MF_NAME;}
int mwe_textw(const char*s,int n,int fs,int b){return wid(s,n,fs,b&1,0);}
void mwe_text(unsigned*buf,int st,int bw,int bh,int x,int base_,const char*s,int n,int fs,int b,unsigned col,int clx0,int clx1){
 tgt(buf,st,0,0,bw,bh);
 if(clx0>kx0)kx0=clx0;
 if(clx1<kx1)kx1=clx1;
 run(x,base_,s,n,fs,b&1,0,(int)col,0,0);}
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
 if(ac<3){static const char u[]="MiniWebEngine v" MWE_VERSION " (font: " MF_NAME ")\nusage: mwe in.html out.bmp [width]\n";fwrite(u,1,sizeof u-1,stderr);return 1;}
 if(ac>3){for(i=0,x=0;av[3][i]>='0'&&av[3][i]<='9'&&x<9999;i++)x=x*10+av[3][i]-'0';if(x>=64&&x<=MAXW)w=x;}
 if(!(f=fopen(av[1],"rb")))return 2;
 n=(int)fread(inbuf,1,IN,f);fclose(f);
 mwe_load(inbuf,n);
 n=mwe_layout(w);
 rs=(w*3+3)&~3;
 if(n>32768)n=32768;
 if((long long)rs*n>2000000000LL)n=(int)(2000000000LL/rs);
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
