/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine module: mwe_layout
 * layout state, measuring, block and inline layout, list markers.
 * Not a standalone translation unit: mwe.c includes the modules in dependency order (unity build). */
/* ---- layout state and paint list ---- */
typedef struct{int k,x,y,w,h,a,c,bg,fs,tf,u,n,hn,vs,ls,op,cl,bi,grp,ex,tsx,tsy,tsc;char*t,*hp;N*nd,*on;}Cmd;
static Cmd cm[MC],dm;
static int nc,W,H,cv=0xffffff;
static int Y,pend,lx,lsx,cx,cw,LH,LA,LD,lf,ca,sp,lend,padp,tip;
static Cmd*bulp;static S*curh;
static int curop=255,curcl=0,isol,fw=-1,fh=-1,fva,fch=-1,ng,ibA[64];
typedef struct{int x,y,w,h,side;}Fl;static Fl flst[64];static int nf;
typedef struct{int x0,y0,x1,y1,par;}Clp;static Clp clps[512];static int nclp;
typedef struct{N*n,*cb;int sx,sy;}Ab;static Ab abq[64];static int nab;
static N*curn;
static Cmd*cadd(int k,int x,int y,int w,int h,int c){
 Cmd*p=nc<MC?&cm[nc++]:&dm;
 p->k=k;p->x=x;p->y=y;p->w=w;p->h=h;p->a=0;p->c=c;p->bg=-1;p->fs=16;p->tf=p->u=p->n=p->hn=p->vs=p->ls=p->bi=p->grp=p->ex=0;
 p->op=curop;p->cl=curcl;p->t=p->hp=0;p->nd=0;p->on=curn;return p;}
static int rc(int c,St*s){return c==-3?s->col:c;}
static void fbounds(int y,int h,int*l,int*r){
 int i;
 *l=cx;*r=cx+cw;
 for(i=0;i<nf;i++)if(y<flst[i].y+flst[i].h&&y+(h>0?h:1)>flst[i].y){
  if(flst[i].side==1){if(flst[i].x+flst[i].w>*l)*l=flst[i].x+flst[i].w;}
  else if(flst[i].x<*r)*r=flst[i].x;}}
static int fbottom(int mask,int from){
 int i,b=0;
 for(i=from;i<nf;i++)if((mask>>(flst[i].side-1))&1&&flst[i].y+flst[i].h>b)b=flst[i].y+flst[i].h;
 return b;}
static void nl(void){
 int i,dx,sl_;
 if(!LH){lx=cx;return;}
 sl_=lend-lx;dx=ca==1?sl_/2:ca==2?sl_:0;if(dx<0)dx=0;
 for(i=lf;i<nc;i++){
  if(cm[i].grp<0)continue;
  cm[i].x+=dx;
  if(cm[i].grp>0)cm[i].y+=LA-ibA[cm[i].grp-1];
  else cm[i].y=Y+LA-cm[i].a-cm[i].vs;}
 Y+=LH;LH=LA=LD=0;lx=cx;lf=nc;sp=0;ng=0;}
static void ls(int need){
 int l,r,i,nb,tries;
 if(LH)return;
 Y+=pend;pend=0;
 for(tries=0;tries<24;tries++){
  fbounds(Y,1,&l,&r);
  if(r-l>=need||!nf)break;
  nb=0;
  for(i=0;i<nf;i++)if(Y<flst[i].y+flst[i].h&&(!nb||flst[i].y+flst[i].h<nb))nb=flst[i].y+flst[i].h;
  if(nb<=Y)break;
  Y=nb;}
 lx=lsx=l+padp;padp=0;lend=r;if(tip){lx+=tip;tip=0;}
 if(bulp){bulp->y=bulp->k==1?Y:Y+bulp->a-bulp->h-bulp->fs/10;bulp=0;}}
static void mt0(St*s,int*as,int*de){
 int fs=s->fs,L,hf;
 *as=(fs*MF_ASC+512)>>10;*de=(fs*MF_DESC+512)>>10;
 if(s->lh>0){L=fs*s->lh/100;hf=(L-*as-*de)/2;*as+=hf;*de=L-*as;if(*de<0)*de=0;}}
static void met(St*s,int*a,int*h){
 int as,de;
 mt0(s,&as,&de);*a=as;*h=as+de;
 if(as>LA)LA=as;
 if(de>LD)LD=de;
 LH=LA+LD;}
static int ttf(St*s){return s->bold|(s->it<<1)|(s->tt<<2);}
static void word(char*p,int n,N*e){
 St*s=e->s;int fs=s->fs,tf=ttf(s),ww=wid(p,n,fs,tf,s->ls),a,h,spw;Cmd*c;
 spw=(sp&&LH)?((ADV(tf&1,0)*fs+512)>>10)+s->ls:0;
 if(LH&&s->ws!=1&&lx+spw+ww>lend){nl();spw=0;}
 ls(ww);lx+=spw;
 met(s,&a,&h);
 c=cadd(1,lx,0,ww,h,s->vis?-1:rc(s->col,s));
 c->on=e;c->a=a;c->bg=s->disp==D_INLINE&&!s->vis?rc(s->bg,s):-1;c->fs=fs;c->tf=tf;c->u=s->ul;c->tsx=s->tsx;c->tsy=s->tsy;c->tsc=s->tsc;c->t=p;c->n=n;c->vs=s->va;c->ls=s->ls;
 if(curh){c->hp=curh->p;c->hn=curh->n;}
 lx+=ww;sp=0;}
static void txtc(N*n,char*p,int len){ /* collapsed-whitespace text */
 St*s=n->par->s;int i=0,j,e,k,m,q,a,fs=s->fs,tf=ttf(s);long long acc;
 while(i<len){
  if(isws(p[i])){
   if(s->ws==3&&p[i]==' '){e=i;while(e<len&&p[e]==' ')e++;word(p+i,e-i,n->par);i=e;sp=0;continue;} /* pre-wrap keeps spaces */
   sp=1;i++;continue;}
  e=i;while(e<len&&!isws(p[e]))e++;
  while(i<e){
   j=cjseg(p,i,e,s->wb);
   while(i<j){
    acc=0;k=m=i;
    while(k<j){q=k;a=ADV(tf&1,gi(tfm(u8d(p,j,&q),tf>>2,k==i)));if(m>i&&cw>0&&s->ws!=1&&(((acc+a)*fs+512)>>10)>cw)break;acc+=a;k=q;m=q;}
    word(p+i,m-i,n->par);i=m;}}}}
static void lbrk(St*s){int a,h;ls(0);met(s,&a,&h);nl();}
static void txt(N*n){
 S t=n->txt;St*s=n->par->s;int i=0,j;
 if(s->ws>=2){ /* pre, pre-wrap, pre-line: newlines are line breaks */
  while(i<=t.n){
   j=i;while(j<t.n&&t.p[j]!='\n')j++;
   if(j>i){
    if(s->ws>=3)txtc(n,t.p+i,j-i);
    else{sp=0;word(t.p+i,j-i,n->par);}}
   if(j<t.n)lbrk(s);
   i=j+1;}}
 else txtc(n,t.p,t.n);}

/* ---- measuring: min-content / max-content widths (border-box, no margins) ---- */
static void blk(N*n,int ax,int aw);
static void walk(N*n);
static void flexc(N*n);
static void gridc(N*n);
static void tablec(N*n);
static int lay_at(N*c,int ax,int aw,int y,int fwv,int fhv,int fvv,int roll);
static void mmbox(N*c,int*mn,int*mx);
static void mm(N*n,int*mn,int*mx);
static int ex_(St*s,int aw){
 int a=rl(s->p[1],aw),b=rl(s->p[3],aw);
 if(a<0)a=0;
 if(b<0)b=0;
 return a+b+s->bw[1]+s->bw[3];}
static int mlr(St*s,int aw){return rl(s->m[3],aw)+rl(s->m[1],aw);}
typedef struct{N*n;int r,c,cs,rs,mn,mx,w,h;}Tc;
typedef struct{N*rows[128];int nrows;Tc cl[256];int ncl;int ncols;int cmn[16],cmx[16],cwd[16],rh[128];}Tb;
static void tbuild(N*n,Tb*t){
 N*c,*g;int r,col,cs,rs,i,j,v;unsigned char occ[128][16];
 t->nrows=t->ncl=t->ncols=0;
 for(c=n->kid;c;c=c->nx){
  if(!c->tag.n||!c->s)continue;
  if(c->s->disp==D_ROW&&t->nrows<128)t->rows[t->nrows++]=c;
  else if(c->s->disp==D_RG)for(g=c->kid;g;g=g->nx)if(g->tag.n&&g->s&&g->s->disp==D_ROW&&t->nrows<128)t->rows[t->nrows++]=g;}
 for(i=0;i<t->nrows;i++)for(j=0;j<16;j++)occ[i][j]=0;
 for(r=0;r<t->nrows;r++){
  col=0;
  for(c=t->rows[r]->kid;c;c=c->nx){
   if(!c->tag.n||!c->s||c->s->disp!=D_CELL||t->ncl>=256)continue;
   while(col<16&&occ[r][col])col++;
   if(col>=16)break;
   cs=numat(c,"colspan",0);if(cs<1)cs=1;
   rs=numat(c,"rowspan",0);if(rs<1)rs=1;
   if(col+cs>16)cs=16-col;
   if(r+rs>t->nrows)rs=t->nrows-r;
   for(i=0;i<rs;i++)for(j=0;j<cs;j++)occ[r+i][col+j]=1;
   t->cl[t->ncl].n=c;t->cl[t->ncl].r=r;t->cl[t->ncl].c=col;t->cl[t->ncl].cs=cs;t->cl[t->ncl].rs=rs;
   t->ncl++;
   if(col+cs>t->ncols)t->ncols=col+cs;
   col+=cs;}}
 (void)v;}
static void tmeas(Tb*t){
 int i,j,sm,sx;Tc*c;
 for(i=0;i<16;i++)t->cmn[i]=t->cmx[i]=0;
 for(i=0;i<t->ncl;i++){c=&t->cl[i];mmbox(c->n,&c->mn,&c->mx);
  if(c->cs==1){if(c->mn>t->cmn[c->c])t->cmn[c->c]=c->mn;if(c->mx>t->cmx[c->c])t->cmx[c->c]=c->mx;}}
 for(i=0;i<t->ncl;i++){c=&t->cl[i];
  if(c->cs>1){
   sm=sx=0;for(j=0;j<c->cs;j++){sm+=t->cmn[c->c+j];sx+=t->cmx[c->c+j];}
   if(sm<c->mn)t->cmn[c->c+c->cs-1]+=c->mn-sm;
   if(sx<c->mx)t->cmx[c->c+c->cs-1]+=c->mx-sx;}}}
static void tdist(Tb*t,int avail,int fixed){
 int i,sm=0,sx=0,x,ex;
 for(i=0;i<t->ncols;i++){sm+=t->cmn[i];sx+=t->cmx[i];}
 for(i=0;i<t->ncols;i++){
  if(sx<=avail){ex=fixed&&sx>0?(int)((long long)(avail-sx)*t->cmx[i]/sx):0;t->cwd[i]=t->cmx[i]+ex;}
  else if(sm>=avail||sx<=sm)t->cwd[i]=t->cmn[i];
  else t->cwd[i]=t->cmn[i]+(int)((long long)(t->cmx[i]-t->cmn[i])*(avail-sm)/(sx-sm));}
 if(fixed&&sx<=avail&&t->ncols){x=0;for(i=0;i<t->ncols;i++)x+=t->cwd[i];t->cwd[t->ncols-1]+=avail-x;}}
static void mmtable(N*n,int*mn,int*mx){
 Tb t;int i,sp_=n->s->bcol?0:n->s->bsp,a=0,b=0;
 tbuild(n,&t);tmeas(&t);
 for(i=0;i<t.ncols;i++){a+=t.cmn[i];b+=t.cmx[i];}
 *mn=a+sp_*(t.ncols+1);*mx=b+sp_*(t.ncols+1);}
static void mmflex(N*n,int*mn,int*mx){
 St*s=n->s;N*c;int a,b,row=(s->fdir&1)==0,sa=0,sb=0,ma=0,mb=0,cnt=0,gm=rl(s->gx,0);
 for(c=n->kid;c;c=c->nx){
  if(!c->tag.n||!c->s||c->s->disp==D_NONE||c->s->pos>=2)continue;
  mmbox(c,&a,&b);a+=mlr(c->s,0);b+=mlr(c->s,0);
  sa+=a;sb+=b;if(a>ma)ma=a;if(b>mb)mb=b;cnt++;}
 if(row){*mx=sb+gm*(cnt>1?cnt-1:0);*mn=s->fwrap?ma:sa+gm*(cnt>1?cnt-1:0);}
 else{*mx=mb;*mn=ma;}}
static void mmgrid(N*n,int*mn,int*mx){
 St*s=n->s;N*c;int a,b,ma=0,mb=0,ncol=s->gtn>0?s->gtn:1,gx=rl(s->gx,0),fixed=0,i;
 for(c=n->kid;c;c=c->nx){
  if(!c->tag.n||!c->s||c->s->disp==D_NONE||c->s->pos>=2)continue;
  mmbox(c,&a,&b);if(a>ma)ma=a;if(b>mb)mb=b;}
 if(s->gtauto){*mn=s->gtmin;*mx=mb;return;}
 for(i=0;i<s->gtn;i++)if(s->gtk[i]==0)fixed+=rl(s->gt[i],0);
 *mn=ma*ncol+gx*(ncol-1);*mx=(s->gtn&&fixed)?fixed+gx*(ncol-1):mb*ncol+gx*(ncol-1);}
static int minw(const char*p,int n,int fs,int tf,int ls,int wb){
 int i=0,j,m=0,w;
 while(i<n){j=cjseg(p,i,n,wb);w=wid(p+i,j-i,fs,tf,ls);if(w>m)m=w;i=j;}
 return m;}
static void mm(N*n,int*mn,int*mx){
 int lmn=0,lmx=0,bmn=0,bmx=0,spp=0,i,j,w,a,b,ex,spw,tf,fs;N*c;St*s,*ps=n->s;S t;
 if(!ps){*mn=*mx=0;return;}
 if(n->tag.n==3&&seq(n->tag,"img")&&(i=imfor(n))>=0){   /* replaced element: intrinsic size (or from an explicit height) */
  w=ims[i].w;if(ps->h.k==0&&!ps->h.pc)w=(int)((float)ps->h.px*(float)ims[i].w/(float)ims[i].h+0.5f);
  *mn=*mx=w;return;}
 fs=ps->fs;tf=ttf(ps);spw=((ADV(tf&1,0)*fs+512)>>10)+ps->ls;
 for(c=n->kid;c;c=c->nx){
  if(!c->tag.n){
   t=c->txt;i=0;
   while(i<t.n){
    if(isws(t.p[i])){
     if(ps->ws>=2&&ps->ws!=4&&t.p[i]=='\n'){if(lmx>bmx)bmx=lmx;if(lmn>bmn)bmn=lmn;lmx=lmn=0;}
     else spp=1;
     i++;continue;}
    j=i;while(j<t.n&&!isws(t.p[j]))j++;
    w=wid(t.p+i,j-i,fs,tf,ps->ls);
    lmx+=(spp&&lmx?spw:0)+w;{int mw=minw(t.p+i,j-i,fs,tf,ps->ls,ps->wb);if(mw>lmn)lmn=mw;}spp=0;i=j;}
   continue;}
  s=c->s;
  if(!s||s->disp==D_NONE||s->pos>=2)continue;
  if(seq(c->tag,"br")){if(lmx>bmx)bmx=lmx;if(lmn>bmn)bmn=lmn;lmx=lmn=0;spp=0;continue;}
  if(s->disp==D_INLINE&&!s->fl){
   mm(c,&a,&b);ex=mlr(s,0)+rl(s->p[1],0)+rl(s->p[3],0)+s->bw[1]+s->bw[3];
   lmx+=b+ex;if(a+ex>lmn)lmn=a+ex;continue;}
  mmbox(c,&a,&b);ex=mlr(s,0);
  if(s->disp==D_IB||s->fl){lmx+=b+ex;if(a+ex>lmn)lmn=a+ex;}
  else{
   if(lmx>bmx)bmx=lmx;
   if(lmn>bmn)bmn=lmn;
   lmx=lmn=0;
   if(b+ex>bmx)bmx=b+ex;
   if(a+ex>bmn)bmn=a+ex;}}
 *mn=bmn>lmn?bmn:lmn;*mx=bmx>lmx?bmx:lmx;
 if(ps->ws==1||ps->ws==2)*mn=*mx;}
static void mmbox(N*c,int*mn,int*mx){
 St*s=c->s;int a,b,eh=ex_(s,0),w;
 if(s->disp==D_TABLE)mmtable(c,&a,&b);
 else if(s->fx)mmflex(c,&a,&b);
 else if(s->gr)mmgrid(c,&a,&b);
 else mm(c,&a,&b);
 if(s->w.k==0&&!s->w.pc){w=s->w.px-(s->bs?eh:0);if(w<0)w=0;a=b=w;}
 *mn=a+eh;*mx=b+eh;}

/* ---- laying a box out at a given place (used by flex, grid, table, inline-block, float) ---- */
static int lay_at(N*c,int ax,int aw,int y,int fwv,int fhv,int fvv,int roll){
 int sY=Y,sP=pend,scx=cx,scw=cw,sca=ca,slx=lx,sLH=LH,sLA=LA,sLD=LD,slf=lf,ssp=sp,snc=nc,snb=nbx,snl=nclp,snf=nf,sab=nab,sng=ng,slend=lend,slsx=lsx,sop=curop,scl=curcl,spad=padp,h;
 Cmd*sb=bulp;S*sh=curh;S hr;
 Y=y;pend=0;bulp=0;padp=0;LH=LA=LD=0;
 if(seq(c->tag,"a")&&c->href.n)curh=&c->href;
 fw=fwv;fh=fhv;fva=fvv;isol=1;
 blk(c,ax,aw);
 h=Y+pend-y;
 (void)hr;
 Y=sY;pend=sP;cx=scx;cw=scw;ca=sca;lx=slx;LH=sLH;LA=sLA;LD=sLD;lf=slf;sp=ssp;bulp=sb;curh=sh;nf=snf;ng=sng;lend=slend;lsx=slsx;curop=sop;curcl=scl;padp=spad;
 if(roll){nc=snc;nbx=snb;nclp=snl;nab=sab;}
 return h;}
static void ibox(N*c){
 St*s=c->s;int mn,mx,eh,wb,hh,ml,mr,mb,pb,bb,spw,outer,c0,a,d,i,g,tf;
 mmbox(c,&mn,&mx);
 eh=ex_(s,cw);ml=rl(s->m[3],cw);mr=rl(s->m[1],cw);mb=rl(s->m[2],cw);pb=rl(s->p[2],cw);bb=s->bw[2];
 if(s->w.k==0)wb=rl(s->w,cw)+(s->bs?0:eh);
 else{wb=cw-ml-mr;if(wb>mx)wb=mx;if(wb<mn)wb=mn;}
 if(s->mxw.k==0&&!s->mxw.pc){i=s->mxw.px+(s->bs?0:eh);if(wb>i)wb=i;}
 if(s->mnw.k==0&&!s->mnw.pc){i=s->mnw.px+(s->bs?0:eh);if(wb<i)wb=i;}
 if(wb<eh)wb=eh;
 outer=wb+ml+mr;
 hh=lay_at(c,0,outer,0,wb,-1,0,1);
 tf=ttf(s);
 spw=(sp&&LH)?((ADV(tf&1,0)*s->fs+512)>>10):0;
 if(LH&&s->ws!=1&&lx+spw+outer>lend){nl();spw=0;}
 ls(outer);lx+=spw;
 c0=nc;
 lay_at(c,lx,outer,Y,wb,-1,0,0);
 a=hh-mb-pb-bb-((s->fs*MF_DESC)>>10);
 if(a<0)a=0;
 if(a>hh)a=hh;
 if(ng<63){ng++;ibA[ng-1]=a;g=ng;}else g=0;
 for(i=c0;i<nc;i++)if(cm[i].grp>=0)cm[i].grp=g;
 d=hh-a;if(a>LA)LA=a;if(d>LD)LD=d;LH=LA+LD;
 lx+=outer;sp=0;}
static void place_float(N*c){
 St*s=c->s;int mn,mx,ml,mr,eh,wb,hh,x,y,l,r,outer,i,tries,nb,c0;
 mmbox(c,&mn,&mx);
 eh=ex_(s,cw);ml=rl(s->m[3],cw);mr=rl(s->m[1],cw);
 if(s->w.k==0)wb=rl(s->w,cw)+(s->bs?0:eh);
 else{wb=cw-ml-mr;if(wb>mx)wb=mx;if(wb<mn)wb=mn;}
 if(wb<eh)wb=eh;
 outer=wb+ml+mr;
 hh=lay_at(c,0,outer,0,wb,-1,0,1);
 if(!LH){Y+=pend;pend=0;}
 y=Y;
 if(s->clr){i=fbottom(s->clr,0);if(i>y)y=i;}
 for(tries=0;tries<30;tries++){
  fbounds(y,hh,&l,&r);
  if(r-l>=outer||!nf)break;
  nb=0;
  for(i=0;i<nf;i++)if(y<flst[i].y+flst[i].h&&(!nb||flst[i].y+flst[i].h<nb))nb=flst[i].y+flst[i].h;
  if(nb<=y)break;
  y=nb;}
 x=s->fl==1?l:r-outer;
 if(x<cx&&s->fl==1)x=cx;
 c0=nc;
 lay_at(c,x,outer,y,wb,-1,0,0);
 for(i=c0;i<nc;i++)cm[i].grp=-1;
 if(nf<64){flst[nf].x=x;flst[nf].y=y;flst[nf].w=outer;flst[nf].h=hh;flst[nf].side=s->fl;nf++;}}
static void walk(N*n){
 N*c;St*s;int a,h,op0,open,closed=0,lpx,rpx;S*old;
 if(seq(n->tag,"details")){open=(getattr(n,"open").p!=0)^(n->det&1);closed=!open;}
 for(c=n->kid;c;c=c->nx){
  if(!c->tag.n){if(!closed)txt(c);continue;}
  s=c->s;
  if(!s||s->disp==D_NONE)continue;
  if(closed&&!seq(c->tag,"summary"))continue;
  if(s->pos>=2){
   if(nab<64){N*p=c->par,*cb=0;
    while(p&&p!=root){if(p->s&&p->s->pos>=1&&p->s->disp!=D_INLINE){cb=p;break;}p=p->par;}
    if(s->pos==3)cb=0;
    abq[nab].n=c;abq[nab].cb=cb;abq[nab].sx=cx;abq[nab].sy=Y+(LH?0:pend);nab++;}
   continue;}
  old=curh;if(c->href.n&&seq(c->tag,"a"))curh=&c->href;
  op0=curop;if(s->op<255)curop=curop*s->op/255;
  if(seq(c->tag,"br")){lbrk(s);}
  else if(s->fl)place_float(c);
  else if(s->disp==D_IB)ibox(c);
  else if(s->disp==D_INLINE){
   lpx=rl(s->m[3],cw)+rl(s->p[3],cw)+s->bw[3];rpx=rl(s->m[1],cw)+rl(s->p[1],cw)+s->bw[1];
   if(lpx>0){if(LH)lx+=lpx;else padp+=lpx;}
   {N*svn=curn;curn=c;walk(c);curn=svn;}
   if(rpx>0&&LH)lx+=rpx;}
  else blk(c,cx,cw);
  curh=old;curop=op0;}
 (void)a;(void)h;}

/* ---- list markers ---- */
static char*mkroman(int n,int up,int*len){
 static const int V[]={1000,900,500,400,100,90,50,40,10,9,5,4,1};
 static const char*R[]={"m","cm","d","cd","c","xc","l","xl","x","ix","v","iv","i"};
 char*o=sal(24);int i,k=0,j;
 if(n<1||n>3999){o[0]='?';*len=1;return o;}
 for(i=0;i<13;i++)while(n>=V[i]&&k<18){for(j=0;R[i][j];j++)o[k++]=up?(char)(R[i][j]-32):R[i][j];n-=V[i];}
 *len=k;return o;}
static char*mktxt(N*li,int lst,int*len){
 int idx=1,i,k=0;N*p=li->par,*c;S v;char*o,tmp[16];
 if(p&&p!=root){v=getattr(p,"start");if(v.p&&v.n&&isdg(v.p[0])){idx=0;for(i=0;i<v.n&&isdg(v.p[i]);i++)if(idx<1000000)idx=idx*10+v.p[i]-'0';}}
 for(c=li->pv;c;c=c->pv)if(c->s&&c->s->disp==D_LI)idx++;
 v=getattr(li,"value");
 if(v.p&&v.n&&isdg(v.p[0])){idx=0;for(i=0;i<v.n&&isdg(v.p[i]);i++)if(idx<1000000)idx=idx*10+v.p[i]-'0';}
 if(lst==10||lst==11){o=mkroman(idx,lst==11,len);o[*len]='.';*len+=1;return o;}
 o=sal(24);
 if(lst==6||lst==7){
  int n=idx>0?idx:1,t=0;char b[8];
  while(n>0&&t<7){n--;b[t++]=(char)((lst==6?'a':'A')+n%26);n/=26;}
  while(t>0)o[k++]=b[--t];
  o[k++]='.';*len=k;return o;}
 i=0;{int n=idx<0?0:idx;do{tmp[i++]=(char)('0'+n%10);n/=10;}while(n&&i<12);}
 if(lst==5&&i<2)tmp[i++]='0';
 while(i>0)o[k++]=tmp[--i];
 o[k++]='.';*len=k;return o;}

/* ---- block layout ---- */
static void bside(int st,int x,int y,int w,int h,int c,int horiz){
 int th=horiz?h:w,len=horiz?w:h,seg,p,gap;
 if(c<0||w<=0||h<=0)return;
 if(st==1||st==0){cadd(0,x,y,w,h,c);return;}
 seg=st==2?th*3:th;gap=st==2?th*2:th;if(seg<1)seg=1;if(gap<1)gap=1;
 for(p=0;p<len;p+=seg+gap){int l=len-p<seg?len-p:seg;if(horiz)cadd(0,x+p,y,l,h,c);else cadd(0,x,y+p,w,l,c);}}
static void blk_(N*n,int ax,int aw);
static void blk(N*n,int ax,int aw){N*sv=curn;curn=n;blk_(n,ax,aw);curn=sv;}
static void blk_(N*n,int ax,int aw){
 St*s=n->s;int fwv=fw,fhv=fh,fvv=fva,iso=isol,d=s->disp,ii=-1;float ar;St sc;
 if(n->tag.n==3&&seq(n->tag,"img")&&(ii=imfor(n))>=0){   /* decoded image: drop the UA placeholder box (grey fill, 1px border, 16px minimum) */
  sc=*s;s=&sc;
  if(s->bg==0xF1F1F1){s->bg=-1;for(ar=0;ar<4;ar++)s->bw[(int)ar]=0;s->mnw=mk(0,0,1);s->mnh=mk(0,0,1);}}
 ar=ii>=0?(float)ims[ii].h/(float)ims[ii].w:s->ar;
 int ml,mr,mt,mb,pl,pr,pt,pb,bl,br,bt,bb,eh,ev,w,box,cplx,bfc,tt,bt_,top,x0,tw,ys,i,nf0=nf,c0=nc,c1,hcnt=-1,autoh,fin,bh;
 int ocx=cx,ocw=cw,oca=ca,oop=curop,ocl=curcl,as,de,mlA=s->m[3].k==1,mrA=s->m[1].k==1,rad=0,shd=0,cli=-1,lim,fr_;
 Cmd*bp=0;Bx*bx=0;
 fw=fh=-1;fva=0;isol=0;
 mt=rl(s->m[0],aw);mb=rl(s->m[2],aw);ml=rl(s->m[3],aw);mr=rl(s->m[1],aw);
 pl=rl(s->p[3],aw);pr=rl(s->p[1],aw);pt=rl(s->p[0],aw);pb=rl(s->p[2],aw);
 pl=pl<0?0:pl;pr=pr<0?0:pr;pt=pt<0?0:pt;pb=pb<0?0:pb;
 bl=s->bw[3];br=s->bw[1];bt=s->bw[0];bb=s->bw[2];
 eh=pl+pr+bl+br;ev=pt+pb+bt+bb;
 bfc=s->ov||s->fl||d==D_IB||d==D_CELL||d==D_TABLE||iso||s->fx||s->gr||s->pos>=2;
 nl();
 if(s->clr&&!iso){i=fbottom(s->clr,0);if(i>Y+pend){Y=i;pend=0;}}
 if(bfc&&nf&&!s->fl&&fwv<0&&s->pos<2&&!iso){int l,r;fbounds(Y,1,&l,&r);if(l>ax){aw-=l-ax;ax=l;}if(r<ax+aw)aw=r-ax;ml=rl(s->m[3],aw);mr=rl(s->m[1],aw);}
 if(fwv>=0){w=fwv-eh;mlA=mrA=0;}
 else if(s->w.k==0){w=rl(s->w,aw);if(s->bs)w-=eh;}
 else{
  w=aw-ml-mr-eh;
  if(ii>=0){w=(s->h.k==0&&!s->h.pc)?(int)((float)(s->h.px-(s->bs?(pt+pb+bt+bb):0))/ar+0.5f):ims[ii].w;if(s->mxw.k==0&&s->mxw.pc){lim=rl(s->mxw,aw)-(s->bs?eh:0);if(w>lim)w=lim;}}
  else if(ar>0&&s->ar>0&&s->h.k==1&&d==D_IB){int a,b;mm(n,&a,&b);if(b<w)w=b;}
  if(d==D_TABLE){int a,b;mmtable(n,&a,&b);if(b<w)w=b;}}
 if(fwv<0){
  if(s->mxw.k==0&&!s->mxw.pc){lim=s->mxw.px-(s->bs?eh:0);if(w>lim)w=lim;}
  if(s->mnw.k==0&&!s->mnw.pc){lim=s->mnw.px-(s->bs?eh:0);if(w<lim)w=lim;}}
 if(w<0)w=0;
 fr_=aw-w-eh-ml-mr;
 if(mlA&&mrA){ml=mr=fr_>0?fr_/2:0;}else if(mlA)ml=fr_>0?fr_:0;
 for(i=0;i<4;i++)if(s->rad[i].k==0&&(s->rad[i].px>0||s->rad[i].pc>0))rad=1;
 if(s->shc!=-1&&(s->shx||s->shy||s->shb||s->shs))shd=1;
 box=s->bg>=0||s->gk||bt||bb||bl||br||rad||shd||ii>=0;
 cplx=(rad||s->gk||shd||ii>=0)&&nbx<8192;
 tt=!box&&!pt&&!bfc&&s->h.k==1&&fhv<0;
 bt_=!box&&!pb&&!bfc&&s->h.k==1&&fhv<0;
 if(tt){if(mt>pend)pend=mt;}else{Y+=mt>pend?mt:pend;pend=0;}
 top=Y;x0=ax+ml;tw=w+eh;
 if(s->op<255)curop=curop*s->op/255;
 if(cplx){
  bx=&bxs[nbx];
  for(i=0;i<(int)sizeof(Bx);i++)((char*)bx)[i]=0;
  for(i=0;i<4;i++){bx->r[i]=rl(s->rad[i],tw);if(bx->r[i]<0)bx->r[i]=0;bx->bw[i]=s->bw[i];bx->bc[i]=rc(s->bc[i],s);}
  bx->im=ii+1;bx->of=s->of;bx->bg=rc(s->bg,s);bx->gk=s->gk;bx->ga=s->ga;bx->gn=s->gn;
  for(i=0;i<s->gn;i++){bx->gc[i]=rc(s->gcol[i],s);bx->gp[i]=s->gpos[i];bx->gpx[i]=s->gpx[i];}
  bx->shx=s->shx;bx->shy=s->shy;bx->shb=s->shb;bx->shs=s->shs;bx->shc=shd?rc(s->shc,s):-1;
  bp=cadd(2,x0,top,tw,0,0);bp->bi=nbx++;bp->ex=shd?(s->shb+s->shs+(s->shx<0?-s->shx:s->shx)+(s->shy<0?-s->shy:s->shy)+2):0;}
 else if(s->bg>=0)bp=cadd(0,x0,top,tw,0,rc(s->bg,s));
 if(!tt)Y+=bt+pt;
 cx=ax+ml+bl+pl;cw=w;ca=s->al;lx=cx;LH=LA=LD=0;sp=0;lf=nc;ys=Y;ng=0;padp=0;tip=s->ti;
 if(fhv>=0)hcnt=fhv-ev;
 else if(s->h.k==0&&!s->h.pc)hcnt=s->h.px-(s->bs?ev:0);
 else if(ar>0&&s->h.k==1&&(ii>=0||s->ar>0))hcnt=(int)((float)w*ar+0.5f);
 if(hcnt<-1)hcnt=0;
 if(s->ov==1&&nclp<512){cli=nclp++;clps[cli].par=curcl;clps[cli].x0=x0+bl;clps[cli].y0=top+bt;clps[cli].x1=x0+tw-br;clps[cli].y1=top+bt+(hcnt>=0?hcnt+pt+pb:100000);curcl=cli+1;}
 fch=hcnt;
 if(d==D_LI&&s->lst>0){
  int lst=s->lst,sz=s->fs*3/8,len_;char*tx;
  if(sz<4)sz=4;
  mt0(s,&as,&de);
  if(lst>=4){tx=mktxt(n,lst,&len_);bulp=cadd(1,0,0,wid(tx,len_,s->fs,0,0),as+de,rc(s->col,s));bulp->t=tx;bulp->n=len_;}
  else if(lst==1){bulp=cadd(1,0,0,wid("\xE2\x80\xA2",3,s->fs,0,0),as+de,rc(s->col,s));bulp->t=(char*)"\xE2\x80\xA2";bulp->n=3;}
  else if(lst==3){bulp=cadd(0,0,0,sz,sz,rc(s->col,s));bulp->a=as-s->fs/4+sz/2;}
  else{
   bulp=cadd(2,0,0,sz,sz,0);
   if(nbx<8192){Bx*q=&bxs[nbx];for(i=0;i<(int)sizeof(Bx);i++)((char*)q)[i]=0;for(i=0;i<4;i++){q->r[i]=sz;q->bw[i]=1;q->bc[i]=rc(s->col,s);}q->bg=-1;q->shc=-1;bulp->bi=nbx++;}
   bulp->a=as-s->fs/4+sz/2;}
  bulp->fs=s->fs;if(bulp->k==1)bulp->a=as;
  bulp->x=cx-bulp->w-s->fs/3;bulp->grp=-1;}
 if(seq(n->tag,"summary")&&n->par&&seq(n->par->tag,"details")){
  int sz=s->fs*2/5;if(sz<5)sz=5;
  mt0(s,&as,&de);
  bulp=cadd(4,cx-sz-s->fs/3-2,0,sz,sz,rc(s->col,s));
  bulp->u=((getattr(n->par,"open").p!=0)^(n->par->det&1));bulp->a=as;bulp->fs=s->fs;bulp->grp=-1;}
 c1=nc;
 if(d==D_TABLE)tablec(n);
 else if(s->fx)flexc(n);
 else if(s->gr)gridc(n);
 else walk(n);
 nl();
 if(bfc){i=fbottom(3,nf0);if(i>Y)Y=i;}
 autoh=Y-ys;fin=hcnt>=0?hcnt:autoh;
 if(s->mnh.k==0&&!s->mnh.pc){lim=s->mnh.px-(s->bs?ev:0);if(fin<lim)fin=lim;}
 if(s->mxh.k==0&&!s->mxh.pc){lim=s->mxh.px-(s->bs?ev:0);if(fin>lim)fin=lim;}
 if(fin<0)fin=0;
 if(fin!=autoh){
  if(fhv>=0&&fvv&&fin>autoh){int dy=fvv==1?(fin-autoh)/2:fin-autoh;for(i=c1;i<nc;i++)cm[i].y+=dy;}
  Y=ys+fin;pend=0;}
 if(bt_){if(mb>pend)pend=mb;}else{Y+=pend+pb+bb;pend=mb;}
 bh=Y-top;
 if(cli>=0){clps[cli].y1=top+bh-bb;}
 curcl=ocl;
 if(bp)bp->h=bh;
 if(!cplx&&(bt||bb||bl||br)){
  bside(s->bst[0],x0,top,tw,bt,rc(s->bc[0],s),1);
  bside(s->bst[2],x0,Y-bb,tw,bb,rc(s->bc[2],s),1);
  bside(s->bst[3],x0,top+bt,bl,bh-bt-bb,rc(s->bc[3],s),0);
  bside(s->bst[1],x0+tw-br,top+bt,br,bh-bt-bb,rc(s->bc[1],s),0);}
 if(seq(n->tag,"select")){Cmd*t2=cadd(4,x0+tw-bl-14,top+(bh-5)/2,9,5,0x555555);t2->u=1;}
 if(seq(n->tag,"summary")&&n->par&&seq(n->par->tag,"details")){Cmd*h2=cadd(3,x0,top,tw,bh,0);h2->nd=n->par;}
 if(s->pos==1){
  int dx=0,dy=0;
  if(s->lf.k==0)dx=rl(s->lf,aw);else if(s->rt.k==0)dx=-rl(s->rt,aw);
  if(s->top.k==0)dy=rl(s->top,0);else if(s->bot.k==0)dy=-rl(s->bot,0);
  if(dx||dy)for(i=c0;i<nc;i++){cm[i].x+=dx;cm[i].y+=dy;}
  n->x0=x0+bl+dx;n->y0=top+bt+dy;n->x1=x0+tw-br+dx;n->y1=Y-bb+dy;}
 else if(s->pos>=2){n->x0=x0+bl;n->y0=top+bt;n->x1=x0+tw-br;n->y1=Y-bb;}
 cx=ocx;cw=ocw;ca=oca;curop=oop;
 lx=cx;LH=LA=LD=0;sp=0;lf=nc;ng=0;}
