/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine module: mwe_api
 * public API (mwe.h), JavaScript glue, command-line tool.
 * Not a standalone translation unit: mwe.c includes the modules in dependency order (unity build). */
#include "mwe_js.h"

/* ---- public API ---- */
int mwe_load(char*html,int len){
 int i;
 nims=0;ipn=0;xpn=0;
 if(len>=3&&(u8)html[0]==0xEF&&(u8)html[1]==0xBB&&(u8)html[2]==0xBF){html+=3;len-=3;}  /* UTF-8 byte order mark */
 ap=0;full=0;nr=0;nnot=0;rdirty=0;ttl.n=0;ttl.p=0;nss=0;nc=0;nbx=0;nclp=0;
 for(i=0;i<4;i++)qb[i]=(i&1)?(1<<30):0;
 css((S){(char*)UA,sl(UA)});
 jready=0;jnscr=0;jlevel=0;jrec=1;
 root=parse(html,len);
 jrec=0;
 imscan(root);
 return full;}
void mwe_viewport_h(int h){VH=h<100?100:h;}
void mwe_add_css(char*text,int len){css((S){text,len});}
const char*mwe_stylesheet(int i,int*len){if(i<0||i>=nss)return 0;*len=sshref[i].n;return sshref[i].p;}
int mwe_layout(int width){
 N*c,*d;
 if(width<16)width=16;
 W=VW=width;
 restyle();
 nc=0;nbx=0;nclp=0;nf=0;nab=0;scp=0;Y=0;pend=0;LH=LA=LD=0;lx=lsx=cx=0;cw=W;lend=W;lf=0;ca=0;sp=0;bulp=0;curh=0;curop=255;curcl=0;isol=0;fw=fh=-1;fva=0;fch=-1;ng=0;padp=0;tip=0;
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
    if(k->tsx||k->tsy)run(k->x+k->tsx,k->y+k->a+k->tsy,k->t,k->n,k->fs,k->tf,k->ls,k->tsc,0,k->w);
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
int mwe_image_count(void){return nims;}
const char*mwe_image_src(int i,int*len){if(i<0||i>=nims)return 0;*len=ims[i].src.n;return ims[i].src.p;}
int mwe_image_state(int i){return i<0||i>=nims?2:ims[i].st;}
int mwe_image_set(int i,const unsigned char*data,int len){return imset(i,data,len);}
void mwe_image_fail(int i){if(i>=0&&i<nims)ims[i].st=2;}
unsigned mwe_bg(void){return(unsigned)cv;}
const char*mwe_font_name(void){return MF_NAME;}
const char*mwe_licenses(void){
 return "MiniWebEngine v" MWE_VERSION " - Copyright (C) 2026 the MiniWebEngine authors. Licensed under GPL-3.0-only.\n"
  "Embedded font outlines (derived from this font, SIL Open Font License 1.1):\n" MF_NOTICE
  "Noto Sans CJK is not sold by itself; the licence text is in fonts/OFL-NotoSansCJK.txt and NOTICE.md.\n";}
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
 if(ac==2&&av[1][0]=='-'&&av[1][1]=='-'&&av[1][2]=='l'){const char*t=mwe_licenses();int tn=0;while(t[tn])tn++;fwrite(t,1,tn,stdout);return 0;}
 if(ac<3){static const char u[]="MiniWebEngine v" MWE_VERSION " (font: " MF_NAME ")\nusage: mwe in.html out.bmp [width]\n       mwe --licenses\n";fwrite(u,1,sizeof u-1,stderr);return 1;}
 if(ac>3){for(i=0,x=0;av[3][i]>='0'&&av[3][i]<='9'&&x<9999;i++)x=x*10+av[3][i]-'0';if(x>=64&&x<=MAXW)w=x;}
 if(!(f=fopen(av[1],"rb")))return 2;
 n=(int)fread(inbuf,1,IN,f);fclose(f);
 mwe_load(inbuf,n);
 mwe_js_time(1767225600000LL,0);mwe_js_url(av[1]);
 mwe_js_run();
 for(i=0;i<=1000;i+=16)mwe_js_tick(i);
 {int ln;const char*lg=mwe_js_log(&ln);if(ln)fwrite(lg,1,ln,stderr);}
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
