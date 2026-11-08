/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine module: mwe_decl
 * style defaults, custom properties / var(), declaration parser.
 * Not a standalone translation unit: mwe.c includes the modules in dependency order (unity build). */
/* ---- style defaults, custom properties, var() ---- */
static St*cpa; /* parent style while a declaration block is applied (for "inherit") */
static void defst(St*s,St*pa){
 int i;char*b=(char*)s;
 for(i=0;i<(int)sizeof(St);i++)b[i]=0;
 s->disp=D_INLINE;s->bg=-1;s->op=255;s->fshr=1000;s->as=-1;
 s->w=s->h=s->mnw=s->mxw=s->mnh=s->mxh=s->top=s->rt=s->bot=s->lf=s->fb=mk(0,0,1);
 s->shc=-1;
 for(i=0;i<4;i++){s->bw[i]=3;s->bc[i]=-3;}
 for(i=0;i<12;i++)s->gtk[i]=2;
 if(pa){s->col=pa->col;s->fs=pa->fs;s->bold=pa->bold;s->it=pa->it;s->ul=pa->ul;s->al=pa->al;s->tt=pa->tt;s->ws=pa->ws;s->wb=pa->wb;s->ti=pa->ti;s->tsx=pa->tsx;s->tsy=pa->tsy;s->tsc=pa->tsc;s->ls=pa->ls;s->lh=pa->lh;s->lst=pa->lst;s->vis=pa->vis;s->bsp=pa->bsp;s->bcol=pa->bcol;}
 else{s->fs=16;s->lst=1;s->bsp=2;}}
#define NV 512
static S vnm[NV],vvl[NV];static int nvar;
static int vfind(S nm){int i,j;for(i=nvar-1;i>=0;i--){if(vnm[i].n!=nm.n)continue;for(j=0;j<nm.n&&vnm[i].p[j]==nm.p[j];j++){}if(j==nm.n)return i;}return-1;}
static char wb[2048];
static S subvar(S v){
 int i,j,d,len,k,loops,c;S nm,fb,val,in;
 for(i=0;i+3<v.n;i++)if(v.p[i]=='v'&&v.p[i+1]=='a'&&v.p[i+2]=='r'&&v.p[i+3]=='(')break;
 if(i+3>=v.n)return v;
 if(v.n>=(int)sizeof wb-1)return(S){0,0};
 for(i=0;i<v.n;i++)wb[i]=v.p[i];
 len=v.n;
 for(loops=0;loops<32;loops++){
  for(i=len-4;i>=0;i--)if(wb[i]=='v'&&wb[i+1]=='a'&&wb[i+2]=='r'&&wb[i+3]=='(')break;
  if(i<0)break;
  d=1;j=i+4;while(j<len&&d){if(wb[j]=='(')d++;else if(wb[j]==')')d--;j++;}
  if(d)return(S){0,0};
  in=(S){wb+i+4,j-1-(i+4)};
  nm=in;fb=(S){0,0};
  for(k=0;k<in.n;k++)if(in.p[k]==','){nm=(S){in.p,k};fb=trim((S){in.p+k+1,in.n-k-1});break;}
  nm=trim(nm);
  c=vfind(nm);val=c>=0?vvl[c]:fb;
  if(len-(j-i)+val.n>=(int)sizeof wb-1)return(S){0,0};
  { char tmp[2048];int tl=len-j,x;
    for(x=0;x<tl;x++)tmp[x]=wb[j+x];
    for(x=0;x<val.n;x++)wb[i+x]=val.p[x];
    for(x=0;x<tl;x++)wb[i+val.n+x]=tmp[x];
    len=i+val.n+tl;}}
 return(S){wb,len};}

/* ---- declarations ---- */
static int kw(S t,const char*a){return seq(t,a);}
static int bstyle(S t){
 if(kw(t,"none")||kw(t,"hidden"))return 0;
 if(kw(t,"dashed"))return 2;
 if(kw(t,"dotted"))return 3;
 if(kw(t,"solid")||kw(t,"double")||kw(t,"groove")||kw(t,"ridge")||kw(t,"inset")||kw(t,"outset"))return 1;
 return-1;}
static int bwidth(S t,int fs){
 Len l;
 if(kw(t,"thin"))return 1;
 if(kw(t,"medium"))return 3;
 if(kw(t,"thick"))return 5;
 l=lenv(t,fs);
 return l.k==0&&l.pc==0&&l.px>=0?l.px:-1;}
static void edge4(Len*a,S v,int fs,int pad){
 S t[4];int n=tk(v,t,4),i;Len x[4];
 if(!n)return;
 for(i=0;i<n;i++){x[i]=lenv(t[i],fs);if(x[i].k==2)return;if(pad&&(x[i].k==1||x[i].px<0||x[i].pc<0))x[i]=mk(0,0,0);}
 a[0]=x[0];a[1]=n>1?x[1]:x[0];a[2]=n>2?x[2]:x[0];a[3]=n>3?x[3]:a[1];}
static int angle(S t){ /* degrees, or -1000 */
 int i=0,dg=0;long long iv=0,fr=0,fd=1;int neg=0;
 if(i<t.n&&t.p[i]=='-'){neg=1;i++;}
 while(i<t.n&&isdg(t.p[i])){iv=iv*10+t.p[i]-'0';i++;dg=1;if(iv>100000)break;}
 if(i<t.n&&t.p[i]=='.'){i++;while(i<t.n&&isdg(t.p[i])){if(fd<1000){fr=fr*10+t.p[i]-'0';fd*=10;}i++;dg=1;}}
 if(!dg)return-1000;
 t.p+=i;t.n-=i;
 iv=iv*1000+fr*1000/fd;if(neg)iv=-iv;
 if(seq(t,"deg"))return(int)(iv/1000);
 if(seq(t,"turn"))return(int)(iv*360/1000);
 if(seq(t,"grad"))return(int)(iv*9/10/1000);
 if(seq(t,"rad"))return(int)(iv*573/10000);
 return-1000;}
static void grad(St*s,S in){
 S a[10],t[3];int n,i,j,m,c,k=0,po,lo;
 for(i=0;i<in.n&&in.p[i]!='(';i++);
 if(i>=in.n)return;
 in=(S){in.p+i+1,in.n-i-1};
 while(in.n&&in.p[in.n-1]!=')')in.n--;
 if(in.n)in.n--;
 n=splitc(in,a,10);s->gn=0;s->ga=180;i=0;
 if(n&&starts(a[0],"to ")){
  m=tk(a[0],t,3);
  if(m>=2){j=0;
   if(kw(t[1],"top"))j=0;else if(kw(t[1],"right"))j=90;else if(kw(t[1],"bottom"))j=180;else if(kw(t[1],"left"))j=270;
   if(m>=3){if((kw(t[1],"top")||kw(t[1],"bottom"))&&(kw(t[2],"right")||kw(t[2],"left")))j=kw(t[1],"top")?(kw(t[2],"right")?45:315):(kw(t[2],"right")?135:225);
    else if(kw(t[2],"top"))j=kw(t[1],"right")?45:315;else j=kw(t[1],"right")?135:225;}
   s->ga=j;}
  i=1;}
 else if(n&&(c=angle(a[0]))!=-1000){s->ga=((c%360)+360)%360;i=1;}
 for(;i<n&&s->gn<6;i++){
  m=tk(a[i],t,3);
  if(!m||(c=clr(t[0]))==-2)continue;
  k=s->gn;s->gcol[k]=c;s->gpos[k]=-1;s->gpx[k]=0;
  if(m>1){Len l=lenv(t[1],16);if(l.k==0){if(l.pc||!l.px){s->gpos[k]=l.pc;}else{s->gpos[k]=l.px;s->gpx[k]=1;}}}
  s->gn++;
  if(m>2&&s->gn<6){Len l=lenv(t[2],16);if(l.k==0){s->gcol[s->gn]=c;s->gpos[s->gn]=l.pc?l.pc:l.px;s->gpx[s->gn]=!l.pc&&l.px;s->gn++;}}}
 if(s->gn<2){s->gn=0;s->gk=0;return;}
 s->gk=1;
 if(s->gpos[0]<0)s->gpos[0]=0;
 if(s->gpos[s->gn-1]<0){s->gpos[s->gn-1]=100;s->gpx[s->gn-1]=0;}
 for(i=1;i<s->gn-1;i++)if(s->gpos[i]<0){
  lo=i-1;po=i+1;while(po<s->gn-1&&s->gpos[po]<0)po++;
  s->gpos[i]=s->gpos[lo]+(s->gpos[po]-s->gpos[lo])*(i-lo)/(po-lo);s->gpx[i]=s->gpx[lo];}}
static int fsize(S t,int pfs,int*ok){
 static const struct{const char*n;int v;}K[]={{"xx-small",9},{"x-small",10},{"small",13},{"medium",16},{"large",18},{"x-large",24},{"xx-large",32},{"xxx-large",48},{0,0}};
 int i;Len l;
 *ok=1;
 for(i=0;K[i].n;i++)if(kw(t,K[i].n))return K[i].v;
 if(kw(t,"smaller"))return pfs*5/6;
 if(kw(t,"larger"))return pfs*6/5;
 l=lenv(t,pfs);
 if(l.k==0){int v=l.px+pfs*l.pc/100;if(v>0)return v>180?180:v;}
 *ok=0;return pfs;}
static void setcont(St*s,S v){
 int i=0,o=cpp,q,c,k;char*p;
 v=trim(v);
 if(kw(v,"none")||kw(v,"normal")){s->hascont=0;return;}
 s->cont.p=cpool+cpp;
 while(i<v.n){
  if(v.p[i]=='"'||v.p[i]=='\''){q=v.p[i++];
   while(i<v.n&&v.p[i]!=q){
    if(cpp>(int)sizeof cpool-8)break;
    if(v.p[i]=='\\'&&i+1<v.n){i++;
     if(hx(v.p[i])>=0){c=0;k=0;while(i<v.n&&k<6&&hx(v.p[i])>=0){c=c*16+hx(v.p[i]);i++;k++;}if(i<v.n&&v.p[i]==' ')i++;cpp+=put8(cpool+cpp,c);}
     else cpool[cpp++]=v.p[i++];}
    else cpool[cpp++]=v.p[i++];}
   i++;}
  else i++;}
 (void)p;
 s->cont.n=cpp-o;s->hascont=1;}
/* logical-property aliases (horizontal writing mode only) */
static const struct{const char*a,*b;}PAL[]={{"inline-size","width"},{"block-size","height"},{"min-inline-size","min-width"},{"max-inline-size","max-width"},
 {"min-block-size","min-height"},{"max-block-size","max-height"},{"word-wrap","overflow-wrap"},{"grid-row-gap","row-gap"},{"grid-column-gap","column-gap"},{0,0}};
static void prop(St*s,S k,S v){
 static const char*mp[]={"margin-top","margin-right","margin-bottom","margin-left"};
 static const char*pp[]={"padding-top","padding-right","padding-bottom","padding-left"};
 static const char*bs_[]={"border-top","border-right","border-bottom","border-left"};
 static const char*rn[]={"border-top-left-radius","border-top-right-radius","border-bottom-right-radius","border-bottom-left-radius"};
 S t[8];int n=tk(v,t,8),i,j,c,x,ok;Len l;
 if(!n)return;
 for(i=0;PAL[i].a;i++)if(kw(k,PAL[i].a)){k=(S){(char*)PAL[i].b,sl(PAL[i].b)};break;}
 if(kw(k,"place-items")||kw(k,"place-content")||kw(k,"place-self")){   /* shorthand: align value, then justify value (default: same) */
  const char*a_=kw(k,"place-items")?"align-items":kw(k,"place-content")?"align-content":"align-self",*j_=kw(k,"place-items")?"justify-items":kw(k,"place-content")?"justify-content":"justify-self";
  prop(s,(S){(char*)a_,sl(a_)},t[0]);prop(s,(S){(char*)j_,sl(j_)},n>1?t[1]:t[0]);return;}
 if(kw(t[0],"inherit")&&cpa){
  if(kw(k,"color"))s->col=cpa->col;else if(kw(k,"font-size"))s->fs=cpa->fs;else if(kw(k,"font-weight"))s->bold=cpa->bold;
  else if(kw(k,"font-style"))s->it=cpa->it;else if(kw(k,"line-height"))s->lh=cpa->lh;else if(kw(k,"text-align"))s->al=cpa->al;
  else if(kw(k,"font")){s->fs=cpa->fs;s->bold=cpa->bold;s->it=cpa->it;s->lh=cpa->lh;}
  else if(kw(k,"text-decoration")||kw(k,"text-decoration-line"))s->ul=cpa->ul;
  return;}
 if(kw(k,"color")){c=clr(t[0]);if(c!=-2)s->col=c==-3?(cpa?cpa->col:0):c;}
 else if(kw(k,"background")||kw(k,"background-color")||kw(k,"background-image")){
  if(kw(k,"background")||kw(k,"background-image")){s->gk=0;s->gn=0;}
  for(i=0;i<n;i++){
   if(starts(t[i],"linear-gradient")||starts(t[i],"repeating-linear-gradient"))grad(s,t[i]);
   else if(kw(k,"background-image"))continue;
   else if((c=clr(t[i]))!=-2)s->bg=c==-3?s->col:c;}
  if(kw(k,"background")&&kw(t[0],"none"))s->bg=-1;}
 else if(kw(k,"font-size")){x=fsize(t[0],cpa?cpa->fs:16,&ok);if(ok)s->fs=x;}
 else if(kw(k,"font-weight")){
  if(kw(t[0],"bold")||kw(t[0],"bolder"))s->bold=1;
  else if(kw(t[0],"normal")||kw(t[0],"lighter"))s->bold=0;
  else if(isdg(t[0].p[0]))s->bold=t[0].p[0]>='6'||t[0].n>3;}
 else if(kw(k,"font-style"))s->it=kw(t[0],"italic")||kw(t[0],"oblique");
 else if(kw(k,"font")){
  for(i=0;i<n;i++){
   if(kw(t[i],"italic")||kw(t[i],"oblique"))s->it=1;
   else if(kw(t[i],"bold")||kw(t[i],"bolder"))s->bold=1;
   else if(kw(t[i],"normal"))continue;
   else if(isdg(t[i].p[0])&&t[i].n==3&&t[i].p[1]=='0'&&t[i].p[2]=='0'&&t[i].p[0]!='0')s->bold=t[i].p[0]>='6';
   else{S sz=t[i];for(j=0;j<sz.n;j++)if(sz.p[j]=='/'){S lh=(S){sz.p+j+1,sz.n-j-1};int c2=0;{Len q=lenv(lh,16);c2=q.k==3?q.px/10:0;}if(c2)s->lh=c2;sz.n=j;break;}
    x=fsize(sz,cpa?cpa->fs:16,&ok);if(ok){s->fs=x;break;}}}}
 else if(kw(k,"line-height")){
  if(kw(t[0],"normal"))s->lh=0;
  else{l=lenv(t[0],s->fs);
   if(l.k==3)s->lh=l.px/10;else if(l.k==0)s->lh=l.pc?l.pc:(s->fs?l.px*100/s->fs:0);
   s->lh=s->lh>2000?2000:s->lh<0?0:s->lh;}}
 else if(kw(k,"text-decoration")||kw(k,"text-decoration-line")){
  s->ul=0;
  for(i=0;i<n;i++){if(kw(t[i],"underline"))s->ul|=1;else if(kw(t[i],"line-through"))s->ul|=2;}}
 else if(kw(k,"text-align"))s->al=kw(t[0],"center")?1:(kw(t[0],"right")||kw(t[0],"end"))?2:0;
 else if(kw(k,"text-transform"))s->tt=kw(t[0],"uppercase")?1:kw(t[0],"lowercase")?2:kw(t[0],"capitalize")?3:0;
 else if(kw(k,"letter-spacing")){l=lenv(t[0],s->fs);s->ls=l.k==0?l.px:0;}
 else if(kw(k,"aspect-ratio")){
  float a_=0,b_=1,*cur=&a_,dv=0;int q;S v=t[0];
  if(!kw(v,"auto")){
   if(kw(t[1],"/")){b_=0;dv=0;}
   for(q=0;q<v.n;q++){
    if(v.p[q]=='/'){cur=&b_;b_=0;dv=0;continue;}
    if(v.p[q]=='.'){dv=1;continue;}
    if(v.p[q]>='0'&&v.p[q]<='9'){if(dv>0){dv*=0.1f;*cur+=(float)(v.p[q]-'0')*dv;}else *cur=*cur*10+(float)(v.p[q]-'0');}}
   if(kw(t[1],"/")){S v2=t[2];dv=0;b_=0;for(q=0;q<v2.n;q++){if(v2.p[q]=='.'){dv=1;continue;}if(v2.p[q]>='0'&&v2.p[q]<='9'){if(dv>0){dv*=0.1f;b_+=(float)(v2.p[q]-'0')*dv;}else b_=b_*10+(float)(v2.p[q]-'0');}}}
   if(b_<=0)b_=1;}
  s->ar=a_>0?b_/a_:0;}
 else if(kw(k,"text-indent")){l=lenv(t[0],s->fs);s->ti=l.k==0?l.px:0;}
 else if(kw(k,"word-break"))s->wb=kw(t[0],"break-all")?1:kw(t[0],"keep-all")?2:0;
 else if(kw(k,"white-space"))s->ws=kw(t[0],"nowrap")?1:kw(t[0],"pre")?2:(kw(t[0],"pre-wrap")||kw(t[0],"break-spaces"))?3:kw(t[0],"pre-line")?4:0;
 else if(kw(k,"vertical-align")){
  s->va=0;s->vaa=0;
  if(kw(t[0],"sub"))s->va=-(s->fs/5);else if(kw(t[0],"super"))s->va=s->fs*2/5;
  else if(kw(t[0],"top")||kw(t[0],"text-top"))s->vaa=1;else if(kw(t[0],"middle"))s->vaa=2;else if(kw(t[0],"bottom")||kw(t[0],"text-bottom"))s->vaa=3;
  else{l=lenv(t[0],s->fs);if(l.k==0)s->va=l.px;}}
 else if(kw(k,"display")){
  s->fx=s->gr=0;
  if(kw(t[0],"none")||kw(t[0],"table-column")||kw(t[0],"table-column-group"))s->disp=D_NONE;
  else if(kw(t[0],"inline"))s->disp=D_INLINE;
  else if(kw(t[0],"inline-block"))s->disp=D_IB;
  else if(kw(t[0],"flex")){s->disp=D_BLOCK;s->fx=1;}
  else if(kw(t[0],"inline-flex")){s->disp=D_IB;s->fx=1;}
  else if(kw(t[0],"grid")){s->disp=D_BLOCK;s->gr=1;}
  else if(kw(t[0],"inline-grid")){s->disp=D_IB;s->gr=1;}
  else if(kw(t[0],"table")||kw(t[0],"inline-table"))s->disp=D_TABLE;
  else if(kw(t[0],"table-row"))s->disp=D_ROW;
  else if(kw(t[0],"table-cell"))s->disp=D_CELL;
  else if(kw(t[0],"table-row-group")||kw(t[0],"table-header-group")||kw(t[0],"table-footer-group"))s->disp=D_RG;
  else if(kw(t[0],"list-item"))s->disp=D_LI;
  else if(kw(t[0],"contents"))s->disp=D_INLINE;
  else s->disp=D_BLOCK;}
 else if(kw(k,"visibility"))s->vis=kw(t[0],"hidden")||kw(t[0],"collapse");
 else if(kw(k,"opacity")){l=lenv(t[0],16);x=l.k==3?l.px*255/1000:l.k==0&&l.pc?l.pc*255/100:-1;if(x>=0)s->op=x>255?255:x;}
 else if(kw(k,"overflow")||kw(k,"overflow-x")||kw(k,"overflow-y"))s->ov=(kw(t[0],"hidden")||kw(t[0],"clip")||kw(t[0],"scroll"))?1:kw(t[0],"auto")?2:0;
 else if(kw(k,"position"))s->pos=kw(t[0],"relative")||kw(t[0],"sticky")?1:kw(t[0],"absolute")?2:kw(t[0],"fixed")?3:0;
 else if(kw(k,"float"))s->fl=kw(t[0],"left")?1:kw(t[0],"right")?2:0;
 else if(kw(k,"clear"))s->clr=kw(t[0],"left")?1:kw(t[0],"right")?2:kw(t[0],"both")?3:0;
 else if(kw(k,"top")){l=lenv(t[0],s->fs);if(l.k!=2)s->top=l;}
 else if(kw(k,"right")){l=lenv(t[0],s->fs);if(l.k!=2)s->rt=l;}
 else if(kw(k,"bottom")){l=lenv(t[0],s->fs);if(l.k!=2)s->bot=l;}
 else if(kw(k,"left")){l=lenv(t[0],s->fs);if(l.k!=2)s->lf=l;}
 else if(kw(k,"inset")){Len a[4];edge4(a,v,s->fs,0);s->top=a[0];s->rt=a[1];s->bot=a[2];s->lf=a[3];}
 else if(kw(k,"width")||kw(k,"height")||kw(k,"min-width")||kw(k,"max-width")||kw(k,"min-height")||kw(k,"max-height")){
  l=lenv(t[0],s->fs);
  if(l.k==2||(l.k==3&&l.px))return;
  if(l.k==3)l=mk(0,0,0);
  if(kw(t[0],"none")||kw(t[0],"auto")||kw(t[0],"fit-content")||kw(t[0],"max-content")||kw(t[0],"min-content"))l=mk(0,0,1);
  if(kw(k,"width"))s->w=l;else if(kw(k,"height"))s->h=l;else if(kw(k,"min-width"))s->mnw=l;else if(kw(k,"max-width"))s->mxw=l;
  else if(kw(k,"min-height"))s->mnh=l;else s->mxh=l;}
 else if(kw(k,"box-sizing"))s->bs=kw(t[0],"border-box");
 else if(kw(k,"margin"))edge4(s->m,v,s->fs,0);
 else if(kw(k,"padding"))edge4(s->p,v,s->fs,1);
 else if(kw(k,"margin-inline")||kw(k,"margin-block")||kw(k,"padding-inline")||kw(k,"padding-block")){
  int pd=k.p[0]=='p'||k.p[0]=='P',inl=k.p[k.n-1]=='e';
  Len a=lenv(t[0],s->fs),b2=n>1?lenv(t[1],s->fs):a;
  if(a.k==2||b2.k==2)return;
  if(pd){if(a.px<0||a.pc<0||a.k)a=mk(0,0,0);if(b2.px<0||b2.pc<0||b2.k)b2=mk(0,0,0);}
  if(inl){(pd?s->p:s->m)[3]=a;(pd?s->p:s->m)[1]=b2;}else{(pd?s->p:s->m)[0]=a;(pd?s->p:s->m)[2]=b2;}}
 else if(kw(k,"border")){
  int hs=0;
  for(i=0;i<n;i++)if(bstyle(t[i])>=0)hs=1;
  for(j=0;j<4;j++){
   for(i=0;i<n;i++){
    if((x=bstyle(t[i]))>=0)s->bst[j]=x;
    else if((x=bwidth(t[i],s->fs))>=0)s->bw[j]=x;
    else if((c=clr(t[i]))!=-2)s->bc[j]=c;}
   if(!hs)s->bst[j]=0;}}
 else{
  for(j=0;j<4;j++)if(kw(k,bs_[j])){
   int hs=0;
   for(i=0;i<n;i++){
    if((x=bstyle(t[i]))>=0){s->bst[j]=x;hs=1;}
    else if((x=bwidth(t[i],s->fs))>=0)s->bw[j]=x;
    else if((c=clr(t[i]))!=-2)s->bc[j]=c;}
   if(!hs)s->bst[j]=0;
   return;}
 if(kw(k,"border-width")||kw(k,"border-style")||kw(k,"border-color")){
   static const char M[5][4]={{0,0,0,0},{0,0,0,0},{0,1,0,1},{0,1,2,1},{0,1,2,3}};
   for(j=0;j<4;j++){
    i=M[n>4?4:n][j];
    if(kw(k,"border-width")){x=bwidth(t[i],s->fs);if(x>=0)s->bw[j]=x;}
    else if(kw(k,"border-style")){x=bstyle(t[i]);if(x>=0)s->bst[j]=x;}
    else{c=clr(t[i]);if(c!=-2)s->bc[j]=c;}}
   return;}
  for(j=0;j<4;j++){
   if(seq(k,mp[j])){l=lenv(t[0],s->fs);if(l.k!=2)s->m[j]=l;return;}
   if(seq(k,pp[j])){l=lenv(t[0],s->fs);if(l.k==0&&l.px>=0&&l.pc>=0)s->p[j]=l;return;}
   if(seq(k,rn[j])){l=lenv(t[0],s->fs);if(l.k==0)s->rad[j]=l;return;}}
  if(kw(k,"border-radius")){
   Len a[4];S h=v;int sl_;
   for(sl_=0;sl_<h.n;sl_++)if(h.p[sl_]=='/'){h.n=sl_;break;}
   edge4(a,h,s->fs,1);
   s->rad[0]=a[0];s->rad[1]=a[1];s->rad[2]=a[2];s->rad[3]=a[3];}
  else if(kw(k,"box-shadow")){
   S a[4],q[8];int m,nn;
   s->shc=-1;s->shb=s->shx=s->shy=s->shs=0;
   if(kw(t[0],"none"))return;
   splitc(v,a,4);
   m=tk(a[0],q,8);nn=0;
   for(i=0;i<m;i++){
    if(kw(q[i],"inset"))return;
    if((c=clr(q[i]))!=-2)s->shc=c==-3?s->col:c;
    else{l=lenv(q[i],s->fs);if(l.k==0||l.k==3){x=l.px;if(x>400)x=400;if(x<-400)x=-400;if(nn==0)s->shx=x;else if(nn==1)s->shy=x;else if(nn==2)s->shb=x<0?0:x;else if(nn==3)s->shs=x;nn++;}}}
   if(s->shc==-2||(s->shc==-1&&nn==0))s->shc=-1;
   else if(s->shc==-1&&nn>=2&&0)s->shc=0x000000;}
  else if(kw(k,"text-shadow")){   /* first shadow only; the blur radius is ignored */
   S a[4],q[8];int m,nn=0,sc_=-2;
   s->tsx=s->tsy=0;s->tsc=-1;
   if(kw(t[0],"none"))return;
   splitc(v,a,4);
   m=tk(a[0],q,8);
   for(i=0;i<m;i++){
    if((c=clr(q[i]))!=-2)sc_=c==-3?s->col:c;
    else{l=lenv(q[i],s->fs);if(l.k==0||l.k==3){x=l.px;if(x>50)x=50;if(x<-50)x=-50;if(nn==0)s->tsx=x;else if(nn==1)s->tsy=x;nn++;}}}
   s->tsc=sc_==-2?s->col:sc_;}
  else if(kw(k,"flex")){
   s->fgrow=0;s->fshr=1000;s->fb=mk(0,0,1);
   if(kw(t[0],"none")){s->fshr=0;return;}
   if(kw(t[0],"auto")){s->fgrow=1000;return;}
   x=0;
   for(i=0;i<n;i++){l=lenv(t[i],s->fs);
    if(l.k==3&&x==0){s->fgrow=l.px;x=1;if(n==1)s->fb=mk(0,0,0);}
    else if(l.k==3&&x==1){s->fshr=l.px;x=2;}
    else if(l.k==0)s->fb=l;}}
  else if(kw(k,"flex-grow")){l=lenv(t[0],16);if(l.k==3)s->fgrow=l.px;}
  else if(kw(k,"flex-shrink")){l=lenv(t[0],16);if(l.k==3)s->fshr=l.px;}
  else if(kw(k,"flex-basis")){l=lenv(t[0],s->fs);if(l.k!=2)s->fb=l;if(kw(t[0],"content"))s->fb=mk(0,0,1);}
  else if(kw(k,"flex-direction")||kw(k,"flex-flow")){
   for(i=0;i<n;i++){
    if(kw(t[i],"row"))s->fdir=0;else if(kw(t[i],"column"))s->fdir=1;else if(kw(t[i],"row-reverse"))s->fdir=2;else if(kw(t[i],"column-reverse"))s->fdir=3;
    else if(kw(t[i],"wrap")||kw(t[i],"wrap-reverse"))s->fwrap=1;else if(kw(t[i],"nowrap"))s->fwrap=0;}}
  else if(kw(k,"flex-wrap"))s->fwrap=kw(t[0],"wrap")||kw(t[0],"wrap-reverse");
  else if(kw(k,"overflow-wrap")){if(kw(t[0],"anywhere"))s->wb=1;}   /* break-word already breaks only overlong words */
  else if(kw(k,"object-fit"))s->of=kw(t[0],"contain")?1:kw(t[0],"cover")?2:kw(t[0],"none")?3:kw(t[0],"scale-down")?4:0;
  else if(kw(k,"justify-content")){
   s->jc=kw(t[0],"center")?2:(kw(t[0],"flex-end")||kw(t[0],"end")||kw(t[0],"right"))?1:kw(t[0],"space-between")?3:kw(t[0],"space-around")?4:kw(t[0],"space-evenly")?5:0;}
  else if(kw(k,"align-items")||kw(k,"align-self")){
   c=kw(t[0],"center")?3:(kw(t[0],"flex-end")||kw(t[0],"end"))?2:(kw(t[0],"flex-start")||kw(t[0],"start")||kw(t[0],"baseline"))?1:0;
   if(kw(k,"align-items"))s->ai=c;else s->as=kw(t[0],"auto")?-1:c;}
  else if(kw(k,"gap")||kw(k,"grid-gap")){l=lenv(t[0],s->fs);if(l.k==0){s->gy=l;s->gx=n>1?lenv(t[1],s->fs):l;if(s->gx.k!=0)s->gx=l;}}
  else if(kw(k,"row-gap")||kw(k,"grid-row-gap")){l=lenv(t[0],s->fs);if(l.k==0)s->gy=l;}
  else if(kw(k,"column-gap")||kw(k,"grid-column-gap")){l=lenv(t[0],s->fs);if(l.k==0)s->gx=l;}
  else if(kw(k,"order")){l=lenv(t[0],16);if(l.k==3)s->ord=l.px/1000;}
  else if(kw(k,"list-style")||kw(k,"list-style-type")){
   static const char*L[]={"none","disc","circle","square","decimal","decimal-leading-zero","lower-alpha","upper-alpha","lower-latin","upper-latin","lower-roman","upper-roman",0};
   for(i=0;i<n;i++)for(j=0;L[j];j++)if(kw(t[i],L[j]))s->lst=j==8?6:j==9?7:j;}
  else if(kw(k,"border-collapse"))s->bcol=kw(t[0],"collapse");
  else if(kw(k,"border-spacing")){l=lenv(t[0],s->fs);if(l.k==0)s->bsp=l.px;}
  else if(kw(k,"content"))setcont(s,v);
  else if(kw(k,"grid-template-columns")){
   s->gtn=0;s->gtauto=0;
   for(i=0;i<n&&s->gtn<12;i++){
    if(starts(t[i],"repeat(")){
     S in=(S){t[i].p+7,t[i].n-8},a[2],tr[4];int cnt,m,q;
     if(splitc(in,a,2)<2)continue;
     m=tk(a[1],tr,4);
     if(kw(a[0],"auto-fill")||kw(a[0],"auto-fit")){
      s->gtauto=1;s->gtmin=0;
      if(m&&starts(tr[0],"minmax(")){S mi[2];if(splitc((S){tr[0].p+7,tr[0].n-8},mi,2)==2){Len ml=lenv(mi[0],s->fs);if(ml.k==0)s->gtmin=ml.px;}}
      else if(m){Len ml=lenv(tr[0],s->fs);if(ml.k==0)s->gtmin=ml.px;}
      continue;}
     {Len cl=lenv(a[0],16);cnt=cl.k==3?cl.px/1000:0;}
     if(cnt>12)cnt=12;
     for(q=0;q<cnt;q++)for(j=0;j<m&&s->gtn<12;j++){
      S tt=tr[j];
      if(starts(tt,"minmax(")){S mi[2];if(splitc((S){tt.p+7,tt.n-8},mi,2)==2)tt=mi[1];}
      Len tl=lenv(tt,s->fs);
      if(tt.n>2&&seq((S){tt.p+tt.n-2,2},"fr")){s->gtk[s->gtn]=1;s->gt[s->gtn]=mk(0,0,3);{S nn_=(S){tt.p,tt.n-2};Len f=lenv(nn_,16);s->gt[s->gtn].px=f.k==3?f.px:1000;}}
      else if(tl.k==0){s->gtk[s->gtn]=0;s->gt[s->gtn]=tl;}
      else s->gtk[s->gtn]=2;
      s->gtn++;}
     continue;}
    {S tt=t[i];Len tl;
     if(starts(tt,"minmax(")){S mi[2];if(splitc((S){tt.p+7,tt.n-8},mi,2)==2)tt=mi[1];}
     tl=lenv(tt,s->fs);
     if(tt.n>2&&seq((S){tt.p+tt.n-2,2},"fr")){Len f=lenv((S){tt.p,tt.n-2},16);s->gtk[s->gtn]=1;s->gt[s->gtn]=mk(f.k==3?f.px:1000,0,3);}
     else if(tl.k==0){s->gtk[s->gtn]=0;s->gt[s->gtn]=tl;}
     else s->gtk[s->gtn]=2;
     s->gtn++;}}}
  else if(kw(k,"grid-column")||kw(k,"grid-column-end")||kw(k,"grid-column-start")){
   for(i=0;i+1<n;i++)if(kw(t[i],"span")){l=lenv(t[i+1],16);if(l.k==3){s->gcs=l.px/1000;if(s->gcs>12)s->gcs=12;}}}}}
static void decl(St*s,S d,int pass){
 int i=0,a,b,pd,e,imp,j;S k,v;
 while(i<d.n){
  a=i;while(i<d.n&&d.p[i]!=':'&&d.p[i]!=';')i++;
  if(i>=d.n||d.p[i]==';'){i++;continue;}
  k=trim((S){d.p+a,i-a});i++;
  b=i;pd=0;while(i<d.n&&(d.p[i]!=';'||pd)){if(d.p[i]=='(')pd++;else if(d.p[i]==')')pd--;i++;}
  v=trim((S){d.p+b,i-b});i++;
  imp=0;
  for(e=v.n-1;e>=0;e--)if(v.p[e]=='!'){imp=1;v.n=e;v=trim(v);break;}
  if(k.n>2&&k.p[0]=='-'&&k.p[1]=='-'){
   if(pass==1&&nvar<NV){vnm[nvar]=k;vvl[nvar]=v;nvar++;}
   continue;}
  if(pass==1)continue;
  if(imp!=(pass==3))continue;
  v=subvar(v);
  if(!v.n)continue;
  (void)j;
  prop(s,k,v);}}
