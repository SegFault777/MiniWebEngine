/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine module: mwe_images
 * <img> store, image sampling (decoders live in mwe_img.h).
 * Not a standalone translation unit: mwe.c includes the modules in dependency order (unity build). */
/* ---- images: <img src> decoded by mwe_img.h; the host supplies bytes for non-data: URLs ---- */
#define MAXIMG 64
typedef struct{S src;unsigned*px;int w,h,st;}Im;   /* st: 0 pending, 1 decoded, 2 failed */
static Im ims[MAXIMG];static int nims;
#define IMGPIX (1<<22)                               /* decoded pixels kept per page (16 MB), each image allocated on its own */
static long long ipn;                                /* pixels in use */
static void imfreeall(void){int i;for(i=0;i<nims;i++)if(ims[i].px){mem_rel(ims[i].px);ims[i].px=0;}ipn=0;}
static int imsvg(int i,const unsigned char*d,int n);   /* mwe_svg.h: SVG files are drawn by the SVG rasterizer */
static int imset(int i,const unsigned char*d,int n){
 int w=0,h=0,f;unsigned*px;
 if(i<0||i>=nims)return 0;
 ims[i].st=2;
 f=mi_probe(d,n,&w,&h);
 if(!f)return imsvg(i,d,n);
 if(w<=0||h<=0||(long long)w*h>IMGPIX-ipn)return 0;
 px=(unsigned*)mem_get((unsigned long)w*(unsigned long)h*4UL);
 if(!px)return 0;
 if(!mi_decode(f,d,n,px,w,h)){mem_rel(px);return 0;}
 ims[i].px=px;ims[i].w=w;ims[i].h=h;ims[i].st=1;ipn+=(long long)w*h;return 1;}
static int b64v(int c){return c>='A'&&c<='Z'?c-'A':c>='a'&&c<='z'?c-'a'+26:c>='0'&&c<='9'?c-'0'+52:c=='+'||c=='-'?62:c=='/'||c=='_'?63:-1;}
static int imhx(int c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
static int imdata(int i){   /* data:[mime];base64,PAYLOAD */
 S u=ims[i].src;int k=5,b64=0,o=0,acc=0,nb=0,v;
 while(k<u.n&&u.p[k]!=',')k++;
 if(k>=u.n)return 0;
 {int j;for(j=5;j+6<=k;j++)if(lc(u.p[j])==';'&&lc(u.p[j+1])=='b'&&lc(u.p[j+2])=='a'&&lc(u.p[j+3])=='s'&&lc(u.p[j+4])=='e'&&u.p[j+5]=='6')b64=1;}
 if(!b64){   /* data:image/svg+xml;utf8,<svg ...> or percent-encoded text */
  int r;mu8*td=(mu8*)mem_get((unsigned long)(u.n-k)+2UL);
  if(!td)return 0;
  for(k++;k<u.n;k++){
   int c=(unsigned char)u.p[k],h1,h2;
   if(c=='%'&&k+2<u.n&&(h1=imhx(u.p[k+1]))>=0&&(h2=imhx(u.p[k+2]))>=0){td[o++]=(mu8)(h1*16+h2);k+=2;}
   else td[o++]=(mu8)c;}
  r=imset(i,td,o);mem_rel(td);return r;}
 {int r;mu8*idat=(mu8*)mem_get((unsigned long)(u.n-k)*3UL/4UL+8UL);   /* decoded payload, released right after the image is decoded */
 if(!idat)return 0;
 for(k++;k<u.n;k++){
  v=b64v((u.p[k]));
  if(v<0)continue;
  acc=acc<<6|v;nb+=6;
  if(nb>=8){nb-=8;idat[o++]=(mu8)(acc>>nb);acc&=(1<<nb)-1;}}
 r=imset(i,idat,o);mem_rel(idat);return r;}}
static int imreg(S src){
 int i,j;
 for(i=0;i<nims;i++){if(ims[i].src.n==src.n){for(j=0;j<src.n&&ims[i].src.p[j]==src.p[j];j++){}if(j==src.n)return i;}}
 if(nims>=MAXIMG)return -1;
 ims[nims].src=src;ims[nims].px=0;ims[nims].w=ims[nims].h=0;ims[nims].st=0;i=nims++;
 if(src.n>5&&lc(src.p[0])=='d'&&lc(src.p[1])=='a'&&lc(src.p[2])=='t'&&lc(src.p[3])=='a'&&src.p[4]==':'){if(!imdata(i))ims[i].st=2;}
 return i;}
/* ---- <picture> / srcset / sizes: choose the source for an <img> ---- */
static S srcsetpick(S ss,int need){   /* candidates "url [Nw|Nx]", comma separated; widths: smallest >= need, else the largest; densities: 1x */
 S best={0,0};int i=0,bw=-1,bx=1<<30,wmode=0,bmax=-1;S bmaxu={0,0};
 while(i<ss.n){
  S u;int ds=0,dd=0,isw=0,v=0,a;
  while(i<ss.n&&(isws(ss.p[i])||ss.p[i]==','))i++;
  a=i;while(i<ss.n&&!isws(ss.p[i]))i++;
  u=(S){ss.p+a,i-a};
  if(u.n&&u.p[u.n-1]==','){u.n--;}
  else{
   while(i<ss.n&&isws(ss.p[i]))i++;
   a=i;while(i<ss.n&&ss.p[i]!=',')i++;
   {int q;for(q=a;q<i;q++){char c=ss.p[q];if(c>='0'&&c<='9'){if(!dd)v=v*10+(c-'0');else ds=ds*10+(c-'0');}else if(c=='.')dd=1;else if(c=='w'){isw=1;}else if(c=='x'){}}}}
  if(!u.n)continue;
  if(isw){wmode=1;if(v>=need&&(bw<0||v<bw)){bw=v;best=u;}if(v>bmax){bmax=v;bmaxu=u;}}
  else{int x=v>0?v*10:10;if(!wmode&&(x>=10&&x<bx)){bx=x;best=u;}if(!best.n)best=u;}}
 if(wmode&&!best.n)best=bmaxu;
 return best;}
static int imtype_ok(S ty){return !ty.n||starts(ty,"image/png")||starts(ty,"image/jpeg")||starts(ty,"image/jpg")||starts(ty,"image/gif")||starts(ty,"image/bmp");}
static S imsrcof(N*n){
 S src=getattr(n,"src"),ss=getattr(n,"srcset"),wa=getattr(n,"width");N*p=n->par,*c;int need=0,i;
 for(i=0;i<wa.n&&wa.p[i]>='0'&&wa.p[i]<='9';i++)need=need*10+(wa.p[i]-'0');
 if(!need)need=VW<1200?VW:1200;
 if(p&&p->tag.n==7&&seq(p->tag,"picture")){
  for(c=p->kid;c&&c!=n;c=c->nx){
   S ty,md,sset;
   if(!c->tag.n||!seq(c->tag,"source"))continue;
   ty=getattr(c,"type");md=getattr(c,"media");sset=getattr(c,"srcset");
   if(!imtype_ok(ty))continue;
   if(md.n){int b[4];b[0]=0;b[1]=1<<30;b[2]=0;b[3]=1<<30;if(!mq1(md,b)||VW<b[0]||VW>b[1])continue;}
   if(sset.n){S u=srcsetpick(sset,need);if(u.n)return u;}}}
 if(ss.n){S u=srcsetpick(ss,need);if(u.n)return u;}
 return src;}
static int imfor(N*n){   /* index of the decoded image for an <img>, registering it on first sight; -1 if none (yet) */
 S src=imsrcof(n);int i;
 if(!src.p||src.n<=0)return -1;
 i=imreg(src);
 return i>=0&&ims[i].st==1?i:-1;}
static int imfind(S u){int i=imreg(u);return i>=0&&ims[i].st==1?i:-1;}   /* decoded image for a CSS url() */
static void imscan(N*n){N*c;if(n->tag.n==3&&seq(n->tag,"img")){S src=imsrcof(n);if(src.p&&src.n>0)imreg(src);}for(c=n->kid;c;c=c->nx)imscan(c);}
