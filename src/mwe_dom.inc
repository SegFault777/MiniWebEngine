/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine module: mwe_dom
 * DOM tree, entities, HTML tokenizer/tree builder.
 * Not a standalone translation unit: mwe.c includes the modules in dependency order (unity build). */
/* ---- DOM + HTML parser ---- */
#define JMAXSCR 64
typedef struct{N*nd;int ext;S src;}JScr;
static JScr jscr[JMAXSCR];static int jrec,jnscr;static void jrecscript(N*e);
static N*root;static S ttl;static S sshref[16];static int nss;
static N*nn(S tag){N*n=al(sizeof(N));n->tag=tag;return n;}
static void add(N*p,N*n){
 n->par=p;
 if(p->last)p->last->nx=n;else p->kid=n;
 p->last=n;
 if(n->tag.n){n->pv=p->lel;p->lel=n;}}
static int isvoid(S t){static const char*v[]={"br","hr","img","meta","link","input","wbr","area","base","col","embed","param","source","track",0};int i;for(i=0;v[i];i++)if(seq(t,v[i]))return 1;return 0;}
static int blkt(S t){static const char*v[]={"p","div","ul","ol","li","hr","pre","blockquote","table","section","article","header","footer","nav","main","aside","form","h1","h2","h3","h4","h5","h6","dl","dt","dd","figure","fieldset","details","address",0};int i;for(i=0;v[i];i++)if(seq(t,v[i]))return 1;return 0;}
static int one(S t,const char*a,const char*b,const char*c,const char*d){return seq(t,a)||(b&&seq(t,b))||(c&&seq(t,c))||(d&&seq(t,d));}
static int autoclose(S nm,S cur){
 if(seq(cur,"p"))return blkt(nm);
 if(seq(cur,"li"))return seq(nm,"li");
 if(one(cur,"dt","dd",0,0))return one(nm,"dt","dd",0,0);
 if(one(cur,"td","th",0,0))return one(nm,"td","th","tr","thead")||one(nm,"tbody","tfoot",0,0);
 if(seq(cur,"tr"))return one(nm,"tr","thead","tbody","tfoot");
 if(one(cur,"thead","tbody","tfoot",0))return one(nm,"thead","tbody","tfoot",0);
 if(seq(cur,"option"))return one(nm,"option","optgroup",0,0);
 if(seq(cur,"optgroup"))return seq(nm,"optgroup");
 return 0;}
static int istag(const char*s,int i,int n){return s[i]=='<'&&i+1<n&&(isa(s[i+1])||s[i+1]=='/'||s[i+1]=='!'||s[i+1]=='?');}
static const struct{const char*n;int c;}EN[]={{"shy",173},{"hearts",9829},{"spades",9824},{"clubs",9827},{"diams",9830},{"harr",8596},{"rArr",8658},{"lArr",8656},{"hArr",8660},{"dagger",8224},{"Dagger",8225},{"prime",8242},{"Prime",8243},{"oline",8254},{"frasl",8260},{"infin",8734},{"ne",8800},{"le",8804},{"ge",8805},{"asymp",8776},{"equiv",8801},{"sum",8721},{"prod",8719},{"radic",8730},{"int",8747},{"part",8706},{"nabla",8711},{"isin",8712},{"cap",8745},{"cup",8746},{"and",8743},{"or",8744},{"forall",8704},{"exist",8707},{"empty",8709},{"plusmn",177},{"sup1",185},{"sup2",178},{"sup3",179},{"frac14",188},{"frac12",189},{"frac34",190},{"micro",181},{"not",172},{"curren",164},{"brvbar",166},{"ensp",8194},{"emsp",8195},{"thinsp",8201},{"zwnj",8204},{"zwj",8205},{"lrm",8206},{"rlm",8207},{"alpha",945},{"beta",946},{"gamma",947},{"delta",948},{"epsilon",949},{"lambda",955},{"mu",956},{"pi",960},{"sigma",963},{"omega",969},{"Omega",937},{"Delta",916},{"Sigma",931},{"Pi",928},{"theta",952},{"phi",966},{"tau",964},{"loz",9674},{"check",10003},{"star",9734},{"starf",9733},{"phone",9742},{"hyphen",8208},{"Aogon",260},{"ccaron",269},{"scaron",353},{"Scaron",352},{"zcaron",382},{"Zcaron",381},{"OElig",338},{"oelig",339},{"Yuml",376},{"fnof",402},{"circ",710},{"tilde",732},{"AElig",198},{"Aacute",193},{"Acirc",194},{"Agrave",192},{"Aring",197},{"Atilde",195},{"Auml",196},{"Ccedil",199},{"ETH",208},{"Eacute",201},{"Ecirc",202},{"Egrave",200},{"Euml",203},{"Iacute",205},{"Icirc",206},{"Igrave",204},{"Iuml",207},{"Ntilde",209},{"Oacute",211},{"Ocirc",212},{"Ograve",210},{"Oslash",216},{"Otilde",213},{"Ouml",214},{"THORN",222},{"Uacute",218},{"Ucirc",219},{"Ugrave",217},{"Uuml",220},{"Yacute",221},{"aacute",225},{"acirc",226},{"acute",180},{"aelig",230},{"agrave",224},{"amp",38},{"apos",39},{"aring",229},{"atilde",227},{"auml",228},{"bdquo",8222},{"bull",8226},{"ccedil",231},{"cedil",184},{"cent",162},{"copy",169},{"darr",8595},{"deg",176},{"divide",247},{"eacute",233},{"ecirc",234},{"egrave",232},{"eth",240},{"euml",235},{"euro",8364},{"gt",62},{"hellip",8230},{"iacute",237},{"icirc",238},{"iexcl",161},{"igrave",236},{"iquest",191},{"iuml",239},{"laquo",171},{"larr",8592},{"ldquo",8220},{"lsaquo",8249},{"lsquo",8216},{"lt",60},{"macr",175},{"mdash",8212},{"middot",183},{"minus",8722},{"nbsp",160},{"ndash",8211},{"ntilde",241},{"oacute",243},{"ocirc",244},{"ograve",242},{"ordf",170},{"ordm",186},{"oslash",248},{"otilde",245},{"ouml",246},{"para",182},{"permil",8240},{"pound",163},{"quot",34},{"raquo",187},{"rarr",8594},{"rdquo",8221},{"reg",174},{"rsaquo",8250},{"rsquo",8217},{"sbquo",8218},{"sect",167},{"szlig",223},{"thorn",254},{"times",215},{"trade",8482},{"uacute",250},{"uarr",8593},{"ucirc",251},{"ugrave",249},{"uml",168},{"uuml",252},{"yacute",253},{"yen",165},{"yuml",255},{0,0}};
static S dec(S t){ /* in-place entity decode; output never longer than input */
 char*p=t.p;int o=0,i=0,j,c,v,k;S e;
 while(i<t.n){
  if(p[i]=='&'){
   j=i+1;while(j<t.n&&j-i<12&&p[j]!=';'&&!isws(p[j]))j++;
   if(j<t.n&&p[j]==';'){
    e=(S){p+i+1,j-i-1};c=0;
    for(k=0;EN[k].n;k++)if(e.n&&seq(e,EN[k].n)){c=EN[k].c;break;}
    if(!c&&e.n>1&&e.p[0]=='#'){
     v=0;
     if(lc(e.p[1])=='x'){for(k=2;k<e.n&&hx(e.p[k])>=0&&v<0x110000;k++)v=v*16+hx(e.p[k]);}
     else for(k=1;k<e.n&&isdg(e.p[k])&&v<0x110000;k++)v=v*10+e.p[k]-'0';
     if(v>=0x80&&v<=0x9F){static const unsigned short W[32]={0x20AC,0x81,0x201A,0x192,0x201E,0x2026,0x2020,0x2021,0x2C6,0x2030,0x160,0x2039,0x152,0x8D,0x17D,0x8F,0x90,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,0x2DC,0x2122,0x161,0x203A,0x153,0x9D,0x17E,0x178};v=W[v-0x80];}
     c=(v>0&&v<0x110000&&!(v>=0xD800&&v<0xE000))?v:0xFFFD;}
    if(c){o+=put8(p+o,c);i=j+1;continue;}}}
  p[o++]=p[i++];}
 t.n=o;return t;}
/* attribute lookup: returns S with p==0 when absent; p!=0 (maybe n==0) when present */
static S getattr(N*n,const char*name){
 S a=n->at,r={0,0};int i=0,j,ka,q;
 while(i<a.n){
  while(i<a.n&&(isws(a.p[i])||a.p[i]=='/'))i++;
  if(i>=a.n)break;
  ka=i;while(i<a.n&&!isws(a.p[i])&&a.p[i]!='='&&a.p[i]!='/')i++;
  j=i;
  while(i<a.n&&isws(a.p[i]))i++;
  r=(S){0,0};
  if(i<a.n&&a.p[i]=='='){
   i++;while(i<a.n&&isws(a.p[i]))i++;
   if(i<a.n&&(a.p[i]=='"'||a.p[i]=='\'')){q=a.p[i++];r.p=a.p+i;while(i<a.n&&a.p[i]!=q)i++;r.n=(int)(a.p+i-r.p);i++;}
   else{r.p=a.p+i;while(i<a.n&&!isws(a.p[i]))i++;r.n=(int)(a.p+i-r.p);}}
  else{r.p=(char*)"";r.n=0;}
  if(seq((S){a.p+ka,j-ka},name))return r;}
 return(S){0,0};}
static int numat(N*n,const char*name,int*pc){ /* "12" or "50%" -> value; -1 absent */
 S v=getattr(n,name);int i=0,x=0;
 if(!v.p||!v.n)return-1;
 v=trim(v);
 if(!isdg(v.p[0]))return-1;
 while(i<v.n&&isdg(v.p[i])){if(x<100000)x=x*10+v.p[i]-'0';i++;}
 if(pc)*pc=i<v.n&&v.p[i]=='%';
 return x;}
static void css(S t);
static N*parse(char*s,int n){
 N*rt=nn((S){"#",1}),*cur=rt,*e,*x;
 int i=0,dp=0,a,j,k,sc,ra,va,j0;S nm,key,v,t;char q;N*p;
 while(i<n&&!full){
  if(!istag(s,i,n)){
   a=i++;while(i<n&&!istag(s,i,n))i++;
   t=dec((S){s+a,i-a});
   if(t.n){x=nn((S){0,0});x->txt=t;add(cur,x);if(cur!=rt&&seq(cur->tag,"title"))ttl=t;}
   continue;}
  if(s[i+1]=='!'||s[i+1]=='?'){
   if(i+3<n&&s[i+2]=='-'&&s[i+3]=='-'){i+=4;while(i+2<n&&!(s[i]=='-'&&s[i+1]=='-'&&s[i+2]=='>'))i++;i+=3;}
   else{while(i<n&&s[i]!='>')i++;i++;}
   continue;}
  if(s[i+1]=='/'){
   a=i+2;j=a;while(j<n&&isal(s[j]))j++;
   nm=(S){s+a,j-a};while(j<n&&s[j]!='>')j++;i=j+1;
   p=cur;k=0;while(p!=rt&&!seqs(p->tag,nm)){p=p->par;k++;}
   if(p!=rt){cur=p->par;dp-=k+1;}
   continue;}
  a=i+1;j=a;while(j<n&&isal(s[j]))j++;
  nm=(S){s+a,j-a};e=nn(nm);sc=0;j0=j;
  for(;;){
   while(j<n&&isws(s[j]))j++;
   if(j>=n)break;
   if(s[j]=='>'){j++;break;}
   if(s[j]=='/'){sc=1;j++;continue;}
   a=j;while(j<n&&!isws(s[j])&&s[j]!='='&&s[j]!='>'&&s[j]!='/')j++;
   key=(S){s+a,j-a};
   while(j<n&&isws(s[j]))j++;
   v=(S){0,0};
   if(j<n&&s[j]=='='){
    j++;while(j<n&&isws(s[j]))j++;
    if(j<n&&(s[j]=='"'||s[j]=='\'')){q=s[j++];va=j;while(j<n&&s[j]!=q)j++;v=(S){s+va,j-va};j++;}
    else{va=j;while(j<n&&!isws(s[j])&&s[j]!='>')j++;v=(S){s+va,j-va};}}
   if(key.n){if(seq(key,"id"))e->id=v;else if(seq(key,"class"))e->cls=v;else if(seq(key,"style"))e->sty=v;else if(seq(key,"href"))e->href=dec(v);}}
  e->at=(S){s+j0,(j>j0?j-1:j)-j0};
  if(e->at.n<0)e->at.n=0;
  while(cur!=rt&&autoclose(nm,cur->tag)){cur=cur->par;dp--;}
  add(cur,e);
  if(seq(nm,"link")&&nss<16){S rel=getattr(e,"rel"),hr=getattr(e,"href");int k2;
   for(k2=0;k2+10<=rel.n;k2++)if(seq((S){rel.p+k2,10},"stylesheet")){if(hr.p&&hr.n)sshref[nss++]=dec(hr);break;}}
  if(seq(nm,"style")||seq(nm,"script")||seq(nm,"textarea")){
   ra=j;while(j<n&&!(s[j]=='<'&&j+2+nm.n<=n&&s[j+1]=='/'&&seqs((S){s+j+2,nm.n},nm)))j++;
   if(seq(nm,"style"))css((S){s+ra,j-ra});
   else if(seq(nm,"script")){if(j>ra){x=nn((S){0,0});x->txt=(S){s+ra,j-ra};add(e,x);}if(jrec)jrecscript(e);}
   else if(seq(nm,"textarea")){
    if(ra<j&&s[ra]=='\n')ra++;
    t=dec((S){s+ra,j-ra});
    if(t.n){x=nn((S){0,0});x->txt=t;add(e,x);}}
   while(j<n&&s[j]!='>')j++;
   j++;}
  else if(!sc&&!isvoid(nm)&&dp<200){cur=e;dp++;}
  i=j;}
 return rt;}
