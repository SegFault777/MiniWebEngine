/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniBrowser module: mb_ui
 * drawing: toolbar, address bar, lock badge, scrollbar, connection panel.
 * Not a standalone translation unit: minibrowser.c includes the modules in order (unity build). */
/* ---- drawing ---- */
static void secdraw(unsigned*px);
static void fillr(unsigned*px,int x,int y,int w,int h,unsigned c){mwe_rect(px,Wd,Wd,Ht,x,y,w,h,c);}
static void seg(unsigned*px,int x0,int y0,int x1,int y1,unsigned c){
 int dx=x1-x0,dy=y1-y0,n=(dx<0?-dx:dx)>(dy<0?-dy:dy)?(dx<0?-dx:dx):(dy<0?-dy:dy),i;
 if(!n)n=1;
 for(i=0;i<=n;i++)fillr(px,x0+dx*i/n,y0+dy*i/n,2,2,c);}
static void icons(unsigned*px){
 unsigned on=0x5F6368,off=0xBDC1C6,c;int cx,cy,dx,dy,d;
 c=hi>0?on:off;cx=23;cy=TB/2;
 seg(px,cx+7,cy,cx-7,cy,c);seg(px,cx-7,cy,cx-1,cy-6,c);seg(px,cx-7,cy,cx-1,cy+6,c);
 c=hi<hn-1?on:off;cx=59;
 seg(px,cx-7,cy,cx+7,cy,c);seg(px,cx+7,cy,cx+1,cy-6,c);seg(px,cx+7,cy,cx+1,cy+6,c);
 cx=95;
 for(dy=-8;dy<=8;dy++)for(dx=-8;dx<=8;dx++){
  d=dx*dx+dy*dy;
  if(d>=30&&d<=52&&!(dx>0&&dy<0&&dx>=-dy*2/3&&dx<=-dy*3))fillr(px,cx+dx,cy+dy,1,1,on);}
 seg(px,cx+3,cy-8,cx+8,cy-8,on);seg(px,cx+8,cy-8,cx+8,cy-3,on);}
static int caretx(int idx){return mwe_textw(ub,idx,14,0);}
static const char*badge(void){return sec==2?"Secure":sec==1?"Not secure":"";}
static int badgew(void){const char*b=badge();return b[0]?mwe_textw(b,len(b),12,1)+14+(sec==2?14:0):0;}
void mb_draw(unsigned*px){
 int i,cw=Wd-SBW,chh=Ht-TB-SBH,bx=120,bw=Wd-bx-10,tx,cxp,th,ty,mx,bwid,t0,av,sw;const char*bt;
 if(cw<32)cw=32;
 fillr(px,0,0,Wd,Ht,0xFFFFFF);
 fillr(px,0,0,Wd,TB,0xF1F3F4);fillr(px,0,TB-1,Wd,1,0xDADCE0);
 icons(px);
 /* address bar */
 fillr(px,bx,7,bw,TB-14,uf?0x1A73E8:0xDADCE0);fillr(px,bx+1,8,bw-2,TB-16,0xFFFFFF);
 bwid=badgew();t0=bx+10+bwid;av=bw-20-bwid;
 bt=badge();
 if(bwid){
  unsigned lc=sec==2?0x137333:0xB3261E;int lw_=sec==2?14:0,cy=TB/2;
  if(sec==2){fillr(px,bx+12,cy-9,6,2,lc);fillr(px,bx+12,cy-9,2,5,lc);fillr(px,bx+16,cy-9,2,5,lc);fillr(px,bx+10,cy-5,10,8,lc);fillr(px,bx+14,cy-3,2,3,0xFFFFFF);}
  mwe_text(px,Wd,Wd,Ht,bx+10+lw_,TB/2+4,bt,len(bt),12,1,lc,bx,bx+bw);fillr(px,t0-7,13,1,TB-26,0xDADCE0);}
 cxp=caretx(ucur);
 if(cxp-uoff>av-14)uoff=cxp-(av-14);
 if(cxp<uoff)uoff=cxp;
 if(uoff<0)uoff=0;
 tx=t0-uoff;
 sw=mwe_textw(ub,len(ub),14,0);
 if(uf&&usel)fillr(px,t0,11,sw<av?sw:av,TB-22,0xC6DAFC);
 mwe_text(px,Wd,Wd,Ht,tx,TB/2+5,ub,len(ub),14,0,0x202124,t0,bx+bw-8);
 if(uf&&!usel&&tx+cxp>=t0)fillr(px,tx+cxp,12,1,TB-24,0x202124);
 /* content */
 if(chh>0){
  mwe_paint(px+TB*Wd,Wd,0,sy,cw,chh);
  fillr(px,cw,TB,SBW,chh,0xF8F9FA);
  if(ph>chh){
   th=chh*chh/ph;if(th<24)th=24;
   mx=ph-chh;ty=TB+(mx?(chh-th)*sy/mx:0);
   fillr(px,cw+2,ty,SBW-4,th,0xBDC1C6);}}
 /* status bar */
 fillr(px,0,Ht-SBH,Wd,SBH,0xF1F3F4);fillr(px,0,Ht-SBH,Wd,1,0xDADCE0);
 mwe_text(px,Wd,Wd,Ht,10,Ht-8,stt,len(stt),12,0,0x5F6368,0,Wd-10);
 i=mwe_textw("MiniWebEngine " MWE_VERSION,sizeof("MiniWebEngine " MWE_VERSION)-1,12,0);
 if(Wd>i+300)mwe_text(px,Wd,Wd,Ht,Wd-i-10,Ht-8,"MiniWebEngine " MWE_VERSION,sizeof("MiniWebEngine " MWE_VERSION)-1,12,0,0x80868B,0,Wd);
 if(secpanel)secdraw(px);}

/* ---- connection-info panel (click the lock / "Not secure" badge) ---- */
static void fdate(char*o,long long v){
 int y=(int)(v/10000000000LL),m=(int)(v/100000000LL%100),d=(int)(v/1000000LL%100);
 o[0]='0'+y/1000%10;o[1]='0'+y/100%10;o[2]='0'+y/10%10;o[3]='0'+y%10;o[4]='-';o[5]='0'+m/10;o[6]='0'+m%10;o[7]='-';o[8]='0'+d/10;o[9]='0'+d%10;o[10]=0;}
static void secdraw(unsigned*px){
 int x=120,y=TB-2,w=Wd-x-10,yy,i;char ln[300],d1[12],d2[12];
 unsigned col=sec==2?0x137333:0xB3261E;
 const char*rows[6][2];int nr=0;
 if(w>460)w=460;
 if(w<200||!sec)return;
 if(sec==2){
  rows[nr][0]="Protocol";rows[nr++][1]=pinfo.tls13?"TLS 1.3":"TLS 1.2";
  rows[nr][0]="Encryption";rows[nr++][1]=pinfo.suite;
  rows[nr][0]="Certificate";rows[nr++][1]=pinfo.cn;
  if(pinfo.issuer[0]){rows[nr][0]="Issued by";rows[nr++][1]=pinfo.issuer;}
  rows[nr][0]="Public key";rows[nr++][1]=pinfo.key;
  fdate(d1,pinfo.nb);fdate(d2,pinfo.na);bs(ln,sizeof ln);ba(d1);ba("  to  ");ba(d2);
  rows[nr][0]="Valid";rows[nr++][1]=ln;}
 yy=sec==2?y+22+nr*20+44:y+22+58;
 fillr(px,x-1,y-1,w+2,yy-y+2,0xDADCE0);fillr(px,x,y,w,yy-y,0xFFFFFF);
 mwe_text(px,Wd,Wd,Ht,x+14,y+22,sec==2?"Connection is secure":"Connection is not secure",sec==2?20:24,14,1,col,x,x+w);
 if(sec==2){
  static const char n1[]="The certificate chain, validity period and host name were",n2[]="verified. Data sent to this site is encrypted.";
  for(i=0;i<nr;i++){
   mwe_text(px,Wd,Wd,Ht,x+14,y+46+i*20,rows[i][0],len(rows[i][0]),12,0,0x5F6368,x,x+w);
   mwe_text(px,Wd,Wd,Ht,x+104,y+46+i*20,rows[i][1],len(rows[i][1]),12,0,0x202124,x,x+w-8);}
  mwe_text(px,Wd,Wd,Ht,x+14,y+46+nr*20+14,n1,len(n1),11,0,0x5F6368,x,x+w);
  mwe_text(px,Wd,Wd,Ht,x+14,y+46+nr*20+30,n2,len(n2),11,0,0x5F6368,x,x+w);}
 else{
  static const char n3[]="Anything you send to this site can be read by others",n4[]="on the network. Do not enter passwords or personal data.";
  mwe_text(px,Wd,Wd,Ht,x+14,y+46,n3,len(n3),12,0,0x202124,x,x+w);
  mwe_text(px,Wd,Wd,Ht,x+14,y+64,n4,len(n4),12,0,0x202124,x,x+w);}}
