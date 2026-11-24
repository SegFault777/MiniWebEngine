/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine: inline <svg> (new in v0.12).
 * The <svg> element is a replaced box (width/height attributes or CSS, default 300x150); its content is rasterized at paint time.
 * Supported: rect (rx/ry), circle, ellipse, line, polyline, polygon, path (M L H V C S Q T A Z, absolute and relative), g,
 * nested svg (x/y), use (href="#id"), viewBox + preserveAspectRatio (meet/slice, x/y alignment), transform (translate scale
 * rotate matrix skewX skewY), fill / stroke / stroke-width / fill-opacity / stroke-opacity / opacity / fill-rule (nonzero,
 * evenodd) / stroke-linecap + linejoin (joins are always drawn round) as attributes or in style="", currentColor.
 * Gradients: linearGradient/radialGradient fills use the colour of the first stop (no real gradients).
 * Not supported: text, filters, masks, clip-path, patterns, markers, dashes (stroke-dasharray), CSS rules from <style> sheets
 * (only presentation attributes and the style attribute), images, animations, <img src="x.svg"> files.
 * Included by mwe_api.inc after mwe_js.h (uses its math helpers). Scratch memory is lent by the host on first use. */

static int memcmp_(const char*a,const char*b,int n);
static double jatan2_(double y,double x);
typedef struct{double a,b,c,d,e,f;}SvM;
typedef struct{int fill,stroke;double sw,fop,sop,op;int evenodd,col,vis;}SvS;
#define SV_PTS 60000
#define SV_SUB 4096
static float*svx,*svy;static int*svs;static int svn_,svsub;        /* flattened path points / sub-path starts */
static unsigned char*svcov;
static int svbx,svby,svbw,svbh;                                        /* svg box in document coordinates (clip) */

static SvM svmul(SvM p,SvM q){SvM r;r.a=p.a*q.a+p.c*q.b;r.b=p.b*q.a+p.d*q.b;r.c=p.a*q.c+p.c*q.d;r.d=p.b*q.c+p.d*q.d;r.e=p.a*q.e+p.c*q.f+p.e;r.f=p.b*q.e+p.d*q.f+p.f;return r;}
static void svxf(SvM m,double x,double y,double*ox,double*oy){*ox=m.a*x+m.c*y+m.e;*oy=m.b*x+m.d*y+m.f;}

/* ---- tokenizing numbers ---- */
static int svws(int c){return c==' '||c=='\t'||c=='\n'||c=='\r'||c==',';}
static double svnum(const char**p,const char*e,int*ok){
 const char*s=*p;double v=0,f=1;int neg=0,got=0,ex=0,eneg=0;
 while(s<e&&svws(*s))s++;
 if(s<e&&(*s=='-'||*s=='+')){neg=*s=='-';s++;}
 while(s<e&&*s>='0'&&*s<='9'){v=v*10+(*s-'0');s++;got=1;}
 if(s<e&&*s=='.'){s++;while(s<e&&*s>='0'&&*s<='9'){f/=10;v+=(*s-'0')*f;s++;got=1;}}
 if(got&&s<e&&(*s=='e'||*s=='E')&&s+1<e&&((s[1]>='0'&&s[1]<='9')||((s[1]=='-'||s[1]=='+')&&s+2<e&&s[2]>='0'&&s[2]<='9'))){
  s++;if(*s=='-'||*s=='+'){eneg=*s=='-';s++;}
  while(s<e&&*s>='0'&&*s<='9'){ex=ex*10+(*s-'0');s++;}
  while(ex-->0)v=eneg?v/10:v*10;}
 *ok=got;*p=s;return neg?-v:v;}
static double svlen(S a,double dflt,double ref){   /* "12", "12px", "50%", "2em" */
 const char*p=a.p,*e=a.p+a.n;int ok;double v;
 if(!a.p||!a.n)return dflt;
 v=svnum(&p,e,&ok);
 if(!ok)return dflt;
 if(p<e&&*p=='%')return v*ref/100;
 if(p+1<e&&p[0]=='e'&&p[1]=='m')return v*16;
 return v;}
static double svat(N*n,const char*nm,double dflt,double ref){S a=getattr(n,nm);return a.p?svlen(a,dflt,ref):dflt;}

/* ---- points ---- */
static int svpt(double x,double y){if(svn_>=SV_PTS)return 0;svx[svn_]=(float)x;svy[svn_]=(float)y;svn_++;return 1;}
static void svbegin(void){if(svsub<SV_SUB)svs[svsub++]=svn_;}
static void svcubic(SvM m,double x0,double y0,double x1,double y1,double x2,double y2,double x3,double y3){
 int i;double ox,oy;
 for(i=1;i<=14;i++){
  double t=i/14.0,u=1-t,x=u*u*u*x0+3*u*u*t*x1+3*u*t*t*x2+t*t*t*x3,y=u*u*u*y0+3*u*u*t*y1+3*u*t*t*y2+t*t*t*y3;
  svxf(m,x,y,&ox,&oy);svpt(ox,oy);}}
static void svquad(SvM m,double x0,double y0,double x1,double y1,double x2,double y2){
 svcubic(m,x0,y0,x0+2.0/3*(x1-x0),y0+2.0/3*(y1-y0),x2+2.0/3*(x1-x2),y2+2.0/3*(y1-y2),x2,y2);}
static void svarc(SvM m,double x0,double y0,double rx,double ry,double rot,int large,int sweep,double x1,double y1){
 double cp,sp,dx,dy,x1p,y1p,lam,sq,num,den,cxp,cyp,cx,cy,th1,dth,k;int i,n;
 if(rx<0)rx=-rx;
 if(ry<0)ry=-ry;
 if((x0==x1&&y0==y1)||rx==0||ry==0){double ox,oy;svxf(m,x1,y1,&ox,&oy);svpt(ox,oy);return;}
 rot=rot*JPI/180;cp=jcos(rot);sp=jsin(rot);
 dx=(x0-x1)/2;dy=(y0-y1)/2;x1p=cp*dx+sp*dy;y1p=-sp*dx+cp*dy;
 lam=x1p*x1p/(rx*rx)+y1p*y1p/(ry*ry);
 if(lam>1){k=jsqrt(lam);rx*=k;ry*=k;}
 num=rx*rx*ry*ry-rx*rx*y1p*y1p-ry*ry*x1p*x1p;den=rx*rx*y1p*y1p+ry*ry*x1p*x1p;
 sq=den==0?0:num/den;if(sq<0)sq=0;sq=jsqrt(sq);if(large==sweep)sq=-sq;
 cxp=sq*rx*y1p/ry;cyp=-sq*ry*x1p/rx;
 cx=cp*cxp-sp*cyp+(x0+x1)/2;cy=sp*cxp+cp*cyp+(y0+y1)/2;
 {double ux=(x1p-cxp)/rx,uy=(y1p-cyp)/ry,vx=(-x1p-cxp)/rx,vy=(-y1p-cyp)/ry;
  th1=jatan2_(uy,ux);dth=jatan2_(vy,vx)-th1;}
 if(!sweep&&dth>0)dth-=2*JPI;else if(sweep&&dth<0)dth+=2*JPI;
 n=(int)(jfabs(dth)/(JPI/16))+2;
 for(i=1;i<=n;i++){
  double t=th1+dth*i/n,ex=rx*jcos(t),ey=ry*jsin(t),ox,oy;
  svxf(m,cp*ex-sp*ey+cx,sp*ex+cp*ey+cy,&ox,&oy);svpt(ox,oy);}}
static double jatan2_(double y,double x){
 double a;
 if(x==0)return y>0?JPI/2:y<0?-JPI/2:0;
 a=jatan(y/x);
 if(x<0)a+=y>=0?JPI:-JPI;
 return a;}

/* parse a path "d" into svx/svy sub-paths; returns whether it should be treated as closed per sub-path via svs only */
static void svpath(SvM m,S d){
 const char*p=d.p,*e=d.p+d.n;char cmd=0;double cx=0,cy=0,sx=0,sy=0,lcx=0,lcy=0;int prevc=0;int ok;double ox,oy;
 while(p<e){
  int rel;double a[7];
  while(p<e&&svws(*p))p++;
  if(p>=e)break;
  if((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')){cmd=*p++;}
  else if(!cmd)break;
  rel=cmd>='a';
  switch(cmd|32){
  case 'z':
   if(svn_>0){svxf(m,sx,sy,&ox,&oy);svpt(ox,oy);}
   cx=sx;cy=sy;prevc=0;
   while(p<e&&svws(*p))p++;
   if(p<e&&!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')))cmd='L'+(rel?32:0);   /* implicit lineto after z is invalid; avoid looping */
   break;
  case 'm':case 'l':{
   a[0]=svnum(&p,e,&ok);if(!ok){p=e;break;}a[1]=svnum(&p,e,&ok);if(!ok){p=e;break;}
   if(rel){a[0]+=cx;a[1]+=cy;}
   if((cmd|32)=='m'){svbegin();sx=a[0];sy=a[1];}
   cx=a[0];cy=a[1];svxf(m,cx,cy,&ox,&oy);svpt(ox,oy);prevc=0;
   if((cmd|32)=='m')cmd=rel?'l':'L';
   break;}
  case 'h':a[0]=svnum(&p,e,&ok);if(!ok){p=e;break;}cx=rel?cx+a[0]:a[0];svxf(m,cx,cy,&ox,&oy);svpt(ox,oy);prevc=0;break;
  case 'v':a[0]=svnum(&p,e,&ok);if(!ok){p=e;break;}cy=rel?cy+a[0]:a[0];svxf(m,cx,cy,&ox,&oy);svpt(ox,oy);prevc=0;break;
  case 'c':case 's':{
   double x1,y1,x2,y2,x,y;int i,n=(cmd|32)=='c'?6:4;
   for(i=0;i<n;i++){a[i]=svnum(&p,e,&ok);if(!ok){p=e;break;}}
   if(!ok)break;
   if((cmd|32)=='c'){x1=a[0]+(rel?cx:0);y1=a[1]+(rel?cy:0);x2=a[2]+(rel?cx:0);y2=a[3]+(rel?cy:0);x=a[4]+(rel?cx:0);y=a[5]+(rel?cy:0);}
   else{x1=prevc==1?2*cx-lcx:cx;y1=prevc==1?2*cy-lcy:cy;x2=a[0]+(rel?cx:0);y2=a[1]+(rel?cy:0);x=a[2]+(rel?cx:0);y=a[3]+(rel?cy:0);}
   svcubic(m,cx,cy,x1,y1,x2,y2,x,y);lcx=x2;lcy=y2;cx=x;cy=y;prevc=1;break;}
  case 'q':case 't':{
   double x1,y1,x,y;int i,n=(cmd|32)=='q'?4:2;
   for(i=0;i<n;i++){a[i]=svnum(&p,e,&ok);if(!ok){p=e;break;}}
   if(!ok)break;
   if((cmd|32)=='q'){x1=a[0]+(rel?cx:0);y1=a[1]+(rel?cy:0);x=a[2]+(rel?cx:0);y=a[3]+(rel?cy:0);}
   else{x1=prevc==2?2*cx-lcx:cx;y1=prevc==2?2*cy-lcy:cy;x=a[0]+(rel?cx:0);y=a[1]+(rel?cy:0);}
   svquad(m,cx,cy,x1,y1,x,y);lcx=x1;lcy=y1;cx=x;cy=y;prevc=2;break;}
  case 'a':{
   int i,fl[2];double x,y;
   for(i=0;i<3;i++){a[i]=svnum(&p,e,&ok);if(!ok){p=e;break;}}
   if(!ok)break;
   for(i=0;i<2;i++){while(p<e&&svws(*p))p++;if(p<e&&(*p=='0'||*p=='1')){fl[i]=*p-'0';p++;}else{p=e;ok=0;break;}}
   if(!ok)break;
   x=svnum(&p,e,&ok);if(!ok){p=e;break;}y=svnum(&p,e,&ok);if(!ok){p=e;break;}
   if(rel){x+=cx;y+=cy;}
   svarc(m,cx,cy,a[0],a[1],a[2],fl[0],fl[1],x,y);cx=x;cy=y;prevc=0;break;}
  default:p=e;break;}}}

/* ---- scanline fill: svcov = coverage (0..255) over the clip box; polygons are svs-delimited point runs, nonzero or even-odd ---- */
static void svfill(int x0,int y0,int w,int h,int color,double opacity,int evenodd){
 int row,sub,i,j,k;
 static float xs[512];static int wn[512];
 for(row=0;row<h;row++){
  int ncov=0;
  for(i=0;i<w;i++)svcov[i]=0;
  {int cnt[1024];int *acc=cnt;(void)acc;}
  /* 4 sub-scanlines per pixel row; coverage is accumulated in svcov as sum of 64 per sub-line fraction */
  {static unsigned short sum[4096];
   int ww=w>4096?4096:w;
   for(i=0;i<ww;i++)sum[i]=0;
   for(sub=0;sub<4;sub++){
    float sy=(float)(y0+row)+(sub+0.5f)/4;int nx=0;
    for(k=0;k<svsub;k++){
     int a=svs[k],b=k+1<svsub?svs[k+1]:svn_;
     for(j=a;j<b;j++){
      int j2=j+1<b?j+1:a;
      float ax=svx[j],ay=svy[j],bx=svx[j2],by=svy[j2];
      int dir=1;
      if(ay==by)continue;
      if(ay>by){float t;t=ax;ax=bx;bx=t;t=ay;ay=by;by=t;dir=-1;}
      if(sy<ay||sy>=by)continue;
      if(nx<512){xs[nx]=ax+(bx-ax)*(sy-ay)/(by-ay);wn[nx]=dir;nx++;}}}
    /* insertion sort by x */
    for(i=1;i<nx;i++){float xv=xs[i];int wv=wn[i];j=i-1;while(j>=0&&xs[j]>xv){xs[j+1]=xs[j];wn[j+1]=wn[j];j--;}xs[j+1]=xv;wn[j+1]=wv;}
    {int wind=0;
     for(i=0;i<nx-1;i++){
      float xa,xb;int px_,qx_;
      wind+=evenodd?(wind?-1:1):wn[i];
      if(!wind)continue;
      xa=xs[i]-x0;xb=xs[i+1]-x0;
      if(xb<=0||xa>=ww)continue;
      if(xa<0)xa=0;
      if(xb>ww)xb=(float)ww;
      px_=(int)xa;qx_=(int)xb;
      if(px_==qx_){sum[px_]=(unsigned short)(sum[px_]+(int)((xb-xa)*64));}
      else{
       sum[px_]=(unsigned short)(sum[px_]+(int)((px_+1-xa)*64));
       for(j=px_+1;j<qx_;j++)sum[j]=(unsigned short)(sum[j]+64);
       if(qx_<ww)sum[qx_]=(unsigned short)(sum[qx_]+(int)((xb-qx_)*64));}}}}
   for(i=0;i<ww;i++){int c=sum[i]>255?255:sum[i];svcov[i]=(unsigned char)c;if(c)ncov=1;}}
  if(ncov)for(i=0;i<w&&i<4096;i++)if(svcov[i]){int a=(int)(svcov[i]*opacity);bp(x0+i,y0+row,color,a>255?255:a);}}}
/* bounding box of the current points, clipped to the svg box and the paint clip; 0 if nothing is visible */
static int svbbox(int pad,int*bx0,int*by0,int*bw,int*bh){
 float x0=1e9f,y0=1e9f,x1=-1e9f,y1=-1e9f;int i;int cx0,cy0,cx1,cy1;
 if(!svn_)return 0;
 for(i=0;i<svn_;i++){if(svx[i]<x0)x0=svx[i];if(svx[i]>x1)x1=svx[i];if(svy[i]<y0)y0=svy[i];if(svy[i]>y1)y1=svy[i];}
 cx0=(int)x0-pad;cy0=(int)y0-pad;cx1=(int)x1+pad+1;cy1=(int)y1+pad+1;
 if(cx0<svbx)cx0=svbx;
 if(cy0<svby)cy0=svby;
 if(cx1>svbx+svbw)cx1=svbx+svbw;
 if(cy1>svby+svbh)cy1=svby+svbh;
 if(cx0<kx0)cx0=kx0;
 if(cy0<ky0)cy0=ky0;
 if(cx1>kx1)cx1=kx1;
 if(cy1>ky1)cy1=ky1;
 if(cx1-cx0<=0||cy1-cy0<=0)return 0;
 if(cx1-cx0>4096)cx1=cx0+4096;
 *bx0=cx0;*by0=cy0;*bw=cx1-cx0;*bh=cy1-cy0;return 1;}
/* the stroke of the current points: every segment becomes a rectangle, every vertex a disc (so joins and caps are round) */
static void svstroke_pts(double w,int closed_hint){
 static float ox[SV_PTS/2],oy[SV_PTS/2];static int os[SV_SUB];int on=0,osn=0,k,j;
 (void)closed_hint;
 for(k=0;k<svsub;k++){
  int a=svs[k],b=k+1<svsub?svs[k+1]:svn_;
  for(j=a;j<b;j++){
   int j2=j+1;double dx,dy,l,nx,ny,x0=svx[j],y0=svy[j],x1,y1;
   if(on+20>SV_PTS/2||osn>=SV_SUB-2)goto done;
   if(j2<b){x1=svx[j2];y1=svy[j2];dx=x1-x0;dy=y1-y0;l=jsqrt(dx*dx+dy*dy);
    if(l>1e-4){nx=-dy/l*w/2;ny=dx/l*w/2;
     os[osn++]=on;
     ox[on]=(float)(x0+nx);oy[on++]=(float)(y0+ny);ox[on]=(float)(x1+nx);oy[on++]=(float)(y1+ny);
     ox[on]=(float)(x1-nx);oy[on++]=(float)(y1-ny);ox[on]=(float)(x0-nx);oy[on++]=(float)(y0-ny);}}
   if(w>1.2){   /* disc at the vertex */
    int q;os[osn++]=on;
    for(q=0;q<10;q++){double t=2*JPI*q/10;ox[on]=(float)(x0+w/2*jcos(t));oy[on++]=(float)(y0+w/2*jsin(t));}}}}
done:
 /* swap in the stroke polygons (all emitted with the same orientation test below so the nonzero union holds) */
 {int i;
  for(i=0;i<osn;i++){   /* normalize orientation to positive area */
   int a=os[i],b=i+1<osn?os[i+1]:on,q;double ar=0;
   for(q=a;q<b;q++){int q2=q+1<b?q+1:a;ar+=(double)ox[q]*oy[q2]-(double)ox[q2]*oy[q];}
   if(ar<0){int lo=a,hi=b-1;while(lo<hi){float t=ox[lo];ox[lo]=ox[hi];ox[hi]=t;t=oy[lo];oy[lo]=oy[hi];oy[hi]=t;lo++;hi--;}}}
  svn_=0;svsub=0;
  for(i=0;i<osn&&svsub<SV_SUB;i++){int a=os[i],b=i+1<osn?os[i+1]:on,q;svs[svsub++]=svn_;for(q=a;q<b&&svn_<SV_PTS;q++){svx[svn_]=ox[q];svy[svn_]=oy[q];svn_++;}}}}

static void svpaint_shape(SvS*st){
 int bx0,by0,bw,bh;
 if(!svn_||st->vis)return;
 if(st->fill>=0&&svbbox(1,&bx0,&by0,&bw,&bh)){
  svfill(bx0,by0,bw,bh,st->fill,st->fop*st->op,st->evenodd);}
 if(st->stroke>=0&&st->sw>0){
  static float sx_[SV_PTS/2],sy_[SV_PTS/2];(void)sx_;(void)sy_;
  svstroke_pts(st->sw,0);
  if(svbbox(1,&bx0,&by0,&bw,&bh))svfill(bx0,by0,bw,bh,st->stroke,st->sop*st->op,0);}}

/* ---- style ---- */
static S svstyleprop(S style,const char*name){   /* value of name in a style="a:b;c:d" string */
 int i=0,nl=0;while(name[nl])nl++;
 while(i<style.n){
  int a=i,c;
  while(i<style.n&&style.p[i]!=';')i++;
  c=a;while(c<i&&style.p[c]!=':')c++;
  if(c<i){S k=trim((S){style.p+a,c-a});if(k.n==nl&&has_s(k,0,name)){i=i<style.n?i+1:i;return trim((S){style.p+c+1,(int)(style.p+(i>0&&style.p[i-1]==';'?i-1:i)-(style.p+c+1))});}}
  i++;}
 return (S){0,0};}
static S svprop(N*n,const char*name){S st=getattr(n,"style"),v;if(st.p){v=svstyleprop(st,name);if(v.p&&v.n)return v;}return trim(getattr(n,name));}
static N*svfindid(N*n,S id,int depth){
 N*c;
 if(n->tag.n){S a=getattr(n,"id");if(a.p&&a.n==id.n&&has_s(a,0,"")&&!memcmp_(a.p,id.p,id.n))return n;}
 if(depth>40)return 0;
 for(c=n->kid;c;c=c->nx){N*r;if(c->tag.n&&(r=svfindid(c,id,depth+1)))return r;}
 return 0;}
static N*svroot_;
static int svpaintcolor(S v,int cur,int*none){   /* -2 = keep inherited */
 int c;
 *none=0;
 if(!v.p||!v.n)return -2;
 if(seq(v,"none")){*none=1;return -1;}
 if(seq(v,"currentColor")||seq(v,"currentcolor"))return cur;
 if(v.n>5&&has_s(v,0,"url(")){   /* gradient: use its first stop */
  const char*p=v.p+4,*e=v.p+v.n;S id;N*g;
  while(p<e&&(*p=='#'||*p=='\''||*p=='"'||*p==' '))p++;
  id.p=(char*)p;while(p<e&&*p!=')'&&*p!='\''&&*p!='"')p++;id.n=(int)(p-id.p);
  g=svroot_?svfindid(svroot_,id,0):0;
  if(g){N*s;for(s=g->kid;s;s=s->nx)if(s->tag.n&&seq(s->tag,"stop")){S sc=svprop(s,"stop-color");int k=sc.p?hcolor(sc):-1;return k>=0?k:0;}}
  return 0;}
 c=hcolor(v);
 return c<0?-2:c;}
static void svapply(N*n,SvS*st){
 S v;int none,c;
 v=svprop(n,"color");if(v.p&&v.n){c=hcolor(v);if(c>=0)st->col=c;}
 c=svpaintcolor(svprop(n,"fill"),st->col,&none);if(c!=-2)st->fill=none?-1:c;
 c=svpaintcolor(svprop(n,"stroke"),st->col,&none);if(c!=-2)st->stroke=none?-1:c;
 v=svprop(n,"stroke-width");if(v.p&&v.n)st->sw=svlen(v,st->sw,100);
 v=svprop(n,"fill-opacity");if(v.p&&v.n)st->fop=svlen(v,st->fop,1);
 v=svprop(n,"stroke-opacity");if(v.p&&v.n)st->sop=svlen(v,st->sop,1);
 v=svprop(n,"opacity");if(v.p&&v.n){double o=svlen(v,1,1);if(v.n&&v.p[v.n-1]=='%')o=svlen(v,100,1)/100;st->op*=o;}
 v=svprop(n,"fill-rule");if(v.p&&v.n)st->evenodd=seq(v,"evenodd");
 v=svprop(n,"display");if(v.p&&seq(v,"none"))st->vis=1;
 v=svprop(n,"visibility");if(v.p&&(seq(v,"hidden")||seq(v,"collapse")))st->vis=1;}
static SvM svtransform(N*n,SvM m){
 S t=getattr(n,"transform");const char*p,*e;
 if(!t.p||!t.n)return m;
 p=t.p;e=t.p+t.n;
 while(p<e){
  int ok,kind=0;double a[6]={0,0,0,0,0,0};int na=0;SvM q={1,0,0,1,0,0};
  while(p<e&&(svws(*p)||*p==')'))p++;
  if(p>=e)break;
  if(has_s(t,(int)(p-t.p),"translate"))kind=1;else if(has_s(t,(int)(p-t.p),"scale"))kind=2;else if(has_s(t,(int)(p-t.p),"rotate"))kind=3;
  else if(has_s(t,(int)(p-t.p),"matrix"))kind=4;else if(has_s(t,(int)(p-t.p),"skewx"))kind=5;else if(has_s(t,(int)(p-t.p),"skewy"))kind=6;
  while(p<e&&*p!='(')p++;
  p++;
  while(na<6){a[na]=svnum(&p,e,&ok);if(!ok)break;na++;}
  while(p<e&&*p!=')')p++;
  switch(kind){
  case 1:q.e=a[0];q.f=na>1?a[1]:0;break;
  case 2:q.a=a[0];q.d=na>1?a[1]:a[0];break;
  case 3:{double r=a[0]*JPI/180,c=jcos(r),s=jsin(r);SvM rm={c,s,-s,c,0,0};
   if(na>=3){SvM t1={1,0,0,1,a[1],a[2]},t2={1,0,0,1,-a[1],-a[2]};q=svmul(svmul(t1,rm),t2);}else q=rm;break;}
  case 4:if(na==6){q.a=a[0];q.b=a[1];q.c=a[2];q.d=a[3];q.e=a[4];q.f=a[5];}break;
  case 5:q.c=jsin(a[0]*JPI/180)/jcos(a[0]*JPI/180);break;
  case 6:q.b=jsin(a[0]*JPI/180)/jcos(a[0]*JPI/180);break;
  default:break;}
  m=svmul(m,q);}
 return m;}

static void svnode(N*n,SvM m,SvS st,int depth);
static void svkids(N*n,SvM m,SvS st,int depth){N*c;for(c=n->kid;c;c=c->nx)if(c->tag.n)svnode(c,m,st,depth+1);}
static void svnode(N*n,SvM m,SvS st,int depth){
 S tg=n->tag;
 if(depth>48)return;
 m=svtransform(n,m);
 svapply(n,&st);
 if(seq(tg,"defs")||seq(tg,"symbol")||seq(tg,"clipPath")||seq(tg,"mask")||seq(tg,"linearGradient")||seq(tg,"radialGradient")||seq(tg,"pattern")||seq(tg,"marker")||seq(tg,"title")||seq(tg,"desc")||seq(tg,"metadata")||seq(tg,"style")||seq(tg,"filter"))return;
 if(seq(tg,"g")||seq(tg,"a")||seq(tg,"switch")){svkids(n,m,st,depth);return;}
 if(seq(tg,"svg")){SvM t={1,0,0,1,svat(n,"x",0,100),svat(n,"y",0,100)};svkids(n,svmul(m,t),st,depth);return;}
 if(seq(tg,"use")){
  S h=getattr(n,"href");N*t;SvM q={1,0,0,1,svat(n,"x",0,100),svat(n,"y",0,100)};
  if(!h.p)h=getattr(n,"xlink:href");
  if(h.p&&h.n>1&&h.p[0]=='#'&&svroot_&&(t=svfindid(svroot_,(S){h.p+1,h.n-1},0))&&depth<24){
   if(seq(t->tag,"symbol")||seq(t->tag,"svg"))svkids(t,svmul(m,q),st,depth+1);else svnode(t,svmul(m,q),st,depth+1);}
  return;}
 svn_=0;svsub=0;
 if(seq(tg,"rect")){
  double x=svat(n,"x",0,100),y=svat(n,"y",0,100),w=svat(n,"width",0,100),h=svat(n,"height",0,100),rx=svat(n,"rx",-1,w),ry=svat(n,"ry",-1,h);
  if(w<=0||h<=0)return;
  if(rx<0&&ry<0)rx=ry=0;else if(rx<0)rx=ry;else if(ry<0)ry=rx;
  if(rx>w/2)rx=w/2;
  if(ry>h/2)ry=h/2;
  svbegin();
  if(rx<=0||ry<=0){double ox,oy;svxf(m,x,y,&ox,&oy);svpt(ox,oy);svxf(m,x+w,y,&ox,&oy);svpt(ox,oy);svxf(m,x+w,y+h,&ox,&oy);svpt(ox,oy);svxf(m,x,y+h,&ox,&oy);svpt(ox,oy);svxf(m,x,y,&ox,&oy);svpt(ox,oy);}
  else{double ox,oy;svxf(m,x+rx,y,&ox,&oy);svpt(ox,oy);svxf(m,x+w-rx,y,&ox,&oy);svpt(ox,oy);
   svarc(m,x+w-rx,y,rx,ry,0,0,1,x+w,y+ry);svxf(m,x+w,y+h-ry,&ox,&oy);svpt(ox,oy);
   svarc(m,x+w,y+h-ry,rx,ry,0,0,1,x+w-rx,y+h);svxf(m,x+rx,y+h,&ox,&oy);svpt(ox,oy);
   svarc(m,x+rx,y+h,rx,ry,0,0,1,x,y+h-ry);svxf(m,x,y+ry,&ox,&oy);svpt(ox,oy);
   svarc(m,x,y+ry,rx,ry,0,0,1,x+rx,y);}
  svpaint_shape(&st);return;}
 if(seq(tg,"circle")||seq(tg,"ellipse")){
  double cx=svat(n,"cx",0,100),cy=svat(n,"cy",0,100),rx,ry;
  if(seq(tg,"circle")){rx=ry=svat(n,"r",0,100);}else{rx=svat(n,"rx",0,100);ry=svat(n,"ry",0,100);}
  if(rx<=0||ry<=0)return;
  svbegin();
  {double ox,oy;svxf(m,cx+rx,cy,&ox,&oy);svpt(ox,oy);
   svarc(m,cx+rx,cy,rx,ry,0,0,1,cx,cy+ry);svarc(m,cx,cy+ry,rx,ry,0,0,1,cx-rx,cy);
   svarc(m,cx-rx,cy,rx,ry,0,0,1,cx,cy-ry);svarc(m,cx,cy-ry,rx,ry,0,0,1,cx+rx,cy);}
  svpaint_shape(&st);return;}
 if(seq(tg,"line")){
  double ox,oy;
  svbegin();svxf(m,svat(n,"x1",0,100),svat(n,"y1",0,100),&ox,&oy);svpt(ox,oy);svxf(m,svat(n,"x2",0,100),svat(n,"y2",0,100),&ox,&oy);svpt(ox,oy);
  {int f=st.fill;st.fill=-1;svpaint_shape(&st);st.fill=f;}return;}
 if(seq(tg,"polyline")||seq(tg,"polygon")){
  S pts=getattr(n,"points");const char*p=pts.p,*e=pts.p+pts.n;int ok,first=1;double fx=0,fy=0;
  if(!pts.p)return;
  svbegin();
  for(;;){
   double x=svnum(&p,e,&ok),y,ox,oy;
   if(!ok)break;
   y=svnum(&p,e,&ok);if(!ok)break;
   if(first){fx=x;fy=y;first=0;}
   svxf(m,x,y,&ox,&oy);svpt(ox,oy);}
  if(seq(tg,"polygon")&&!first){double ox,oy;svxf(m,fx,fy,&ox,&oy);svpt(ox,oy);}
  svpaint_shape(&st);return;}
 if(seq(tg,"path")){
  S d=getattr(n,"d");
  if(!d.p)return;
  svpath(m,d);svpaint_shape(&st);return;}}

/* paint the <svg> element svg whose content box is (x,y,w,h) in document coordinates */
static void svg_paint(N*svg,int x,int y,int w,int h){
 SvS st;SvM m={1,0,0,1,0,0};double vx=0,vy=0,vw=0,vh=0,sx,sy;S vb,par;
 if(w<=0||h<=0)return;
 if(!svx){   /* scratch for flattened paths, lent on first use */
  svx=(float*)mem_get(SV_PTS*sizeof(float));svy=(float*)mem_get(SV_PTS*sizeof(float));svs=(int*)mem_get(SV_SUB*sizeof(int));svcov=(unsigned char*)mem_get(4096);
  if(!svx||!svy||!svs||!svcov){mem_rel(svx);mem_rel(svy);mem_rel(svs);mem_rel(svcov);svx=svy=0;svs=0;svcov=0;return;}}
 svroot_=svg;svbx=x;svby=y;svbw=w;svbh=h;
 st.fill=0;st.stroke=-1;st.sw=1;st.fop=st.sop=st.op=1;st.evenodd=0;st.vis=0;st.col=svg->s?svg->s->col:0;
 vb=getattr(svg,"viewbox");
 if(!vb.p)vb=getattr(svg,"viewBox");
 if(vb.p){const char*p=vb.p,*e=vb.p+vb.n;int ok;vx=svnum(&p,e,&ok);vy=svnum(&p,e,&ok);vw=svnum(&p,e,&ok);vh=svnum(&p,e,&ok);}
 if(vw>0&&vh>0){
  int slice=0,ax=1,ay=1,none=0;
  sx=w/vw;sy=h/vh;
  par=getattr(svg,"preserveaspectratio");if(!par.p)par=getattr(svg,"preserveAspectRatio");
  if(par.p){if(has_s(par,0,"none"))none=1;{int i;for(i=0;i+5<=par.n;i++){if(has_s(par,i,"xmin"))ax=0;if(has_s(par,i,"xmax"))ax=2;if(has_s(par,i,"ymin")||has_s(par,i+4,"ymin"))ay=0;if(has_s(par,i,"ymax")||has_s(par,i+4,"ymax"))ay=2;if(has_s(par,i,"slice"))slice=1;}}}
  if(!none){double s=slice?(sx>sy?sx:sy):(sx<sy?sx:sy);sx=sy=s;}
  m.a=sx;m.d=sy;
  m.e=x-vx*sx+(w-vw*sx)*ax/2.0;m.f=y-vy*sy+(h-vh*sy)*ay/2.0;}
 else{m.e=x;m.f=y;}
 {int sk0=kx0,sk1=kx1,sl0=ky0,sl1=ky1;   /* never draw outside the box */
  if(x>kx0)kx0=x;
  if(y>ky0)ky0=y;
  if(x+w<kx1)kx1=x+w;
  if(y+h<ky1)ky1=y+h;
  svkids(svg,m,st,0);
  kx0=sk0;kx1=sk1;ky0=sl0;ky1=sl1;}}
