/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine: form controls you can use (new in v0.11).
 * Focus, text editing (<input> text-like types, <textarea>), <select> (click / Up / Down cycles the options),
 * checkbox/radio (in mwe_js.h), form submission. The value lives in the DOM (the value attribute, or the text of a
 * textarea), so scripts see what the user typed; `input`, `change`, `focus`, `blur` and `submit` events fire.
 * Included by mwe_api.inc after mwe_js.h (uses its DOM helpers and event dispatch).
 *
 * Not done: text selection, clipboard copy, undo, IME composition, <input type=file/date/color/range>,
 * <select multiple>, a real drop-down list, spell-check. The caret is drawn as a '|' character inside the text. */

static int strcmp_k(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a-*b;}
static char fsm[8],fsa[2048],fsd[16384];static int fspend,fchg;

/* 1 text-like input, 2 textarea, 3 select, 4 button / submit / reset, 0 anything else */
static int fkind(N*n){
 S ty;
 if(!n||!n->tag.n)return 0;
 if(seq(n->tag,"textarea"))return 2;
 if(seq(n->tag,"select"))return 3;
 if(seq(n->tag,"button"))return 4;
 if(!seq(n->tag,"input"))return 0;
 ty=getattr(n,"type");
 if(!ty.p||!ty.n||seq(ty,"text")||seq(ty,"search")||seq(ty,"email")||seq(ty,"url")||seq(ty,"tel")||seq(ty,"number")||seq(ty,"password"))return 1;
 if(seq(ty,"submit")||seq(ty,"button")||seq(ty,"reset")||seq(ty,"image"))return 4;
 return 0;}
static int fdis(N*n){return getattr(n,"disabled").p!=0;}
static int fro(N*n){return getattr(n,"readonly").p!=0;}
static S fget(N*n){
 N*t;
 if(seq(n->tag,"textarea")){for(t=n->kid;t;t=t->nx)if(!t->tag.n&&!t->gen)return t->txt;return (S){0,0};}
 {S v=getattr(n,"value");if(!v.p)v.n=0;return v;}}
static void fset(N*n,S v){
 S p=jpers(v.p?v.p:"",v.n);
 if(seq(n->tag,"textarea"))jn_settext(n,p);
 else jn_setattr(n,(S){(char*)"value",5},p,1);
 jdirty=1;}
static void fev(N*n,const char*type,int bub){
 if(!jready)return;
 jenter();
 {JOb*e=jmkev(type,bub);jfire(n,jcstr(type),e,bub);}
 jleave();}
static void focus_set(N*n){
 N*old=focn;
 if(n==old)return;
 if(old){
  focn=0;
  if(fchg&&fkind(old)!=3)fev(old,"change",1);
  fchg=0;
  fev(old,"blur",0);}
 focn=n;focpos=0;
 if(n){
  if(fkind(n)==1||fkind(n)==2){S v=fget(n);focpos=v.n;}
  fev(n,"focus",0);}
 jdirty=1;}
void mwe_blur(void){focus_set(0);}
int mwe_focus_kind(void){return focn&&!fdis(focn)?fkind(focn):0;}

static int fprev(S v,int p){if(p<=0)return 0;p--;while(p>0&&((unsigned char)v.p[p]&0xC0)==0x80)p--;return p;}
static int fnext(S v,int p){if(p>=v.n)return v.n;p++;while(p<v.n&&((unsigned char)v.p[p]&0xC0)==0x80)p++;return p;}
static int fcps(S v,int upto){int i=0,k=0;while(i<upto&&i<v.n){u8d(v.p,v.n,&i);k++;}return k;}

/* insert text at the caret; returns 1 if the value changed */
static int fins(const char*s,int n){
 N*t=focn;S v;char b[8200];int i,o=0,k,num,ml=-1,room,cps;
 if(!t||fdis(t)||fro(t))return 0;
 k=fkind(t);
 if(k!=1&&k!=2)return 0;
 v=fget(t);
 if(focpos>v.n)focpos=v.n;
 num=k==1&&seq(getattr(t,"type"),"number");
 {S m=getattr(t,"maxlength");if(m.p&&m.n){ml=0;for(i=0;i<m.n&&isdg(m.p[i]);i++)ml=ml*10+m.p[i]-'0';}}
 if(v.n+n>8000)n=8000-v.n>0?8000-v.n:0;
 for(i=0;i<focpos;i++)b[o++]=v.p[i];
 cps=fcps(v,v.n);
 room=ml<0?1<<30:ml-cps;
 for(i=0;i<n&&o<8100;){
  int q=i,c=u8d(s,n,&q);
  if(c=='\r'){i=q;continue;}
  if(c=='\n'&&k==1){i=q;continue;}
  if(num&&!(isdg(c)||c=='.'||c=='-'||c=='+'||c=='e'||c=='E')){i=q;continue;}
  if(c!='\n'&&c<32){i=q;continue;}
  if(room<=0)break;
  room--;
  while(i<q)b[o++]=s[i++];}
 k=o-focpos;
 if(k<=0)return 0;
 for(i=focpos;i<v.n;i++)b[o++]=v.p[i];
 fset(t,(S){b,o});
 focpos+=k;fchg=1;
 fev(t,"input",1);
 return 1;}
static int fdel(int fwd){
 N*t=focn;S v;char b[8200];int a,e,i,o=0;
 if(!t||fdis(t)||fro(t))return 0;
 v=fget(t);if(focpos>v.n)focpos=v.n;
 if(fwd){a=focpos;e=fnext(v,focpos);}else{a=fprev(v,focpos);e=focpos;}
 if(a==e||v.n>8000)return 0;
 for(i=0;i<a;i++)b[o++]=v.p[i];
 for(i=e;i<v.n;i++)b[o++]=v.p[i];
 fset(t,(S){b,o});focpos=a;fchg=1;
 fev(t,"input",1);
 return 1;}

/* ---- <select> ---- */
static int fopts(N*s,N**o,int mx){
 N*c;int n=0;
 for(c=s->kid;c&&n<mx;c=c->nx){
  if(!c->tag.n)continue;
  if(seq(c->tag,"option"))o[n++]=c;
  else if(seq(c->tag,"optgroup")){N*d;for(d=c->kid;d&&n<mx;d=d->nx)if(d->tag.n&&seq(d->tag,"option"))o[n++]=d;}}
 return n;}
static S fselval(N*sel){   /* value of the selected option (for submission) */
 N*o[64];int n=fopts(sel,o,64),i,cur=0;
 if(!n)return (S){0,0};
 for(i=0;i<n;i++)if(getattr(o[i],"selected").p){cur=i;break;}
 {S v=getattr(o[cur],"value");
  if(v.p)return v;
  {N*t;for(t=o[cur]->kid;t;t=t->nx)if(!t->tag.n)return trim(t->txt);}
  return (S){0,0};}}
static int fselcycle(N*sel,int dir){
 N*o[64];int n=fopts(sel,o,64),i,cur=0;
 if(n<2)return 0;
 for(i=0;i<n;i++)if(getattr(o[i],"selected").p){cur=i;break;}
 cur=(cur+dir+n)%n;
 for(i=0;i<n;i++)jn_setbool(o[i],"selected",i==cur);
 fev(sel,"input",1);fev(sel,"change",1);
 jdirty=1;
 return 1;}

/* ---- submission ---- */
static void fenc(S s,int*o,int nl){
 int i;
 static const char hexd[]="0123456789ABCDEF";
 for(i=0;i<s.n&&*o<(int)sizeof fsd-8;i++){
  unsigned char c=(unsigned char)s.p[i];
  if(isal(c)&&c!='-'&&c!='_'){fsd[(*o)++]=(char)c;}
  else if(c=='-'||c=='_'||c=='.'||c=='*')fsd[(*o)++]=(char)c;
  else if(c==' ')fsd[(*o)++]='+';
  else if(c=='\n'&&nl){fsd[(*o)++]='%';fsd[(*o)++]='0';fsd[(*o)++]='D';fsd[(*o)++]='%';fsd[(*o)++]='0';fsd[(*o)++]='A';}
  else if(c=='\r')continue;
  else{fsd[(*o)++]='%';fsd[(*o)++]=hexd[c>>4];fsd[(*o)++]=hexd[c&15];}}}
static void fpair(int*o,S name,S val,int nl){
 if(*o)fsd[(*o)++]='&';
 fenc(name,o,0);fsd[(*o)++]='=';fenc(val,o,nl);}
static void fcollect(N*n,N*sub,int*o){
 N*c;
 for(c=n->kid;c;c=c->nx){
  S nm,ty;
  if(!c->tag.n)continue;
  if(seq(c->tag,"form"))continue;
  nm=getattr(c,"name");
  if(nm.p&&nm.n&&!fdis(c)){
   if(seq(c->tag,"input")){
    ty=getattr(c,"type");
    if(seq(ty,"checkbox")||seq(ty,"radio")){if(getattr(c,"checked").p){S v=getattr(c,"value");fpair(o,nm,v.p?v:(S){(char*)"on",2},0);}}
    else if(seq(ty,"submit")||seq(ty,"image")){if(c==sub){S v=getattr(c,"value");fpair(o,nm,v.p?v:(S){0,0},0);}}
    else if(seq(ty,"button")||seq(ty,"reset")||seq(ty,"file")){}
    else{S v=getattr(c,"value");fpair(o,nm,v.p?v:(S){0,0},0);}}
   else if(seq(c->tag,"textarea")){S v=fget(c);fpair(o,nm,v,1);}
   else if(seq(c->tag,"select")){fpair(o,nm,fselval(c),0);}
   else if(seq(c->tag,"button")){if(c==sub){S v=getattr(c,"value");fpair(o,nm,v.p?v:(S){0,0},0);}}}
  fcollect(c,sub,o);}}
static N*fform(N*n){while(n&&n!=root){if(n->tag.n&&seq(n->tag,"form"))return n;n=n->par;}return 0;}
static int fsubmit(N*form,N*sub){
 int o=0,pv=0;S m,a;
 if(jready){
  jenter();
  {JOb*e=jmkev("submit",1);jfire(form,jcstr("submit"),e,1);pv=jprevented(e);}
  jleave();}
 if(pv)return 0;
 fcollect(form,sub,&o);
 fsd[o]=0;
 m=getattr(form,"method");a=getattr(form,"action");
 fsm[0]=(m.p&&seq(m,"post"))?'P':'G';fsm[1]=0;
 {int i;for(i=0;a.p&&i<a.n&&i<(int)sizeof fsa-1;i++)fsa[i]=a.p[i];fsa[a.p?(a.n<(int)sizeof fsa-1?a.n:(int)sizeof fsa-1):0]=0;}
 fspend=1;
 return 1;}
/* host: poll after input; returns 1 once per submitted form (method "GET"/"POST", action as written, urlencoded data) */
int mwe_form_submit(const char**method,const char**action,const char**data,int*dlen){
 if(!fspend)return 0;
 fspend=0;*method=fsm[0]=='P'?"POST":"GET";*action=fsa;*data=fsd;*dlen=sl(fsd);
 return 1;}

/* ---- caret placement from a click ---- */
static void fcaretat(N*n,int x){
 int i;
 if(fkind(n)!=1)return;
 for(i=0;i<nc;i++){
  Cmd*c=&cm[i];
  if(c->k!=1||!c->on||c->on->gen!=3||c->on->par!=n||!c->t)continue;
  {S v=fget(n);int j=0,cpi=0,cur=fcps(v,focpos<=v.n?focpos:v.n),pw=seq(getattr(n,"type"),"password"),vi=0,best=v.n;
   (void)pw;
   /* c->t is the displayed text (value with a '|' at the old caret); find the codepoint boundary closest to x */
   while(j<=c->n){
    int w=wid(c->t,j,c->fs,c->tf,c->ls),nx=j;
    if(c->x+w>=x){best=cpi;break;}
    if(j>=c->n){best=cpi;break;}
    u8d(c->t,c->n,&nx);j=nx;cpi++;}
   /* the displayed '|' occupies one codepoint at index `cur` */
   if(best>cur)best--;
   for(vi=0;best>0&&vi<v.n;best--)vi=fnext(v,vi);
   focpos=vi;}
  return;}}

/* ---- called from mwe_event_click for clicks that no script prevented; returns bit 1 when the page changed ---- */
static int form_click(N*t,int x){
 N*q=t;int k;
 while(q&&q!=root&&!fkind(q))q=q->par;
 if(q&&q!=root&&fkind(q)){
  k=fkind(q);
  if(fdis(q))return 0;
  if(k==4){
   S ty=getattr(q,"type");
   int sub=seq(q->tag,"button")?(!ty.p||!ty.n||seq(ty,"submit")):seq(ty,"submit")||seq(ty,"image");
   focus_set(0);
   if(sub){N*f=fform(q);if(f)fsubmit(f,q);}
   return 1;}
  if(k==3){focus_set(q);fselcycle(q,1);return 1;}
  focus_set(q);
  fcaretat(q,x);
  return 1;}
 if(focn){focus_set(0);return 1;}
 return 0;}

/* ---- keyboard: text for the focused control (utf8) and named keys ---- */
int mwe_input_text(const char*s,int n){
 if(!focn)return 0;
 return fins(s,n)?1:0;}
int mwe_input_key(const char*key){   /* bit 1: page changed; bit 8: focus moved off the page (Tab past the last control) */
 N*t=focn;S v;int k,r=0;
 if(!t)return 0;
 k=fkind(t);
 if(!strcmp_k(key,"Tab")||!strcmp_k(key,"ShiftTab")){
  N*list[256];int n=0,i,at=-1;
  {N*stack[256];int sp=0;stack[sp++]=root;
   while(sp&&n<256){N*q=stack[--sp],*c;
    if(q!=root&&fkind(q)&&fkind(q)!=4&&!fdis(q)){if(q==t)at=n;list[n++]=q;}
    {N*kids[256];int nk=0;for(c=q->kid;c&&nk<256;c=c->nx)if(c->tag.n)kids[nk++]=c;
     while(nk>0&&sp<256)stack[sp++]=kids[--nk];}}}
  if(at<0||!n)return 0;
  i=key[0]=='S'?at-1:at+1;
  if(i<0||i>=n){focus_set(0);return 1|8;}
  focus_set(list[i]);return 1;}
 if(k==3){
  if(!strcmp_k(key,"ArrowDown")||!strcmp_k(key,"ArrowRight"))return fselcycle(t,1);
  if(!strcmp_k(key,"ArrowUp")||!strcmp_k(key,"ArrowLeft"))return fselcycle(t,-1);
  if(!strcmp_k(key,"Enter")){N*f=fform(t);if(f)fsubmit(f,0);return 1;}
  return 0;}
 if(k!=1&&k!=2)return 0;
 v=fget(t);if(focpos>v.n)focpos=v.n;
 if(!strcmp_k(key,"Backspace"))return fdel(0);
 if(!strcmp_k(key,"Delete"))return fdel(1);
 if(!strcmp_k(key,"ArrowLeft")){focpos=fprev(v,focpos);r=1;}
 else if(!strcmp_k(key,"ArrowRight")){focpos=fnext(v,focpos);r=1;}
 else if(!strcmp_k(key,"Home")){if(k==2){while(focpos>0&&v.p[focpos-1]!='\n')focpos--;}else focpos=0;r=1;}
 else if(!strcmp_k(key,"End")){if(k==2){while(focpos<v.n&&v.p[focpos]!='\n')focpos++;}else focpos=v.n;r=1;}
 else if(k==2&&(!strcmp_k(key,"ArrowUp")||!strcmp_k(key,"ArrowDown"))){
  int ls=focpos,col,i;
  while(ls>0&&v.p[ls-1]!='\n')ls--;
  col=focpos-ls;
  if(key[5]=='U'){
   if(ls==0)return 0;
   i=ls-1;while(i>0&&v.p[i-1]!='\n')i--;
   focpos=i+col<ls-1?i+col:ls-1;}
  else{
   i=focpos;while(i<v.n&&v.p[i]!='\n')i++;
   if(i>=v.n)return 0;
   {int ne=i+1;while(ne<v.n&&v.p[ne]!='\n')ne++;focpos=i+1+col<ne?i+1+col:ne;}}
  r=1;}
 else if(!strcmp_k(key,"Enter")){
  if(k==2)return fins("\n",1);
  {N*f=fform(t);if(f){fsubmit(f,0);r=1;}}}
 if(r)jdirty=1;
 return r;}
