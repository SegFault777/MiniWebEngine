/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine module: mwe_flex
 * flexbox.
 * Not a standalone translation unit: mwe.c includes the modules in dependency order (unity build). */
/* ---- flexbox ---- */
typedef struct{N*n;St*s;int ord,bas,mnf,w,h,mlA,mrA,ml,mr,mt,mb,xb,eh;}Fi;
static void flexc(N*n){
 St*s=n->s;Fi it[64];int fchc=fch,ni=0,row=(s->fdir&1)==0,rev=s->fdir>=2,gm=rl(row?s->gx:s->gy,cw),gc_=rl(row?s->gy:s->gx,cw),i,j,a,b,lim,nln=0,ls_[66],li;
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
   if(nln==1&&fchc>=0&&lineH<fchc)lineH=fchc;
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
  free=fchc>=0?fchc-tot:0;
  if(free>0&&tg>0)for(i=0;i<ni;i++)grow[i]=(int)((long long)free*it[i].s->fgrow/tg);
  for(j=0;j<ni;j++){
   i=rev?ni-1-j:j;
   {St*is=it[i].s;int ai=is->as>=0?is->as:s->ai,ax=cx,aw=cw,fhv=-1,hh;
    if(it[i].w>=0){aw=it[i].w+it[i].ml+it[i].mr;ax=ai==2?cx+cw-aw:ai==3?cx+(cw-aw)/2:cx;}
    if(grow[i]>0&&is->h.k==1)fhv=it[i].h-it[i].mt-it[i].mb+grow[i];
    hh=lay_at(it[i].n,ax,aw,Y,it[i].w,fhv,0,0);
    Y+=(fhv>=0?it[i].h+grow[i]:hh)+gm;}}
  if(ni)Y-=gm;}}
