/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine module: mwe_table
 * table layout.
 * Not a standalone translation unit: mwe.c includes the modules in dependency order (unity build). */
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
