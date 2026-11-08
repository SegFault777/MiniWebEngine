/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine module: mwe_raster
 * paint target, blending, vector-font rasterizer, rounded boxes/gradients/shadows.
 * Not a standalone translation unit: mwe.c includes the modules in dependency order (unity build). */
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
#if MF_CJ_N
 if(g>=ZWG)p=(const u8*)"";else
 if(g>=MF_N)p=MF_CJ_D+MF_CJ_OFF[g-MF_N];else
#else
 if(g>=ZWG)p=(const u8*)"";else
#endif
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
 GC*e;u8*bm;int i,j,a,dbl=0;
#if MF_CJ_N
 if(g>=MF_N&&(tf&1)){tf&=~1;dbl=fs>=40?2:1;}  /* CJK has one weight: synthesize bold by double strike */
#endif
 e=glyph(g,fs,tf);
 if(!e->w)return;
 bm=pool+e->off;
 for(j=0;j<e->h;j++)for(i=0;i<e->w;i++)if((a=bm[j*e->w+i])){
  bp(x+e->ox+i,base+e->oy+j,col,a);
  if(dbl)bp(x+e->ox+i+dbl,base+e->oy+j,col,a);}}
static void run(int x,int base,const char*s,int n,int fs,int tf,int ls,int col,int ul,int w){
 int i=0,pre=0,g,k=0;
 while(i<n){
  g=gi(tfm(u8d(s,n,&i),tf>>2,k==0));
  gl(x+((pre*fs+512)>>10)+k*ls,base,g,fs,tf&3,col);
  pre+=ADV(tf&1,g);k++;}
 if(ul&1)fr(x,base+fs/10+1,w,fs/14+1,col);
 if(ul&2)fr(x,base-fs*3/10,w,fs/14+1,col);}
