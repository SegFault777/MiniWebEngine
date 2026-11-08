/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine module: mwe_selectors
 * selector parsing, :is()/:where() expansion, rules, @media, matching.
 * Not a standalone translation unit: mwe.c includes the modules in dependency order (unity build). */
/* ---- selectors, rules, @media ---- */
typedef struct{S n,v;int op;}At;
typedef struct{int t,a,b;}Ps;
typedef struct{S tag,id,cls[3];int nc;At at[3];int na;Ps ps[3];int np;int nt;}Cp;
typedef struct{Cp c[4];char comb[4];int ns,spec,pe,mnw,mxw,mnh,mxh;S d;}Rule;
static Rule rls[MR];static int nr,rdirty;
static Cp nots[96];static int nnot;
static int qb[4]={0,1<<30,0,1<<30};
enum{PS_FIRST=1,PS_LAST,PS_ONLY,PS_NTH,PS_NTHL,PS_FIRSTT,PS_LASTT,PS_ONLYT,PS_NTHT,PS_EMPTY,PS_ROOT,PS_LINK,PS_CHECKED,PS_DISABLED,PS_ENABLED};
static int nthp(S a,int*A,int*B){
 char b[32];int n=0,i=0,sg=1,v=0,dg=0,hn=0;
 for(;a.n&&n<31;a.p++,a.n--)if(!isws(*a.p))b[n++]=(char)lc(*a.p);
 b[n]=0;
 if(!sl(b))return 0;
 if(b[0]=='o'&&b[1]=='d'&&b[2]=='d'&&!b[3]){*A=2;*B=1;return 1;}
 if(b[0]=='e'&&b[1]=='v'&&b[2]=='e'&&b[3]=='n'&&!b[4]){*A=2;*B=0;return 1;}
 *A=0;*B=0;
 if(b[i]=='-'){sg=-1;i++;}else if(b[i]=='+')i++;
 while(isdg(b[i])){v=v*10+b[i]-'0';i++;dg=1;if(v>100000)return 0;}
 if(b[i]=='n'){hn=1;*A=(dg?v:1)*sg;i++;
  if(b[i]=='+'||b[i]=='-'){int s2=b[i]=='-'?-1:1;i++;v=0;dg=0;while(isdg(b[i])){v=v*10+b[i]-'0';i++;dg=1;if(v>100000)return 0;}if(!dg)return 0;*B=v*s2;}}
 else{if(!dg)return 0;*B=v*sg;}
 (void)hn;
 return b[i]==0;}
static int pcomp(S s,int*pi,Cp*c,int*spec,int*pe,int depth){
 int i=*pi,j,st,k,q,cl_=0;char ch;S v,nm,arg;
 c->tag.n=c->id.n=0;c->nc=c->na=c->np=c->nt=0;
 while(i<s.n&&!isws(s.p[i])&&s.p[i]!='>'&&s.p[i]!='+'&&s.p[i]!='~'&&s.p[i]!=','){
  ch=s.p[i];
  if(ch=='*'){i++;continue;}
  if(ch=='#'||ch=='.'){
   st=i+1;j=st;while(j<s.n&&isal(s.p[j]))j++;
   if(j==st)return 0;
   v=(S){s.p+st,j-st};
   if(ch=='#'){c->id=v;*spec+=10000;}else{if(c->nc>=3)return 0;c->cls[c->nc++]=v;*spec+=100;}
   i=j;continue;}
  if(ch=='['){
   At*a;
   if(c->na>=3)return 0;
   st=i+1;k=st;q=0;
   while(k<s.n&&(q||s.p[k]!=']')){if(s.p[k]=='"'||s.p[k]=='\''){if(q==s.p[k])q=0;else if(!q)q=s.p[k];}k++;}
   if(k>=s.n)return 0;
   arg=trim((S){s.p+st,k-st});
   cl_=k;
   a=&c->at[c->na++];
   j=0;while(j<arg.n&&!isws(arg.p[j])&&!(arg.p[j]=='='||((arg.p[j]=='~'||arg.p[j]=='|'||arg.p[j]=='^'||arg.p[j]=='$'||arg.p[j]=='*')&&j+1<arg.n&&arg.p[j+1]=='=')))j++;
   a->n=(S){arg.p,j};a->op=0;a->v=(S){0,0};
   while(j<arg.n&&isws(arg.p[j]))j++;
   if(j<arg.n){
    a->op=arg.p[j]=='='?1:arg.p[j]=='~'?2:arg.p[j]=='^'?3:arg.p[j]=='$'?4:arg.p[j]=='*'?5:arg.p[j]=='|'?6:0;
    if(!a->op)return 0;
    j+=a->op==1?1:2;
    while(j<arg.n&&isws(arg.p[j]))j++;
    v=(S){arg.p+j,arg.n-j};
    if(v.n&&(v.p[0]=='"'||v.p[0]=='\'')){q=v.p[0];v.p++;v.n--;for(k=0;k<v.n&&v.p[k]!=q;k++){}v.n=k;}
    else{for(k=0;k<v.n&&!isws(v.p[k]);k++){}v.n=k;}
    a->v=v;}
   *spec+=100;i=cl_+1;continue;}
  if(ch==':'){
   if(i+1<s.n&&s.p[i+1]==':'){
    st=i+2;j=st;while(j<s.n&&isal(s.p[j]))j++;
    nm=(S){s.p+st,j-st};
    if(seq(nm,"before"))*pe=1;else if(seq(nm,"after"))*pe=2;else return 0;
    *spec+=1;i=j;continue;}
   st=i+1;j=st;while(j<s.n&&isal(s.p[j]))j++;
   nm=(S){s.p+st,j-st};arg=(S){0,0};
   if(j<s.n&&s.p[j]=='('){
    int d=1;k=j+1;while(k<s.n&&d){if(s.p[k]=='(')d++;else if(s.p[k]==')')d--;k++;}
    if(d)return 0;
    arg=(S){s.p+j+1,k-j-2};j=k;}
   if(seq(nm,"before")||seq(nm,"after")){*pe=seq(nm,"before")?1:2;*spec+=1;i=j;continue;}
   if(seq(nm,"not")){
    int sp2=0,pe2=0,ii=0;Cp*nc_;
    if(depth||nnot>=96||c->nt)return 0;
    arg=trim(arg);nc_=&nots[nnot];c->nt=++nnot;
    if(!pcomp(arg,&ii,nc_,&sp2,&pe2,1)||ii<arg.n||pe2)return 0;
    *spec+=sp2;i=j;continue;}
   {Ps*p;int t=0,A=0,B=0;
    if(c->np>=3)return 0;
    if(seq(nm,"first-child"))t=PS_FIRST;else if(seq(nm,"last-child"))t=PS_LAST;else if(seq(nm,"only-child"))t=PS_ONLY;
    else if(seq(nm,"first-of-type"))t=PS_FIRSTT;else if(seq(nm,"last-of-type"))t=PS_LASTT;else if(seq(nm,"only-of-type"))t=PS_ONLYT;
    else if(seq(nm,"empty"))t=PS_EMPTY;else if(seq(nm,"root"))t=PS_ROOT;
    else if(seq(nm,"link")||seq(nm,"any-link"))t=PS_LINK;
    else if(seq(nm,"checked"))t=PS_CHECKED;else if(seq(nm,"disabled"))t=PS_DISABLED;else if(seq(nm,"enabled"))t=PS_ENABLED;
    else if(seq(nm,"nth-child")||seq(nm,"nth-last-child")||seq(nm,"nth-of-type")){
     if(!nthp(arg,&A,&B))return 0;
     t=seq(nm,"nth-child")?PS_NTH:seq(nm,"nth-last-child")?PS_NTHL:PS_NTHT;}
    else return 0; /* :hover :focus :active :visited ... cannot apply to a static page */
    p=&c->ps[c->np++];p->t=t;p->a=A;p->b=B;*spec+=100;i=j;continue;}}
  if(isa(ch)){st=i;j=i;while(j<s.n&&isal(s.p[j]))j++;c->tag=(S){s.p+st,j-st};*spec+=1;i=j;continue;}
  return 0;}
 *pi=i;return 1;}
/* :is(a,b) / :where(a,b) with compound alternatives are expanded textually into one selector per alternative
 * (so "p:is(.x,.y)" becomes "p.x, p.y"); the text lives in xpool for as long as the page does. */
static char xpool[1<<16];static int xpn;
static void addsel(S s,S d);
static int xcompound(S a){int i;if(!a.n)return 0;for(i=0;i<a.n;i++){char c=a.p[i];if(isws(c)||c=='>'||c=='+'||c=='~')return 0;}return 1;}
static void xexp(S s,S d,int dep){
 int i,k,lv=0,st=-1,pl=0,np=0,j,a0,dd;S part[8];
 for(i=0;i<s.n;i++){
  char c=s.p[i];
  if(c=='['){while(i<s.n&&s.p[i]!=']')i++;continue;}
  if(c==':'&&i+4<s.n&&lc(s.p[i+1])=='i'&&lc(s.p[i+2])=='s'&&s.p[i+3]=='('){st=i;pl=4;break;}
  if(c==':'&&i+7<s.n&&lc(s.p[i+1])=='w'&&lc(s.p[i+2])=='h'&&lc(s.p[i+3])=='e'&&lc(s.p[i+4])=='r'&&lc(s.p[i+5])=='e'&&s.p[i+6]=='('){st=i;pl=7;break;}}
 if(st<0||dep>2){addsel(s,d);return;}
 lv=1;k=st+pl;a0=k;
 while(k<s.n&&lv){
  if(s.p[k]=='(')lv++;
  else if(s.p[k]==')'){lv--;if(!lv)break;}
  else if(s.p[k]==','&&lv==1){if(np>=7){addsel(s,d);return;}part[np++]=(S){s.p+a0,k-a0};a0=k+1;}
  k++;}
 if(lv){addsel(s,d);return;}
 part[np++]=(S){s.p+a0,k-a0};
 for(j=0;j<np;j++){part[j]=trim(part[j]);if(!xcompound(part[j])){addsel(s,d);return;}}
 for(j=0;j<np;j++){
  int tot=st+part[j].n+(s.n-k-1);
  S ns;
  if(xpn+tot+1>(int)sizeof xpool){addsel(s,d);return;}
  ns=(S){xpool+xpn,tot};
  for(dd=0;dd<st;dd++)xpool[xpn++]=s.p[dd];
  for(dd=0;dd<part[j].n;dd++)xpool[xpn++]=part[j].p[dd];
  for(dd=k+1;dd<s.n;dd++)xpool[xpn++]=s.p[dd];
  xpool[xpn++]=0;
  xexp(ns,d,dep+1);}}
static void addsel(S s,S d){
 Rule*r;int i=0,cb,pe=0,sp,k;
 if(nr>=MR)return;
 r=&rls[nr];r->ns=0;r->spec=0;r->pe=0;r->d=d;
 r->mnw=qb[0];r->mxw=qb[1];r->mnh=qb[2];r->mxh=qb[3];
 s=trim(s);
 while(i<s.n){
  cb=0;
  while(i<s.n&&isws(s.p[i]))i++;
  if(i>=s.n)break;
  if(s.p[i]=='>'||s.p[i]=='+'||s.p[i]=='~'){
   if(!r->ns)return;
   cb=s.p[i]=='>'?1:s.p[i]=='+'?2:3;i++;
   while(i<s.n&&isws(s.p[i]))i++;}
  if(r->ns>=4||pe)return;
  sp=0;
  if(!pcomp(s,&i,&r->c[r->ns],&sp,&pe,0))return;
  if(i==0)return;
  r->comb[r->ns]=(char)cb;r->ns++;r->spec+=sp;
  (void)k;}
 r->pe=pe;
 if(r->ns){nr++;rdirty=1;}}
static int mq1(S q,int*b){
 S t[16],nm,val;int n=tk(q,t,16),i,j,l,ok=1;
 for(i=0;i<n;i++){
  if(seq(t[i],"print")||seq(t[i],"speech")||seq(t[i],"not"))return 0;
  if(t[i].p[0]!='(')continue;
  nm=trim((S){t[i].p+1,t[i].n-2});val=(S){0,0};
  for(j=0;j<nm.n;j++)if(nm.p[j]==':'){val=trim((S){nm.p+j+1,nm.n-j-1});nm=trim((S){nm.p,j});break;}
  {Len ln_=lenv(val,16);l=ln_.k==0?ln_.px:-1;}
  if(seq(nm,"min-width")&&l>=0){if(l>b[0])b[0]=l;}
  else if(seq(nm,"max-width")&&l>=0){if(l<b[1])b[1]=l;}
  else if(seq(nm,"min-height")&&l>=0){if(l>b[2])b[2]=l;}
  else if(seq(nm,"max-height")&&l>=0){if(l<b[3])b[3]=l;}
  else if(seq(nm,"width")&&l>=0){b[0]=b[1]=l;}
  else if(seq(nm,"height")&&l>=0){b[2]=b[3]=l;}
  else if(seq(nm,"prefers-color-scheme")&&seq(val,"dark"))return 0;
  else if(seq(nm,"prefers-reduced-motion")&&seq(val,"reduce"))return 0;
  else if((seq(nm,"hover")&&seq(val,"none"))||(seq(nm,"pointer")&&(seq(val,"coarse")||seq(val,"none")))||(seq(nm,"forced-colors")&&seq(val,"active")))return 0;}
 (void)ok;return 1;}
static void css(S t){
 int i=0,a,b,d,be,p0,q,sv[4],k,m,lvl;S qs[6],pre;
 while(i<t.n){
  while(i<t.n&&isws(t.p[i]))i++;
  if(i+1<t.n&&t.p[i]=='/'&&t.p[i+1]=='*'){i+=2;while(i+1<t.n&&!(t.p[i]=='*'&&t.p[i+1]=='/'))i++;i+=2;continue;}
  if(i>=t.n)break;
  a=i;while(i<t.n&&t.p[i]!='{'&&t.p[i]!=';')i++;
  if(i>=t.n)break;
  if(t.p[i]==';'){i++;continue;}
  be=i;i++;b=i;d=1;
  while(i<t.n&&d){if(t.p[i]=='{')d++;else if(t.p[i]=='}')d--;i++;}
  d=i-(d?0:1);
  if(t.p[a]=='@'){
   pre=trim((S){t.p+a,be-a});
   if(starts(pre,"@media")){
    for(k=0;k<4;k++)sv[k]=qb[k];
    m=splitc((S){pre.p+6,pre.n-6},qs,6);
    for(k=0;k<m;k++){int tb[4]={sv[0],sv[1],sv[2],sv[3]};
     if(mq1(qs[k],tb)){qb[0]=tb[0];qb[1]=tb[1];qb[2]=tb[2];qb[3]=tb[3];css((S){t.p+b,d-b});break;}}
    for(k=0;k<4;k++)qb[k]=sv[k];}
   else if((starts(pre,"@supports")&&!(pre.n>10&&(pre.p[10]=='n'||pre.p[10]=='N')))||starts(pre,"@layer"))css((S){t.p+b,d-b});
   continue;}
  p0=a;lvl=0;
  for(q=a;q<=be;q++){
   if(q<be){if(t.p[q]=='('||t.p[q]=='[')lvl++;else if(t.p[q]==')'||t.p[q]==']')lvl--;}
   if(q==be||(t.p[q]==','&&lvl<=0)){xexp(trim((S){t.p+p0,q-p0}),(S){t.p+b,d-b},0);p0=q+1;}}}}
/* ---- matching ---- */
static int hc(S cl,S k){int i=0,a;while(i<cl.n){while(i<cl.n&&isws(cl.p[i]))i++;a=i;while(i<cl.n&&!isws(cl.p[i]))i++;if(i>a&&seqs((S){cl.p+a,i-a},k))return 1;}return 0;}
static N*nxel(N*n){N*c=n->nx;while(c&&(!c->tag.n||c->gen))c=c->nx;return c;}
static int eidx(N*n,int last,int type){
 int k=1;N*c;
 if(!last){for(c=n->pv;c;c=c->pv)if(!type||seqs(c->tag,n->tag))k++;}
 else for(c=nxel(n);c;c=nxel(c))if(!type||seqs(c->tag,n->tag))k++;
 return k;}
static int nthok(int idx,int A,int B){
 if(A==0)return idx==B;
 return((idx-B)%A==0)&&((idx-B)/A>=0);}
static int cp1(N*n,Cp*c){
 int k,i;S v;
 if(c->tag.n&&!seqs(n->tag,c->tag))return 0;
 if(c->id.n&&!seqs(n->id,c->id))return 0;
 for(k=0;k<c->nc;k++)if(!hc(n->cls,c->cls[k]))return 0;
 for(k=0;k<c->na;k++){
  At*a=&c->at[k];
  char nb[64];
  for(i=0;i<a->n.n&&i<63;i++)nb[i]=a->n.p[i];
  nb[i]=0;
  v=getattr(n,nb);
  if(!v.p)return 0;
  switch(a->op){
   case 1:if(!seqs(v,a->v))return 0;break;
   case 2:if(!hc(v,a->v))return 0;break;
   case 3:if(!a->v.n||v.n<a->v.n||!seqs((S){v.p,a->v.n},a->v))return 0;break;
   case 4:if(!a->v.n||v.n<a->v.n||!seqs((S){v.p+v.n-a->v.n,a->v.n},a->v))return 0;break;
   case 5:{int f=0;if(!a->v.n)return 0;for(i=0;i+a->v.n<=v.n;i++)if(seqs((S){v.p+i,a->v.n},a->v)){f=1;break;}if(!f)return 0;break;}
   case 6:if(!(seqs(v,a->v)||(v.n>a->v.n&&seqs((S){v.p,a->v.n},a->v)&&v.p[a->v.n]=='-')))return 0;break;}}
 for(k=0;k<c->np;k++){
  Ps*p=&c->ps[k];
  switch(p->t){
   case PS_FIRST:if(n->pv)return 0;break;
   case PS_LAST:if(nxel(n))return 0;break;
   case PS_ONLY:if(n->pv||nxel(n))return 0;break;
   case PS_NTH:if(!nthok(eidx(n,0,0),p->a,p->b))return 0;break;
   case PS_NTHL:if(!nthok(eidx(n,1,0),p->a,p->b))return 0;break;
   case PS_FIRSTT:if(eidx(n,0,1)!=1)return 0;break;
   case PS_LASTT:if(eidx(n,1,1)!=1)return 0;break;
   case PS_ONLYT:if(eidx(n,0,1)!=1||eidx(n,1,1)!=1)return 0;break;
   case PS_NTHT:if(!nthok(eidx(n,0,1),p->a,p->b))return 0;break;
   case PS_EMPTY:if(n->kid)return 0;break;
   case PS_ROOT:if(n->par!=root)return 0;break;
   case PS_LINK:if(!(seq(n->tag,"a")&&n->href.n))return 0;break;
   case PS_CHECKED:if(!getattr(n,"checked").p)return 0;break;
   case PS_DISABLED:if(!getattr(n,"disabled").p)return 0;break;
   case PS_ENABLED:if(getattr(n,"disabled").p)return 0;break;}}
 if(c->nt&&cp1(n,&nots[c->nt-1]))return 0;
 return 1;}
static int mtchk(N*n,Rule*r,int k){
 N*a;
 if(!cp1(n,&r->c[k]))return 0;
 if(!k)return 1;
 switch(r->comb[k]){
  case 0:for(a=n->par;a&&a!=root;a=a->par)if(mtchk(a,r,k-1))return 1;return 0;
  case 1:a=n->par;return a&&a!=root&&mtchk(a,r,k-1);
  case 2:return n->pv&&mtchk(n->pv,r,k-1);
  default:for(a=n->pv;a;a=a->pv)if(mtchk(a,r,k-1))return 1;return 0;}}
static int rmatch(N*n,Rule*r,int pe){
 return r->pe==pe&&VW>=r->mnw&&VW<=r->mxw&&VH>=r->mnh&&VH<=r->mxh&&mtchk(n,r,r->ns-1);}
static void sortrules(void){
 int i,j;Rule t;
 if(!rdirty)return;
 for(i=1;i<nr;i++){t=rls[i];j=i-1;while(j>=0&&rls[j].spec>t.spec){rls[j+1]=rls[j];j--;}rls[j+1]=t;}
 rdirty=0;}
