/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine module: mwe_boxes
 * box painter: borders, rounded corners, gradients, shadows, background images (the Bx command).
 * Not a standalone translation unit: mwe.c includes the modules in dependency order (unity build). */
/* ---- rounded boxes, gradients, shadows ---- */
typedef struct{int r[4],bw[4],bc[4],bg,gk,ga,gn,gc[6],gp[6],gpx[6],shx,shy,shb,shs,shc,x,y,w,h,im,of;}Bx;
/* bilinear sample of image i (0xAARRGGBB) at image-space (u,v) in pixels; returns alpha in the top byte */
static unsigned imsamp(const Im*m,float u,float v){
 int x0,y0,x1,y1,k;float fx,fy,wgt[4],a=0,r=0,g=0,b=0;unsigned p;
 u-=0.5f;v-=0.5f;
 x0=(int)(u<0?u-1:u);y0=(int)(v<0?v-1:v);fx=u-(float)x0;fy=v-(float)y0;
 x1=x0+1;y1=y0+1;
 if(x0<0)x0=0;
 if(x0>=m->w)x0=m->w-1;
 if(x1<0)x1=0;
 if(x1>=m->w)x1=m->w-1;
 if(y0<0)y0=0;
 if(y0>=m->h)y0=m->h-1;
 if(y1<0)y1=0;
 if(y1>=m->h)y1=m->h-1;
 wgt[0]=(1-fx)*(1-fy);wgt[1]=fx*(1-fy);wgt[2]=(1-fx)*fy;wgt[3]=fx*fy;
 for(k=0;k<4;k++){
  p=m->px[(long long)((k&2)?y1:y0)*m->w+((k&1)?x1:x0)];
  {float pa=(float)(p>>24)*wgt[k];a+=pa;r+=pa*(float)((p>>16)&255);g+=pa*(float)((p>>8)&255);b+=pa*(float)(p&255);}}
 if(a<=0.5f)return 0;
 return ((unsigned)(a+0.5f)<<24)|((unsigned)(r/a+0.5f)<<16)|((unsigned)(g/a+0.5f)<<8)|(unsigned)(b/a+0.5f);}
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
   if(b->im>0){
    int ix=x0+b->bw[3],iy=y0+b->bw[0],iw_=w-b->bw[1]-b->bw[3],ih_=h-b->bw[0]-b->bw[2];
    if(iw_>0&&ih_>0&&x>=ix&&x<ix+iw_&&y>=iy&&y<iy+ih_){
     const Im*m=&ims[b->im-1];
     float sx=(float)iw_/(float)m->w,sy=(float)ih_/(float)m->h,ox=0,oy=0,u,v;   /* object-fit: fill | contain | cover | none | scale-down */
     if(b->of){
      float sc=(b->of==1||b->of==4)?(sx<sy?sx:sy):b->of==2?(sx>sy?sx:sy):1.0f;
      if(b->of==4&&sc>1.0f)sc=1.0f;
      sx=sy=sc;ox=((float)iw_-(float)m->w*sc)*0.5f;oy=((float)ih_-(float)m->h*sc)*0.5f;}
     if(b->of){u=((float)(x-ix)+0.5f-ox)/sx;v=((float)(y-iy)+0.5f-oy)/sy;}
     else{u=((float)(x-ix)+0.5f)*(float)m->w/(float)iw_;v=((float)(y-iy)+0.5f)*(float)m->h/(float)ih_;}
     if(u>=0&&v>=0&&u<(float)m->w&&v<(float)m->h){
      unsigned ip=imsamp(m,u,v);
      if(ip>>24)bp(x,y,(int)(ip&0xFFFFFF),(int)((ip>>24)*(unsigned)ob/255));}}}
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
