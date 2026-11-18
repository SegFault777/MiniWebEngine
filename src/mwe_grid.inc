/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine module: mwe_grid
 * grid layout: fixed / fr / auto / repeat(auto-fit|auto-fill, minmax()) column tracks, auto rows (grid-auto-rows),
 * grid-template-areas + grid-area, grid-column / grid-row lines and spans, dense-less auto placement.
 * Not a standalone translation unit: mwe.c includes the modules in dependency order (unity build). */
/* ---- grid ---- */
/* area name -> 1-based row/col start and spans, looked up in the container's grid-template-areas */
static int garea_find(St*cs,S name,int*r0,int*c0,int*rs,int*cs_){
 int r,c,nc,found=0,rmax=0,cmax=0;
 for(r=0;r<cs->gtan;r++){
  S row=cs->gta[r];int i=0;c=0;
  while(i<row.n){
   int a;
   while(i<row.n&&isws(row.p[i]))i++;
   if(i>=row.n)break;
   a=i;while(i<row.n&&!isws(row.p[i]))i++;
   nc=i-a;
   if(nc==name.n){int q;for(q=0;q<nc&&row.p[a+q]==name.p[q];q++){}
    if(q==nc){if(!found){*r0=r+1;*c0=c+1;rmax=r;cmax=c;found=1;}else{if(r>rmax)rmax=r;if(c>cmax)cmax=c;}}}
   c++;}}
 if(found){*rs=rmax-*r0+2;*cs_=cmax-*c0+2;}
 return found;}
static void gridc(N*n){
 St*s=n->s;N*c,*items[64];
 int ni=0,i,j,ncol,gx=rl(s->gx,cw),gy=rl(s->gy,cw),tw_[12],tx_[12],fixedsum=0,frsum=0,rem,r,Y0=Y,nrows=0,minrh=s->gar.k==0?rl(s->gar,cw):0;
 int kind[12],cs[64],rs[64],c0[64],r0[64],col[64],row[64],hh[64],rh[66],ry[67];long long fr[12];
 static unsigned char occ[66][12];
 for(c=n->kid;c&&ni<64;c=c->nx)if(c->tag.n&&c->s&&c->s->disp!=D_NONE&&c->s->pos<2)items[ni++]=c;
 if(!ni)return;
 /* ---- column tracks ---- */
 if(s->gtauto){int mw_=s->gtmin>0?s->gtmin:1;ncol=(cw+gx)/(mw_+gx);if(ncol<1)ncol=1;if(ncol>12)ncol=12;if(s->gtauto==2&&ncol>ni)ncol=ni;for(i=0;i<ncol;i++){kind[i]=1;fr[i]=1000;}}
 else if(s->gtn>0){ncol=s->gtn;for(i=0;i<ncol;i++){kind[i]=s->gtk[i];fr[i]=s->gtk[i]==1?s->gt[i].px:1000;if(kind[i]==2)kind[i]=1;}}
 else if(s->gtan>0){   /* only grid-template-areas: as many 1fr columns as the first row names */
  S row0=s->gta[0];int k=0;i=0;
  while(i<row0.n){while(i<row0.n&&isws(row0.p[i]))i++;if(i>=row0.n)break;while(i<row0.n&&!isws(row0.p[i]))i++;k++;}
  ncol=k<1?1:k>12?12:k;for(i=0;i<ncol;i++){kind[i]=1;fr[i]=1000;}}
 else{ncol=1;kind[0]=1;fr[0]=1000;}
 for(i=0;i<ncol;i++){if(kind[i]==0){tw_[i]=rl(s->gt[i],cw);fixedsum+=tw_[i];}else frsum+=(int)fr[i];}
 rem=cw-fixedsum-gx*(ncol-1);if(rem<0)rem=0;
 for(i=0;i<ncol;i++)if(kind[i]!=0)tw_[i]=frsum?(int)((long long)rem*fr[i]/frsum):0;
 tx_[0]=cx;for(i=1;i<ncol;i++)tx_[i]=tx_[i-1]+tw_[i-1]+gx;
 /* ---- item placement: explicit (areas / lines) first, then auto ---- */
 for(i=0;i<ni;i++){
  St*is=items[i]->s;int ar0,ac0,ars,acs;
  c0[i]=is->gc0;r0[i]=is->gr0;cs[i]=is->gcs>1?is->gcs:1;rs[i]=is->grs>1?is->grs:1;
  if(is->gce>is->gc0&&is->gc0>0)cs[i]=is->gce-is->gc0;
  if(is->gre>is->gr0&&is->gr0>0)rs[i]=is->gre-is->gr0;
  if(is->garea.n&&garea_find(s,is->garea,&ar0,&ac0,&ars,&acs)){r0[i]=ar0;c0[i]=ac0;rs[i]=ars;cs[i]=acs;}
  if(c0[i]>ncol)c0[i]=ncol;
  if(cs[i]>ncol)cs[i]=ncol;
  if(c0[i]>0&&c0[i]-1+cs[i]>ncol)c0[i]=ncol-cs[i]+1;
  if(rs[i]>64)rs[i]=64;}
 for(r=0;r<66;r++)for(j=0;j<12;j++)occ[r][j]=0;
 for(i=0;i<ni;i++)col[i]=row[i]=-1;
 for(i=0;i<ni;i++)if(c0[i]>0&&r0[i]>0){   /* both lines given */
  int a,b;col[i]=c0[i]-1;row[i]=r0[i]-1;
  if(row[i]>62)row[i]=62;
  for(a=0;a<rs[i]&&row[i]+a<66;a++)for(b=0;b<cs[i];b++)occ[row[i]+a][col[i]+b]=1;}
 for(i=0;i<ni;i++)if(col[i]<0&&(c0[i]>0||r0[i]>0)){   /* one line given: the other axis is searched */
  int a,b,rr,cc,ok;
  if(c0[i]>0){cc=c0[i]-1;for(rr=0;rr<64;rr++){ok=1;for(a=0;a<rs[i]&&ok;a++)for(b=0;b<cs[i];b++)if(occ[rr+a][cc+b])ok=0;if(ok)break;}}
  else{rr=r0[i]-1;if(rr>62)rr=62;for(cc=0;cc+cs[i]<=ncol;cc++){ok=1;for(a=0;a<rs[i]&&ok;a++)for(b=0;b<cs[i];b++)if(occ[rr+a][cc+b])ok=0;if(ok)break;}if(cc+cs[i]>ncol)cc=0;}
  col[i]=cc;row[i]=rr;
  for(a=0;a<rs[i]&&row[i]+a<66;a++)for(b=0;b<cs[i];b++)occ[row[i]+a][col[i]+b]=1;}
 {int cr=0,cc=0;
  for(i=0;i<ni;i++)if(col[i]<0){
   int a,b,ok;
   for(;;){
    if(cc+cs[i]>ncol){cc=0;cr++;}
    if(cr>=62)break;
    ok=1;for(a=0;a<rs[i]&&ok;a++)for(b=0;b<cs[i];b++)if(occ[cr+a][cc+b])ok=0;
    if(ok)break;
    cc++;}
   col[i]=cc;row[i]=cr;
   for(a=0;a<rs[i]&&row[i]+a<66;a++)for(b=0;b<cs[i];b++)occ[row[i]+a][col[i]+b]=1;
   cc+=cs[i];}}
 for(i=0;i<ni;i++){if(row[i]+rs[i]>nrows)nrows=row[i]+rs[i];}
 if(nrows>64)nrows=64;
 /* ---- row heights: single-row items first, then spanning items grow their last row ---- */
 for(r=0;r<nrows;r++)rh[r]=minrh;
 for(i=0;i<ni;i++){
  St*is=items[i]->s;int w=0,ml=rl(is->m[3],cw),mr=rl(is->m[1],cw);
  for(j=0;j<cs[i];j++)w+=tw_[col[i]+j]+(j?gx:0);
  hh[i]=lay_at(items[i],tx_[col[i]],w,Y0,is->w.k==0?-1:w-ml-mr,-1,0,1);
  if(rs[i]==1&&row[i]<nrows&&hh[i]>rh[row[i]])rh[row[i]]=hh[i];}
 for(i=0;i<ni;i++)if(rs[i]>1){
  int tot=0,last=row[i]+rs[i]-1;
  if(last>=nrows)last=nrows-1;
  for(j=row[i];j<=last;j++)tot+=rh[j]+(j>row[i]?gy:0);
  if(hh[i]>tot)rh[last]+=hh[i]-tot;}
 ry[0]=0;for(r=0;r<nrows;r++)ry[r+1]=ry[r]+rh[r]+(r+1<nrows?gy:0);
 /* ---- final placement ---- */
 for(i=0;i<ni;i++){
  St*is=items[i]->s;int w=0,ml=rl(is->m[3],cw),mr=rl(is->m[1],cw),mt=rl(is->m[0],cw),mb=rl(is->m[2],cw),ai=is->as>=0?is->as:s->ai,yoff=0,fhv=-1,area=0,last=row[i]+rs[i]-1;
  if(last>=nrows)last=nrows-1;
  for(j=0;j<cs[i];j++)w+=tw_[col[i]+j]+(j?gx:0);
  area=ry[last+1]-ry[row[i]]-(last+1<nrows?gy:0);
  if(ai==0&&is->h.k==1)fhv=area-mt-mb;else if(ai==2)yoff=area-hh[i];else if(ai==3)yoff=(area-hh[i])/2;
  if(yoff<0)yoff=0;
  lay_at(items[i],tx_[col[i]],w,Y0+ry[row[i]]+yoff,is->w.k==0?-1:w-ml-mr,fhv,0,0);}
 Y=Y0+ry[nrows];}
