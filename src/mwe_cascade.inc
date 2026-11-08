/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine module: mwe_cascade
 * user-agent stylesheet, presentational hints, generated content, cascade.
 * Not a standalone translation unit: mwe.c includes the modules in dependency order (unity build). */
/* ---- user-agent stylesheet ---- */
static const char UA[]=
"html,body,div,p,h1,h2,h3,h4,h5,h6,ul,ol,li,hr,pre,blockquote,section,article,header,footer,nav,main,aside,form,fieldset,legend,figure,figcaption,address,dl,dt,dd,details,summary,hgroup,menu,dir,center,optgroup,caption,search{display:block}"
"head,style,script,noscript,title,meta,link,base,template,noembed,area,datalist,param,source,track,svg,option,audio,colgroup,col,[hidden],dialog:not([open]),input[type=hidden]{display:none}"
"li{display:list-item}table{display:table;border-spacing:2px}tr{display:table-row}td,th{display:table-cell;padding:1px}thead,tbody,tfoot{display:table-row-group}"
"body{margin:8px}p,dl,blockquote,figure,pre,ul,ol,menu,hgroup,fieldset{margin:1em 0}"
"h1{font-size:2em;margin:.67em 0}h2{font-size:1.5em;margin:.83em 0}h3{font-size:1.17em;margin:1em 0}h4{margin:1.33em 0}h5{font-size:.83em;margin:1.67em 0}h6{font-size:.67em;margin:2.33em 0}"
"h1,h2,h3,h4,h5,h6,b,strong,th,dt{font-weight:bold}dt{font-weight:normal}"
"i,em,cite,dfn,var,address{font-style:italic}u,ins{text-decoration:underline}s,strike,del{text-decoration:line-through}"
"a{color:#0b57d0;text-decoration:underline}mark{background:#ff0;color:#000}small{font-size:smaller}big{font-size:larger}"
"sub{vertical-align:sub;font-size:smaller}sup{vertical-align:super;font-size:smaller}"
"q::before{content:\"\\201C\"}q::after{content:\"\\201D\"}"
"ul,ol,menu{padding-left:28px}ul,menu{list-style-type:disc}ol{list-style-type:decimal}ul ul,ol ul{list-style-type:circle}ul ul ul,ol ul ul,ul ol ul{list-style-type:square}"
"dd{margin-left:40px}blockquote{margin:1em 40px}hr{height:1px;background:#bbb;margin:12px 0}"
"pre{white-space:pre}textarea{white-space:pre-wrap}center{text-align:center}th{text-align:center}caption{text-align:center}"
"fieldset{margin:0 2px;padding:.35em .75em .625em;border:1px solid #c0c0c0}legend{padding:0 2px}"
"img{display:inline-block;min-width:16px;min-height:16px;background:#f1f1f1;border:1px solid #d0d0d0;color:#666;box-sizing:border-box}"
"iframe,video,canvas,object,embed{display:inline-block;width:300px;height:150px;background:#eee;border:1px solid #ccc;box-sizing:border-box}"
"input,button,select,textarea{display:inline-block;font-size:13px;color:#000;box-sizing:border-box}"
"input,select,textarea{background:#fff;border:1px solid #767676;border-radius:2px;padding:2px 4px}input{width:170px}"
"input[type=checkbox],input[type=radio]{width:13px;height:13px;padding:0;margin:3px 3px 3px 4px}input[type=radio]{border-radius:50%}"
"input[type=checkbox][checked],input[type=radio][checked]{background:#0b57d0;border-color:#0b57d0}"
"button,input[type=button],input[type=submit],input[type=reset]{width:auto;background:#efefef;border:1px solid #767676;border-radius:3px;padding:2px 10px;text-align:center}"
"select{padding:2px 22px 2px 6px;min-width:60px}textarea{width:200px;height:56px;vertical-align:top}"
"label{display:inline}details>summary{display:block;padding-left:18px}"
"code,kbd,samp,tt{font-size:.92em;background:#f3f3f3;padding:0 3px;border-radius:3px}kbd{border:1px solid #c8c8c8;border-bottom-width:2px;background:#fafafa}"
"abbr[title],acronym[title]{text-decoration:underline}progress,meter{display:inline-block;width:160px;height:1em;background:#e6e6e6;border-radius:4px;vertical-align:middle}"
"rp{display:none}rt{font-size:.5em}nobr{white-space:nowrap}";

/* ---- presentational attributes (HTML4-style hints) ---- */
static int hcolor(S a){
 int c;char b[8];int i;
 a=trim(a);
 if(!a.n)return-2;
 c=clr(a);
 if(c!=-2)return c;
 if(a.n==6){for(i=0;i<6;i++)if(hx(a.p[i])<0)return-2;b[0]='#';for(i=0;i<6;i++)b[i+1]=a.p[i];return clr((S){b,7});}
 return-2;}
static N*up(N*c,const char*t){N*p=c->par;while(p&&p!=root&&!seq(p->tag,t))p=p->par;return p&&p!=root?p:0;}
static void hints(N*c,St*s){
 int v,pc=0,i,k;S a;N*t;
 a=getattr(c,"bgcolor");
 if(a.p&&(k=hcolor(a))!=-2)s->bg=k;
 if(seq(c->tag,"table")){
  v=numat(c,"border",0);
  if(v>0)for(i=0;i<4;i++){s->bw[i]=v;s->bst[i]=1;s->bc[i]=0x808080;}
  v=numat(c,"cellspacing",0);if(v>=0)s->bsp=v;
  a=getattr(c,"align");if(a.p&&seq(a,"center")){s->m[1]=s->m[3]=mk(0,0,1);}}
 if(seq(c->tag,"td")||seq(c->tag,"th")){
  t=up(c,"table");
  if(t){
   v=numat(t,"cellpadding",0);if(v>=0)for(i=0;i<4;i++)s->p[i]=mk(v,0,0);
   v=numat(t,"border",0);if(v>0)for(i=0;i<4;i++){s->bw[i]=1;s->bst[i]=1;s->bc[i]=0xa0a0a0;}}
  a=getattr(c,"valign");if(a.p)s->vaa=seq(a,"top")?1:seq(a,"bottom")?3:seq(a,"middle")?2:0;
  if(getattr(c,"nowrap").p)s->ws=1;}
 if(seq(c->tag,"table")||seq(c->tag,"td")||seq(c->tag,"th")||seq(c->tag,"img")||seq(c->tag,"iframe")||seq(c->tag,"canvas")||seq(c->tag,"video")||seq(c->tag,"embed")||seq(c->tag,"object")||seq(c->tag,"hr")||seq(c->tag,"col")){
  v=numat(c,"width",&pc);if(v>=0)s->w=pc?mk(0,v,0):mk(v,0,0);
  v=numat(c,"height",&pc);if(v>=0&&!pc)s->h=mk(v,0,0);}
 if(seq(c->tag,"div")||seq(c->tag,"p")||seq(c->tag,"td")||seq(c->tag,"th")||seq(c->tag,"tr")||(c->tag.n==2&&lc(c->tag.p[0])=='h'&&isdg(c->tag.p[1]))){
  a=getattr(c,"align");if(a.p)s->al=seq(a,"center")?1:seq(a,"right")?2:0;}
 if(seq(c->tag,"font")){
  a=getattr(c,"color");if(a.p&&(k=hcolor(a))>=0)s->col=k;
  v=numat(c,"size",0);
  if(v>=1){static const int F[]={10,13,16,18,24,32,48};s->fs=F[v>7?6:v-1];}}
 if(seq(c->tag,"body")){a=getattr(c,"text");if(a.p&&(k=hcolor(a))>=0)s->col=k;}
 if(seq(c->tag,"ol")||seq(c->tag,"ul")){
  a=getattr(c,"type");
  if(a.p&&a.n==1){s->lst=a.p[0]=='1'?4:a.p[0]=='a'?6:a.p[0]=='A'?7:a.p[0]=='i'?10:a.p[0]=='I'?11:s->lst;}
  else if(a.p&&seq(a,"circle"))s->lst=2;else if(a.p&&seq(a,"square"))s->lst=3;else if(a.p&&seq(a,"disc"))s->lst=1;}
 if(seq(c->tag,"input")){
  v=numat(c,"size",0);
  if(v>0&&getattr(c,"type").n==0)s->w=mk(v*7+10,0,0);}
 if(seq(c->tag,"textarea")){
  v=numat(c,"cols",0);if(v>0)s->w=mk(v*8+12,0,0);
  v=numat(c,"rows",0);if(v>0)s->h=mk(v*16+8,0,0);}}

/* ---- generated boxes: ::before / ::after, form-control text ---- */
static N*findgen(N*c,int slot){N*g;for(g=c->kid;g;g=g->nx)if(g->gen==slot)return g;return 0;}
static N*mkgen(N*c,int slot){
 N*g=nn((S){slot==1?(char*)"::before":slot==2?(char*)"::after":(char*)"::value",slot==1?8:slot==2?7:7});
 g->gen=slot;g->par=c;g->s=al(sizeof(St));
 if(slot==1){g->nx=c->kid;c->kid=g;if(!c->last)c->last=g;}
 else{if(c->last)c->last->nx=g;else c->kid=g;c->last=g;}
 return g;}
static void gentext(N*g,S text){
 N*tn=g->kid;
 if(!tn){tn=nn((S){0,0});tn->par=g;g->kid=g->last=tn;}
 tn->txt=text;}
static void decls(N*c,St*s,int pe,int*mi,int nm,int pass){
 int i;
 for(i=0;i<nm;i++)decl(s,rls[mi[i]].d,pass);
 if(!pe&&c->sty.n)decl(s,c->sty,pass);}
static int collect(N*c,int pe,int*mi,int max){
 int i,nm=0;
 for(i=0;i<nr&&nm<max;i++)if(rmatch(c,&rls[i],pe))mi[nm++]=i;
 return nm;}
static N*findopt(N*n){
 N*c,*r,*first=0;
 for(c=n->kid;c;c=c->nx){
  if(!c->tag.n)continue;
  if(seq(c->tag,"option")){if(getattr(c,"selected").p)return c;if(!first)first=c;}
  else if(seq(c->tag,"optgroup")){r=findopt(c);if(r&&!first)first=r;}}
 return first;}
static void gens(N*c,St*s){
 int mi[256],nm,slot;N*g;St*gs;S txt;int col=-9;
 for(slot=1;slot<=2;slot++){
  nm=collect(c,slot,mi,256);
  g=findgen(c,slot);
  if(!nm){if(g){g->s->disp=D_NONE;}continue;}
  if(!g)g=mkgen(c,slot);
  gs=g->s;defst(gs,s);gs->disp=D_INLINE;cpa=s;
  decls(c,gs,1,mi,nm,2);decls(c,gs,1,mi,nm,3);
  gs->bg=gs->bg;
  if(!gs->hascont||!gs->cont.n)gs->disp=D_NONE;
  gentext(g,gs->hascont?gs->cont:(S){0,0});}
 /* text shown inside replaced / form elements */
 txt=(S){0,0};
 if(seq(c->tag,"input")){
  S ty=getattr(c,"type"),va=getattr(c,"value"),ph=getattr(c,"placeholder");
  int btn=seq(ty,"submit")||seq(ty,"button")||seq(ty,"reset");
  int tx=!ty.p||!ty.n||seq(ty,"text")||seq(ty,"search")||seq(ty,"email")||seq(ty,"url")||seq(ty,"tel")||seq(ty,"number")||seq(ty,"password");
  if(btn){txt=va.p&&va.n?va:seq(ty,"submit")?(S){(char*)"Submit",6}:seq(ty,"reset")?(S){(char*)"Reset",5}:(S){0,0};}
  else if(tx){
   if(va.p&&va.n){
    if(seq(ty,"password")){int k,o=cpp;for(k=0;k<va.n&&k<48&&cpp<(int)sizeof cpool-8;k++)cpp+=put8(cpool+cpp,0x2022);txt=(S){cpool+o,cpp-o};}
    else txt=va;}
   else if(ph.p&&ph.n){txt=ph;col=0x757575;}}}
 else if(seq(c->tag,"img")){S al_=getattr(c,"alt");if(al_.p&&al_.n&&imfor(c)<0)txt=al_;}
 else if(seq(c->tag,"select")){N*o=findopt(c);if(o){N*t;for(t=o->kid;t;t=t->nx)if(!t->tag.n){txt=trim(t->txt);break;}}}
 g=findgen(c,3);
 if(!txt.n){if(g)g->s->disp=D_NONE;return;}
 if(!g)g=mkgen(c,3);
 gs=g->s;defst(gs,s);gs->disp=D_INLINE;
 if(col!=-9)gs->col=col;
 gentext(g,txt);}

/* ---- cascade ---- */
static void styleone(N*c,St*s,St*pa){
 int mi[256],nm,i;
 cpa=pa;
 hints(c,s);
 nm=collect(c,0,mi,256);
 decls(c,s,0,mi,nm,1);
 decls(c,s,0,mi,nm,2);
 decls(c,s,0,mi,nm,3);
 for(i=0;i<4;i++)if(!s->bst[i])s->bw[i]=0;
 if((s->fl||s->pos>=2)&&s->disp==D_INLINE)s->disp=D_BLOCK;
 if(s->bs==0&&0)s->bs=0;
 if(c->par==root&&seq(c->tag,"html"))rootfs=s->fs;}
static void stl(N*n,St*pa){
 N*c;St*s;int base;
 for(c=n->kid;c;c=c->nx){
  if(!c->tag.n||c->gen)continue;
  if(!c->s)c->s=al(sizeof(St));
  s=c->s;base=nvar;
  defst(s,pa);
  styleone(c,s,pa);
  gens(c,s);
  stl(c,s);
  nvar=base;}}
static St rootst;
static void restyle(void){
 sortrules();
 cpp=0;nvar=0;rootfs=16;
 defst(&rootst,0);rootst.disp=D_BLOCK;
 root->s=&rootst;
 stl(root,&rootst);}
