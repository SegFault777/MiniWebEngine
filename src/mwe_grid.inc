/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine module: mwe_grid
 * grid layout.
 * Not a standalone translation unit: mwe.c includes the modules in dependency order (unity build). */
/* ---- grid (auto-placement, fixed / fr / auto tracks, span) ---- */
static void gridc(N*n){
 St*s=n->s;N*c,*items[64];int ni=0,i,j,ncol,gx=rl(s->gx,cw),gy=rl(s->gy,cw),tw_[12],tx_[12],fixedsum=0,frsum=0,rem,span[64],col[64],row[64],r,maxr=0;
 int kind[12];long long fr[12];
 for(c=n->kid;c&&ni<64;c=c->nx)if(c->tag.n&&c->s&&c->s->disp!=D_NONE&&c->s->pos<2)items[ni++]=c;
 if(!ni)return;
 if(s->gtauto){int mw_=s->gtmin>0?s->gtmin:1;ncol=(cw+gx)/(mw_+gx);if(ncol<1)ncol=1;if(ncol>12)ncol=12;for(i=0;i<ncol;i++){kind[i]=1;fr[i]=1000;}}
 else if(s->gtn>0){ncol=s->gtn;for(i=0;i<ncol;i++){kind[i]=s->gtk[i];fr[i]=s->gtk[i]==1?s->gt[i].px:1000;if(kind[i]==2)kind[i]=1;}}
 else{ncol=1;kind[0]=1;fr[0]=1000;}
 for(i=0;i<ncol;i++){if(kind[i]==0){tw_[i]=rl(s->gt[i],cw);fixedsum+=tw_[i];}else frsum+=(int)fr[i];}
 rem=cw-fixedsum-gx*(ncol-1);if(rem<0)rem=0;
 for(i=0;i<ncol;i++)if(kind[i]!=0)tw_[i]=frsum?(int)((long long)rem*fr[i]/frsum):0;
 tx_[0]=cx;for(i=1;i<ncol;i++)tx_[i]=tx_[i-1]+tw_[i-1]+gx;
 {int cc=0,rr=0;
  for(i=0;i<ni;i++){
   int sp_=items[i]->s->gcs>1?items[i]->s->gcs:1;
   if(sp_>ncol)sp_=ncol;
   if(cc+sp_>ncol){cc=0;rr++;}
   col[i]=cc;span[i]=sp_;row[i]=rr;cc+=sp_;if(cc>=ncol){cc=0;rr++;}
   if(row[i]>maxr)maxr=row[i];}}
 for(r=0;r<=maxr;r++){
  int rh=0,hh[64];
  for(i=0;i<ni;i++)if(row[i]==r){
   St*is=items[i]->s;int w=0,ml=rl(is->m[3],cw),mr=rl(is->m[1],cw);
   for(j=0;j<span[i];j++)w+=tw_[col[i]+j]+(j?gx:0);
   hh[i]=lay_at(items[i],tx_[col[i]],w,Y,is->w.k==0?-1:w-ml-mr,-1,0,1);
   if(hh[i]>rh)rh=hh[i];}
  for(i=0;i<ni;i++)if(row[i]==r){
   St*is=items[i]->s;int w=0,ml=rl(is->m[3],cw),mr=rl(is->m[1],cw),mt=rl(is->m[0],cw),mb=rl(is->m[2],cw),ai=is->as>=0?is->as:s->ai,yoff=0,fhv=-1;
   for(j=0;j<span[i];j++)w+=tw_[col[i]+j]+(j?gx:0);
   if(ai==0&&is->h.k==1)fhv=rh-mt-mb;else if(ai==2)yoff=rh-hh[i];else if(ai==3)yoff=(rh-hh[i])/2;
   if(yoff<0)yoff=0;
   lay_at(items[i],tx_[col[i]],w,Y+yoff,is->w.k==0?-1:w-ml-mr,fhv,0,0);}
  Y+=rh+(r<maxr?gy:0);}}
