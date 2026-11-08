/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine module: mwe_abspos
 * absolutely positioned and fixed boxes.
 * Not a standalone translation unit: mwe.c includes the modules in dependency order (unity build). */
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
