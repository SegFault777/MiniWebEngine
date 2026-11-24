/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine module: mwe_api
 * public API (mwe.h), JavaScript glue, command-line tool.
 * Not a standalone translation unit: mwe.c includes the modules in dependency order (unity build). */
#include "mwe_js.h"
#include "mwe_forms.h"
#include "mwe_svg.h"

/* ---- public API ---- */
static void mem_doc_free(void){
 mem_rel(ar);mem_rel(cm);mem_rel(bxs);mem_rel(rls);mem_rel(pool);mem_rel(zo);mem_rel(zt);mem_rel(zdone);
 ar=0;cm=0;bxs=0;rls=0;pool=0;zo=zt=0;zdone=0;}
static int mem_doc(void){   /* the document-side buffers, lent by the host on the first mwe_load() and kept */
 if(ar)return 1;
 ar=(long long*)mem_get(ARSZ);cm=(Cmd*)mem_get((unsigned long)MC*sizeof(Cmd));bxs=(Bx*)mem_get(8192UL*sizeof(Bx));
 rls=(Rule*)mem_get((unsigned long)MR*sizeof(Rule));
 zo=(int*)mem_get((unsigned long)MC*4);zt=(int*)mem_get((unsigned long)MC*4);zdone=(char*)mem_get(MC);
 if(!ar||!cm||!bxs||!rls||!zo||!zt||!zdone){mem_doc_free();return 0;}
 return 1;}
int mwe_load(char*html,int len){
 int i;
 if(!mem_doc())return -1;   /* the host could not lend memory: nothing can be shown */
 imfreeall();
 nims=0;ipn=0;xpn=0;
 if(len>=3&&(u8)html[0]==0xEF&&(u8)html[1]==0xBB&&(u8)html[2]==0xBF){html+=3;len-=3;}  /* UTF-8 byte order mark */
 focn=0;focpos=0;ap=0;full=0;nr=0;nnot=0;rdirty=0;ttl.n=0;ttl.p=0;nss=0;nc=0;nbx=0;nclp=0;
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
/* Paint order: boxes with a higher stacking key are drawn later (and hit-tested first). Stable, so equal keys keep document order. */
static void zsort(void){
 int i,j,w,n=nc>MC?MC:nc,any=0;
 for(i=0;i<n;i++){zo[i]=i;if(cm[i].z)any=1;}
 if(!any)return;
 for(w=1;w<n;w*=2){   /* bottom-up stable merge sort of the index array */
  for(i=0;i<n;i+=2*w){
   int a=i,b=i+w<n?i+w:n,ae=b,be=i+2*w<n?i+2*w:n,k=i;
   while(a<ae&&b<be)zt[k++]=cm[zo[b]].z<cm[zo[a]].z?zo[b++]:zo[a++];
   while(a<ae)zt[k++]=zo[a++];
   while(b<be)zt[k++]=zo[b++];}
  for(i=0;i<n;i++)zo[i]=zt[i];}
 {char*done=zdone;Cmd tmp;   /* apply the permutation in place (cycle following) */
  for(i=0;i<n;i++)done[i]=0;
  for(i=0;i<n;i++){
   if(done[i]||zo[i]==i){done[i]=1;continue;}
   tmp=cm[i];j=i;
   for(;;){int s=zo[j];done[j]=1;if(s==i){cm[j]=tmp;break;}cm[j]=cm[s];j=s;}}}}
int mwe_layout(int width){
 N*c,*d;
 if(width<16)width=16;
 W=VW=width;
 restyle();
 nc=0;nbx=0;nclp=0;nf=0;nab=0;scp=0;Y=0;pend=0;LH=LA=LD=0;lx=lsx=cx=0;cw=W;lend=W;lf=0;ca=0;sp=0;bulp=0;curh=0;curop=255;curcl=0;curz=curz2=curctx=0;nsk=cursk=0;isol=0;fw=fh=-1;fva=0;fch=-1;ng=0;padp=0;tip=0;
 walk(root);nl();
 {int ey=Y+pend;doabs();Y=ey;pend=0;}
 zsort();
 {int i;for(i=0;i<nc;i++)cm[i].y0=cm[i].y;}
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
 if(nsk){   /* position: sticky - stay `top` px below the viewport top while the containing block lasts */
  int nat[64],bot[64],q;
  for(q=0;q<nsk;q++){nat[q]=1<<30;bot[q]=0;}
  for(i=0;i<nc;i++)if(cm[i].sk){q=cm[i].sk-1;if(cm[i].y0<nat[q])nat[q]=cm[i].y0;if(cm[i].y0+cm[i].h>bot[q])bot[q]=cm[i].y0+cm[i].h;}
  for(i=0;i<nc;i++)if(cm[i].sk){
   int d;q=cm[i].sk-1;
   d=y+sks[q].top-nat[q];
   if(d<0)d=0;
   if(bot[q]+d>sks[q].maxy)d=sks[q].maxy-bot[q];
   if(d<0)d=0;
   cm[i].y=cm[i].y0+d;}}
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
    if(k->tov&&k->x<kx1&&k->x+k->w>kx1){   /* text-overflow: ellipsis -- the part that does not fit is replaced by an ellipsis */
     static const char el[]="\xE2\x80\xA6";
     int ew=wid(el,3,k->fs,k->tf,k->ls),avail=kx1-k->x-ew,i=0,best=0,q;
     while(i<k->n){q=i;u8d(k->t,k->n,&q);if(wid(k->t,q,k->fs,k->tf,k->ls)>avail)break;best=q;i=q;}
     run(k->x,k->y+k->a,k->t,best,k->fs,k->tf,k->ls,k->c,k->u,k->w);
     run(k->x+wid(k->t,best,k->fs,k->tf,k->ls),k->y+k->a,el,3,k->fs,k->tf,k->ls,k->c,0,ew);
     break;}
    run(k->x,k->y+k->a,k->t,k->n,k->fs,k->tf,k->ls,k->c,k->u,k->w);break;
   case 2:b=&bxs[k->bi];b->x=k->x;b->y=k->y;b->w=k->w;b->h=k->h;pbox(b);break;
   case 4:ptri(k->x,k->y,k->w,k->u,k->c);break;
   case 5:svg_paint(k->nd,k->x,k->y,k->w,k->h);break;}}
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
static int memcmp_(const char*a,const char*b,int n);
static int imy_walk(N*n,S src){
 N*c;int y;
 if(n->tag.n==3&&seq(n->tag,"img")){S u=imsrcof(n);if(u.n==src.n&&memcmp_(u.p,src.p,u.n)==0)return n->y0;}
 for(c=n->kid;c;c=c->nx){y=imy_walk(c,src);if(y>=0)return y;}
 return -1;}
static int memcmp_(const char*a,const char*b,int n){int i;for(i=0;i<n;i++)if(a[i]!=b[i])return 1;return 0;}
int mwe_image_y(int i){   /* document y of the box showing image i (also while it is still loading), or -1 */
 int k;
 if(i<0||i>=nims)return -1;
 for(k=0;k<nbx;k++)if(bxs[k].im==i+1||bxs[k].ibi==i+1)return bxs[k].y;
 return root?imy_walk(root,ims[i].src):-1;}
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
/* ---- CLI: html -> BMP (streamed in 32-row bands) ----
 * Two builds of the same program:
 *   default:                cc -std=c99 -Os mwe.c -o mwe                      (uses the C library for files and memory)
 *   no C library at all:    cc -std=c99 -Os -DMWE_NOLIBC -ffreestanding -fno-builtin -nostdlib -static mwe.c -o mwe
 *                           (Linux x86-64 only: raw system calls, own memcpy/memset; the program links against nothing) */
#define MAXW 2048
#define BH 32
static char inbuf[IN+1];
static unsigned bd[MAXW*BH];
static u8 rb[MAXW*3+4];
static void pl(u8*p,int v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
#if defined(MWE_NOLIBC) && defined(__linux__) && defined(__x86_64__)
static long sys6(long n,long a,long b,long c,long d,long e,long f){
 long r;register long r10 __asm__("r10")=d;register long r8 __asm__("r8")=e;register long r9 __asm__("r9")=f;
 __asm__ volatile("syscall":"=a"(r):"a"(n),"D"(a),"S"(b),"d"(c),"r"(r10),"r"(r8),"r"(r9):"rcx","r11","memory");
 return r;}
static int cli_open_r(const char*p){long r=sys6(2,(long)p,0,0,0,0,0);return r<0?-1:(int)r;}
static int cli_open_w(const char*p){long r=sys6(2,(long)p,0x241,0644,0,0,0);return r<0?-1:(int)r;}
static int cli_read(int h,char*b,int n){int got=0;while(got<n){long r=sys6(0,h,(long)(b+got),n-got,0,0,0);if(r<=0)break;got+=(int)r;}return got;}
static void cli_write(int h,const void*b,int n){const char*c=(const char*)b;while(n>0){long r=sys6(1,h,(long)c,n,0,0,0);if(r<=0)break;c+=r;n-=(int)r;}}
static void cli_close(int h){sys6(3,h,0,0,0,0,0);}
static void*cli_alloc(unsigned long n){long r=sys6(9,0,(long)n,3,0x22,-1,0);return r<0&&r>-4096?0:(void*)r;}   /* anonymous mmap: zero-filled, committed lazily */
static void cli_free(void*p){(void)p;}                                                                       /* process is short-lived: the OS reclaims everything */
#define CLI_OUT 1
#define CLI_ERR 2
/* the compiler may call these even with -ffreestanding */
void*memcpy(void*d,const void*s,unsigned long n){void*r=d;__asm__ volatile("rep movsb":"+D"(d),"+S"(s),"+c"(n)::"memory");return r;}
void*memset(void*d,int c,unsigned long n){void*r=d;__asm__ volatile("rep stosb":"+D"(d),"+c"(n):"a"(c):"memory");return r;}
void*memmove(void*d,const void*s,unsigned long n){
 void*r=d;
 if((unsigned long)d<=(unsigned long)s||(unsigned long)d>=(unsigned long)s+n){__asm__ volatile("rep movsb":"+D"(d),"+S"(s),"+c"(n)::"memory");}
 else{d=(char*)d+n-1;s=(const char*)s+n-1;__asm__ volatile("std\n\trep movsb\n\tcld":"+D"(d),"+S"(s),"+c"(n)::"memory");}
 return r;}
int memcmp(const void*a,const void*b,unsigned long n){const unsigned char*x=(const unsigned char*)a,*y=(const unsigned char*)b;unsigned long i;for(i=0;i<n;i++)if(x[i]!=y[i])return x[i]-y[i];return 0;}
static int cli_main(int ac,char**av);
__attribute__((used)) static void cli_start(long*sp){
 int rc=cli_main((int)sp[0],(char**)(sp+1));
 sys6(231,rc,0,0,0,0,0);for(;;){}}
__asm__(".text\n.global _start\n_start:\n\txor %ebp,%ebp\n\tmov %rsp,%rdi\n\tand $-16,%rsp\n\tcall cli_start\n");
#else
#include <stdio.h>
#include <stdlib.h>
static FILE*cli_fh[8];
static int cli_slot(FILE*f){int i;if(!f)return -1;for(i=3;i<8;i++)if(!cli_fh[i]){cli_fh[i]=f;return i;}fclose(f);return -1;}
static int cli_open_r(const char*p){return cli_slot(fopen(p,"rb"));}
static int cli_open_w(const char*p){return cli_slot(fopen(p,"wb"));}
static int cli_read(int h,char*b,int n){return (int)fread(b,1,(size_t)n,cli_fh[h]);}
static void cli_write(int h,const void*b,int n){fwrite(b,1,(size_t)n,h==1?stdout:h==2?stderr:cli_fh[h]);}
static void cli_close(int h){if(h>=3){fclose(cli_fh[h]);cli_fh[h]=0;}}
static void*cli_alloc(unsigned long n){return calloc(1,n);}
static void cli_free(void*p){free(p);}
#define CLI_OUT 1
#define CLI_ERR 2
#endif
static int cli_main(int ac,char**av){
 int hf,n,i,r,x,w=800,rs,by,bh;u8 h[54]={'B','M'};
 mwe_set_alloc(cli_alloc,cli_free);
 if(ac==2&&av[1][0]=='-'&&av[1][1]=='-'&&av[1][2]=='l'){const char*t=mwe_licenses();int tn=0;while(t[tn])tn++;cli_write(CLI_OUT,t,tn);return 0;}
 if(ac<3){static const char u[]="MiniWebEngine v" MWE_VERSION " (font: " MF_NAME ")\nusage: mwe in.html out.bmp [width]\n       mwe --licenses\n";cli_write(CLI_ERR,u,(int)sizeof u-1);return 1;}
 if(ac>3){for(i=0,x=0;av[3][i]>='0'&&av[3][i]<='9'&&x<9999;i++)x=x*10+av[3][i]-'0';if(x>=64&&x<=MAXW)w=x;}
 if((hf=cli_open_r(av[1]))<0)return 2;
 n=cli_read(hf,inbuf,IN);cli_close(hf);
 mwe_load(inbuf,n);
 mwe_js_time(1767225600000LL,0);mwe_js_url(av[1]);
 mwe_js_run();
 for(i=0;i<=1000;i+=16)mwe_js_tick(i);
 {int ln;const char*lg=mwe_js_log(&ln);if(ln)cli_write(CLI_ERR,lg,ln);}
 n=mwe_layout(w);
 rs=(w*3+3)&~3;
 if(n>32768)n=32768;
 if((long long)rs*n>2000000000LL)n=(int)(2000000000LL/rs);
 if((hf=cli_open_w(av[2]))<0)return 3;
 pl(h+2,54+rs*n);pl(h+10,54);pl(h+14,40);pl(h+18,w);pl(h+22,-n);h[26]=1;h[28]=24;pl(h+34,rs*n);pl(h+38,2835);pl(h+42,2835);
 cli_write(hf,h,54);
 for(by=0;by<n;by+=BH){
  bh=n-by<BH?n-by:BH;
  mwe_paint(bd,w,0,by,w,bh);
  for(r=0;r<bh;r++){unsigned*q=bd+r*w;for(x=0;x<w;x++){rb[x*3]=q[x];rb[x*3+1]=q[x]>>8;rb[x*3+2]=q[x]>>16;}cli_write(hf,rb,rs);}}
 cli_close(hf);
 return 0;}
#if !(defined(MWE_NOLIBC) && defined(__linux__) && defined(__x86_64__))
int main(int ac,char**av){return cli_main(ac,av);}
#endif
#endif
