/* ===================================================================================
 * MiniWebEngine JavaScript subset (v0.5)
 * Tree-walking interpreter, mark/sweep heap (collected between events), DOM bindings.
 * No libc, no malloc, no libm. Strings are UTF-8; length/index/slice count code points.
 * ================================================================================== */
#define JHEAP (24<<20)
#define JASTSZ (3<<20)
#define JMAXDEPTH 256
#ifndef JMAXSTEPS
#define JMAXSTEPS 20000000
#endif
typedef long long j64;typedef unsigned long long ju64;typedef unsigned int ju32;
typedef struct JStr JStr;typedef struct JOb JOb;typedef struct JNd JNd;
typedef struct{int t;union{double d;int b;JStr*s;JOb*o;}u;}JV;
enum{JT_U,JT_NUL,JT_B,JT_N,JT_S,JT_O};
struct JStr{int len,asc;char d[1];};
typedef struct{JStr*k;JV v;int fl;}JPr;
typedef JV(*JNF)(JV,JV*,int);
enum{OK_PLAIN,OK_ARR,OK_FN,OK_NAT,OK_ENV,OK_NODE,OK_STYLE,OK_CLS,OK_DATE,OK_ERR,OK_BOUND,OK_DOC};
struct JOb{unsigned char k,fl;short pad;int np,cp,na,ca;JOb*proto;JPr*pr;JV*it;JNd*fn;JOb*env;JV ex;N*dn;JNF nf;};
typedef struct{unsigned sz;unsigned char ty,mk,x1,x2;}JHd;
enum{BT_FREE,BT_STR,BT_OBJ,BT_PRS,BT_VALS};

/* ---- heap ---- */
static long long jhp[JHEAP/8];
static ju64 jbm[JHEAP/512];            /* one bit per 8 bytes: set where a block starts (interior pointer lookup) */
static int jtop,joom,jgcn,jlimit=JHEAP-(512<<10),jgcok,jingc;
static long jallocd,jthresh=4<<20;
static char*jstackbase;
static JHd*jfree[130],*jbig;
static JOb jdmyo;
static void jgc(void);
static int jinheap(const void*p){return(const char*)p>=(const char*)jhp&&(const char*)p<(const char*)jhp+JHEAP;}
#define JBSET(h) (jbm[((char*)(h)-(char*)jhp)>>9]|=1ULL<<((((char*)(h)-(char*)jhp)>>3)&63))
static void jputfree(JHd*h,unsigned sz){
 unsigned c=sz>>3;h->sz=sz;h->ty=BT_FREE;h->mk=0;JBSET(h);
 if(c<=128){*(JHd**)(h+1)=jfree[c];jfree[c]=h;}
 else{*(JHd**)(h+1)=jbig;jbig=h;}}
static void*jalloc(int n,int ty){
 unsigned sz,c,i;JHd*h=0,**pp;char*p;int tried=0;
 if(n<0||n>(JHEAP/2)){joom=1;return 0;}
 sz=((unsigned)n+(unsigned)sizeof(JHd)+7)&~7u;if(sz<16)sz=16;
 c=sz>>3;
 if(jgcok&&!jingc&&jallocd>jthresh)jgc();
 for(;;){
  h=0;
  if(c<=128){for(i=c;i<=128;i++)if(jfree[i]){h=jfree[i];jfree[i]=*(JHd**)(h+1);break;}}
  if(!h){for(pp=&jbig;*pp;pp=(JHd**)(*pp+1))if((*pp)->sz>=sz){h=*pp;*pp=*(JHd**)(h+1);break;}}
  if(h){if(h->sz-sz>=16){jputfree((JHd*)((char*)h+sz),h->sz-sz);h->sz=sz;}break;}
  if(jtop+(int)sz<=jlimit){h=(JHd*)((char*)jhp+jtop);jtop+=(int)sz;h->sz=sz;JBSET(h);break;}
  if(jgcok&&!jingc&&!tried){tried=1;jgc();continue;}
  joom=1;jlimit=JHEAP;
  if(jtop+(int)sz>JHEAP)return 0;
  h=(JHd*)((char*)jhp+jtop);jtop+=(int)sz;h->sz=sz;JBSET(h);break;}
 jallocd+=sz;
 h->ty=(unsigned char)ty;h->mk=0;
 p=(char*)(h+1);for(i=0;i<h->sz-sizeof(JHd);i++)p[i]=0;
 return p;}
static JHd*jhdr(const void*p){return(JHd*)p-1;}

/* ---- scalars ---- */
static JV jundef(void){JV v;v.t=JT_U;v.u.d=0;return v;}
static JV jnull(void){JV v;v.t=JT_NUL;v.u.d=0;return v;}
static JV jbool(int b){JV v;v.t=JT_B;v.u.d=0;v.u.b=b?1:0;return v;}
static JV jnum(double d){JV v;v.t=JT_N;v.u.d=d;return v;}
static JV jobj(JOb*o){JV v;v.t=JT_O;v.u.d=0;v.u.o=o;return v;}
static JV jstrv(JStr*s){JV v;v.t=JT_S;v.u.d=0;v.u.s=s;return v;}
static double jNAN(void){union{double d;ju64 u;}x;x.u=0x7ff8000000000000ULL;return x.d;}
static double jINF(void){union{double d;ju64 u;}x;x.u=0x7ff0000000000000ULL;return x.d;}
static int jisnan(double x){return x!=x;}
static int jisinf(double x){return x==jINF()||x==-jINF();}
static double jtrunc(double x){if(x!=x||x>9.2e18||x<-9.2e18)return x;return(double)(j64)x;}
static double jfloor(double x){double t=jtrunc(x);return t>x?t-1:t;}
static double jround(double x){if(x!=x||x>4.5e15||x<-4.5e15)return x;return jfloor(x+0.5);}
static double jfabs(double x){return x<0?-x:x;}
static double jmod(double a,double b){
 double q;
 if(b==0||a!=a||b!=b||jisinf(a))return jNAN();
 if(jisinf(b))return a;
 if(jfabs(a)<jfabs(b))return a;
 q=jtrunc(a/b);return a-q*b;}
static int jtoi32(double x){double m;ju32 u;
 if(x!=x||jisinf(x))return 0;
 x=jtrunc(x);m=jmod(x,4294967296.0);if(m<0)m+=4294967296.0;
 u=(ju32)m;return(int)u;}
static double jp10(int e){double r=1,b=10;if(e<0)e=-e;while(e){if(e&1)r*=b;b*=b;e>>=1;}return r;}

/* ---- strings ---- */
static JStr jemptys={0,1,{0}};
static JStr*jnewstr(const char*p,int n){
 JStr*s;int i,a=1;
 if(n<=0)return&jemptys;
 s=jalloc(8+n+1,BT_STR);
 if(!s)return&jemptys;
 s->len=n;for(i=0;i<n;i++){s->d[i]=p[i];if((unsigned char)p[i]>=0x80)a=0;}
 s->d[n]=0;s->asc=a;return s;}
static JStr*jcstr(const char*p){int n=0;while(p[n])n++;return jnewstr(p,n);}
typedef struct{char*p;int n,cap;}JSB;
static void jsb_add(JSB*b,const char*s,int n){
 int i;
 if(n<=0)return;
 if(b->n+n>b->cap){
  int nc=b->cap?b->cap*2:64;char*q;
  while(nc<b->n+n)nc*=2;
  q=(char*)jalloc(nc,BT_STR);if(!q)return;
  for(i=0;i<b->n;i++)q[i]=b->p[i];
  b->p=q;b->cap=nc;}
 for(i=0;i<n;i++)b->p[b->n+i]=s[i];
 b->n+=n;}
static void jsb_str(JSB*b,JStr*s){jsb_add(b,s->d,s->len);}
static void jsb_c(JSB*b,const char*s){int n=0;while(s[n])n++;jsb_add(b,s,n);}
static JStr*jsb_done(JSB*b){return jnewstr(b->p,b->n);}
static JV jsv(const char*p,int n){return jstrv(jnewstr(p,n));}
static JV jcv(const char*p){return jstrv(jcstr(p));}
static int jcl_(const char*s){int n=0;while(s[n])n++;return n;}
static int jkeq(const JStr*a,const JStr*b){int i;if(a==b)return 1;if(a->len!=b->len)return 0;for(i=0;i<a->len;i++)if(a->d[i]!=b->d[i])return 0;return 1;}
static int jkeqc(const JStr*a,const char*b){int i=0;while(i<a->len){if(!b[i]||a->d[i]!=b[i])return 0;i++;}return!b[i];}
static JStr*jcat(JStr*a,JStr*b){
 JStr*s;int i,n=a->len+b->len;
 if(!a->len)return b;
 if(!b->len)return a;
 s=jalloc(8+n+1,BT_STR);if(!s)return&jemptys;
 s->len=n;s->asc=a->asc&&b->asc;
 for(i=0;i<a->len;i++)s->d[i]=a->d[i];
 for(i=0;i<b->len;i++)s->d[a->len+i]=b->d[i];
 s->d[n]=0;return s;}
/* code point helpers: pointer arithmetic by code points (ASCII fast path) */
static int jcplen(JStr*s){int i,n=0;if(s->asc)return s->len;for(i=0;i<s->len;i++)if(((unsigned char)s->d[i]&0xC0)!=0x80)n++;return n;}
static int jcpoff(JStr*s,int cp){ /* byte offset of code point index cp (clamped to len) */
 int i=0,n=0;
 if(s->asc)return cp<s->len?(cp<0?0:cp):s->len;
 while(i<s->len){if(n==cp)return i;i++;while(i<s->len&&((unsigned char)s->d[i]&0xC0)==0x80)i++;n++;}
 return s->len;}
static int jdec8(const char*p,int n,int*len){
 unsigned char c=(unsigned char)p[0];int k,cp,i;
 if(c<0x80||n<2){*len=1;return c;}
 if(c>=0xF0){k=3;cp=c&7;}else if(c>=0xE0){k=2;cp=c&15;}else if(c>=0xC0){k=1;cp=c&31;}else{*len=1;return 0xFFFD;}
 if(k>=n){*len=1;return 0xFFFD;}
 for(i=1;i<=k;i++){if(((unsigned char)p[i]&0xC0)!=0x80){*len=1;return 0xFFFD;}cp=cp<<6|(p[i]&63);}
 *len=k+1;return cp;}
static int jenc8(char*p,int c){
 if(c<0||c>0x10FFFF||(c>=0xD800&&c<0xE000))c=0xFFFD;
 if(c<0x80){p[0]=(char)c;return 1;}
 if(c<0x800){p[0]=(char)(0xC0|c>>6);p[1]=(char)(0x80|(c&63));return 2;}
 if(c<0x10000){p[0]=(char)(0xE0|c>>12);p[1]=(char)(0x80|(c>>6&63));p[2]=(char)(0x80|(c&63));return 3;}
 p[0]=(char)(0xF0|c>>18);p[1]=(char)(0x80|(c>>12&63));p[2]=(char)(0x80|(c>>6&63));p[3]=(char)(0x80|(c&63));return 4;}
static JStr*jsub(JStr*s,int a,int b){ /* code point range [a,b) */
 int x,y;if(a<0)a=0;if(b<=a)return&jemptys;
 x=jcpoff(s,a);y=jcpoff(s,b);if(y<=x)return&jemptys;
 if(x==0&&y==s->len)return s;
 return jnewstr(s->d+x,y-x);}

/* ---- number <-> string ---- */
/* ---- exact decimal conversion (small bignum; shortest round-trip digits, Burger & Dybvig) ---- */
#define BNW 40
typedef struct{ju32 w[BNW];int n;}JBn;
static void bn_trim(JBn*b){while(b->n&&!b->w[b->n-1])b->n--;}
static void bn_set(JBn*b,ju64 v){int i;for(i=0;i<BNW;i++)b->w[i]=0;b->w[0]=(ju32)v;b->w[1]=(ju32)(v>>32);b->n=2;bn_trim(b);}
static void bn_mul(JBn*b,ju32 m){ju64 c=0;int i;for(i=0;i<b->n;i++){c+=(ju64)b->w[i]*m;b->w[i]=(ju32)c;c>>=32;}if(c&&b->n<BNW)b->w[b->n++]=(ju32)c;}
static void bn_shl(JBn*b,int s){
 int ws=s/32,bs=s%32,i;
 if(!b->n||s<=0)return;
 if(b->n+ws+1>BNW)return;
 if(bs){ju32 c=0;for(i=0;i<b->n;i++){ju32 t=b->w[i];b->w[i]=(t<<bs)|c;c=t>>(32-bs);}if(c)b->w[b->n++]=c;}
 if(ws){for(i=b->n-1;i>=0;i--)b->w[i+ws]=b->w[i];for(i=0;i<ws;i++)b->w[i]=0;b->n+=ws;}}
static int bn_cmp(const JBn*a,const JBn*b){int i;if(a->n!=b->n)return a->n<b->n?-1:1;for(i=a->n-1;i>=0;i--)if(a->w[i]!=b->w[i])return a->w[i]<b->w[i]?-1:1;return 0;}
static void bn_add(JBn*a,const JBn*b){ju64 c=0;int i,n=a->n>b->n?a->n:b->n;for(i=0;i<n||c;i++){if(i>=BNW)break;c+=(ju64)(i<a->n?a->w[i]:0)+(i<b->n?b->w[i]:0);a->w[i]=(ju32)c;c>>=32;if(i+1>a->n)a->n=i+1;}}
static void bn_sub(JBn*a,const JBn*b){ju64 br=0;int i;for(i=0;i<a->n;i++){ju64 t=(ju64)a->w[i]-(i<b->n?b->w[i]:0)-br;a->w[i]=(ju32)t;br=(t>>32)&1;}bn_trim(a);}
static void bn_p10(JBn*b,int k){while(k>=9){bn_mul(b,1000000000u);k-=9;}while(k>0){bn_mul(b,10);k--;}}
static ju32 bn_divs(JBn*b,ju32 d){ju64 r=0;int i;for(i=b->n-1;i>=0;i--){r=(r<<32)|b->w[i];b->w[i]=(ju32)(r/d);r%=d;}bn_trim(b);return(ju32)r;}
static int bn_bit(const JBn*b,int i){return(i/32<b->n)?(int)((b->w[i/32]>>(i%32))&1):0;}
static void bn_shr(JBn*b,int s){
 int ws=s/32,bs=s%32,i;
 if(ws>=b->n){b->n=0;return;}
 for(i=0;i<b->n-ws;i++){ju64 v=b->w[i+ws];if(bs)v=(v>>bs)|((i+ws+1<b->n)?((ju64)b->w[i+ws+1]<<(32-bs)):0);b->w[i]=(ju32)v;}
 b->n-=ws;for(i=b->n;i<b->n+ws&&i<BNW;i++)b->w[i]=0;bn_trim(b);}
static void jdecomp(double x,ju64*f,int*e){
 union{double d;ju64 u;}c;int eb;
 c.d=x;eb=(int)((c.u>>52)&0x7ff);*f=c.u&((1ULL<<52)-1);
 if(eb==0)*e=-1074;else{*f|=1ULL<<52;*e=eb-1075;}}
static int jdtoa(double x,char*dg,int*kout){ /* x>0 finite; value = 0.dg * 10^k; returns digit count */
 JBn r,s,mp,mm,t;ju64 f;int e,k,n=0,even,i,d;
 jdecomp(x,&f,&e);even=!(f&1);
 bn_set(&r,f);bn_set(&mp,1);
 if(e>=0){
  if(f!=(1ULL<<52)){bn_shl(&r,e+1);bn_set(&s,2);bn_shl(&mp,e);mm=mp;}
  else{bn_shl(&r,e+2);bn_set(&s,4);bn_shl(&mp,e+1);bn_set(&mm,1);bn_shl(&mm,e);}}
 else{
  if(e==-1074||f!=(1ULL<<52)){bn_shl(&r,1);bn_set(&s,1);bn_shl(&s,-e+1);bn_set(&mp,1);mm=mp;}
  else{bn_shl(&r,2);bn_set(&s,1);bn_shl(&s,-e+2);bn_set(&mp,2);bn_set(&mm,1);}}
 {int bl=0;ju64 tf=f;while(tf){bl++;tf>>=1;}k=(int)jfloor((double)(e+bl-1)*0.30102999566398114);}
 if(k>=0)bn_p10(&s,k);else{bn_p10(&r,-k);bn_p10(&mp,-k);bn_p10(&mm,-k);}
 for(;;){t=r;bn_add(&t,&mp);i=bn_cmp(&t,&s);if(even?i>=0:i>0){bn_mul(&s,10);k++;}else break;}
 bn_mul(&r,10);bn_mul(&mp,10);bn_mul(&mm,10);
 for(;;){
  int tc1,tc2;
  d=0;while(bn_cmp(&r,&s)>=0){bn_sub(&r,&s);d++;}
  i=bn_cmp(&r,&mm);tc1=even?i<=0:i<0;
  t=r;bn_add(&t,&mp);i=bn_cmp(&t,&s);tc2=even?i>=0:i>0;
  if(!tc1&&!tc2){dg[n++]=(char)('0'+d);if(n>=24)break;bn_mul(&r,10);bn_mul(&mp,10);bn_mul(&mm,10);continue;}
  if(tc1&&!tc2){dg[n++]=(char)('0'+d);}
  else if(!tc1&&tc2){dg[n++]=(char)('0'+d+1);}
  else{int c2;t=r;bn_shl(&t,1);c2=bn_cmp(&t,&s);dg[n++]=(char)('0'+(c2<0?d:c2>0?d+1:((d&1)?d+1:d)));}
  break;}
 *kout=k;return n;}
static int jd2s(double x,char*o){
 int n=0,i,np,dp;char dg[32];
 if(x!=x){o[0]='N';o[1]='a';o[2]='N';return 3;}
 if(x==0){o[0]='0';return 1;}
 if(x<0){o[n++]='-';x=-x;}
 if(jisinf(x)){const char*t="Infinity";for(i=0;t[i];i++)o[n++]=t[i];return n;}
 np=jdtoa(x,dg,&dp);
 if(dp>0&&dp<=21){
  for(i=0;i<dp;i++)o[n++]=i<np?dg[i]:'0';
  if(np>dp){o[n++]='.';for(i=dp;i<np;i++)o[n++]=dg[i];}}
 else if(dp<=0&&dp>-6){
  o[n++]='0';o[n++]='.';for(i=0;i<-dp;i++)o[n++]='0';for(i=0;i<np;i++)o[n++]=dg[i];}
 else{
  int ex=dp-1;char t[8];int k2=0;
  o[n++]=dg[0];if(np>1){o[n++]='.';for(i=1;i<np;i++)o[n++]=dg[i];}
  o[n++]='e';o[n++]=ex<0?'-':'+';if(ex<0)ex=-ex;
  while(ex){t[k2++]=(char)('0'+ex%10);ex/=10;}
  while(k2)o[n++]=t[--k2];}
 return n;}
static int jtofixed(double x,int d,char*o){ /* 0<=x<1e21, exact round-half-up on the true binary value */
 JBn b;ju64 f;int e,n=0,k=0,i;char t[64];
 jdecomp(x,&f,&e);
 bn_set(&b,f);bn_p10(&b,d);
 if(x==0){b.n=0;}
 else if(e>=0)bn_shl(&b,e);
 else{int rb=bn_bit(&b,-e-1);bn_shr(&b,-e);if(rb){JBn one;bn_set(&one,1);bn_add(&b,&one);}}
 while(b.n&&k<60){t[k++]=(char)('0'+bn_divs(&b,10));}
 while(k<=d)t[k++]='0';
 for(i=k-1;i>=0;i--){o[n++]=t[i];if(i==d&&d>0)o[n++]='.';}
 return n;}

static double jp2(int e){union{double d;ju64 u;}c;if(e>1023)return jINF();if(e<-1022){c.u=1ULL<<(52+e+1022>=0?52+e+1022:0);return e<-1074?0:c.d;}c.u=(ju64)(e+1023)<<52;return c.d;}
/* sign of (F*2^E) - (m*10^ex), exact */
static int jcmpexact(ju64 F,int E,ju64 m,int ex){
 JBn L,R;
 bn_set(&L,F);bn_set(&R,m);
 if(ex>=0)bn_p10(&R,ex);else bn_p10(&L,-ex);
 if(E>=0)bn_shl(&L,E);else bn_shl(&R,-E);
 return bn_cmp(&L,&R);}
static double jdecimal(ju64 m,int ex){ /* correctly rounded m*10^ex */
 double r;int it,nd=0;ju64 t=m;
 if(!m)return 0;
 while(t){nd++;t/=10;}
 if(nd+ex>310)return jINF();
 if(nd+ex<-326)return 0;
 r=(double)m;
 if(ex>0){r=ex>22?r*jp10(22)*jp10(ex-22):r*jp10(ex);}
 else if(ex<0){r=-ex>22?r/jp10(22)/jp10(-ex-22):r/jp10(-ex);}
 if(jisinf(r))r=1.7976931348623157e308;
 if(r==0)r=5e-324;
 for(it=0;it<4;it++){
  ju64 f;int e;int c;
  jdecomp(r,&f,&e);
  c=jcmpexact(2*f+1,e-1,m,ex);   /* candidate midpoint above r */
  if(c<0||(c==0&&(f&1))){      /* exact value beyond the upper midpoint -> next double */
   union{double d;ju64 u;}u;u.d=r;u.u++;r=u.d;if(jisinf(r))return r;continue;}
  c=jcmpexact(f>(1ULL<<52)||e==-1074?2*f-1:4*f-1,f>(1ULL<<52)||e==-1074?e-1:e-2,m,ex);
  if(c>0||(c==0&&(f&1))){
   union{double d;ju64 u;}u;u.d=r;if(u.u==0)return 0;u.u--;r=u.d;continue;}
  break;}
 return r;}
static double jstod(const char*s,int n,int*ok){
 int i=0,neg=0,any=0,ex=0,dropped=0,es,ev;ju64 m=0;double r;
 *ok=1;
 while(i<n&&isws(s[i]))i++;
 while(n>i&&isws(s[n-1]))n--;
 if(i>=n)return 0;
 if(s[i]=='+'||s[i]=='-'){neg=s[i]=='-';i++;}
 if(n-i>=8&&s[i]=='I'&&s[i+1]=='n'&&s[i+2]=='f'){r=jINF();return neg?-r:r;}
 if(n-i>2&&s[i]=='0'&&!neg&&(lc(s[i+1])=='x'||lc(s[i+1])=='b'||lc(s[i+1])=='o')){
  int base=lc(s[i+1])=='x'?16:lc(s[i+1])=='b'?2:8;
  r=0;i+=2;for(;i<n;i++){int h=hx(s[i]);if(h<0||h>=base){*ok=0;return jNAN();}r=r*base+h;}return r;}
 while(i<n&&isdg(s[i])){any=1;if(m<1000000000000000000ULL)m=m*10+(ju64)(s[i]-'0');else dropped++;i++;}
 if(i<n&&s[i]=='.'){i++;while(i<n&&isdg(s[i])){any=1;if(m<1000000000000000000ULL){m=m*10+(ju64)(s[i]-'0');ex--;}i++;}}
 if(!any){*ok=0;return jNAN();}
 if(i<n&&(s[i]=='e'||s[i]=='E')){
  i++;es=1;ev=0;if(i<n&&(s[i]=='+'||s[i]=='-')){es=s[i]=='-'?-1:1;i++;}
  if(i>=n||!isdg(s[i])){*ok=0;return jNAN();}
  while(i<n&&isdg(s[i])){if(ev<10000)ev=ev*10+s[i]-'0';i++;}
  ex+=es*ev;}
 if(i<n){*ok=0;return jNAN();}
 ex+=dropped;
 r=jdecimal(m,ex);
 return neg?-r:r;}
static JStr*jnum2s(double d){char b[40];int n=jd2s(d,b);return jnewstr(b,n);}

/* ---- objects ---- */
static JOb*jGlob,*jObjProto,*jFnProto,*jArrProto,*jStrProto,*jNumProto,*jBoolProto,*jErrProto,*jDateProto,*jNodeProto,*jClsProto,*jStyleProto,*jDocProto;
static JOb*jErrT,*jErrR,*jErrS,*jErrF;
static JOb*jnewob(int k,JOb*proto){
 JOb*o=jalloc((int)sizeof(JOb),BT_OBJ);
 if(!o){unsigned i;char*p=(char*)&jdmyo;for(i=0;i<sizeof jdmyo;i++)p[i]=0;jdmyo.k=OK_PLAIN;return&jdmyo;}
 o->k=(unsigned char)k;o->proto=proto;return o;}
static JV jrecv;
static JPr*jfind(JOb*o,JStr*k){int i;for(i=0;i<o->np;i++)if(jkeq(o->pr[i].k,k))return&o->pr[i];return 0;}
static JPr*jaddpr(JOb*o,JStr*k,JV v,int fl){
 JPr*n;int i,nc;
 if(o==&jdmyo)return 0;
 if(o->np>=o->cp){
  nc=o->cp?o->cp*2:4;
  n=jalloc(nc*(int)sizeof(JPr),BT_PRS);if(!n)return 0;
  for(i=0;i<o->np;i++)n[i]=o->pr[i];
  o->pr=n;o->cp=nc;}
 o->pr[o->np].k=k;o->pr[o->np].v=v;o->pr[o->np].fl=fl;
 return&o->pr[o->np++];}
static void jsetown(JOb*o,JStr*k,JV v){JPr*p=jfind(o,k);if(p)p->v=v;else jaddpr(o,k,v,0);}
static void jsetc(JOb*o,const char*k,JV v){jsetown(o,jcstr(k),v);}
static void jdelpr(JOb*o,JStr*k){int i,j;for(i=0;i<o->np;i++)if(jkeq(o->pr[i].k,k)){for(j=i;j<o->np-1;j++)o->pr[j]=o->pr[j+1];o->np--;return;}}
static JOb*jnewarr(int cap){
 JOb*a=jnewob(OK_ARR,jArrProto);
 if(cap>0&&a!=&jdmyo){a->it=jalloc(cap*(int)sizeof(JV),BT_VALS);if(a->it)a->ca=cap;}
 return a;}
static void jarrgrow(JOb*a,int need){
 JV*n;int nc,i;
 if(need<=a->ca||a==&jdmyo)return;
 nc=a->ca?a->ca*2:4;while(nc<need)nc*=2;
 if(nc>2000000){joom=1;return;}
 n=jalloc(nc*(int)sizeof(JV),BT_VALS);if(!n)return;
 for(i=0;i<a->na;i++)n[i]=a->it[i];
 a->it=n;a->ca=nc;}
static void jarrpush(JOb*a,JV v){jarrgrow(a,a->na+1);if(a->na<a->ca)a->it[a->na++]=v;}
static JOb*jnewenv(JOb*up){return jnewob(OK_ENV,up);}
static JPr*jlookup(JOb*env,JStr*name){JOb*e;JPr*p;for(e=env;e;e=e->proto){p=jfind(e,name);if(p)return p;}return 0;}

/* ---- roots registered by the host layer ---- */
#define JMAXLST 256
#define JMAXTMR 128
#define JMAXWRAP 8192
typedef struct{N*nd;JStr*type;JV fn;int cap;}JLst;
typedef struct{JV fn;int id,iv,live;j64 due;JV a[3];int na;}JTm;
static JLst jlst[JMAXLST];static int jnl;
static JTm jtm[JMAXTMR];static int jnt,jtmid=1;
static JOb*jwrap[JMAXWRAP];static int jnw;
static JV jstorev[2];
static JV jexv,jrv;
static JOb*jroot2[8];

/* ---- garbage collector: precise heap marking + conservative scan of the C stack ---- */
#define JSTK 65536
static JHd*jstk[JSTK];static int jsp,jmo;
static void jmarkp(const void*p){
 JHd*h;
 if(!p||!jinheap(p))return;
 h=jhdr(p);if(h->mk)return;
 h->mk=1;
 if(h->ty==BT_STR)return;
 if(jsp<JSTK)jstk[jsp++]=h;else jmo=1;}
static void jmarkv(JV v){if(v.t==JT_S)jmarkp(v.u.s);else if(v.t==JT_O)jmarkp(v.u.o);}
static void jscan(JHd*h){
 void*p=h+1;int n,i;
 switch(h->ty){
  case BT_OBJ:{JOb*o=(JOb*)p;jmarkp(o->proto);jmarkp(o->pr);jmarkp(o->it);jmarkp(o->env);jmarkv(o->ex);break;}
  case BT_PRS:{JPr*r=(JPr*)p;n=(int)((h->sz-8)/sizeof(JPr));for(i=0;i<n;i++){jmarkp(r[i].k);jmarkv(r[i].v);}break;}
  case BT_VALS:{JV*v=(JV*)p;n=(int)((h->sz-8)/sizeof(JV));for(i=0;i<n;i++)jmarkv(v[i]);break;}
  default:break;}}
static void jdrain(void){while(jsp>0)jscan(jstk[--jsp]);}
static JHd*jfindblk(const void*q){ /* block containing address q (interior pointers allowed), or 0 */
 unsigned long off,u,w;ju64 m;int b;JHd*h;
 if((const char*)q<(const char*)jhp||(const char*)q>=(const char*)jhp+jtop)return 0;
 off=(unsigned long)((const char*)q-(const char*)jhp);u=off>>3;w=u>>6;
 m=jbm[w]&(~0ULL>>(63-(u&63)));
 while(!m){if(!w)return 0;m=jbm[--w];}
 b=63;while(!((m>>b)&1))b--;
 u=(w<<6)+(unsigned long)b;
 h=(JHd*)((char*)jhp+(u<<3));
 if(h->ty==BT_FREE||off>=(u<<3)+h->sz)return 0;
 return h;}
#if defined(__SANITIZE_ADDRESS__)
__attribute__((no_sanitize_address))
#endif
#if defined(__GNUC__)
__attribute__((noinline))
#endif
static void jscanstack(void){
 volatile char top;char*lo=(char*)&top,*hi=jstackbase;char**p;
 if(!hi)return;
 for(p=(char**)((unsigned long)lo&~7UL);(char*)p<hi;p++){
  JHd*h=jfindblk(*p);
  if(h&&!h->mk){h->mk=1;if(h->ty!=BT_STR){if(jsp<JSTK)jstk[jsp++]=h;else jmo=1;}}}
 jdrain();}
static void jgc(void){
 char*p,*end;JHd*h,*run;int i;unsigned rs;long live=0;
#if defined(__GNUC__)
 __builtin_unwind_init();
#endif
 if(jingc)return;
 jingc=1;jsp=0;jmo=0;
 jmarkp(jGlob);jmarkp(jObjProto);jmarkp(jFnProto);jmarkp(jArrProto);jmarkp(jStrProto);jmarkp(jNumProto);jmarkp(jBoolProto);
 jmarkp(jErrProto);jmarkp(jDateProto);jmarkp(jNodeProto);jmarkp(jClsProto);jmarkp(jStyleProto);jmarkp(jDocProto);
 jmarkp(jErrT);jmarkp(jErrR);jmarkp(jErrS);jmarkp(jErrF);
 for(i=0;i<8;i++)jmarkp(jroot2[i]);
 for(i=0;i<jnl;i++){jmarkp(jlst[i].type);jmarkv(jlst[i].fn);}
 for(i=0;i<jnt;i++){int k;jmarkv(jtm[i].fn);for(k=0;k<3;k++)jmarkv(jtm[i].a[k]);}
 for(i=0;i<jnw;i++)jmarkp(jwrap[i]);
 for(i=0;i<2;i++)jmarkv(jstorev[i]);
 jmarkv(jexv);jmarkv(jrv);jmarkv(jrecv);
 jdrain();
 jscanstack();
 while(jmo){
  jmo=0;p=(char*)jhp;end=p+jtop;
  while(p<end){h=(JHd*)p;if(h->mk&&h->ty>=BT_OBJ)jscan(h);p+=h->sz;}
  jdrain();}
 for(i=0;i<130;i++)jfree[i]=0;
 jbig=0;
 for(i=0;i<(int)(sizeof jbm/sizeof jbm[0]);i++)jbm[i]=0;
 p=(char*)jhp;end=p+jtop;run=0;rs=0;
 while(p<end){
  h=(JHd*)p;
  if(h->ty==BT_FREE||!h->mk){
   if(!run){run=h;rs=0;}
   rs+=h->sz;}
  else{
   if(run){run->ty=BT_FREE;jputfree(run,rs);run=0;}
   h->mk=0;JBSET(h);live+=h->sz;}
  p+=h->sz;}
 if(run){jtop=(int)((char*)run-(char*)jhp);}
 jgcn++;jlimit=JHEAP-(512<<10);jallocd=0;
 jthresh=live/2>(4L<<20)?live/2:(4L<<20);
 if(jthresh>(12L<<20))jthresh=12L<<20;
#ifdef JGC_STRESS
 jthresh=JGC_STRESS;
#endif
 jingc=0;}

/* ---- AST (permanent per page) ---- */
enum{ND_NUM=1,ND_STR,ND_TPL,ND_ID,ND_THIS,ND_NULL,ND_TRUE,ND_FALSE,ND_ARR,ND_OBJ,ND_FUNC,ND_CLASS,ND_UN,ND_BIN,ND_LOG,ND_ASG,ND_UPD,ND_COND,
 ND_CALL,ND_NEW,ND_MEM,ND_IDX,ND_SPREAD,ND_SEQ,ND_TYPEOF,ND_DEL,ND_VOID,ND_HOLE,ND_OPT,ND_SUPER,ND_PROP,ND_PARM,ND_PATA,ND_PATO,
 ND_VAR,ND_DECL,ND_EXPR,ND_BLOCK,ND_IF,ND_FOR,ND_FORIN,ND_FOROF,ND_WHILE,ND_DO,ND_RET,ND_BRK,ND_CONT,ND_THROW,ND_TRY,ND_SWITCH,ND_CASE,
 ND_EMPTY,ND_FUNCD,ND_CLASSD,ND_LABEL,ND_VARN,ND_MEMBER,ND_REGEX};
enum{B_ADD=1,B_SUB,B_MUL,B_DIV,B_MOD,B_EXP,B_SHL,B_SHR,B_USHR,B_AND,B_OR,B_XOR,B_EQ,B_NE,B_SEQ,B_SNE,B_LT,B_GT,B_LE,B_GE,B_IN,B_INSTOF,
 L_AND=40,L_OR,L_NUL};
#define JF_ARROW 1
#define JF_EXPR 2
#define JF_ARGS 4
#define JF_LEX 8
#define JF_PREFIX 1
#define JF_OPT 1
#define JF_COMP 2
#define JF_STATIC 4
#define JF_REST 1
#define JF_SHORT 8
struct JNd{unsigned char t,op,fl,x;int line;JNd*a,*b,*c,*d,*nx;JStr*s;double num;};
static char jast[JASTSZ];static int jastp;
static JNd*jnd(int t){
 JNd*n;unsigned i;
 jastp=(jastp+7)&~7;
 if(jastp+(int)sizeof(JNd)>JASTSZ){static JNd dm;char*p=(char*)&dm;for(i=0;i<sizeof dm;i++)p[i]=0;return&dm;}
 n=(JNd*)(jast+jastp);jastp+=(int)sizeof(JNd);n->t=(unsigned char)t;return n;}
#define JIH 512
typedef struct JIn{JStr*s;struct JIn*nx;}JIn;
static JIn*jih[JIH];
static JStr*jintern(const char*p,int n){
 unsigned h=5381;int i;JIn*e;JStr*s;
 for(i=0;i<n;i++)h=h*33+(unsigned char)p[i];
 for(e=jih[h%JIH];e;e=e->nx)if(e->s->len==n){for(i=0;i<n&&e->s->d[i]==p[i];i++);if(i==n)return e->s;}
 jastp=(jastp+7)&~7;
 if(jastp+8+n+1+(int)sizeof(JIn)+8>JASTSZ)return&jemptys;
 s=(JStr*)(jast+jastp);jastp+=8+n+1;s->len=n;s->asc=1;
 for(i=0;i<n;i++){s->d[i]=p[i];if((unsigned char)p[i]>=0x80)s->asc=0;}
 s->d[n]=0;
 jastp=(jastp+7)&~7;
 e=(JIn*)(jast+jastp);jastp+=(int)sizeof(JIn);e->s=s;e->nx=jih[h%JIH];jih[h%JIH]=e;
 return s;}
static JStr*jintc(const char*p){int n=0;while(p[n])n++;return jintern(p,n);}

/* ---- lexer ---- */
enum{TK_EOF,TK_NUM,TK_STR,TK_ID,TK_TPL,TK_P};
static const char*jsrc;static int jpos,jend,jline,jtk,jnlb,jpfail;
static JStr*jtstr;static double jtnum;static char jtp[8];
static int jtstart,jtend; /* template body range */
static char jperrm[96];static int jperrl;
static JNd*jfnc; /* current function node (var collection) */
static void jperr(const char*m){
 int i=0;
 if(!jpfail){
  jpfail=1;jperrl=jline;
  while(m[i]&&i<94){jperrm[i]=m[i];i++;}
  jperrm[i]=0;}
 jtk=TK_EOF;jpos=jend;}
static int jidst(int c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'||c=='$'||c>=0x80;}
static int jidch(int c){return jidst(c)||(c>='0'&&c<='9');}
static char jlexb[1<<20];
static int jbraceend(int i);
static int jtplend(int i){
 while(i<jend){
  if(jsrc[i]=='\\'){i+=2;continue;}
  if(jsrc[i]=='`')return i+1;
  if(jsrc[i]=='$'&&i+1<jend&&jsrc[i+1]=='{'){i=jbraceend(i+2);continue;}
  i++;}
 return jend;}
static int jbraceend(int i){
 int d=1;char q;
 while(i<jend){
  q=jsrc[i];
  if(q=='{')d++;
  else if(q=='}'){d--;if(!d)return i+1;}
  else if(q=='\''||q=='"'){i++;while(i<jend&&jsrc[i]!=q){if(jsrc[i]=='\\')i++;i++;}}
  else if(q=='`'){i=jtplend(i+1);continue;}
  else if(q=='/'&&i+1<jend&&jsrc[i+1]=='/'){while(i<jend&&jsrc[i]!='\n')i++;continue;}
  else if(q=='/'&&i+1<jend&&jsrc[i+1]=='*'){i+=2;while(i+1<jend&&!(jsrc[i]=='*'&&jsrc[i+1]=='/'))i++;i+=2;continue;}
  i++;}
 return jend;}
static int jesc(const char*s,int*i,int e,char*o){ /* parse escape at s[*i] (after the backslash); returns bytes written */
 int c,v,k,n;
 if(*i>=e)return 0;
 c=s[(*i)++];
 switch(c){
  case 'n':o[0]='\n';return 1;case 't':o[0]='\t';return 1;case 'r':o[0]='\r';return 1;
  case 'b':o[0]='\b';return 1;case 'f':o[0]='\f';return 1;case 'v':o[0]='\v';return 1;
  case '0':if(*i<e&&isdg(s[*i])){o[0]='0';return 1;}o[0]=0;return 1;
  case '\r':if(*i<e&&s[*i]=='\n')(*i)++;return 0;
  case '\n':return 0;
  case 'x':v=0;for(k=0;k<2&&*i<e&&hx(s[*i])>=0;k++)v=v*16+hx(s[(*i)++]);return jenc8(o,v);
  case 'u':
   v=0;
   if(*i<e&&s[*i]=='{'){(*i)++;while(*i<e&&s[*i]!='}'){if(hx(s[*i])>=0&&v<0x110000)v=v*16+hx(s[*i]);(*i)++;}(*i)++;return jenc8(o,v);}
   for(k=0;k<4&&*i<e&&hx(s[*i])>=0;k++)v=v*16+hx(s[(*i)++]);
   if(v>=0xD800&&v<0xDC00&&*i+5<e&&s[*i]=='\\'&&s[*i+1]=='u'){
    int w=0,j=*i+2;
    for(k=0;k<4&&j<e&&hx(s[j])>=0;k++)w=w*16+hx(s[j++]);
    if(w>=0xDC00&&w<0xE000){*i=j;v=0x10000+((v-0xD800)<<10)+(w-0xDC00);}}
   n=jenc8(o,v);return n;
  default:o[0]=(char)c;return 1;}}
static void jnext(void){
 int c,i;
 jnlb=0;
 for(;;){
  while(jpos<jend&&(isws(jsrc[jpos])||(unsigned char)jsrc[jpos]==0xC2||(unsigned char)jsrc[jpos]==0xA0)){
   if(jsrc[jpos]=='\n'){jnlb=1;jline++;}
   if((unsigned char)jsrc[jpos]==0xC2&&!(jpos+1<jend&&(unsigned char)jsrc[jpos+1]==0xA0))break;
   jpos++;}
  if(jpos+1<jend&&jsrc[jpos]=='/'&&jsrc[jpos+1]=='/'){while(jpos<jend&&jsrc[jpos]!='\n')jpos++;continue;}
  if(jpos+1<jend&&jsrc[jpos]=='/'&&jsrc[jpos+1]=='*'){
   jpos+=2;while(jpos+1<jend&&!(jsrc[jpos]=='*'&&jsrc[jpos+1]=='/')){if(jsrc[jpos]=='\n'){jnlb=1;jline++;}jpos++;}
   jpos+=2;continue;}
  if(jpos+3<jend&&jsrc[jpos]=='<'&&jsrc[jpos+1]=='!'&&jsrc[jpos+2]=='-'&&jsrc[jpos+3]=='-'){while(jpos<jend&&jsrc[jpos]!='\n')jpos++;continue;}
  break;}
 if(jpos>=jend){jtk=TK_EOF;return;}
 c=(unsigned char)jsrc[jpos];
 if(jidst(c)){
  i=jpos;while(jpos<jend&&jidch((unsigned char)jsrc[jpos]))jpos++;
  jtstr=jintern(jsrc+i,jpos-i);jtk=TK_ID;return;}
 if(isdg(c)||(c=='.'&&jpos+1<jend&&isdg(jsrc[jpos+1]))){
  double r=0;
  i=jpos;
  if(c=='0'&&jpos+1<jend&&(lc(jsrc[jpos+1])=='x'||lc(jsrc[jpos+1])=='b'||lc(jsrc[jpos+1])=='o')){
   int base=lc(jsrc[jpos+1])=='x'?16:lc(jsrc[jpos+1])=='b'?2:8;
   jpos+=2;
   while(jpos<jend&&(hx(jsrc[jpos])>=0||jsrc[jpos]=='_')){if(jsrc[jpos]!='_')r=r*base+hx(jsrc[jpos]);jpos++;}
   jtnum=r;jtk=TK_NUM;return;}
  {char b[64];int n=0,ok;
   while(jpos<jend&&(isdg(jsrc[jpos])||jsrc[jpos]=='.'||jsrc[jpos]=='_'||((lc(jsrc[jpos])=='e')&&jpos+1<jend&&(isdg(jsrc[jpos+1])||((jsrc[jpos+1]=='+'||jsrc[jpos+1]=='-')&&jpos+2<jend&&isdg(jsrc[jpos+2])))))){
    if(lc(jsrc[jpos])=='e'){if(n<62)b[n++]='e';jpos++;if(jsrc[jpos]=='+'||jsrc[jpos]=='-'){if(n<62)b[n++]=jsrc[jpos];jpos++;}continue;}
    if(jsrc[jpos]!='_'&&n<62)b[n++]=jsrc[jpos];
    jpos++;}
   jtnum=jstod(b,n,&ok);jtk=TK_NUM;
   if(jpos<jend&&jsrc[jpos]=='n'){jpos++;}
   return;}}
 if(c=='"'||c=='\''){
  char*o=jlexb;int n=0;
  i=++jpos;
  while(jpos<jend&&jsrc[jpos]!=c){
   if(n>(int)sizeof jlexb-8){jperr("string literal too long");return;}
   if(jsrc[jpos]=='\n'){jperr("unterminated string");return;}
   if(jsrc[jpos]=='\\'){jpos++;n+=jesc(jsrc,&jpos,jend,o+n);}
   else o[n++]=jsrc[jpos++];}
  jpos++;
  jtstr=jintern(o,n);
  jtk=TK_STR;(void)i;return;}
 if(c=='`'){jtstart=jpos+1;jpos=jtplend(jpos+1);jtend=jpos-1;if(jtend<jtstart)jtend=jtstart;jtk=TK_TPL;return;}
 {static const char*P[]={">>>=","...","===","!==","**=","<<=",">>=",">>>","&&=","||=","\?\?=","=>","==","!=","<=",">=","&&","||","??","?.","++","--","+=","-=","*=","/=","%=","&=","|=","^=","<<",">>","**",0};
  int k,l;
  for(k=0;P[k];k++){
   for(l=0;P[k][l]&&jpos+l<jend&&jsrc[jpos+l]==P[k][l];l++);
   if(!P[k][l]){
    if(P[k][0]=='?'&&P[k][1]=='.'&&jpos+2<jend&&isdg(jsrc[jpos+2]))continue;
    for(i=0;i<l;i++)jtp[i]=P[k][i];
    jtp[l]=0;jpos+=l;jtk=TK_P;return;}}
  jtp[0]=(char)c;jtp[1]=0;jpos++;jtk=TK_P;}}
static int jis(const char*p){int i=0;if(jtk!=TK_P)return 0;while(p[i]){if(jtp[i]!=p[i])return 0;i++;}return jtp[i]==0;}
static int jkw(const char*p){return jtk==TK_ID&&jkeqc(jtstr,p);}
static int jeat(const char*p){if(jis(p)){jnext();return 1;}return 0;}
static void jexpect(const char*p){if(!jeat(p)){char m[64];int i=0,k=0;const char*t="expected '";while(t[i])m[k++]=t[i++];for(i=0;p[i]&&k<60;i++)m[k++]=p[i];m[k++]='\'';m[k]=0;jperr(m);}}
static void jsemi(void){
 if(jis(";")){jnext();return;}
 if(jis("}")||jtk==TK_EOF||jnlb)return;
 jperr("unexpected token");}
static JNd*jn2(int t,JNd*a,JNd*b){JNd*n=jnd(t);n->a=a;n->b=b;n->line=jline;return n;}
static JNd*jpexpr(void);static JNd*jpassign(void);static JNd*jpstmt(void);static JNd*jpunary(void);static JNd*jpprimary(void);
static JNd*jpblock(void);static JNd*jpfunc(int arrow,JNd*pre);static JNd*jpclass(int decl);
static int jisres(JStr*s){
 static const char*R[]={"break","case","catch","class","const","continue","default","delete","do","else","finally","for","function","if","in","instanceof","let","new","return","switch","this","throw","try","typeof","var","void","while","null","true","false","super",0};
 int i;for(i=0;R[i];i++)if(jkeqc(s,R[i]))return 1;return 0;}
static JNd*jptpl(void){
 int i=jtstart,e=jtend,st,n;char*buf;JNd*head=0,*tail=0,*sn,*en;
 buf=(char*)jalloc((e-i)*2+16,BT_STR);
 if(!buf){jperr("out of memory");return jnd(ND_NULL);}
 n=0;st=i;
 for(;;){
  if(i>=e||(jsrc[i]=='$'&&i+1<e&&jsrc[i+1]=='{')){
   sn=jnd(ND_STR);sn->s=jintern(buf,n);n=0;
   if(tail)tail->nx=sn;else head=sn;tail=sn;
   if(i>=e)break;
   {int j=jbraceend(i+2)-1,sp=jpos,se=jend,sl=jline,stk=jtk,snl=jnlb;JStr*ss=jtstr;double sn2=jtnum;char sp2[8];int q;
    for(q=0;q<8;q++)sp2[q]=jtp[q];
    jpos=i+2;jend=j;jnext();
    en=jpassign();
    if(jtk!=TK_EOF)jperr("bad template expression");
    jpos=sp;jend=se;jline=sl;jtk=stk;jnlb=snl;jtstr=ss;jtnum=sn2;for(q=0;q<8;q++)jtp[q]=sp2[q];
    if(jpfail){jhdr(buf)->ty=BT_FREE;return jnd(ND_NULL);}
    tail->nx=en;tail=en;
    i=j+1;}
   continue;}
  if(jsrc[i]=='\\'){i++;n+=jesc(jsrc,&i,e,buf+n);continue;}
  if(jsrc[i]=='\n')jline++;
  buf[n++]=jsrc[i++];}
 jhdr(buf)->ty=BT_FREE;jhdr(buf)->mk=0;
 (void)st;
 sn=jnd(ND_TPL);sn->a=head;sn->line=jline;
 return sn;}
static JNd*jtopat(JNd*e){ /* expression -> assignment pattern */
 JNd*p,*c,*t,*h=0,*tl=0;
 if(!e)return e;
 if(e->t==ND_ARR){
  p=jnd(ND_PATA);
  for(c=e->a;c;c=c->nx){
   JNd*el=jnd(ND_PARM);
   if(c->t==ND_HOLE){el->a=0;}
   else if(c->t==ND_SPREAD){el->fl=JF_REST;el->a=jtopat(c->a);}
   else if(c->t==ND_ASG&&c->op==0){el->a=jtopat(c->a);el->b=c->b;}
   else el->a=jtopat(c);
   if(tl)tl->nx=el;else h=el;tl=el;}
  p->a=h;return p;}
 if(e->t==ND_OBJ){
  p=jnd(ND_PATO);
  for(c=e->a;c;c=c->nx){
   JNd*el=jnd(ND_PARM);
   if(c->t==ND_SPREAD){el->fl=JF_REST;el->a=jtopat(c->a);}
   else{
    t=c->b;el->s=c->s;el->c=c->a;el->x=c->fl&JF_COMP;
    if(t&&t->t==ND_ASG&&t->op==0){el->a=jtopat(t->a);el->b=t->b;}
    else el->a=jtopat(t);}
   if(tl)tl->nx=el;else h=el;tl=el;}
  p->a=h;return p;}
 return e;}
static void jaddvar(JStr*s){JNd*v;if(!jfnc)return;v=jnd(ND_VARN);v->s=s;v->nx=jfnc->c;jfnc->c=v;}
static void jpatvars(JNd*p){
 JNd*c;
 if(!p)return;
 if(p->t==ND_ID)jaddvar(p->s);
 else if(p->t==ND_PATA||p->t==ND_PATO)for(c=p->a;c;c=c->nx)jpatvars(c->a);}
static JNd*jpbind(void){ /* binding target: identifier or pattern */
 JNd*n;
 if(jis("[")||jis("{")){n=jpprimary();return jtopat(n);}
 if(jtk!=TK_ID||jisres(jtstr)){jperr("expected identifier");return jnd(ND_NULL);}
 n=jnd(ND_ID);n->s=jtstr;n->line=jline;jnext();return n;}
static JNd*jpargs(void){
 JNd*h=0,*t=0,*a;
 while(!jis(")")&&jtk!=TK_EOF){
  if(jeat("...")){a=jn2(ND_SPREAD,jpassign(),0);}
  else a=jpassign();
  if(t)t->nx=a;else h=a;t=a;
  if(!jeat(","))break;}
 jexpect(")");return h;}
static int jarrowahead(void){ /* at '(' : is it an arrow function head? */
 int sp=jpos,se=jend,sl=jline,stk=jtk,snl=jnlb,d=0,r=0,q,sf=jpfail;JStr*ss=jtstr;double sn=jtnum;char sp2[8];int ts=jtstart,te=jtend;
 for(q=0;q<8;q++)sp2[q]=jtp[q];
 for(;;){
  if(jis("("))d++;
  else if(jis(")")){d--;if(!d){jnext();r=jis("=>");break;}}
  else if(jtk==TK_EOF)break;
  jnext();}
 jpos=sp;jend=se;jline=sl;jtk=stk;jnlb=snl;jtstr=ss;jtnum=sn;jtstart=ts;jtend=te;for(q=0;q<8;q++)jtp[q]=sp2[q];
 jpfail=sf;
 return r;}
static JNd*jpparams(void){ /* after '(' consumed */
 JNd*h=0,*t=0,*p;
 while(!jis(")")&&jtk!=TK_EOF){
  p=jnd(ND_PARM);
  if(jeat("..."))p->fl=JF_REST;
  p->a=jpbind();
  if(jeat("="))p->b=jpassign();
  if(t)t->nx=p;else h=p;t=p;
  if(!jeat(","))break;}
 jexpect(")");return h;}
static JNd*jpfunc(int arrow,JNd*pre){
 JNd*f=jnd(ND_FUNC),*sv=jfnc,*p;
 f->line=jline;f->fl=(unsigned char)(arrow?JF_ARROW:0);
 if(!arrow){
  if(jtk==TK_ID&&!jis("(")){f->s=jtstr;jnext();}
  jexpect("(");f->a=jpparams();}
 else{
  if(pre){f->a=pre;}
  }
 jfnc=f;
 if(arrow)jexpect("=>");
 if(jis("{")){f->b=jpblock();}
 else if(arrow){f->fl|=JF_EXPR;f->b=jpassign();}
 else jperr("expected function body");
 jfnc=sv;
 (void)p;
 return f;}
static JNd*jpclass(int decl){
 JNd*c=jnd(ND_CLASS),*h=0,*t=0,*m;JStr*nm;
 c->line=jline;
 if(jtk==TK_ID&&!jkw("extends")&&!jis("{")){c->s=jtstr;jnext();}
 else if(decl)jperr("class name expected");
 if(jkw("extends")){jnext();c->a=jpunary();
  {JNd*e=c->a;while(jis(".")||jis("(")||jis("[")){ /* allow member/call chains in extends */
   if(jis(".")){jnext();JNd*mm=jn2(ND_MEM,e,0);mm->s=jtstr;jnext();e=mm;}
   else if(jis("(")){jnext();JNd*cc=jn2(ND_CALL,e,jpargs());e=cc;}
   else{jnext();JNd*ix=jn2(ND_IDX,e,jpexpr());jexpect("]");e=ix;}}
   c->a=e;}}
 jexpect("{");
 while(!jis("}")&&jtk!=TK_EOF){
  int st=0;
  if(jeat(";"))continue;
  m=jnd(ND_PROP);m->line=jline;
  if(jkw("static")){jnext();if(jis("(")||jis("=")){nm=jintc("static");m->s=nm;}else st=1;}
  if(st)m->fl|=JF_STATIC;
  if(!m->s){
   JStr*first=0;
   if(jtk==TK_ID&&(jkeqc(jtstr,"get")||jkeqc(jtstr,"set"))){first=jtstr;jnext();
    if(jis("(")||jis("=")||jis(";")||jis("}")){m->s=first;first=0;}
    else m->x=(unsigned char)(jkeqc(first,"get")?1:2);}
   if(!m->s){
    if(jis("[")){jnext();m->a=jpassign();jexpect("]");m->fl|=JF_COMP;}
    else if(jtk==TK_ID||jtk==TK_STR){m->s=jtstr;jnext();}
    else if(jtk==TK_NUM){char b[40];int n=jd2s(jtnum,b);m->s=jintern(b,n);jnext();}
    else{jperr("bad class member");break;}}}
  if(jis("(")){
   JNd*f=jnd(ND_FUNC),*sv=jfnc;f->line=jline;jnext();f->a=jpparams();jfnc=f;f->b=jpblock();jfnc=sv;
   m->b=f;m->op=1;}
  else{
   if(jeat("=")){JNd*f=jnd(ND_FUNC),*sv=jfnc;f->fl=JF_ARROW|JF_EXPR;jfnc=f;f->b=jpassign();jfnc=sv;m->b=f;}
   m->op=2;jsemi();}
  if(t)t->nx=m;else h=m;t=m;}
 jexpect("}");
 c->b=h;return c;}
static JNd*jpprimary(void){
 JNd*n;
 if(jtk==TK_NUM){n=jnd(ND_NUM);n->num=jtnum;n->line=jline;jnext();return n;}
 if(jtk==TK_STR){n=jnd(ND_STR);n->s=jtstr;n->line=jline;jnext();return n;}
 if(jtk==TK_TPL){n=jptpl();jnext();return n;}
 if(jtk==TK_ID){
  if(jkw("function")){jnext();return jpfunc(0,0);}
  if(jkw("class")){jnext();return jpclass(0);}
  if(jkw("this")){n=jnd(ND_THIS);jnext();return n;}
  if(jkw("super")){n=jnd(ND_SUPER);n->line=jline;jnext();return n;}
  if(jkw("null")){n=jnd(ND_NULL);jnext();return n;}
  if(jkw("true")){n=jnd(ND_TRUE);jnext();return n;}
  if(jkw("false")){n=jnd(ND_FALSE);jnext();return n;}
  if(jisres(jtstr)){jperr("unexpected keyword");return jnd(ND_NULL);}
  n=jnd(ND_ID);n->s=jtstr;n->line=jline;
  if(jkeqc(jtstr,"arguments")&&jfnc)jfnc->fl|=JF_ARGS;
  jnext();
  if(jis("=>")){JNd*p=jnd(ND_PARM);p->a=n;return jpfunc(1,p);}
  return n;}
 if(jis("(")){
  if(jarrowahead()){jnext();JNd*pr=jpparams();return jpfunc(1,pr);}
  jnext();n=jpexpr();jexpect(")");
  if(n&&n->t==ND_ID)n->x|=1;
  return n;}
 if(jis("[")){
  JNd*h=0,*t=0,*e;
  n=jnd(ND_ARR);n->line=jline;jnext();
  while(!jis("]")&&jtk!=TK_EOF){
   if(jis(",")){e=jnd(ND_HOLE);jnext();}
   else{
    if(jeat("..."))e=jn2(ND_SPREAD,jpassign(),0);else e=jpassign();
    if(!jis("]"))jexpect(",");}
   if(t)t->nx=e;else h=e;t=e;}
  jexpect("]");n->a=h;return n;}
 if(jis("{")){
  JNd*h=0,*t=0,*p;
  n=jnd(ND_OBJ);n->line=jline;jnext();
  while(!jis("}")&&jtk!=TK_EOF){
   if(jeat("...")){p=jn2(ND_SPREAD,jpassign(),0);}
   else{
    p=jnd(ND_PROP);p->line=jline;
    if(jtk==TK_ID&&(jkeqc(jtstr,"get")||jkeqc(jtstr,"set"))){
     JStr*first=jtstr;int sp=jpos,sl=jline,stk=jtk,snl=jnlb;
     jnext();
     if(jis(",")||jis(":")||jis("(")||jis("}")||jis("=")){p->s=first;}
     else{(void)sp;(void)sl;(void)stk;(void)snl;p->x=(unsigned char)(jkeqc(first,"get")?1:2);}}
    if(!p->s){
     if(jis("[")){jnext();p->a=jpassign();jexpect("]");p->fl|=JF_COMP;}
     else if(jtk==TK_ID||jtk==TK_STR){p->s=jtstr;jnext();}
     else if(jtk==TK_NUM){char b[40];int k=jd2s(jtnum,b);p->s=jintern(b,k);jnext();}
     else{jperr("bad object key");break;}}
    if(jis("(")){
     JNd*f=jnd(ND_FUNC),*sv=jfnc;f->line=jline;jnext();f->a=jpparams();jfnc=f;f->b=jpblock();jfnc=sv;p->b=f;}
    else if(jeat(":"))p->b=jpassign();
    else{
     JNd*id=jnd(ND_ID);id->s=p->s;id->line=jline;
     if(jkeqc(p->s,"arguments")&&jfnc)jfnc->fl|=JF_ARGS;
     if(jis("=")){jnext();p->b=jn2(ND_ASG,id,jpassign());}
     else p->b=id;}}
   if(t)t->nx=p;else h=p;t=p;
   if(!jis("}"))jexpect(",");}
  jexpect("}");n->a=h;return n;}
 if(jis("/")||jis("/=")){
  int st=jpos-(int)(jtp[1]?1:0)-0,i=st,incls=0;JNd*rx;JStr*pat,*fl;
  if(jtp[1])st=jpos-1;else st=jpos;
  i=st;
  while(i<jend&&jsrc[i]!='\n'){
   if(jsrc[i]=='\\')i+=2;
   else if(jsrc[i]=='[')incls=1,i++;
   else if(jsrc[i]==']')incls=0,i++;
   else if(jsrc[i]=='/'&&!incls)break;
   else i++;}
  if(i>=jend||jsrc[i]!='/'){jperr("unterminated regular expression");return jnd(ND_NULL);}
  pat=jintern(jsrc+st,i-st);
  {int fs=++i;while(i<jend&&jidch((unsigned char)jsrc[i]))i++;fl=jintern(jsrc+fs,i-fs);}
  jpos=i;
  rx=jnd(ND_REGEX);rx->s=pat;rx->a=jnd(ND_STR);rx->a->s=fl;rx->line=jline;
  jnext();return rx;}
 jperr("unexpected token");return jnd(ND_NULL);}
static JNd*jpmember(void);
static JNd*jpnewee(void){
 JNd*c,*e;
 if(jkw("new")){
  jnext();c=jpnewee();e=jnd(ND_NEW);e->line=jline;e->a=c;
  if(jis("("))e->b=(jnext(),jpargs());
  c=e;}
 else c=jpprimary();
 for(;;){
  if(jis(".")){jnext();e=jn2(ND_MEM,c,0);e->line=jline;if(jtk!=TK_ID){jperr("expected property name");break;}e->s=jtstr;jnext();c=e;}
  else if(jis("[")){jnext();e=jn2(ND_IDX,c,jpexpr());e->line=jline;jexpect("]");c=e;}
  else break;}
 return c;}
static JNd*jpmember(void){
 JNd*e,*c;int chain=0;
 if(jkw("new")){
  jnext();
  if(jis(".")){jnext();jnext();return jnd(ND_NULL);} /* new.target: unsupported -> undefined-ish */
  c=jpnewee();
  e=jnd(ND_NEW);e->line=jline;e->a=c;
  if(jis("("))e->b=(jnext(),jpargs());
  c=e;
  goto tail;}
 c=jpprimary();
 tail:
 for(;;){
  if(jis(".")){
   jnext();e=jn2(ND_MEM,c,0);e->line=jline;
   if(jtk!=TK_ID){jperr("expected property name");break;}
   e->s=jtstr;jnext();c=e;}
  else if(jis("?.")){
   jnext();chain=1;
   if(jis("(")){jnext();e=jn2(ND_CALL,c,jpargs());e->fl|=JF_OPT;e->line=jline;}
   else if(jis("[")){jnext();e=jn2(ND_IDX,c,jpexpr());jexpect("]");e->fl|=JF_OPT;e->line=jline;}
   else{e=jn2(ND_MEM,c,0);e->fl|=JF_OPT;e->line=jline;if(jtk!=TK_ID){jperr("expected property name");break;}e->s=jtstr;jnext();}
   c=e;}
  else if(jis("[")){jnext();e=jn2(ND_IDX,c,jpexpr());e->line=jline;jexpect("]");c=e;}
  else if(jis("(")){jnext();e=jn2(ND_CALL,c,jpargs());e->line=jline;c=e;}
  else break;}
 if(chain){e=jn2(ND_OPT,c,0);return e;}
 return c;}
static JNd*jppostfix(void){
 JNd*e=jpmember(),*u;
 if((jis("++")||jis("--"))&&!jnlb){
  u=jn2(ND_UPD,e,0);u->op=jis("++")?1:2;u->fl=0;u->line=jline;jnext();return u;}
 return e;}
static JNd*jpunary(void){
 JNd*n;int op;
 if(jtk==TK_P){
  if(jis("!")||jis("-")||jis("+")||jis("~")){op=jtp[0];jnext();n=jn2(ND_UN,jpunary(),0);n->op=(unsigned char)op;return n;}
  if(jis("++")||jis("--")){op=jis("++")?1:2;jnext();n=jn2(ND_UPD,jpunary(),0);n->op=(unsigned char)op;n->fl=JF_PREFIX;n->line=jline;return n;}}
 if(jtk==TK_ID){
  if(jkw("typeof")){jnext();return jn2(ND_TYPEOF,jpunary(),0);}
  if(jkw("void")){jnext();return jn2(ND_VOID,jpunary(),0);}
  if(jkw("delete")){jnext();return jn2(ND_DEL,jpunary(),0);}}
 n=jppostfix();
 if(jis("**")){jnext();{JNd*r=jn2(ND_BIN,n,jpunary());r->op=B_EXP;r->line=jline;return r;}}
 return n;}
static int jbinprec(int*op,int noin){
 static const struct{const char*s;int op,pr;}T[]={{"??",L_NUL,1},{"||",L_OR,2},{"&&",L_AND,3},{"|",B_OR,4},{"^",B_XOR,5},{"&",B_AND,6},
  {"===",B_SEQ,7},{"!==",B_SNE,7},{"==",B_EQ,7},{"!=",B_NE,7},{"<=",B_LE,8},{">=",B_GE,8},{"<",B_LT,8},{">",B_GT,8},
  {"<<",B_SHL,9},{">>>",B_USHR,9},{">>",B_SHR,9},{"+",B_ADD,10},{"-",B_SUB,10},{"*",B_MUL,11},{"/",B_DIV,11},{"%",B_MOD,11},{0,0,0}};
 int i;
 if(jtk==TK_ID){
  if(jkw("instanceof")){*op=B_INSTOF;return 8;}
  if(jkw("in")&&!noin){*op=B_IN;return 8;}
  return 0;}
 if(jtk!=TK_P)return 0;
 for(i=0;T[i].s;i++)if(jis(T[i].s)){*op=T[i].op;return T[i].pr;}
 return 0;}
static int jnoin;
static JNd*jpbin(int minp){
 JNd*l=jpunary(),*r,*n;int op,pr;
 for(;;){
  pr=jbinprec(&op,jnoin);
  if(!pr||pr<minp)break;
  jnext();
  r=jpbin(pr+1);
  n=jn2(op>=L_AND?ND_LOG:ND_BIN,l,r);n->op=(unsigned char)op;n->line=jline;l=n;}
 return l;}
static JNd*jpcond(void){
 JNd*c=jpbin(1),*n;
 if(jis("?")){
  jnext();n=jnd(ND_COND);n->a=c;n->line=jline;
  {int sv=jnoin;jnoin=0;n->b=jpassign();jnoin=sv;}
  jexpect(":");n->c=jpassign();return n;}
 return c;}
static JNd*jpassign(void){
 static const struct{const char*s;int op;}A[]={{"=",0},{"+=",B_ADD},{"-=",B_SUB},{"*=",B_MUL},{"/=",B_DIV},{"%=",B_MOD},{"**=",B_EXP},{"<<=",B_SHL},{">>=",B_SHR},{">>>=",B_USHR},
  {"&=",B_AND},{"|=",B_OR},{"^=",B_XOR},{"&&=",L_AND},{"||=",L_OR},{"\?\?=",L_NUL},{0,0}};
 JNd*l=jpcond(),*n;int i;
 if(jtk==TK_P)for(i=0;A[i].s;i++)if(jis(A[i].s)){
  jnext();
  if(A[i].op==0&&l&&(l->t==ND_ARR||l->t==ND_OBJ))l=jtopat(l);
  n=jn2(ND_ASG,l,jpassign());n->op=(unsigned char)A[i].op;n->line=jline;return n;}
 return l;}
static JNd*jpexpr(void){
 JNd*e=jpassign(),*s;
 while(jis(",")){jnext();s=jn2(ND_SEQ,e,jpassign());e=s;}
 return e;}
static JNd*jpvar(int kind){ /* after var/let/const consumed */
 JNd*v=jnd(ND_VAR),*h=0,*t=0,*d;
 v->op=(unsigned char)kind;v->line=jline;
 do{
  d=jnd(ND_DECL);d->line=jline;
  d->a=jpbind();
  if(kind==0)jpatvars(d->a);
  if(jeat("="))d->b=jpassign();
  if(t)t->nx=d;else h=d;t=d;}while(jeat(","));
 v->a=h;return v;}
static JNd*jpblock(void){
 JNd*b=jnd(ND_BLOCK),*h=0,*t=0,*s;
 b->line=jline;
 jexpect("{");
 while(!jis("}")&&jtk!=TK_EOF){
  s=jpstmt();
  if(s&&(s->t==ND_FUNCD||s->t==ND_CLASSD||(s->t==ND_VAR&&s->op>0)))b->fl|=JF_LEX;
  if(t)t->nx=s;else h=s;t=s;}
 jexpect("}");b->a=h;return b;}
static JNd*jpstmt(void){
 JNd*s,*c;JStr*lbl=0;
 if(jtk==TK_P){
  if(jis("{"))return jpblock();
  if(jis(";")){jnext();return jnd(ND_EMPTY);}}
 if(jtk==TK_ID){
  if(jkw("var")||jkw("let")||jkw("const")){
   int k=jkw("var")?0:jkw("let")?1:2;jnext();s=jpvar(k);jsemi();return s;}
  if(jkw("function")){
   jnext();s=jnd(ND_FUNCD);s->line=jline;s->a=jpfunc(0,0);s->s=s->a->s;return s;}
  if(jkw("class")){jnext();s=jnd(ND_CLASSD);s->a=jpclass(1);s->s=s->a->s;return s;}
  if(jkw("if")){
   s=jnd(ND_IF);s->line=jline;jnext();jexpect("(");s->a=jpexpr();jexpect(")");s->b=jpstmt();
   if(jkw("else")){jnext();s->c=jpstmt();}
   return s;}
  if(jkw("while")){s=jnd(ND_WHILE);s->line=jline;jnext();jexpect("(");s->a=jpexpr();jexpect(")");s->b=jpstmt();return s;}
  if(jkw("do")){
   s=jnd(ND_DO);s->line=jline;jnext();s->b=jpstmt();
   if(!jkw("while"))jperr("expected while");else jnext();
   jexpect("(");s->a=jpexpr();jexpect(")");if(jis(";"))jnext();return s;}
  if(jkw("for")){
   JNd*init=0;int isv=0,kind=0;
   s=jnd(ND_FOR);s->line=jline;jnext();jexpect("(");
   if(jis(";")){}
   else{
    jnoin=1;
    if(jkw("var")||jkw("let")||jkw("const")){kind=jkw("var")?0:jkw("let")?1:2;jnext();init=jpvar(kind);isv=1;}
    else init=jpexpr();
    jnoin=0;
    if(jkw("of")||jkw("in")){
     int of=jkw("of");
     jnext();
     s->t=(unsigned char)(of?ND_FOROF:ND_FORIN);
     if(isv){s->op=(unsigned char)kind;s->a=init->a->a;s->x=1;}
     else{s->a=jtopat(init);s->x=0;}
     s->b=of?jpassign():jpexpr();
     jexpect(")");s->c=jpstmt();return s;}}
   jexpect(";");
   s->a=init;
   if(!jis(";"))s->b=jpexpr();
   jexpect(";");
   if(!jis(")"))s->c=jpexpr();
   jexpect(")");
   s->d=jpstmt();
   if(isv&&kind>0)s->fl|=JF_LEX;
   return s;}
  if(jkw("return")){
   s=jnd(ND_RET);s->line=jline;jnext();
   if(!jis(";")&&!jis("}")&&jtk!=TK_EOF&&!jnlb)s->a=jpexpr();
   jsemi();return s;}
  if(jkw("break")||jkw("continue")){
   s=jnd(jkw("break")?ND_BRK:ND_CONT);s->line=jline;jnext();
   if(jtk==TK_ID&&!jnlb&&!jisres(jtstr)){s->s=jtstr;jnext();}
   jsemi();return s;}
  if(jkw("throw")){s=jnd(ND_THROW);s->line=jline;jnext();s->a=jpexpr();jsemi();return s;}
  if(jkw("try")){
   s=jnd(ND_TRY);s->line=jline;jnext();s->a=jpblock();
   if(jkw("catch")){
    jnext();
    if(jeat("(")){s->b=jpbind();jexpect(")");}
    s->c=jpblock();}
   if(jkw("finally")){jnext();s->d=jpblock();}
   return s;}
  if(jkw("switch")){
   JNd*h=0,*t=0;
   s=jnd(ND_SWITCH);s->line=jline;jnext();jexpect("(");s->a=jpexpr();jexpect(")");jexpect("{");
   while(!jis("}")&&jtk!=TK_EOF){
    c=jnd(ND_CASE);
    if(jkw("case")){jnext();c->a=jpexpr();}
    else if(jkw("default")){jnext();c->op=1;}
    else{jperr("expected case");break;}
    jexpect(":");
    {JNd*bh=0,*bt=0,*st;
     while(!jis("}")&&!jkw("case")&&!jkw("default")&&jtk!=TK_EOF){
      st=jpstmt();
      if(st&&(st->t==ND_FUNCD||st->t==ND_CLASSD||(st->t==ND_VAR&&st->op>0)))s->fl|=JF_LEX;
      if(bt)bt->nx=st;else bh=st;bt=st;}
     c->b=bh;}
    if(t)t->nx=c;else h=c;t=c;}
   jexpect("}");s->b=h;return s;}
  if(jtstr&&jtk==TK_ID&&!jisres(jtstr)){ /* label? */
   int sp=jpos,sl=jline,stk=jtk,snl=jnlb,sf=jpfail;JStr*ss=jtstr;
   jnext();
   if(jis(":")){jnext();lbl=ss;s=jpstmt();
    if(s&&(s->t==ND_FOR||s->t==ND_FORIN||s->t==ND_FOROF||s->t==ND_WHILE||s->t==ND_DO)){s->s=lbl;return s;}
    c=jnd(ND_LABEL);c->s=lbl;c->a=s;return c;}
   jpos=sp;jline=sl;jtk=stk;jnlb=snl;jtstr=ss;jpfail=sf;}}
 s=jnd(ND_EXPR);s->line=jline;s->a=jpexpr();jsemi();return s;}
static JNd*jparse(const char*src,int n){
 JNd*prog=jnd(ND_FUNC),*h=0,*t=0,*s;
 jsrc=src;jpos=0;jend=n;jline=1;jpfail=0;jperrm[0]=0;jnoin=0;
 jfnc=prog;jnext();
 while(jtk!=TK_EOF&&!jpfail){
  s=jpstmt();
  if(t)t->nx=s;else h=s;t=s;}
 prog->b=jnd(ND_BLOCK);prog->b->a=h;
 jfnc=0;
 return prog;}

/* ---- interpreter state ---- */
static int jthrow,jabort,jshort,jdepth,jline_cur;static j64 jsteps;static JStr*jlabel;
static int jnewcall;
static JV jmkregex(JStr*pat,JStr*flags);
static int jdomget(JOb*o,JStr*k,JV*out);
static int jdomset(JOb*o,JStr*k,JV v);
static JV jcall(JV f,JV thisv,JV*args,int argc);
static JV jconstruct(JV f,JV*args,int argc);
static JV jget(JV base,JStr*key);
static void jput(JV base,JStr*key,JV v);
static JStr*jtos(JV v);
static JStr*K_length,*K_prototype,*K_constructor,*K_message,*K_name,*K_toString,*K_this,*K_cls,*K_args,*K_valueOf,*K_proto,*K_stack;
#define JARG(i) ((i)<argc?args[i]:jundef())
static JV jthrowerr(JOb*proto,const char*msg){
 JOb*e=jnewob(OK_ERR,proto);
 jaddpr(e,K_message,jcv(msg),1);
 jexv=jobj(e);jthrow=1;return jundef();}
static JV jthrowerr2(JOb*proto,const char*a,JStr*b,const char*c){
 char buf[200];int n=0,i;
 for(i=0;a[i]&&n<150;i++)buf[n++]=a[i];
 for(i=0;i<b->len&&n<180;i++)buf[n++]=b->d[i];
 for(i=0;c[i]&&n<198;i++)buf[n++]=c[i];
 buf[n]=0;return jthrowerr(proto,buf);}
static void jfatal(const char*m){if(!jabort){jthrowerr(jErrR,m);jabort=1;}jthrow=1;}
static int jtruthy(JV v){
 switch(v.t){
  case JT_U:case JT_NUL:return 0;
  case JT_B:return v.u.b;
  case JT_N:return!(v.u.d==0||v.u.d!=v.u.d);
  case JT_S:return v.u.s->len>0;
  default:return 1;}}
static int jiscallable(JV v){return v.t==JT_O&&(v.u.o->k==OK_FN||v.u.o->k==OK_NAT||v.u.o->k==OK_BOUND);}
static JStr*jtypeof(JV v){
 switch(v.t){case JT_U:return jcstr("undefined");case JT_NUL:return jcstr("object");case JT_B:return jcstr("boolean");case JT_N:return jcstr("number");case JT_S:return jcstr("string");}
 return jcstr(jiscallable(v)?"function":"object");}
static JV jtoprim(JV v){
 JV f,r;
 if(v.t!=JT_O)return v;
 if(v.u.o->k==OK_DATE)return v.u.o->ex;
 f=jget(v,K_valueOf);
 if(jiscallable(f)){r=jcall(f,v,0,0);if(jthrow)return jundef();if(r.t!=JT_O)return r;}
 f=jget(v,K_toString);
 if(jiscallable(f)){r=jcall(f,v,0,0);if(jthrow)return jundef();if(r.t!=JT_O)return r;}
 return jcv("[object Object]");}
static double jtonum(JV v){
 int ok;
 switch(v.t){
  case JT_U:return jNAN();
  case JT_NUL:return 0;
  case JT_B:return v.u.b;
  case JT_N:return v.u.d;
  case JT_S:return jstod(v.u.s->d,v.u.s->len,&ok);
  default:v=jtoprim(v);if(jthrow)return 0;if(v.t==JT_O)return jNAN();return jtonum(v);}}
static JStr*jtos(JV v){
 switch(v.t){
  case JT_U:return jcstr("undefined");
  case JT_NUL:return jcstr("null");
  case JT_B:return jcstr(v.u.b?"true":"false");
  case JT_N:return jnum2s(v.u.d);
  case JT_S:return v.u.s;
  default:{
   JV f=jget(v,K_toString),r;
   if(jiscallable(f)){
    if(jdepth>JMAXDEPTH-8){jthrowerr(jErrR,"Maximum call stack size exceeded");return&jemptys;}
    r=jcall(f,v,0,0);if(jthrow)return&jemptys;
    if(r.t!=JT_O)return jtos(r);}
   return jcstr("[object Object]");}}}
static JStr*jkey(JV v){if(v.t==JT_S)return v.u.s;return jtos(v);}
static int jidx(JStr*s){int i,n=0;if(!s->len||s->len>9)return-1;if(s->d[0]=='0'&&s->len>1)return-1;for(i=0;i<s->len;i++){if(!isdg(s->d[i]))return-1;n=n*10+s->d[i]-'0';}return n;}
static int jseq(JV a,JV b){
 if(a.t!=b.t)return 0;
 switch(a.t){case JT_U:case JT_NUL:return 1;case JT_B:return a.u.b==b.u.b;case JT_N:return a.u.d==b.u.d;case JT_S:return jkeq(a.u.s,b.u.s);}
 return a.u.o==b.u.o;}
static int jleq(JV a,JV b){
 if(a.t==b.t)return jseq(a,b);
 if((a.t==JT_U||a.t==JT_NUL)&&(b.t==JT_U||b.t==JT_NUL))return 1;
 if(a.t==JT_U||a.t==JT_NUL||b.t==JT_U||b.t==JT_NUL)return 0;
 if(a.t==JT_O){a=jtoprim(a);if(jthrow)return 0;return jleq(a,b);}
 if(b.t==JT_O){b=jtoprim(b);if(jthrow)return 0;return jleq(a,b);}
 return jtonum(a)==jtonum(b);}
static int jsamevalue(JV a,JV b){if(a.t==JT_N&&b.t==JT_N){if(a.u.d!=a.u.d&&b.u.d!=b.u.d)return 1;return a.u.d==b.u.d;}return jseq(a,b);}

/* ---- property access ---- */
static JV jstrat(JStr*s,int i){
 int off,l;
 if(i<0)return jundef();
 if(s->asc){if(i>=s->len)return jundef();return jsv(s->d+i,1);}
 off=jcpoff(s,i);if(off>=s->len)return jundef();
 jdec8(s->d+off,s->len-off,&l);return jsv(s->d+off,l);}
static JV jaccget(JPr*p,JV recv){
 JOb*h=p->v.u.o;
 if(!h->it||h->it[0].t!=JT_O)return jundef();
 return jcall(h->it[0],recv,0,0);}
static int jdefacc(JOb*o,JStr*k,JV fn,int isset,int fl){
 JPr*p=jfind(o,k);JOb*h;
 if(p&&(p->fl&4))h=p->v.u.o;
 else{
  h=jnewob(OK_PLAIN,0);h->it=jalloc(2*(int)sizeof(JV),BT_VALS);h->ca=2;h->na=2;
  if(!h->it)return 0;
  if(p){p->v=jobj(h);p->fl=fl|4;}else jaddpr(o,k,jobj(h),fl|4);}
 h->it[isset?1:0]=fn;return 1;}
static JV jgetown(JOb*o,JStr*key,int*found){
 JPr*p;int i;
 *found=1;
 if(o->k==OK_ARR){
  if(key==K_length||jkeq(key,K_length))return jnum(o->na);
  i=jidx(key);
  if(i>=0){if(i<o->na)return o->it[i];*found=0;return jundef();}}
 if(o->k>=OK_NODE&&o->k!=OK_DATE&&o->k!=OK_BOUND){JV out;if(jdomget(o,key,&out))return out;}
 if((o->fl&12)&&o->k==OK_PLAIN&&jkeqc(key,"size"))return jnum(o->na/((o->fl&4)?2:1));
 p=jfind(o,key);
 if(p){if(p->fl&4)return jaccget(p,jrecv);return p->v;}
 if(o->k==OK_FN&&o->fn&&o->fn->t==ND_FUNC&&!(o->fn->fl&JF_ARROW)&&jkeq(key,K_prototype)){
  JOb*pr=jnewob(OK_PLAIN,jObjProto);
  jaddpr(pr,K_constructor,jobj(o),1);
  jaddpr(o,K_prototype,jobj(pr),1);
  return jobj(pr);}
 if(o->k==OK_FN&&o->fn&&jkeq(key,K_name)&&o->fn->s)return jstrv(o->fn->s);
 *found=0;return jundef();}
static JV jget(JV base,JStr*key){
 JOb*o,*p;int found,i;
 switch(base.t){
  case JT_U:case JT_NUL:return jthrowerr2(jErrT,"Cannot read properties of ",jcstr(base.t==JT_U?"undefined":"null")," (reading '"),jthrow?jundef():jundef();
  case JT_S:
   if(jkeq(key,K_length))return jnum(jcplen(base.u.s));
   i=jidx(key);if(i>=0)return jstrat(base.u.s,i);
   o=jStrProto;break;
  case JT_N:o=jNumProto;break;
  case JT_B:o=jBoolProto;break;
  default:o=base.u.o;}
 jrecv=base;
 for(p=o;p;p=p->proto){
  JV r;jrecv=base;r=jgetown(p,key,&found);
  if(found)return r;
  if(jthrow)return jundef();}
 return jundef();}
static JV jgetv(JV base,JV key){
 if(key.t==JT_N&&base.t==JT_O&&base.u.o->k==OK_ARR){
  double d=key.u.d;int i=(int)d;
  if((double)i==d&&i>=0){if(i<base.u.o->na)return base.u.o->it[i];return jundef();}}
 if(key.t==JT_N&&base.t==JT_S&&key.u.d>=0&&key.u.d==jtrunc(key.u.d)&&key.u.d<2e9)return jstrat(base.u.s,(int)key.u.d);
 if(base.t==JT_U||base.t==JT_NUL){
  JStr*k=jtos(key);
  return jthrowerr2(jErrT,base.t==JT_U?"Cannot read properties of undefined (reading '":"Cannot read properties of null (reading '",k,"')");}
 {JStr*k=jkey(key);if(jthrow)return jundef();return jget(base,k);}}
static void jarrset(JOb*a,int i,JV v){
 int j;
 if(i<a->na){a->it[i]=v;return;}
 if(i>1000000){jthrowerr(jErrR,"Invalid array length");return;}
 jarrgrow(a,i+1);
 if(i>=a->ca)return;
 for(j=a->na;j<i;j++)a->it[j]=jundef();
 a->it[i]=v;a->na=i+1;}
static void jput(JV base,JStr*key,JV v){
 JOb*o;JPr*p;int i;
 if(base.t!=JT_O){
  if(base.t==JT_U||base.t==JT_NUL)jthrowerr2(jErrT,"Cannot set properties of ",jcstr(base.t==JT_U?"undefined":"null")," (setting '");
  return;}
 o=base.u.o;
 if(o->k==OK_ARR){
  if(jkeq(key,K_length)){
   double d=jtonum(v);int n=(int)d;
   if(n<0||(double)n!=d){jthrowerr(jErrR,"Invalid array length");return;}
   if(n<=o->na)o->na=n;else{jarrgrow(o,n);if(n<=o->ca){for(i=o->na;i<n;i++)o->it[i]=jundef();o->na=n;}}
   return;}
  i=jidx(key);
  if(i>=0){jarrset(o,i,v);return;}}
 if(o->k>=OK_NODE&&o->k!=OK_DATE&&o->k!=OK_BOUND){if(jdomset(o,key,v))return;}
 p=jfind(o,key);
 if(!p){JOb*q;for(q=o->proto;q;q=q->proto){JPr*pp=jfind(q,key);if(pp){if(pp->fl&4)p=pp;break;}}}
 if(p&&(p->fl&4)){JOb*h=p->v.u.o;if(h->it&&h->it[1].t==JT_O){JV a[1];a[0]=v;jcall(h->it[1],base,a,1);}return;}
 if(p){if(p->fl&2){jthrowerr(jErrT,"Assignment to constant variable.");return;}p->v=v;return;}
 jaddpr(o,key,v,0);}
static void jputv(JV base,JV key,JV v){
 if(key.t==JT_N&&base.t==JT_O&&base.u.o->k==OK_ARR){
  double d=key.u.d;int i=(int)d;
  if((double)i==d&&i>=0){jarrset(base.u.o,i,v);return;}}
 {JStr*k=jkey(key);if(jthrow)return;jput(base,k,v);}}
static int jhasprop(JV base,JStr*key){
 JOb*p;int found,i;
 if(base.t==JT_S){i=jidx(key);return jkeq(key,K_length)||(i>=0&&i<jcplen(base.u.s));}
 if(base.t!=JT_O)return 0;
 for(p=base.u.o;p;p=p->proto){jgetown(p,key,&found);if(found)return 1;}
 return 0;}

/* ---- functions ---- */
static JOb*jmkfn(JNd*f,JOb*env){JOb*o=jnewob(OK_FN,jFnProto);o->fn=f;o->env=env;return o;}
static JOb*jmknat(const char*name,JNF fn){JOb*o=jnewob(OK_NAT,jFnProto);o->nf=fn;if(name)jaddpr(o,K_name,jcv(name),1);return o;}
static void jdef(JOb*o,const char*name,JNF fn){JOb*f=jmknat(name,fn);jaddpr(o,jcstr(name),jobj(f),1);}
static void jdeclare(JOb*env,JStr*name,JV v,int fl){JPr*p=jfind(env,name);if(p){p->v=v;p->fl=fl;}else jaddpr(env,name,v,fl);}
static JV jev(JNd*n,JOb*env);
static int jexec(JNd*s,JOb*env);
enum{JC_NORM,JC_BRK,JC_CONT,JC_RET,JC_THROW};
static int jexeclist(JNd*s,JOb*env);
static void jhoist(JNd*s,JOb*env){for(;s;s=s->nx)if(s->t==ND_FUNCD&&s->a){JOb*f=jmkfn(s->a,env);jdeclare(env,s->s,jobj(f),0);}}
static int jbindpat(JNd*p,JV v,JOb*env,int kind);
static JV jevclass(JNd*c,JOb*env);
static void jclassfields(JOb*cls,JV self);
static JV jinitparent(JOb*par,JV self,JV*args,int argc);
static JV jcallfn(JOb*fo,JV thisv,JV*args,int argc,int isnew){
 JNd*f=fo->fn,*p;JOb*env;int i=0;JV v;
 if(f->t==ND_CLASS){
  if(!isnew)return jthrowerr(jErrT,"Class constructor cannot be invoked without 'new'");
  return jundef();}
 if(jdepth>=JMAXDEPTH)return jthrowerr(jErrR,"Maximum call stack size exceeded");
 jdepth++;
 env=jnewenv(fo->env);
 if(!(f->fl&JF_ARROW)){
  if((thisv.t==JT_U||thisv.t==JT_NUL)&&!(fo->fl&2))thisv=jobj(jGlob);
  jaddpr(env,K_this,thisv,0);
  if(f->fl&JF_ARGS){JOb*a=jnewarr(argc);int k;for(k=0;k<argc;k++)jarrpush(a,args[k]);jaddpr(env,K_args,jobj(a),0);}}
 for(p=f->a;p;p=p->nx){
  if(p->fl&JF_REST){
   JOb*r=jnewarr(argc>i?argc-i:0);
   for(;i<argc;i++)jarrpush(r,args[i]);
   v=jobj(r);}
  else{
   v=i<argc?args[i]:jundef();i++;
   if(v.t==JT_U&&p->b){v=jev(p->b,env);if(jthrow){jdepth--;return jundef();}}}
  if(p->a&&p->a->t==ND_ID)jdeclare(env,p->a->s,v,0);
  else if(p->a){jbindpat(p->a,v,env,0);if(jthrow){jdepth--;return jundef();}}}
 {JNd*vn;for(vn=f->c;vn;vn=vn->nx)if(!jfind(env,vn->s))jaddpr(env,vn->s,jundef(),0);}
 if(f->fl&JF_EXPR){v=jev(f->b,env);jdepth--;return jthrow?jundef():v;}
 jhoist(f->b->a,env);
 {int c=jexeclist(f->b->a,env);
  jdepth--;
  if(c==JC_THROW){jthrow=1;return jundef();}
  if(c==JC_RET){v=jrv;jrv=jundef();return v;}}
 return jundef();}
static JV jcall(JV fv,JV thisv,JV*args,int argc){
 JOb*o;
 if(!jiscallable(fv))return jthrowerr(jErrT,"value is not a function");
 o=fv.u.o;
 if(jabort){jthrow=1;return jundef();}
 if(o->k==OK_NAT){
  JV r;
  if(jdepth>=JMAXDEPTH)return jthrowerr(jErrR,"Maximum call stack size exceeded");
  jdepth++;r=o->nf(thisv,args,argc);jdepth--;
  return jthrow?jundef():r;}
 if(o->k==OK_BOUND){
  JV na[24];int i,n=0;
  for(i=1;i<o->na&&n<12;i++)na[n++]=o->it[i];
  for(i=0;i<argc&&n<24;i++)na[n++]=args[i];
  return jcall(o->ex,o->it[0],na,n);}
 return jcallfn(o,thisv,args,argc,0);}
static JV jconstruct(JV fv,JV*args,int argc){
 JOb*o,*self;JV proto,th,r;
 if(!jiscallable(fv))return jthrowerr(jErrT,"value is not a constructor");
 o=fv.u.o;
 if(o->k==OK_BOUND)return jconstruct(o->ex,args,argc);
 proto=jget(fv,K_prototype);
 if(jthrow)return jundef();
 self=jnewob(OK_PLAIN,proto.t==JT_O?proto.u.o:jObjProto);
 th=jobj(self);
 if(o->k==OK_NAT){
  if(jdepth>=JMAXDEPTH)return jthrowerr(jErrR,"Maximum call stack size exceeded");
  jnewcall=1;jdepth++;r=o->nf(th,args,argc);jdepth--;jnewcall=0;
  if(jthrow)return jundef();
  return r.t==JT_O?r:th;}
 if(o->fn&&o->fn->t==ND_CLASS){
  JNd*cn=o->fn,*m,*ctor=0;JOb*par;
  if(jdepth>=JMAXDEPTH)return jthrowerr(jErrR,"Maximum call stack size exceeded");
  for(m=cn->b;m;m=m->nx)if(m->op==1&&!(m->fl&(JF_STATIC|JF_COMP))&&m->s&&jkeqc(m->s,"constructor")){ctor=m->b;break;}
  par=cn->a?o->proto:0;
  if(!cn->a)jclassfields(o,th);
  if(ctor){
   JOb*cf=jmkfn(ctor,o->env);cf->fl|=2;
   jdepth++;r=jcallfn(cf,th,args,argc,1);jdepth--;
   if(jthrow)return jundef();
   return r.t==JT_O?r:th;}
  if(par){jinitparent(par,th,args,argc);if(jthrow)return jundef();jclassfields(o,th);}
  return th;}
 r=jcallfn(o,th,args,argc,1);
 if(jthrow)return jundef();
 return r.t==JT_O?r:th;}
static JV jinitparent(JOb*par,JV self,JV*args,int argc){
 if(par->k==OK_FN&&par->fn&&par->fn->t==ND_CLASS){
  JNd*cn=par->fn,*m,*ctor=0;JOb*pp=cn->a?par->proto:0;
  for(m=cn->b;m;m=m->nx)if(m->op==1&&!(m->fl&(JF_STATIC|JF_COMP))&&m->s&&jkeqc(m->s,"constructor")){ctor=m->b;break;}
  if(!cn->a)jclassfields(par,self);
  if(ctor){JOb*cf=jmkfn(ctor,par->env);cf->fl|=2;return jcallfn(cf,self,args,argc,1);}
  if(pp){jinitparent(pp,self,args,argc);if(jthrow)return jundef();jclassfields(par,self);}
  return jundef();}
 if(par->k==OK_NAT){JV r;jnewcall=1;r=par->nf(self,args,argc);jnewcall=0;return r;}
 return jcall(jobj(par),self,args,argc);}
static void jclassfields(JOb*cls,JV self){
 JNd*m;JOb*fe;JV v;
 for(m=cls->fn->b;m;m=m->nx){
  if(m->op!=2||(m->fl&JF_STATIC))continue;
  fe=jnewenv(cls->env);jaddpr(fe,K_this,self,0);
  v=m->b?jev(m->b->b,fe):jundef();
  if(jthrow)return;
  if(m->fl&JF_COMP){JV k=m->a?jev(m->a,fe):jundef();if(jthrow)return;jputv(self,k,v);}
  else jput(self,m->s,v);
  if(jthrow)return;}}

static int jmfind(JOb*m,JV k);
static JOb*jiterable(JV v){
 JOb*r;int i;
 if(v.t==JT_O&&v.u.o->k==OK_ARR)return v.u.o;
 if(v.t==JT_S){r=jnewarr(8);for(i=0;i<jcplen(v.u.s);i++)jarrpush(r,jstrat(v.u.s,i));return r;}
 if(v.t==JT_O&&(v.u.o->fl&4)){JOb*m=v.u.o;r=jnewarr(8);for(i=0;i<m->na;i+=2){JOb*p=jnewarr(2);jarrpush(p,m->it[i]);jarrpush(p,m->it[i+1]);jarrpush(r,jobj(p));}return r;}
 if(v.t==JT_O&&(v.u.o->fl&8)){JOb*m=v.u.o;r=jnewarr(8);for(i=0;i<m->na;i++)jarrpush(r,m->it[i]);return r;}
 return 0;}

/* ---- evaluator ---- */
static double jpow(double x,double y);
static JV jthrowprop(JV base,JStr*key,const char*verb){
 char b[260];int n=0,i;const char*a="Cannot ",*t=base.t==JT_U?"undefined":"null";
 for(i=0;a[i];i++)b[n++]=a[i];
 for(i=0;verb[i];i++)b[n++]=verb[i];
 {const char*m=" properties of ";for(i=0;m[i];i++)b[n++]=m[i];}
 for(i=0;t[i];i++)b[n++]=t[i];
 {const char*m=verb[0]=='r'?" (reading '":" (setting '";for(i=0;m[i];i++)b[n++]=m[i];}
 for(i=0;i<key->len&&n<240;i++)b[n++]=key->d[i];
 b[n++]='\'';b[n++]=')';b[n]=0;
 return jthrowerr(jErrT,b);}
static void jassignid(JStr*name,JV v,JOb*env){
 JPr*p=jlookup(env,name);
 if(p){if(p->fl&2){jthrowerr(jErrT,"Assignment to constant variable.");return;}p->v=v;return;}
 jaddpr(jGlob,name,v,0);}
static JV jbinop(int op,JV a,JV b){
 double x,y;
 switch(op){
  case B_ADD:
   a=jtoprim(a);if(jthrow)return jundef();b=jtoprim(b);if(jthrow)return jundef();
   if(a.t==JT_S||b.t==JT_S){JStr*sa=jtos(a),*sb;if(jthrow)return jundef();sb=jtos(b);if(jthrow)return jundef();return jstrv(jcat(sa,sb));}
   return jnum(jtonum(a)+jtonum(b));
  case B_SEQ:return jbool(jseq(a,b));
  case B_SNE:return jbool(!jseq(a,b));
  case B_EQ:{int r=jleq(a,b);return jbool(r);}
  case B_NE:{int r=jleq(a,b);return jbool(!r);}
  case B_LT:case B_GT:case B_LE:case B_GE:
   a=jtoprim(a);if(jthrow)return jundef();b=jtoprim(b);if(jthrow)return jundef();
   if(a.t==JT_S&&b.t==JT_S){
    int i,n=a.u.s->len<b.u.s->len?a.u.s->len:b.u.s->len,c=0;
    for(i=0;i<n;i++){unsigned char p=(unsigned char)a.u.s->d[i],q=(unsigned char)b.u.s->d[i];if(p!=q){c=p<q?-1:1;break;}}
    if(!c)c=a.u.s->len<b.u.s->len?-1:a.u.s->len>b.u.s->len?1:0;
    return jbool(op==B_LT?c<0:op==B_GT?c>0:op==B_LE?c<=0:c>=0);}
   x=jtonum(a);y=jtonum(b);
   if(x!=x||y!=y)return jbool(0);
   return jbool(op==B_LT?x<y:op==B_GT?x>y:op==B_LE?x<=y:x>=y);
  case B_IN:{
   JStr*k;
   if(b.t!=JT_O)return jthrowerr(jErrT,"Cannot use 'in' operator to search for a key in a non-object");
   k=jkey(a);if(jthrow)return jundef();
   return jbool(jhasprop(b,k));}
  case B_INSTOF:{
   JV pv;JOb*p;
   if(!jiscallable(b))return jthrowerr(jErrT,"Right-hand side of 'instanceof' is not callable");
   if(a.t!=JT_O)return jbool(0);
   pv=jget(b,K_prototype);if(jthrow)return jundef();
   if(pv.t!=JT_O)return jbool(0);
   for(p=a.u.o->proto;p;p=p->proto)if(p==pv.u.o)return jbool(1);
   return jbool(0);}}
 x=jtonum(a);if(jthrow)return jundef();y=jtonum(b);if(jthrow)return jundef();
 switch(op){
  case B_SUB:return jnum(x-y);
  case B_MUL:return jnum(x*y);
  case B_DIV:return jnum(x/y);
  case B_MOD:return jnum(jmod(x,y));
  case B_EXP:return jnum(jpow(x,y));
  case B_AND:return jnum(jtoi32(x)&jtoi32(y));
  case B_OR:return jnum(jtoi32(x)|jtoi32(y));
  case B_XOR:return jnum(jtoi32(x)^jtoi32(y));
  case B_SHL:return jnum((int)((ju32)jtoi32(x)<<(jtoi32(y)&31)));
  case B_SHR:return jnum(jtoi32(x)>>(jtoi32(y)&31));
  case B_USHR:return jnum((double)((ju32)jtoi32(x)>>(jtoi32(y)&31)));}
 return jundef();}
static JV*jargs(JNd*l,JOb*env,JV*sb,int*cnt){
 JNd*a;int n=0,spread=0;JV v;JOb*arr;
 for(a=l;a;a=a->nx){n++;if(a->t==ND_SPREAD)spread=1;}
 if(!spread&&n<=16){
  n=0;
  for(a=l;a;a=a->nx){sb[n++]=jev(a,env);if(jthrow)return 0;}
  *cnt=n;return sb;}
 arr=jnewarr(n+4);
 for(a=l;a;a=a->nx){
  if(a->t==ND_SPREAD){
   v=jev(a->a,env);if(jthrow)return 0;
   {JOb*itr=jiterable(v);int i;
    if(!itr){jthrowerr(jErrT,"spread syntax requires an iterable");return 0;}
    for(i=0;i<itr->na;i++)jarrpush(arr,itr->it[i]);}}
  else{v=jev(a,env);if(jthrow)return 0;jarrpush(arr,v);}}
 *cnt=arr->na;
 return arr->it?arr->it:sb;}
static void jkeysof(JV v,JOb*out){
 JOb*o;int i,k;JPr*p;
 if(v.t==JT_S){for(i=0;i<jcplen(v.u.s);i++)jarrpush(out,jnum(i));return;}
 if(v.t!=JT_O)return;
 for(o=v.u.o;o;o=o->proto){
  if(o->k==OK_ARR)for(i=0;i<o->na;i++){JStr*s=jnum2s(i);jarrpush(out,jstrv(s));}
  if(o->k>=OK_NODE&&o->k!=OK_DATE&&o->k!=OK_BOUND)continue;
  for(i=0;i<o->np;i++){
   p=&o->pr[i];if(p->fl&1)continue;
   for(k=0;k<out->na;k++)if(out->it[k].t==JT_S&&jkeq(out->it[k].u.s,p->k))break;
   if(k==out->na)jarrpush(out,jstrv(p->k));}}}
static void jobjspread(JOb*dst,JV v){
 int i;
 if(v.t==JT_S){for(i=0;i<jcplen(v.u.s);i++)jsetown(dst,jnum2s(i),jstrat(v.u.s,i));return;}
 if(v.t!=JT_O)return;
 if(v.u.o->k==OK_ARR)for(i=0;i<v.u.o->na;i++)jsetown(dst,jnum2s(i),v.u.o->it[i]);
 for(i=0;i<v.u.o->np;i++)if(!(v.u.o->pr[i].fl&1)){JV x=v.u.o->pr[i].v;if(v.u.o->pr[i].fl&4){jrecv=v;x=jaccget(&v.u.o->pr[i],v);}jsetown(dst,v.u.o->pr[i].k,x);}}
static int jbindpat(JNd*p,JV v,JOb*env,int kind){
 JNd*e;int i=0;
 if(!p)return 0;
 if(p->t==ND_ID){
  if(kind==3)jassignid(p->s,v,env);
  else if(kind==0){JPr*q=jlookup(env,p->s);if(q)q->v=v;else jdeclare(env,p->s,v,0);}
  else jdeclare(env,p->s,v,kind==2?2:0);
  return jthrow;}
 if(p->t==ND_MEM||p->t==ND_IDX){
  JV b=jev(p->a,env);if(jthrow)return 1;
  if(p->t==ND_MEM)jput(b,p->s,v);else{JV k=jev(p->b,env);if(jthrow)return 1;jputv(b,k,v);}
  return jthrow;}
 if(p->t==ND_PATA){
  JOb*src=jiterable(v);JStr*str=0;
  if(!src){jthrowerr(jErrT,"value is not iterable");return 1;}
  for(e=p->a;e;e=e->nx,i++){
   JV x;
   if(e->fl&JF_REST){
    JOb*r=jnewarr(4);int k;
    if(src)for(k=i;k<src->na;k++)jarrpush(r,src->it[k]);
    else for(k=i;k<jcplen(str);k++)jarrpush(r,jstrat(str,k));
    jbindpat(e->a,jobj(r),env,kind);if(jthrow)return 1;break;}
   x=src?(i<src->na?src->it[i]:jundef()):jstrat(str,i);
   if(!e->a)continue;
   if(x.t==JT_U&&e->b){x=jev(e->b,env);if(jthrow)return 1;}
   jbindpat(e->a,x,env,kind);if(jthrow)return 1;}
  return 0;}
 if(p->t==ND_PATO){
  JStr*used[32];int nu=0;
  if(v.t==JT_U||v.t==JT_NUL){jthrowerr(jErrT,"Cannot destructure undefined or null");return 1;}
  for(e=p->a;e;e=e->nx){
   JV x;JStr*k;
   if(e->fl&JF_REST){
    JOb*r=jnewob(OK_PLAIN,jObjProto);int j,q;
    if(v.t==JT_O)for(j=0;j<v.u.o->np;j++){
     if(v.u.o->pr[j].fl&1)continue;
     for(q=0;q<nu;q++)if(jkeq(used[q],v.u.o->pr[j].k))break;
     if(q==nu)jsetown(r,v.u.o->pr[j].k,v.u.o->pr[j].v);}
    jbindpat(e->a,jobj(r),env,kind);if(jthrow)return 1;break;}
   if(e->x&JF_COMP){JV kv=jev(e->c,env);if(jthrow)return 1;k=jkey(kv);}else k=e->s;
   if(nu<32)used[nu++]=k;
   x=jget(v,k);if(jthrow)return 1;
   if(x.t==JT_U&&e->b){x=jev(e->b,env);if(jthrow)return 1;}
   jbindpat(e->a,x,env,kind);if(jthrow)return 1;}
  return 0;}
 jthrowerr(jErrS,"Invalid destructuring target");return 1;}
static JV jevclass(JNd*c,JOb*env){
 JOb*ce=jnewenv(env),*cls,*proto,*par=0;JNd*m;JV pv;
 cls=jnewob(OK_FN,jFnProto);cls->fn=c;cls->env=ce;
 proto=jnewob(OK_PLAIN,jObjProto);
 if(c->a){
  pv=jev(c->a,env);if(jthrow)return jundef();
  if(!jiscallable(pv)&&pv.t!=JT_NUL){return jthrowerr(jErrT,"Class extends value is not a constructor");}
  if(pv.t==JT_O){par=pv.u.o;cls->proto=par;{JV pp=jget(pv,K_prototype);if(jthrow)return jundef();if(pp.t==JT_O)proto->proto=pp.u.o;}}}
 jaddpr(cls,K_prototype,jobj(proto),1);
 jaddpr(proto,K_constructor,jobj(cls),1);
 jaddpr(ce,K_cls,jobj(cls),0);
 if(c->s)jaddpr(ce,c->s,jobj(cls),0);
 for(m=c->b;m;m=m->nx){
  JStr*k=m->s;JOb*fo;
  if(m->fl&JF_COMP){JV kv=jev(m->a,ce);if(jthrow)return jundef();k=jkey(kv);}
  if(m->op==1){
   if(!(m->fl&JF_COMP)&&!(m->fl&JF_STATIC)&&jkeqc(m->s,"constructor"))continue;
   fo=jmkfn(m->b,ce);fo->fl|=2;
   if(m->x){jdefacc((m->fl&JF_STATIC)?cls:proto,k,jobj(fo),m->x==2,1);continue;}
   jaddpr((m->fl&JF_STATIC)?cls:proto,k,jobj(fo),1);}
  else if(m->op==2&&(m->fl&JF_STATIC)){
   JOb*fe=jnewenv(ce);JV v;
   jaddpr(fe,K_this,jobj(cls),0);
   v=m->b?jev(m->b->b,fe):jundef();if(jthrow)return jundef();
   jsetown(cls,k,v);}}
 (void)par;
 return jobj(cls);}
static JV jev(JNd*n,JOb*env){
 JV a,b,r;
 if(!n)return jundef();
 if(++jsteps>JMAXSTEPS){jfatal("script is taking too long");return jundef();}
 if(joom){jfatal("out of memory");return jundef();}
 switch(n->t){
  case ND_NUM:return jnum(n->num);
  case ND_STR:return jstrv(n->s);
  case ND_NULL:return jnull();
  case ND_TRUE:return jbool(1);
  case ND_FALSE:return jbool(0);
  case ND_THIS:{JPr*p=jlookup(env,K_this);return p?p->v:jobj(jGlob);}
  case ND_ID:{
   JPr*p=jlookup(env,n->s);
   if(p)return p->v;
   return jthrowerr2(jErrF,"",n->s," is not defined");}
  case ND_TPL:{
   JNd*c;JStr*acc=&jemptys;
   for(c=n->a;c;c=c->nx){
    if(c->t==ND_STR&&c->s&&(c==n->a||1)&&0){}
    if(c->t==ND_STR&&!(c->x&1)){acc=jcat(acc,c->s);}
    else{a=jev(c,env);if(jthrow)return jundef();{JStr*s=jtos(a);if(jthrow)return jundef();acc=jcat(acc,s);}}}
   return jstrv(acc);}
  case ND_ARR:{
   JOb*arr=jnewarr(8);JNd*c;
   for(c=n->a;c;c=c->nx){
    if(c->t==ND_HOLE)jarrpush(arr,jundef());
    else if(c->t==ND_SPREAD){
     a=jev(c->a,env);if(jthrow)return jundef();
     {JOb*itr=jiterable(a);int i;
      if(!itr)return jthrowerr(jErrT,"spread syntax requires an iterable");
      for(i=0;i<itr->na;i++)jarrpush(arr,itr->it[i]);}}
    else{a=jev(c,env);if(jthrow)return jundef();jarrpush(arr,a);}}
   return jobj(arr);}
  case ND_OBJ:{
   JOb*o=jnewob(OK_PLAIN,jObjProto);JNd*p;
   for(p=n->a;p;p=p->nx){
    JStr*k;
    if(p->t==ND_SPREAD){a=jev(p->a,env);if(jthrow)return jundef();jobjspread(o,a);continue;}
    if(p->fl&JF_COMP){a=jev(p->a,env);if(jthrow)return jundef();k=jkey(a);}else k=p->s;
    a=jev(p->b,env);if(jthrow)return jundef();
    if(p->x){jdefacc(o,k,a,p->x==2,0);continue;}
    jsetown(o,k,a);}
   return jobj(o);}
  case ND_FUNC:{
   JOb*fe=env,*fo;
   if(n->s&&!(n->fl&JF_ARROW))fe=jnewenv(env);
   fo=jmkfn(n,fe);
   if(fe!=env)jaddpr(fe,n->s,jobj(fo),0);
   return jobj(fo);}
  case ND_CLASS:return jevclass(n,env);
  case ND_REGEX:return jmkregex(n->s,n->a->s);
  case ND_OPT:a=jev(n->a,env);jshort=0;return a;
  case ND_MEM:
   if(n->a->t==ND_SUPER){JPr*cp=jlookup(env,K_cls);JPr*tp=jlookup(env,K_this);
    if(cp&&cp->v.t==JT_O){JV pr=jget(cp->v,K_prototype);if(pr.t==JT_O&&pr.u.o->proto){int f;JOb*q;JV x=jundef();for(q=pr.u.o->proto;q;q=q->proto){jrecv=tp?tp->v:jundef();x=jgetown(q,n->s,&f);if(f)break;}return x;}}
    return jundef();}
   a=jev(n->a,env);if(jthrow||jshort)return jundef();
   if((a.t==JT_U||a.t==JT_NUL)){if(n->fl&JF_OPT){jshort=1;return jundef();}return jthrowprop(a,n->s,"read");}
   return jget(a,n->s);
  case ND_IDX:
   a=jev(n->a,env);if(jthrow||jshort)return jundef();
   if((a.t==JT_U||a.t==JT_NUL)&&(n->fl&JF_OPT)){jshort=1;return jundef();}
   b=jev(n->b,env);if(jthrow)return jundef();
   return jgetv(a,b);
  case ND_CALL:{
   JV fv,thisv=jundef(),sb[16],*av;int ac=0;JNd*cal=n->a;
   if(cal->t==ND_SUPER){
    JPr*cp=jlookup(env,K_cls),*tp=jlookup(env,K_this);
    av=jargs(n->b,env,sb,&ac);if(!av)return jundef();
    if(!cp||!tp||cp->v.t!=JT_O)return jthrowerr(jErrS,"'super' keyword unexpected here");
    if(cp->v.u.o->proto&&cp->v.u.o->proto!=jFnProto){jinitparent(cp->v.u.o->proto,tp->v,av,ac);if(jthrow)return jundef();}
    jclassfields(cp->v.u.o,tp->v);
    return jundef();}
   if(cal->t==ND_MEM||cal->t==ND_IDX){
    if(cal->a->t==ND_SUPER){
     JPr*cp=jlookup(env,K_cls),*tp=jlookup(env,K_this);
     thisv=tp?tp->v:jundef();fv=jundef();
     if(cp&&cp->v.t==JT_O){JV pr=jget(cp->v,K_prototype);if(pr.t==JT_O&&pr.u.o->proto){JStr*k=cal->s;int f;if(cal->t==ND_IDX){JV kv=jev(cal->b,env);if(jthrow)return jundef();k=jkey(kv);}fv=jgetown(pr.u.o->proto,k,&f);if(!f)fv=jget(jobj(pr.u.o->proto),k);}}}
    else{
     thisv=jev(cal->a,env);if(jthrow||jshort)return jundef();
     if((thisv.t==JT_U||thisv.t==JT_NUL)&&(cal->fl&JF_OPT)){jshort=1;return jundef();}
     if(cal->t==ND_MEM){
      if(thisv.t==JT_U||thisv.t==JT_NUL)return jthrowprop(thisv,cal->s,"read");
      fv=jget(thisv,cal->s);}
     else{b=jev(cal->b,env);if(jthrow)return jundef();fv=jgetv(thisv,b);}
     if(jthrow)return jundef();}}
   else{fv=jev(cal,env);if(jthrow||jshort)return jundef();}
   if((fv.t==JT_U||fv.t==JT_NUL)&&(n->fl&JF_OPT)){jshort=1;return jundef();}
   av=jargs(n->b,env,sb,&ac);if(!av)return jundef();
   if(!jiscallable(fv)){
    JStr*nm=cal->t==ND_ID||cal->t==ND_MEM?cal->s:0;
    if(nm)return jthrowerr2(jErrT,"",nm," is not a function");
    return jthrowerr(jErrT,"expression is not a function");}
   jline_cur=n->line;
   r=jcall(fv,thisv,av,ac);
   return r;}
  case ND_NEW:{
   JV fv,sb[16],*av;int ac=0;
   fv=jev(n->a,env);if(jthrow)return jundef();
   av=jargs(n->b,env,sb,&ac);if(!av)return jundef();
   if(!jiscallable(fv)){
    JStr*nm=n->a->t==ND_ID||n->a->t==ND_MEM?n->a->s:0;
    if(nm)return jthrowerr2(jErrT,"",nm," is not a constructor");
    return jthrowerr(jErrT,"expression is not a constructor");}
   return jconstruct(fv,av,ac);}
  case ND_SEQ:a=jev(n->a,env);if(jthrow)return jundef();return jev(n->b,env);
  case ND_COND:a=jev(n->a,env);if(jthrow)return jundef();return jev(jtruthy(a)?n->b:n->c,env);
  case ND_UN:
   a=jev(n->a,env);if(jthrow)return jundef();
   switch(n->op){
    case '!':return jbool(!jtruthy(a));
    case '-':{double d=jtonum(a);return jnum(-d);}
    case '+':{double d=jtonum(a);return jnum(d);}
    case '~':{double d=jtonum(a);return jnum(~jtoi32(d));}}
   return jundef();
  case ND_TYPEOF:
   if(n->a->t==ND_ID){JPr*p=jlookup(env,n->a->s);if(!p)return jcv("undefined");a=p->v;}
   else{a=jev(n->a,env);if(jthrow)return jundef();}
   return jstrv(jtypeof(a));
  case ND_VOID:jev(n->a,env);return jundef();
  case ND_DEL:{
   JNd*t=n->a;
   if(t->t==ND_MEM||t->t==ND_IDX){
    JStr*k;
    a=jev(t->a,env);if(jthrow)return jundef();
    if(t->t==ND_MEM)k=t->s;else{b=jev(t->b,env);if(jthrow)return jundef();k=jkey(b);}
    if(a.t==JT_O){
     if(a.u.o->k==OK_ARR&&jidx(k)>=0){int i=jidx(k);if(i<a.u.o->na)a.u.o->it[i]=jundef();}
     else jdelpr(a.u.o,k);}}
   return jbool(1);}
  case ND_BIN:
   a=jev(n->a,env);if(jthrow)return jundef();
   b=jev(n->b,env);if(jthrow)return jundef();
   return jbinop(n->op,a,b);
  case ND_LOG:
   a=jev(n->a,env);if(jthrow)return jundef();
   if(n->op==L_AND)return jtruthy(a)?jev(n->b,env):a;
   if(n->op==L_OR)return jtruthy(a)?a:jev(n->b,env);
   return(a.t==JT_U||a.t==JT_NUL)?jev(n->b,env):a;
  case ND_UPD:{
   JNd*t=n->a;JV base=jundef(),key=jundef(),old;double d;
   if(t->t==ND_ID){old=jev(t,env);if(jthrow)return jundef();}
   else if(t->t==ND_MEM||t->t==ND_IDX){
    base=jev(t->a,env);if(jthrow)return jundef();
    if(t->t==ND_MEM)old=jget(base,t->s);else{key=jev(t->b,env);if(jthrow)return jundef();old=jgetv(base,key);}
    if(jthrow)return jundef();}
   else return jthrowerr(jErrS,"Invalid left-hand side expression in postfix operation");
   d=jtonum(old);if(jthrow)return jundef();
   r=jnum(n->op==1?d+1:d-1);
   if(t->t==ND_ID)jassignid(t->s,r,env);
   else if(t->t==ND_MEM)jput(base,t->s,r);else jputv(base,key,r);
   if(jthrow)return jundef();
   return(n->fl&JF_PREFIX)?r:jnum(d);}
  case ND_ASG:{
   JNd*t=n->a;JV base=jundef(),key=jundef();
   if(t->t==ND_PATA||t->t==ND_PATO){r=jev(n->b,env);if(jthrow)return jundef();jbindpat(t,r,env,3);return r;}
   if(t->t==ND_MEM||t->t==ND_IDX){
    base=jev(t->a,env);if(jthrow)return jundef();
    if(t->t==ND_IDX){key=jev(t->b,env);if(jthrow)return jundef();}}
   if(n->op==0){r=jev(n->b,env);if(jthrow)return jundef();}
   else{
    JV old;
    if(t->t==ND_ID)old=jev(t,env);
    else if(t->t==ND_MEM)old=jget(base,t->s);
    else old=jgetv(base,key);
    if(jthrow)return jundef();
    if(n->op>=L_AND){
     if(n->op==L_AND&&!jtruthy(old))return old;
     if(n->op==L_OR&&jtruthy(old))return old;
     if(n->op==L_NUL&&!(old.t==JT_U||old.t==JT_NUL))return old;
     r=jev(n->b,env);if(jthrow)return jundef();}
    else{b=jev(n->b,env);if(jthrow)return jundef();r=jbinop(n->op,old,b);if(jthrow)return jundef();}}
   if(t->t==ND_ID)jassignid(t->s,r,env);
   else if(t->t==ND_MEM)jput(base,t->s,r);
   else if(t->t==ND_IDX)jputv(base,key,r);
   else return jthrowerr(jErrS,"Invalid left-hand side in assignment");
   return jthrow?jundef():r;}
  case ND_SPREAD:return jthrowerr(jErrS,"unexpected spread");
  default:return jundef();}}

/* ---- statements ---- */
static JOb*jcopyenv(JOb*e){JOb*n=jnewenv(e->proto);int i;for(i=0;i<e->np;i++)jaddpr(n,e->pr[i].k,e->pr[i].v,e->pr[i].fl);return n;}
#define JLOOPCTL(c,n) \
 if(c==JC_BRK){if(!jlabel||((n)->s&&jkeq(jlabel,(n)->s))){jlabel=0;break;}return c;} \
 if(c==JC_CONT){if(!jlabel||((n)->s&&jkeq(jlabel,(n)->s))){jlabel=0;}else return c;} \
 else if(c!=JC_NORM)return c;
static int jexeclist(JNd*s,JOb*env){int c;for(;s;s=s->nx){c=jexec(s,env);if(c)return c;}return 0;}
static int jexec(JNd*s,JOb*env){
 JV v;int c;
 if(!s)return 0;
 if(jabort||jthrow)return JC_THROW;
 if(++jsteps>JMAXSTEPS){jfatal("script is taking too long");return JC_THROW;}
 jline_cur=s->line;
 switch(s->t){
  case ND_EMPTY:case ND_FUNCD:return 0;
  case ND_EXPR:jev(s->a,env);if(jthrow){return JC_THROW;}return 0;
  case ND_VAR:{
   JNd*d;
   for(d=s->a;d;d=d->nx){
    if(d->b){v=jev(d->b,env);if(jthrow)return JC_THROW;}
    else{if(s->op==0)continue;v=jundef();}
    jbindpat(d->a,v,env,s->op==0?0:s->op==1?1:2);
    if(jthrow)return JC_THROW;}
   return 0;}
  case ND_CLASSD:v=jevclass(s->a,env);if(jthrow)return JC_THROW;jdeclare(env,s->s,v,0);return 0;
  case ND_BLOCK:{
   JOb*e2=env;
   if(s->fl&JF_LEX){e2=jnewenv(env);jhoist(s->a,e2);}
   return jexeclist(s->a,e2);}
  case ND_IF:
   v=jev(s->a,env);if(jthrow)return JC_THROW;
   if(jtruthy(v))return jexec(s->b,env);
   return s->c?jexec(s->c,env):0;
  case ND_WHILE:
   for(;;){
    v=jev(s->a,env);if(jthrow)return JC_THROW;
    if(!jtruthy(v))break;
    c=jexec(s->b,env);
    JLOOPCTL(c,s)}
   return 0;
  case ND_DO:
   for(;;){
    c=jexec(s->b,env);
    JLOOPCTL(c,s)
    v=jev(s->a,env);if(jthrow)return JC_THROW;
    if(!jtruthy(v))break;}
   return 0;
  case ND_FOR:{
   JOb*cur=env;
   if(s->fl&JF_LEX){cur=jnewenv(env);}
   if(s->a){
    if(s->a->t==ND_VAR){c=jexec(s->a,cur);if(c)return c;}
    else{jev(s->a,cur);if(jthrow)return JC_THROW;}}
   if(s->fl&JF_LEX)cur=jcopyenv(cur);
   for(;;){
    if(s->b){v=jev(s->b,cur);if(jthrow)return JC_THROW;if(!jtruthy(v))break;}
    c=jexec(s->d,cur);
    JLOOPCTL(c,s)
    if(s->fl&JF_LEX)cur=jcopyenv(cur);
    if(s->c){jev(s->c,cur);if(jthrow)return JC_THROW;}}
   return 0;}
  case ND_FORIN:case ND_FOROF:{
   JV coll;JOb*keys=0,*arr=0;JStr*str=0;int i=0,n,kind=s->x?(s->op==0?0:s->op==1?1:2):3;JOb*it;
   coll=jev(s->b,env);if(jthrow)return JC_THROW;
   if(s->t==ND_FORIN){keys=jnewarr(8);jkeysof(coll,keys);arr=keys;}
   else{arr=jiterable(coll);if(!arr){jthrowerr(jErrT,"value is not iterable");return JC_THROW;}}
   n=0;
   for(;;){
    JV x;
    if(arr){if(i>=arr->na)break;x=arr->it[i];}
    else{if(i>=jcplen(str))break;x=jstrat(str,i);}
    i++;(void)n;
    it=(kind==1||kind==2)?jnewenv(env):env;
    jbindpat(s->a,x,it,kind);if(jthrow)return JC_THROW;
    c=jexec(s->c,it);
    JLOOPCTL(c,s)}
   return 0;}
  case ND_RET:
   v=s->a?jev(s->a,env):jundef();if(jthrow)return JC_THROW;
   jrv=v;return JC_RET;
  case ND_BRK:jlabel=s->s;return JC_BRK;
  case ND_CONT:jlabel=s->s;return JC_CONT;
  case ND_THROW:v=jev(s->a,env);if(jthrow)return JC_THROW;jexv=v;return JC_THROW;
  case ND_LABEL:
   c=jexec(s->a,env);
   if(c==JC_BRK&&jlabel&&jkeq(jlabel,s->s)){jlabel=0;return 0;}
   return c;
  case ND_TRY:{
   c=jexec(s->a,env);
   if(c==JC_THROW&&!jabort&&s->c){
    JOb*ce=jnewenv(env);
    jthrow=0;
    if(s->b){jbindpat(s->b,jexv,ce,1);if(jthrow)c=JC_THROW;else c=jexec(s->c,ce);}
    else c=jexec(s->c,ce);}
   if(s->d&&!jabort){
    JV sr=jrv,se=jexv;JStr*sl=jlabel;int fc;
    jthrow=0;
    fc=jexec(s->d,env);
    if(fc!=JC_NORM)c=fc;else{jrv=sr;jexv=se;jlabel=sl;}}
   return c;}
  case ND_SWITCH:{
   JOb*e2=env;JNd*cs,*bd;int found=0;JV d;
   d=jev(s->a,env);if(jthrow)return JC_THROW;
   if(s->fl&JF_LEX){e2=jnewenv(env);for(cs=s->b;cs;cs=cs->nx)jhoist(cs->b,e2);}
   for(cs=s->b;cs&&!found;cs=cs->nx){
    if(cs->op==1)continue;
    v=jev(cs->a,e2);if(jthrow)return JC_THROW;
    if(jseq(d,v)){found=1;break;}}
   if(!found){for(cs=s->b;cs;cs=cs->nx)if(cs->op==1){found=1;break;}}
   if(!found)return 0;
   for(;cs;cs=cs->nx){
    for(bd=cs->b;bd;bd=bd->nx){
     c=jexec(bd,e2);
     if(c==JC_BRK&&!jlabel)return 0;
     if(c)return c;}}
   return 0;}
  default:return 0;}}

/* ---- builtins ---- */
#if defined(__GNUC__)
#define JUN __attribute__((unused))
#else
#define JUN
#endif
#define JNAT(name) static JV name(JV th JUN,JV*args JUN,int argc JUN)
#define JUNUSED (void)th;(void)args;(void)argc
static int jrx_isre(JV v);static JV jrx_replace(JStr*s,JOb*re,JV rep,int forceall);static JV jrx_split(JStr*s,JOb*re,int lim);
static int jb2cp(JStr*s,int b);
/* math helpers (no libm) */
static double jsqrt(double x){
 ju64 f;int e;unsigned __int128 F,res=0,bit,rem;ju64 m;union{double d;ju64 u;}c;int eb;
 if(x<0||x!=x)return jNAN();
 if(x==0||jisinf(x))return x;
 jdecomp(x,&f,&e);
 while(!(f&(1ULL<<52))){f<<=1;e--;}
 if(e&1){f<<=1;e--;}
 F=(unsigned __int128)f<<54;e-=54;
 rem=F;bit=(unsigned __int128)1<<126;
 while(bit>rem)bit>>=2;
 while(bit){if(rem>=res+bit){rem-=res+bit;res=(res>>1)+bit;}else res>>=1;bit>>=2;}
 m=(ju64)res;
 if(m&1){if(rem>0)m=(m>>1)+1;else{m>>=1;if(m&1)m++;}}else m>>=1;
 e=e/2+1;
 /* result = m * 2^e, m < 2^53+1 */
 while(m>=(1ULL<<53)){m>>=1;e++;}
 while(m&&m<(1ULL<<52)){m<<=1;e--;}
 eb=e+52+1023;
 c.u=((ju64)eb<<52)|(m&((1ULL<<52)-1));
 return c.d;}
static const double JLN2HI=6.93147180369123816490e-01,JLN2LO=1.90821492927058770002e-10;
static double jln(double x){
 union{double d;ju64 u;}c;int k=0,eb,i;double m,f,s,s2,t,r;
 if(x<0||x!=x)return jNAN();
 if(x==0)return-jINF();
 if(jisinf(x))return x;
 c.d=x;eb=(int)((c.u>>52)&0x7ff);
 if(eb==0){c.d=x*jp2(54);eb=(int)((c.u>>52)&0x7ff);k=-54;}
 k+=eb-1023;c.u=(c.u&((1ULL<<52)-1))|((ju64)1023<<52);m=c.d;
 if(m>1.4142135623730951){m/=2;k++;}
 f=m-1;s=f/(2+f);s2=s*s;t=s;r=0;
 for(i=1;i<40;i+=2){r+=t/i;t*=s2;}
 r*=2;
 return(k*JLN2HI)+(r+k*JLN2LO);}
static double jexpd(double x){
 int k,i;double r,t,s;
 if(x!=x)return x;
 if(x>709.78)return jINF();
 if(x<-745.2)return 0;
 k=(int)jround(x/0.6931471805599453);
 r=(x-k*JLN2HI)-k*JLN2LO;
 s=1;t=1;for(i=1;i<22;i++){t*=r/i;s+=t;}
 if(k>1000){return s*jp2(k-100)*jp2(100);}
 if(k<-1000){return s*jp2(k+100)*jp2(-100);}
 return s*jp2(k);}
static double jpow(double x,double y){
 if(y==0)return 1;
 if(x!=x||y!=y)return jNAN();
 if(y==jtrunc(y)&&jfabs(y)<1e9){
  double r=1,b=x;j64 e=(j64)jfabs(y);
  while(e){if(e&1)r*=b;b*=b;e>>=1;}
  return y<0?1/r:r;}
 if(x<0)return jNAN();
 if(x==0)return y>0?0:jINF();
 if(y==0.5)return jsqrt(x);
 return jexpd(y*jln(x));}
static const double JPI=3.14159265358979323846;
static double jsin(double x){
 double t,s;int i;
 if(x!=x||jisinf(x))return jNAN();
 x=jmod(x,2*JPI);if(x>JPI)x-=2*JPI;if(x<-JPI)x+=2*JPI;
 if(x>JPI/2)x=JPI-x;else if(x<-JPI/2)x=-JPI-x;
 t=x;s=x;for(i=1;i<14;i++){t*=-x*x/((2*i)*(2*i+1));s+=t;}
 return s;}
static double jcos(double x){return jsin(x+JPI/2);}
static double jatan(double x){
 double s,t;int i,inv=0,neg=0;
 if(x!=x)return x;
 if(x<0){neg=1;x=-x;}
 if(x>1){inv=1;x=1/x;}
 {int h=0;while(x>0.4){x=x/(1+jsqrt(1+x*x));h++;}
  t=x;s=x;for(i=1;i<40;i++){t*=-x*x;s+=t/(2*i+1);}
  while(h--)s*=2;}
 if(inv)s=JPI/2-s;
 return neg?-s:s;}
static ju64 jrng=88172645463325252ULL;
static double jrand(void){jrng^=jrng<<13;jrng^=jrng>>7;jrng^=jrng<<17;return(double)(jrng>>11)/9007199254740992.0;}
static j64 jnowunix,jnowmono;

/* Object */
JNAT(jf_Object){JUNUSED;if(argc>0&&args[0].t==JT_O)return args[0];if(th.t==JT_O&&jnewcall)return th;return jobj(jnewob(OK_PLAIN,jObjProto));}
JNAT(jf_keys){
 JOb*r=jnewarr(8);JV o=JARG(0);int i;
 JUNUSED;
 if(o.t==JT_S){for(i=0;i<jcplen(o.u.s);i++)jarrpush(r,jstrv(jnum2s(i)));return jobj(r);}
 if(o.t!=JT_O)return jobj(r);
 if(o.u.o->k==OK_ARR)for(i=0;i<o.u.o->na;i++)jarrpush(r,jstrv(jnum2s(i)));
 if(o.u.o->k>=OK_NODE&&o.u.o->k!=OK_DATE&&o.u.o->k!=OK_BOUND)return jobj(r);
 for(i=0;i<o.u.o->np;i++)if(!(o.u.o->pr[i].fl&1))jarrpush(r,jstrv(o.u.o->pr[i].k));
 return jobj(r);}
JNAT(jf_values){
 JV o=JARG(0);JOb*k;JOb*r=jnewarr(8);int i;
 JUNUSED;
 k=jnewarr(8);jkeysof(o,k);
 for(i=0;i<k->na;i++){JV v=jget(o,k->it[i].t==JT_S?k->it[i].u.s:jtos(k->it[i]));if(jthrow)return jundef();jarrpush(r,v);}
 return jobj(r);}
JNAT(jf_entries){
 JV o=JARG(0);JOb*k,*r=jnewarr(8);int i;
 JUNUSED;
 k=jnewarr(8);jkeysof(o,k);
 for(i=0;i<k->na;i++){
  JStr*ks=k->it[i].u.s;JV v=jget(o,ks);JOb*pr;
  if(jthrow)return jundef();
  pr=jnewarr(2);jarrpush(pr,jstrv(ks));jarrpush(pr,v);jarrpush(r,jobj(pr));}
 return jobj(r);}
JNAT(jf_assign){
 int i;JUNUSED;
 if(argc<1||args[0].t!=JT_O)return jthrowerr(jErrT,"Cannot convert undefined or null to object");
 for(i=1;i<argc;i++){
  JV s=args[i];int k;
  if(s.t==JT_O){
   if(s.u.o->k==OK_ARR)for(k=0;k<s.u.o->na;k++)jputv(args[0],jnum(k),s.u.o->it[k]);
   for(k=0;k<s.u.o->np;k++)if(!(s.u.o->pr[k].fl&1)){JV x=s.u.o->pr[k].v;if(s.u.o->pr[k].fl&4)x=jaccget(&s.u.o->pr[k],s);jput(args[0],s.u.o->pr[k].k,x);}}}
 return args[0];}
JNAT(jf_freeze){JUNUSED;return JARG(0);}
JNAT(jf_create){
 JOb*o;JUNUSED;
 o=jnewob(OK_PLAIN,argc>0&&args[0].t==JT_O?args[0].u.o:0);
 return jobj(o);}
JNAT(jf_getproto){JUNUSED;if(argc>0&&args[0].t==JT_O&&args[0].u.o->proto)return jobj(args[0].u.o->proto);return jnull();}
JNAT(jf_defprop){
 JV o=JARG(0),k=JARG(1),d=JARG(2);JV v;JStr*ks;JPr*p;
 JUNUSED;
 if(o.t!=JT_O||d.t!=JT_O)return jthrowerr(jErrT,"Object.defineProperty called on non-object");
 ks=jkey(k);
 {JV g=jget(d,jcstr("get")),st=jget(d,jcstr("set"));
  if(jiscallable(g)||jiscallable(st)){
   int fl=jtruthy(jget(d,jcstr("enumerable")))?0:1;
   if(jiscallable(g))jdefacc(o.u.o,ks,g,0,fl);
   if(jiscallable(st))jdefacc(o.u.o,ks,st,1,fl);
   return o;}}
 v=jget(d,jcstr("value"));
 jput(o,ks,v);
 p=jfind(o.u.o,ks);
 if(p){JV en=jget(d,jcstr("enumerable"));if(!jtruthy(en))p->fl|=1;}
 return o;}
JNAT(jf_hasOwn){
 JV o=JARG(0);JStr*k=jkey(JARG(1));int f;
 JUNUSED;
 if(o.t!=JT_O)return jbool(0);
 jgetown(o.u.o,k,&f);return jbool(f);}
JNAT(jfp_hasOwnProperty){
 int f;JStr*k=jkey(JARG(0));
 if(th.t==JT_S)return jbool(jhasprop(th,k));
 if(th.t!=JT_O)return jbool(0);
 jgetown(th.u.o,k,&f);return jbool(f);}
JNAT(jfp_objToString){
 JUNUSED;
 if(th.t==JT_O&&th.u.o->k==OK_ARR)return jcv("[object Array]");
 if(th.t==JT_U)return jcv("[object Undefined]");
 if(th.t==JT_NUL)return jcv("[object Null]");
 if(th.t==JT_O&&th.u.o->k==OK_NODE)return jcv("[object HTMLElement]");
 return jcv("[object Object]");}
JNAT(jfp_valueOf){JUNUSED;return th;}
JNAT(jf_isArray){JUNUSED;return jbool(argc>0&&args[0].t==JT_O&&args[0].u.o->k==OK_ARR);}
/* Function.prototype */
JNAT(jfp_call){return jcall(th,JARG(0),argc>1?args+1:0,argc>1?argc-1:0);}
JNAT(jfp_apply){
 JV a=JARG(1);
 if(a.t==JT_O&&a.u.o->k==OK_ARR)return jcall(th,JARG(0),a.u.o->it,a.u.o->na);
 return jcall(th,JARG(0),0,0);}
JNAT(jfp_bind){
 JOb*b;int i;
 if(!jiscallable(th))return jthrowerr(jErrT,"Bind must be called on a function");
 b=jnewob(OK_BOUND,jFnProto);b->ex=th;
 jarrpush(b,JARG(0));for(i=1;i<argc;i++)jarrpush(b,args[i]);
 return jobj(b);}
/* Error */
static JV jmkerr(JOb*proto,JV th,JV*args,int argc){
 JOb*e;
 if(th.t==JT_O&&jnewcall)e=th.u.o;else e=jnewob(OK_ERR,proto);
 e->k=OK_ERR;
 if(argc>0&&args[0].t!=JT_U){JStr*m=jtos(args[0]);jsetown(e,K_message,jstrv(m));{JPr*p=jfind(e,K_message);if(p)p->fl|=1;}}
 return jobj(e);}
JNAT(jf_Error){return jmkerr(jErrProto,th,args,argc);}
JNAT(jf_TypeError){return jmkerr(jErrT,th,args,argc);}
JNAT(jf_RangeError){return jmkerr(jErrR,th,args,argc);}
JNAT(jf_SyntaxError){return jmkerr(jErrS,th,args,argc);}
JNAT(jf_RefError){return jmkerr(jErrF,th,args,argc);}
JNAT(jfp_errToString){
 JStr*n=jtos(jget(th,K_name)),*m;JUNUSED;
 m=jtos(jget(th,K_message));
 if(!m->len)return jstrv(n);
 return jstrv(jcat(jcat(n,jcstr(": ")),m));}
/* Array */
JNAT(jf_Array){
 JOb*a=jnewarr(4);int i;
 if(argc==1&&args[0].t==JT_N){
  int n=(int)args[0].u.d;
  if(n<0||(double)n!=args[0].u.d)return jthrowerr(jErrR,"Invalid array length");
  jarrgrow(a,n);for(i=0;i<n&&i<a->ca;i++)a->it[i]=jundef();a->na=n<=a->ca?n:a->na;(void)th;return jobj(a);}
 for(i=0;i<argc;i++)jarrpush(a,args[i]);
 (void)th;
 return jobj(a);}
#define JTHISARR JOb*a;if(th.t!=JT_O||th.u.o->k!=OK_ARR)return jthrowerr(jErrT,"not an array");a=th.u.o;
JNAT(jfa_push){int i;JTHISARR for(i=0;i<argc;i++)jarrpush(a,args[i]);return jnum(a->na);}
JNAT(jfa_pop){JTHISARR(void)args;(void)argc;if(!a->na)return jundef();return a->it[--a->na];}
JNAT(jfa_shift){int i;JV v;JTHISARR(void)args;(void)argc;if(!a->na)return jundef();v=a->it[0];for(i=1;i<a->na;i++)a->it[i-1]=a->it[i];a->na--;return v;}
JNAT(jfa_unshift){int i;JTHISARR jarrgrow(a,a->na+argc);if(a->na+argc>a->ca)return jundef();for(i=a->na-1;i>=0;i--)a->it[i+argc]=a->it[i];for(i=0;i<argc;i++)a->it[i]=args[i];a->na+=argc;return jnum(a->na);}
static int jrelidx(JV v,int len,int def){double d;if(v.t==JT_U)return def;d=jtonum(v);if(d!=d)return 0;if(d<0){d+=len;if(d<0)d=0;}if(d>len)d=len;return(int)d;}
JNAT(jfa_slice){int s,e,i;JOb*r;JTHISARR s=jrelidx(JARG(0),a->na,0);e=jrelidx(JARG(1),a->na,a->na);r=jnewarr(e>s?e-s:0);for(i=s;i<e;i++)jarrpush(r,a->it[i]);return jobj(r);}
JNAT(jfa_splice){
 int s,dc,i,ni=argc>2?argc-2:0;JOb*r,*t;JTHISARR
 s=jrelidx(JARG(0),a->na,0);
 dc=argc<2?a->na-s:(int)jtonum(args[1]);if(argc==0)dc=0;
 if(dc<0)dc=0;
 if(dc>a->na-s)dc=a->na-s;
 r=jnewarr(dc);for(i=0;i<dc;i++)jarrpush(r,a->it[s+i]);
 t=jnewarr(a->na+ni);
 for(i=0;i<s;i++)jarrpush(t,a->it[i]);
 for(i=0;i<ni;i++)jarrpush(t,args[2+i]);
 for(i=s+dc;i<a->na;i++)jarrpush(t,a->it[i]);
 a->it=t->it;a->ca=t->ca;a->na=t->na;
 return jobj(r);}
JNAT(jfa_concat){
 JOb*r;int i,k;JTHISARR r=jnewarr(a->na+4);
 for(i=0;i<a->na;i++)jarrpush(r,a->it[i]);
 for(i=0;i<argc;i++){
  if(args[i].t==JT_O&&args[i].u.o->k==OK_ARR)for(k=0;k<args[i].u.o->na;k++)jarrpush(r,args[i].u.o->it[k]);
  else jarrpush(r,args[i]);}
 return jobj(r);}
static JStr*jjoin(JOb*a,JStr*sep,int depth){
 JSB b={0,0,0};int i;
 for(i=0;i<a->na;i++){
  JV v=a->it[i];JStr*s;
  if(i)jsb_str(&b,sep);
  if(v.t==JT_U||v.t==JT_NUL)continue;
  if(v.t==JT_O&&v.u.o->k==OK_ARR&&depth>8)continue;
  s=jtos(v);if(jthrow)return jsb_done(&b);
  jsb_str(&b,s);}
 return jsb_done(&b);}
JNAT(jfa_join){JStr*sep;JTHISARR sep=argc>0&&args[0].t!=JT_U?jtos(args[0]):jcstr(",");return jstrv(jjoin(a,sep,jdepth));}
JNAT(jfa_toString){JTHISARR(void)args;(void)argc;return jstrv(jjoin(a,jcstr(","),jdepth));}
JNAT(jfa_reverse){int i;JTHISARR(void)args;(void)argc;for(i=0;i<a->na/2;i++){JV t=a->it[i];a->it[i]=a->it[a->na-1-i];a->it[i]=a->it[i];a->it[a->na-1-i]=t;}return th;}
JNAT(jfa_indexOf){int i,s;JTHISARR s=jrelidx(JARG(1),a->na,0);for(i=s;i<a->na;i++)if(jseq(a->it[i],JARG(0)))return jnum(i);return jnum(-1);}
JNAT(jfa_lastIndexOf){int i;JTHISARR for(i=a->na-1;i>=0;i--)if(jseq(a->it[i],JARG(0)))return jnum(i);return jnum(-1);}
JNAT(jfa_includes){int i;JTHISARR for(i=0;i<a->na;i++)if(jsamevalue(a->it[i],JARG(0)))return jbool(1);return jbool(0);}
#define JCB JV f=JARG(0);if(!jiscallable(f))return jthrowerr(jErrT,"callback is not a function");
JNAT(jfa_forEach){int i;JCB JTHISARR for(i=0;i<a->na;i++){JV av[3];av[0]=a->it[i];av[1]=jnum(i);av[2]=th;jcall(f,JARG(1),av,3);if(jthrow)return jundef();}return jundef();}
JNAT(jfa_map){int i;JOb*r;JCB JTHISARR r=jnewarr(a->na);for(i=0;i<a->na;i++){JV av[3],v;av[0]=a->it[i];av[1]=jnum(i);av[2]=th;v=jcall(f,JARG(1),av,3);if(jthrow)return jundef();jarrpush(r,v);}return jobj(r);}
JNAT(jfa_filter){int i;JOb*r;JCB JTHISARR r=jnewarr(4);for(i=0;i<a->na;i++){JV av[3],v;av[0]=a->it[i];av[1]=jnum(i);av[2]=th;v=jcall(f,JARG(1),av,3);if(jthrow)return jundef();if(jtruthy(v))jarrpush(r,a->it[i]);}return jobj(r);}
JNAT(jfa_find){int i;JCB JTHISARR for(i=0;i<a->na;i++){JV av[3],v;av[0]=a->it[i];av[1]=jnum(i);av[2]=th;v=jcall(f,JARG(1),av,3);if(jthrow)return jundef();if(jtruthy(v))return a->it[i];}return jundef();}
JNAT(jfa_findIndex){int i;JCB JTHISARR for(i=0;i<a->na;i++){JV av[3],v;av[0]=a->it[i];av[1]=jnum(i);av[2]=th;v=jcall(f,JARG(1),av,3);if(jthrow)return jundef();if(jtruthy(v))return jnum(i);}return jnum(-1);}
JNAT(jfa_some){int i;JCB JTHISARR for(i=0;i<a->na;i++){JV av[3],v;av[0]=a->it[i];av[1]=jnum(i);av[2]=th;v=jcall(f,JARG(1),av,3);if(jthrow)return jundef();if(jtruthy(v))return jbool(1);}return jbool(0);}
JNAT(jfa_every){int i;JCB JTHISARR for(i=0;i<a->na;i++){JV av[3],v;av[0]=a->it[i];av[1]=jnum(i);av[2]=th;v=jcall(f,JARG(1),av,3);if(jthrow)return jundef();if(!jtruthy(v))return jbool(0);}return jbool(1);}
JNAT(jfa_reduce){
 int i=0;JV acc;JCB JTHISARR
 if(argc>1)acc=args[1];else{if(!a->na)return jthrowerr(jErrT,"Reduce of empty array with no initial value");acc=a->it[0];i=1;}
 for(;i<a->na;i++){JV av[4];av[0]=acc;av[1]=a->it[i];av[2]=jnum(i);av[3]=th;acc=jcall(f,jundef(),av,4);if(jthrow)return jundef();}
 return acc;}
JNAT(jfa_reduceRight){
 int i;JV acc;JCB JTHISARR
 i=a->na-1;
 if(argc>1)acc=args[1];else{if(!a->na)return jthrowerr(jErrT,"Reduce of empty array with no initial value");acc=a->it[i--];}
 for(;i>=0;i--){JV av[4];av[0]=acc;av[1]=a->it[i];av[2]=jnum(i);av[3]=th;acc=jcall(f,jundef(),av,4);if(jthrow)return jundef();}
 return acc;}
JNAT(jfa_findLast){int i;JCB JTHISARR for(i=a->na-1;i>=0;i--){JV av[3],v;av[0]=a->it[i];av[1]=jnum(i);av[2]=th;v=jcall(f,JARG(1),av,3);if(jthrow)return jundef();if(jtruthy(v))return a->it[i];}return jundef();}
JNAT(jfa_findLastIndex){int i;JCB JTHISARR for(i=a->na-1;i>=0;i--){JV av[3],v;av[0]=a->it[i];av[1]=jnum(i);av[2]=th;v=jcall(f,JARG(1),av,3);if(jthrow)return jundef();if(jtruthy(v))return jnum(i);}return jnum(-1);}
JNAT(jfa_fill){int i,s,e;JTHISARR s=jrelidx(JARG(1),a->na,0);e=jrelidx(JARG(2),a->na,a->na);for(i=s;i<e;i++)a->it[i]=JARG(0);return th;}
static void jflat(JOb*r,JOb*a,int d){int i;for(i=0;i<a->na;i++){if(d>0&&a->it[i].t==JT_O&&a->it[i].u.o->k==OK_ARR)jflat(r,a->it[i].u.o,d-1);else jarrpush(r,a->it[i]);}}
JNAT(jfa_flat){JOb*r;JTHISARR r=jnewarr(a->na);{double dd=argc>0?jtonum(args[0]):1;jflat(r,a,dd>50?50:(int)dd);};return jobj(r);}
static int jsortless(JV f,JV x,JV y){
 JV av[2],r;
 if(x.t==JT_U)return 0;
 if(y.t==JT_U)return 1;
 if(f.t==JT_U){JStr*a=jtos(x),*b=jtos(y);int i,n=a->len<b->len?a->len:b->len;for(i=0;i<n;i++){unsigned char p=(unsigned char)a->d[i],q=(unsigned char)b->d[i];if(p!=q)return p<q;}return a->len<b->len;}
 av[0]=x;av[1]=y;r=jcall(f,jundef(),av,2);if(jthrow)return 0;return jtonum(r)<0;}
JNAT(jfa_sort){
 JV f=JARG(0);int n,w,i,l,m,r,k;JV*t;JTHISARR
 n=a->na;if(n<2)return th;
 t=jalloc(n*(int)sizeof(JV),BT_VALS);if(!t)return th;
 for(w=1;w<n;w*=2){
  for(l=0;l<n;l+=2*w){
   m=l+w<n?l+w:n;r=l+2*w<n?l+2*w:n;i=l;k=l;{int j=m;
   while(i<m&&j<r){if(jsortless(f,a->it[j],a->it[i]))t[k++]=a->it[j++];else t[k++]=a->it[i++];if(jthrow)return jundef();}
   while(i<m)t[k++]=a->it[i++];
   while(j<r)t[k++]=a->it[j++];}}
  for(i=0;i<n;i++)a->it[i]=t[i];}
 return th;}
JNAT(jfa_at){int i;JTHISARR i=(int)jtonum(JARG(0));if(i<0)i+=a->na;if(i<0||i>=a->na)return jundef();return a->it[i];}
JNAT(jf_from){
 JV s=JARG(0),f=JARG(1);JOb*r=jnewarr(8);int i,n;JUNUSED;
 if(s.t==JT_S){n=jcplen(s.u.s);for(i=0;i<n;i++)jarrpush(r,jstrat(s.u.s,i));}
 else if(s.t==JT_O&&(s.u.o->k==OK_ARR||(s.u.o->fl&12))){JOb*it=jiterable(s);for(i=0;i<it->na;i++)jarrpush(r,it->it[i]);}
 else if(s.t==JT_O){JV l=jget(s,K_length);n=(int)jtonum(l);for(i=0;i<n&&i<100000;i++){JV v=jgetv(s,jnum(i));jarrpush(r,v);}}
 if(jiscallable(f))for(i=0;i<r->na;i++){JV av[2],v;av[0]=r->it[i];av[1]=jnum(i);v=jcall(f,jundef(),av,2);if(jthrow)return jundef();r->it[i]=v;}
 return jobj(r);}
JNAT(jf_of){JOb*r=jnewarr(argc);int i;JUNUSED;for(i=0;i<argc;i++)jarrpush(r,args[i]);return jobj(r);}
/* String */
JNAT(jf_String){JUNUSED;if(argc==0)return jcv("");if(args[0].t==JT_O&&args[0].u.o->k==OK_PLAIN&&0)return jundef();{JStr*s=jtos(args[0]);return jstrv(s);}}
#define JTHISS JStr*s;if(th.t==JT_S)s=th.u.s;else if(th.t==JT_U||th.t==JT_NUL)return jthrowerr(jErrT,"String.prototype method called on null or undefined");else{s=jtos(th);if(jthrow)return jundef();}
static int jfindstr(JStr*h,JStr*n,int from){int i,j;if(!n->len)return from<=h->len?from:h->len;for(i=from;i+n->len<=h->len;i++){for(j=0;j<n->len&&h->d[i+j]==n->d[j];j++);if(j==n->len)return i;}return-1;}
static int jb2cp(JStr*s,int b){int i,n=0;if(s->asc||b<0)return b;for(i=0;i<b&&i<s->len;i++)if(((unsigned char)s->d[i]&0xC0)!=0x80)n++;return n;}
JNAT(jfs_charAt){JTHISS{double d=jtonum(JARG(0));JV v;if(d!=d)d=0;v=jstrat(s,(int)d);return v.t==JT_U?jcv(""):v;}}
JNAT(jfs_charCodeAt){JTHISS{double d=jtonum(JARG(0));JV v;int l;if(d!=d)d=0;v=jstrat(s,(int)d);if(v.t==JT_U)return jnum(jNAN());return jnum(jdec8(v.u.s->d,v.u.s->len,&l));}}
JNAT(jfs_indexOf){JTHISS{JStr*n=jtos(JARG(0));int r=jfindstr(s,n,jcpoff(s,jrelidx(JARG(1),jcplen(s),0)));return jnum(r<0?-1:jb2cp(s,r));}}
JNAT(jfs_lastIndexOf){JTHISS{JStr*n=jtos(JARG(0));int i,last=-1;for(i=0;(i=jfindstr(s,n,i))>=0;i++)last=i;return jnum(last<0?-1:jb2cp(s,last));}}
JNAT(jfs_includes){JTHISS{JStr*n=jtos(JARG(0));return jbool(jfindstr(s,n,0)>=0);}}
JNAT(jfs_startsWith){JTHISS{JStr*n=jtos(JARG(0));int i;if(n->len>s->len)return jbool(0);for(i=0;i<n->len;i++)if(s->d[i]!=n->d[i])return jbool(0);return jbool(1);}}
JNAT(jfs_endsWith){JTHISS{JStr*n=jtos(JARG(0));int i;if(n->len>s->len)return jbool(0);for(i=0;i<n->len;i++)if(s->d[s->len-n->len+i]!=n->d[i])return jbool(0);return jbool(1);}}
JNAT(jfs_slice){JTHISS{int l=jcplen(s);return jstrv(jsub(s,jrelidx(JARG(0),l,0),jrelidx(JARG(1),l,l)));}}
JNAT(jfs_substring){JTHISS{int l=jcplen(s);double a=argc>0?jtonum(args[0]):0,b=argc>1&&args[1].t!=JT_U?jtonum(args[1]):l;int x,y,t;if(a!=a||a<0)a=0;if(b!=b||b<0)b=0;if(a>l)a=l;if(b>l)b=l;x=(int)a;y=(int)b;if(x>y){t=x;x=y;y=t;}return jstrv(jsub(s,x,y));}}
JNAT(jfs_substr){JTHISS{int l=jcplen(s),a=jrelidx(JARG(0),l,0),n=argc>1&&args[1].t!=JT_U?(int)jtonum(args[1]):l-a;if(n<0)n=0;return jstrv(jsub(s,a,a+n));}}
JNAT(jfs_upper){JTHISS{JStr*r;int i;r=jnewstr(s->d,s->len);if(r==&jemptys)return jstrv(r);for(i=0;i<r->len;i++)if(r->d[i]>='a'&&r->d[i]<='z')r->d[i]-=32;return jstrv(r);}}
JNAT(jfs_lower){JTHISS{JStr*r;int i;r=jnewstr(s->d,s->len);if(r==&jemptys)return jstrv(r);for(i=0;i<r->len;i++)if(r->d[i]>='A'&&r->d[i]<='Z')r->d[i]+=32;return jstrv(r);}}
static int jiswsc(unsigned char c){return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f'||c=='\v';}
JNAT(jfs_trim){JTHISS{int a=0,b=s->len;while(a<b&&jiswsc((unsigned char)s->d[a]))a++;while(b>a&&jiswsc((unsigned char)s->d[b-1]))b--;return jsv(s->d+a,b-a);}}
JNAT(jfs_trimStart){JTHISS{int a=0;while(a<s->len&&jiswsc((unsigned char)s->d[a]))a++;return jsv(s->d+a,s->len-a);}}
JNAT(jfs_trimEnd){JTHISS{int b=s->len;while(b>0&&jiswsc((unsigned char)s->d[b-1]))b--;return jsv(s->d,b);}}
JNAT(jfs_split){
 JTHISS{
  JOb*r=jnewarr(8);
  if(argc==0||args[0].t==JT_U){jarrpush(r,th.t==JT_S?th:jstrv(s));return jobj(r);}
  if(jrx_isre(args[0]))return jrx_split(s,args[0].u.o,argc>1&&args[1].t!=JT_U?(int)jtonum(args[1]):1<<30);
  {JStr*sep=jtos(args[0]);int i=0,p;int lim=argc>1&&args[1].t!=JT_U?(int)jtonum(args[1]):1<<30;
   if(!sep->len){int o=0,l2;while(o<s->len&&r->na<lim){jdec8(s->d+o,s->len-o,&l2);jarrpush(r,jsv(s->d+o,l2));o+=l2;}return jobj(r);}
   while((p=jfindstr(s,sep,i))>=0&&r->na<lim){jarrpush(r,jsv(s->d+i,p-i));i=p+sep->len;}
   if(r->na<lim)jarrpush(r,jsv(s->d+i,s->len-i));
   return jobj(r);}}}
static JStr*jrepl(JStr*s,JStr*n,JV rep,int all,int*err){
 JSB b={0,0,0};int i=0,p;
 for(;;){
  p=jfindstr(s,n,i);
  if(p<0)break;
  jsb_add(&b,s->d+i,p-i);
  if(jiscallable(rep)){JV av[3],r;av[0]=jstrv(n);av[1]=jnum(jb2cp(s,p));av[2]=jstrv(s);r=jcall(rep,jundef(),av,3);if(jthrow){*err=1;return jsb_done(&b);}jsb_str(&b,jtos(r));}
  else jsb_str(&b,jtos(rep));
  i=p+n->len;
  if(!all)break;
  if(!n->len){if(i<s->len){jsb_add(&b,s->d+i,1);i++;}else break;}}
 jsb_add(&b,s->d+i,s->len-i);
 return jsb_done(&b);}
JNAT(jfs_replace){JTHISS{int err=0;JStr*r;if(argc>0&&jrx_isre(args[0]))return jrx_replace(s,args[0].u.o,JARG(1),0);r=jrepl(s,jtos(JARG(0)),JARG(1),0,&err);return jstrv(r);}}
JNAT(jfs_replaceAll){JTHISS{int err=0;JStr*r;if(argc>0&&jrx_isre(args[0]))return jrx_replace(s,args[0].u.o,JARG(1),1);r=jrepl(s,jtos(JARG(0)),JARG(1),1,&err);return jstrv(r);}}
JNAT(jfs_repeat){JTHISS{
 double dn=jtonum(JARG(0));int n,i;JStr*r;
 if(dn<0||dn!=dn||jisinf(dn)||(double)s->len*dn>8e6)return jthrowerr(jErrR,"Invalid count value");
 n=(int)dn;if(!n||!s->len)return jcv("");
 r=jalloc(8+s->len*n+1,BT_STR);if(!r)return jcv("");
 r->len=s->len*n;r->asc=s->asc;
 for(i=0;i<r->len;i++)r->d[i]=s->d[i%s->len];
 r->d[r->len]=0;return jstrv(r);}}
JNAT(jfs_pad){
 JTHISS{
  int n=(int)jtonum(JARG(0)),l=jcplen(s);JStr*p=argc>1&&args[1].t!=JT_U?jtos(args[1]):jcstr(" "),*acc=&jemptys;
  if(n<=l||!p->len||n>1000000)return jstrv(s);
  while(jcplen(acc)<n-l)acc=jcat(acc,p);
  acc=jsub(acc,0,n-l);
  return jstrv(jcat(acc,s));}}
JNAT(jfs_padEnd){
 JTHISS{
  int n=(int)jtonum(JARG(0)),l=jcplen(s);JStr*p=argc>1&&args[1].t!=JT_U?jtos(args[1]):jcstr(" "),*acc=&jemptys;
  if(n<=l||!p->len||n>1000000)return jstrv(s);
  while(jcplen(acc)<n-l)acc=jcat(acc,p);
  return jstrv(jcat(s,jsub(acc,0,n-l)));}}
JNAT(jfs_concat){JTHISS{int i;JStr*acc=s;for(i=0;i<argc;i++)acc=jcat(acc,jtos(args[i]));return jstrv(acc);}}
JNAT(jfs_toString){JUNUSED;if(th.t==JT_S)return th;return jstrv(jtos(th));}
JNAT(jfs_at){JTHISS{int i=(int)jtonum(JARG(0)),l=jcplen(s);if(i<0)i+=l;return jstrat(s,i);}}
JNAT(jfs_localeCompare){JTHISS{JStr*o=jtos(JARG(0));int i,n=s->len<o->len?s->len:o->len;for(i=0;i<n;i++){unsigned char p=(unsigned char)s->d[i],q=(unsigned char)o->d[i];if(p!=q)return jnum(p<q?-1:1);}return jnum(s->len<o->len?-1:s->len>o->len?1:0);}}
JNAT(jf_fromCharCode){char b[4*16];int n=0,i;JUNUSED;for(i=0;i<argc&&i<16;i++)n+=jenc8(b+n,(int)jtonum(args[i]));return jsv(b,n);}
/* Number / Boolean */
JNAT(jf_Number){
 JUNUSED;
 if(argc==0)return jnum(0);
 {double d=jtonum(args[0]);return jnum(d);}}
JNAT(jf_Boolean){JUNUSED;return jbool(argc>0&&jtruthy(args[0]));}
JNAT(jf_parseFloat){
 JStr*s;int i=0,n,ok;double d;JUNUSED;
 s=jtos(JARG(0));
 while(i<s->len&&jiswsc((unsigned char)s->d[i]))i++;
 n=i;if(n<s->len&&(s->d[n]=='+'||s->d[n]=='-'))n++;
 if(s->len-n>=8&&s->d[n]=='I'){d=jstod(s->d+i,s->len-i,&ok);return jnum(d);}
 {int dg=0,e=n;
  while(e<s->len&&isdg(s->d[e])){e++;dg=1;}
  if(e<s->len&&s->d[e]=='.'){e++;while(e<s->len&&isdg(s->d[e])){e++;dg=1;}}
  if(!dg)return jnum(jNAN());
  if(e<s->len&&(s->d[e]=='e'||s->d[e]=='E')){int k=e+1;if(k<s->len&&(s->d[k]=='+'||s->d[k]=='-'))k++;if(k<s->len&&isdg(s->d[k])){while(k<s->len&&isdg(s->d[k]))k++;e=k;}}
  d=jstod(s->d+i,e-i,&ok);return jnum(d);}}
JNAT(jf_parseInt){
 JStr*s;int i=0,neg=0,radix=10,any=0;double r=0;JUNUSED;
 s=jtos(JARG(0));
 if(argc>1&&args[1].t!=JT_U){radix=(int)jtonum(args[1]);if(radix==0)radix=10;}
 while(i<s->len&&jiswsc((unsigned char)s->d[i]))i++;
 if(i<s->len&&(s->d[i]=='+'||s->d[i]=='-')){neg=s->d[i]=='-';i++;}
 if((radix==16||argc<2)&&i+1<s->len&&s->d[i]=='0'&&(s->d[i+1]=='x'||s->d[i+1]=='X')){radix=16;i+=2;}
 if(radix<2||radix>36)return jnum(jNAN());
 for(;i<s->len;i++){
  int c=s->d[i],v;
  if(c>='0'&&c<='9')v=c-'0';else if(c>='a'&&c<='z')v=c-'a'+10;else if(c>='A'&&c<='Z')v=c-'A'+10;else break;
  if(v>=radix)break;
  r=r*radix+v;any=1;}
 if(!any)return jnum(jNAN());
 return jnum(neg?-r:r);}
JNAT(jf_isNaN){JUNUSED;return jbool(jisnan(jtonum(JARG(0))));}
JNAT(jf_isFinite){double d;JUNUSED;d=jtonum(JARG(0));return jbool(!jisnan(d)&&!jisinf(d));}
JNAT(jf_nIsInteger){JUNUSED;return jbool(argc>0&&args[0].t==JT_N&&args[0].u.d==jtrunc(args[0].u.d)&&!jisinf(args[0].u.d));}
JNAT(jf_nIsFinite){JUNUSED;return jbool(argc>0&&args[0].t==JT_N&&!jisnan(args[0].u.d)&&!jisinf(args[0].u.d));}
JNAT(jf_nIsNaN){JUNUSED;return jbool(argc>0&&args[0].t==JT_N&&jisnan(args[0].u.d));}
static JStr*jradix(double d,int radix){
 char b[80];int n=0,i;int neg=d<0;j64 ip;double fp;
 if(neg)d=-d;
 ip=(j64)d;fp=d-(double)ip;
 {char t[70];int k=0;if(!ip)t[k++]='0';while(ip){int dg=(int)(ip%radix);t[k++]=(char)(dg<10?'0'+dg:'a'+dg-10);ip/=radix;}if(neg)b[n++]='-';while(k)b[n++]=t[--k];}
 if(fp>0){b[n++]='.';for(i=0;i<20&&fp>0&&n<76;i++){int dg;fp*=radix;dg=(int)fp;b[n++]=(char)(dg<10?'0'+dg:'a'+dg-10);fp-=dg;}}
 return jnewstr(b,n);}
JNAT(jfn_toString){
 double d;
 if(th.t!=JT_N)return jstrv(jtos(th));
 d=th.u.d;
 if(argc>0&&args[0].t!=JT_U){int r=(int)jtonum(args[0]);if(r>=2&&r<=36&&r!=10&&!jisnan(d)&&!jisinf(d))return jstrv(jradix(d,r));}
 return jstrv(jnum2s(d));}
JNAT(jfn_toFixed){
 double d=th.t==JT_N?th.u.d:jtonum(th);int dg=(int)jtonum(JARG(0));char b[100];int n=0,neg=0;
 if(dg<0||dg>20)return jthrowerr(jErrR,"toFixed() digits argument must be between 0 and 20");
 if(d!=d)return jcv("NaN");
 if(jfabs(d)>=1e21)return jstrv(jnum2s(d));
 if(d<0){neg=1;d=-d;}
 n=jtofixed(d,dg,b+1);
 {int z=1,i;for(i=1;i<=n;i++)if(b[i]!='0'&&b[i]!='.'){z=0;break;}
  if(neg&&!z){b[0]='-';return jsv(b,n+1);}}
 return jsv(b+1,n);}
static JV jprec(double x,int p,int forceexp){
 char dg[40],ds[120],o[160];int n=0,e10,np,dp,i,neg=0,nd=0;
 if(x!=x)return jcv("NaN");
 if(jisinf(x))return jstrv(jnum2s(x));
 if(x<0){neg=1;x=-x;}
 if(x==0){e10=0;for(i=0;i<p;i++)dg[i]='0';}
 else{
  np=jdtoa(x,ds,&dp);(void)np;e10=dp-1;
  {int dec=p-1-e10;char b[160];
   if(dec>=0&&dec<=60&&x<1e21){int m=jtofixed(x,dec,b),z=0;for(i=0;i<m;i++)if(b[i]!='.'){if(!z&&b[i]=='0')continue;z=1;if(nd<p)dg[nd++]=b[i];}
    if(m>0&&nd>0){int digs=0;for(i=0;i<m;i++)if(b[i]!='.')digs++;
     {int lead=0;for(i=0;i<m&&(b[i]=='0'||b[i]=='.');i++)if(b[i]=='0')lead++;
      if(digs-lead>p+0&&digs-lead!=p){} }}
    {int tot=0,lz=0;for(i=0;i<m;i++)if(b[i]!='.'){tot++;}for(i=0;i<m&&(b[i]=='0'||b[i]=='.');i++)if(b[i]=='0')lz++;
     if(tot-lz>p){e10++;} }
    while(nd<p)dg[nd++]='0';}
   else{double q=jround(x/jp10(-dec>0?-dec:0));int k=0;char t[40];ju64 qq=(ju64)q;while(qq&&k<39){t[k++]=(char)('0'+qq%10);qq/=10;}
    for(i=0;i<k&&nd<p;i++)dg[nd++]=t[k-1-i];
    while(nd<p)dg[nd++]='0';
    if(k>p)e10+=k-p;}}}
 if(neg)o[n++]='-';
 if(forceexp||e10<-6||e10>=p){
  o[n++]=dg[0];if(p>1){o[n++]='.';for(i=1;i<p;i++)o[n++]=dg[i];}
  o[n++]='e';o[n++]=e10<0?'-':'+';{int ex=e10<0?-e10:e10;char t[8];int k=0;if(!ex)t[k++]='0';while(ex){t[k++]=(char)('0'+ex%10);ex/=10;}while(k)o[n++]=t[--k];}}
 else if(e10>=0){for(i=0;i<p;i++){if(i==e10+1)o[n++]='.';o[n++]=dg[i];}}
 else{o[n++]='0';o[n++]='.';for(i=0;i<-e10-1;i++)o[n++]='0';for(i=0;i<p;i++)o[n++]=dg[i];}
 return jsv(o,n);}
JNAT(jfn_toPrecision){double d=th.t==JT_N?th.u.d:jtonum(th);int p;if(argc<1||args[0].t==JT_U)return jstrv(jnum2s(d));p=(int)jtonum(args[0]);if(p<1||p>40)return jthrowerr(jErrR,"toPrecision() argument must be between 1 and 100");return jprec(d,p,0);}
JNAT(jfn_toExponential){double d=th.t==JT_N?th.u.d:jtonum(th);int p;if(argc<1||args[0].t==JT_U){char dg[32];int dp,np,i,n=0;char o[64];if(d==0)return jcv("0e+0");if(d!=d||jisinf(d))return jstrv(jnum2s(d));if(d<0){o[n++]='-';d=-d;}np=jdtoa(d,dg,&dp);o[n++]=dg[0];if(np>1){o[n++]='.';for(i=1;i<np;i++)o[n++]=dg[i];}o[n++]='e';o[n++]=dp-1<0?'-':'+';n+=jd2s((double)(dp-1<0?1-dp:dp-1),o+n);return jsv(o,n);}p=(int)jtonum(args[0]);if(p<0||p>39)return jthrowerr(jErrR,"toExponential() argument must be between 0 and 100");return jprec(d,p+1,1);}
JNAT(jfn_valueOf){JUNUSED;return th;}
JNAT(jfn_toLocaleString){
 double d=th.t==JT_N?th.u.d:0;j64 ip;char b[64];int n=0,k=0,i,neg=d<0;char t[40];
 JUNUSED;
 if(neg)d=-d;
 if(d!=d||jisinf(d)||d>9e15)return jstrv(jnum2s(th.u.d));
 d=jround(d*1000)/1000;ip=(j64)d;
 {double fr=d-(double)ip;char fb[8];int fn=0;
  if(!ip)t[k++]='0';
  while(ip){t[k++]=(char)('0'+ip%10);ip/=10;}
  if(neg)b[n++]='-';
  for(i=k-1;i>=0;i--){b[n++]=t[i];if(i%3==0&&i>0)b[n++]=',';}
  if(fr>0.0005){j64 f=(j64)jround(fr*1000);fb[0]=(char)('0'+f/100%10);fb[1]=(char)('0'+f/10%10);fb[2]=(char)('0'+f%10);fn=3;while(fn>0&&fb[fn-1]=='0')fn--;if(fn){b[n++]='.';for(i=0;i<fn;i++)b[n++]=fb[i];}}}
 return jsv(b,n);}
JNAT(jfb_toString){JUNUSED;return jcv(th.t==JT_B&&th.u.b?"true":"false");}
/* Math */
JNAT(jm_abs){JUNUSED;return jnum(jfabs(jtonum(JARG(0))));}
JNAT(jm_floor){JUNUSED;return jnum(jfloor(jtonum(JARG(0))));}
JNAT(jm_ceil){double d;JUNUSED;d=jtonum(JARG(0));return jnum(-jfloor(-d));}
JNAT(jm_round){JUNUSED;return jnum(jround(jtonum(JARG(0))));}
JNAT(jm_trunc){JUNUSED;return jnum(jtrunc(jtonum(JARG(0))));}
JNAT(jm_sign){double d;JUNUSED;d=jtonum(JARG(0));return jnum(d!=d?d:d>0?1:d<0?-1:0);}
JNAT(jm_sqrt){JUNUSED;return jnum(jsqrt(jtonum(JARG(0))));}
JNAT(jm_pow){JUNUSED;return jnum(jpow(jtonum(JARG(0)),jtonum(JARG(1))));}
JNAT(jm_random){JUNUSED;return jnum(jrand());}
JNAT(jm_sin){JUNUSED;return jnum(jsin(jtonum(JARG(0))));}
JNAT(jm_cos){JUNUSED;return jnum(jcos(jtonum(JARG(0))));}
JNAT(jm_tan){double d;JUNUSED;d=jtonum(JARG(0));return jnum(jsin(d)/jcos(d));}
JNAT(jm_atan){JUNUSED;return jnum(jatan(jtonum(JARG(0))));}
JNAT(jm_atan2){double y,x,r;JUNUSED;y=jtonum(JARG(0));x=jtonum(JARG(1));
 if(x>0)r=jatan(y/x);else if(x<0)r=y>=0?jatan(y/x)+JPI:jatan(y/x)-JPI;else r=y>0?JPI/2:y<0?-JPI/2:0;return jnum(r);}
JNAT(jm_exp){JUNUSED;return jnum(jexpd(jtonum(JARG(0))));}
JNAT(jm_log){JUNUSED;return jnum(jln(jtonum(JARG(0))));}
JNAT(jm_log2){JUNUSED;return jnum(jln(jtonum(JARG(0)))/0.6931471805599453);}
JNAT(jm_log10){JUNUSED;return jnum(jln(jtonum(JARG(0)))/2.302585092994046);}
JNAT(jm_hypot){double s=0;int i;JUNUSED;for(i=0;i<argc;i++){double d=jtonum(args[i]);s+=d*d;}return jnum(jsqrt(s));}
JNAT(jm_max){double r=-jINF();int i;JUNUSED;for(i=0;i<argc;i++){double d=jtonum(args[i]);if(d!=d)return jnum(d);if(d>r)r=d;}return jnum(r);}
JNAT(jm_min){double r=jINF();int i;JUNUSED;for(i=0;i<argc;i++){double d=jtonum(args[i]);if(d!=d)return jnum(d);if(d<r)r=d;}return jnum(r);}
/* JSON */
static void jjsonq(JSB*o,JStr*s){
 int i,st=0;
 jsb_c(o,"\"");
 for(i=0;i<s->len;i++){
  unsigned char c=(unsigned char)s->d[i];const char*e=0;char u[8];
  if(c=='"')e="\\\"";else if(c=='\\')e="\\\\";else if(c=='\n')e="\\n";else if(c=='\r')e="\\r";else if(c=='\t')e="\\t";
  else if(c<32){u[0]='\\';u[1]='u';u[2]='0';u[3]='0';u[4]="0123456789abcdef"[c>>4];u[5]="0123456789abcdef"[c&15];u[6]=0;e=u;}
  if(e){jsb_add(o,s->d+st,i-st);jsb_c(o,e);st=i+1;}}
 jsb_add(o,s->d+st,s->len-st);
 jsb_c(o,"\"");}
static int jstr_into(JV v,JStr*ind,JStr*cur,int depth,JSB*o){ /* returns 1 if the value is skipped (undefined/function) */
 if(depth>40){jthrowerr(jErrT,"Converting circular structure to JSON");return 0;}
 if(v.t==JT_O){JV tj=jget(v,jcstr("toJSON"));if(jiscallable(tj)){v=jcall(tj,v,0,0);if(jthrow)return 0;}}
 switch(v.t){
  case JT_U:return 1;
  case JT_NUL:jsb_c(o,"null");return 0;
  case JT_B:jsb_c(o,v.u.b?"true":"false");return 0;
  case JT_N:if(jisnan(v.u.d)||jisinf(v.u.d))jsb_c(o,"null");else jsb_str(o,jnum2s(v.u.d));return 0;
  case JT_S:jjsonq(o,v.u.s);return 0;}
 if(jiscallable(v))return 1;
 {JOb*ob=v.u.o;JStr*in2=jcat(cur,ind);int i,first=1;
  if(ob->k==OK_ARR){
   jsb_c(o,"[");
   for(i=0;i<ob->na;i++){
    JSB tmp={0,0,0};int sk;
    if(!first)jsb_c(o,",");
    first=0;
    if(ind->len){jsb_c(o,"\n");jsb_str(o,in2);}
    sk=jstr_into(ob->it[i],ind,in2,depth+1,o);
    if(jthrow)return 0;
    if(sk)jsb_c(o,"null");
    (void)tmp;}
   if(!first&&ind->len){jsb_c(o,"\n");jsb_str(o,cur);}
   jsb_c(o,"]");return 0;}
  if(ob->k==OK_DATE){JV f=jget(v,jcstr("toISOString"));JV r=jcall(f,v,0,0);if(jthrow)return 0;jjsonq(o,jtos(r));return 0;}
  jsb_c(o,"{");
  for(i=0;i<ob->np;i++){
   JPr*p=&ob->pr[i];JV pv;int mark;JSB sub;
   if(p->fl&1)continue;
   pv=p->v;if(p->fl&4)pv=jaccget(p,v);
   if(jthrow)return 0;
   /* value first into a scratch builder so that skipped members leave no key behind */
   sub.p=0;sub.n=sub.cap=0;
   mark=jstr_into(pv,ind,in2,depth+1,&sub);
   if(jthrow)return 0;
   if(mark)continue;
   if(!first)jsb_c(o,",");
   first=0;
   if(ind->len){jsb_c(o,"\n");jsb_str(o,in2);}
   jjsonq(o,p->k);jsb_c(o,ind->len?": ":":");
   jsb_add(o,sub.p,sub.n);}
  if(!first&&ind->len){jsb_c(o,"\n");jsb_str(o,cur);}
  jsb_c(o,"}");return 0;}}
static JStr*jstringify(JV v,JStr*ind,JStr*cur,int depth,int*skip){
 JSB o={0,0,0};
 *skip=jstr_into(v,ind,cur,depth,&o);
 if(*skip||jthrow)return&jemptys;
 return jsb_done(&o);}
JNAT(jj_stringify){
 JStr*ind=&jemptys,*r;int sk;JUNUSED;
 if(argc>2){if(args[2].t==JT_N){int n=(int)args[2].u.d,i;char b[12];if(n>10)n=10;for(i=0;i<n;i++)b[i]=' ';ind=jnewstr(b,n>0?n:0);}else if(args[2].t==JT_S)ind=args[2].u.s;}
 r=jstringify(JARG(0),ind,&jemptys,0,&sk);
 if(jthrow)return jundef();
 if(sk)return jundef();
 return jstrv(r);}
static const char*jjp;static int jjn,jji,jjerr;
static JV jjval(int depth);
static void jjws(void){while(jji<jjn&&jiswsc((unsigned char)jjp[jji]))jji++;}
static JV jjval(int depth){
 jjws();
 if(jji>=jjn||depth>60){jjerr=1;return jundef();}
 {char c=jjp[jji];
  if(c=='{'){
   JOb*o=jnewob(OK_PLAIN,jObjProto);jji++;jjws();
   if(jji<jjn&&jjp[jji]=='}'){jji++;return jobj(o);}
   for(;;){
    JV k,v;jjws();
    if(jji>=jjn||jjp[jji]!='"'){jjerr=1;return jundef();}
    k=jjval(depth+1);if(jjerr)return jundef();
    jjws();if(jji>=jjn||jjp[jji]!=':'){jjerr=1;return jundef();}jji++;
    v=jjval(depth+1);if(jjerr)return jundef();
    jsetown(o,k.u.s,v);
    jjws();if(jji<jjn&&jjp[jji]==','){jji++;continue;}
    if(jji<jjn&&jjp[jji]=='}'){jji++;break;}
    jjerr=1;return jundef();}
   return jobj(o);}
  if(c=='['){
   JOb*a=jnewarr(8);jji++;jjws();
   if(jji<jjn&&jjp[jji]==']'){jji++;return jobj(a);}
   for(;;){
    JV v=jjval(depth+1);if(jjerr)return jundef();
    jarrpush(a,v);
    jjws();if(jji<jjn&&jjp[jji]==','){jji++;continue;}
    if(jji<jjn&&jjp[jji]==']'){jji++;break;}
    jjerr=1;return jundef();}
   return jobj(a);}
  if(c=='"'){
   char*b;int n=0,cap=jjn-jji+8;JV r;
   b=jalloc(cap,BT_STR);if(!b){jjerr=1;return jundef();}
   jji++;
   while(jji<jjn&&jjp[jji]!='"'){
    if(jjp[jji]=='\\'){jji++;n+=jesc(jjp,&jji,jjn,b+n);}
    else b[n++]=jjp[jji++];}
   if(jji>=jjn){jjerr=1;jhdr(b)->ty=BT_FREE;return jundef();}
   jji++;r=jsv(b,n);jhdr(b)->ty=BT_FREE;return r;}
  if(jji+4<=jjn&&jjp[jji]=='t'&&jjp[jji+1]=='r'&&jjp[jji+2]=='u'&&jjp[jji+3]=='e'){jji+=4;return jbool(1);}
  if(jji+5<=jjn&&jjp[jji]=='f'&&jjp[jji+1]=='a'&&jjp[jji+2]=='l'&&jjp[jji+3]=='s'&&jjp[jji+4]=='e'){jji+=5;return jbool(0);}
  if(jji+4<=jjn&&jjp[jji]=='n'&&jjp[jji+1]=='u'&&jjp[jji+2]=='l'&&jjp[jji+3]=='l'){jji+=4;return jnull();}
  if(c=='-'||isdg(c)){
   int st=jji,ok;double d;
   jji++;while(jji<jjn&&(isdg(jjp[jji])||jjp[jji]=='.'||jjp[jji]=='e'||jjp[jji]=='E'||jjp[jji]=='+'||jjp[jji]=='-'))jji++;
   d=jstod(jjp+st,jji-st,&ok);if(!ok){jjerr=1;return jundef();}return jnum(d);}
  jjerr=1;return jundef();}}
JNAT(jj_parse){
 JStr*s;JV r;JUNUSED;
 s=jtos(JARG(0));jjp=s->d;jjn=s->len;jji=0;jjerr=0;
 r=jjval(0);
 jjws();
 if(jjerr||jji<jjn)return jthrowerr(jErrS,"Unexpected token in JSON");
 return r;}
/* Date (UTC) */
static void jcivil(j64 z,int*y,int*m,int*d){
 j64 era,doe,yoe,doy,mp;
 z+=719468;era=(z>=0?z:z-146096)/146097;doe=z-era*146097;
 yoe=(doe-doe/1460+doe/36524-doe/146096)/365;
 *y=(int)(yoe+era*400);doy=doe-(365*yoe+yoe/4-yoe/100);mp=(5*doy+2)/153;
 *d=(int)(doy-(153*mp+2)/5+1);*m=(int)(mp<10?mp+3:mp-9);if(*m<=2)(*y)++;}
static j64 jdays(int y,int m,int d){
 j64 era,yoe,doy,doe;
 if(m<=2)y--;
 era=(y>=0?y:y-399)/400;yoe=y-era*400;
 doy=(153*(m>2?m-3:m+9)+2)/5+d-1;doe=yoe*365+yoe/4-yoe/100+doy;
 return era*146097+doe-719468;}
static JV jmkdate(double t){JOb*o=jnewob(OK_DATE,jDateProto);o->ex=jnum(t);return jobj(o);}
JNAT(jf_Date){
 double t;
 if(argc==0)t=(double)jnowunix;
 else if(argc==1){
  if(args[0].t==JT_S){
   int y=0,mo=1,d=1,h=0,mi=0,s2=0,i=0;JStr*s=args[0].u.s;
   #define JRD(v,n) {int k=0;while(k<n&&i<s->len&&isdg(s->d[i])){v=v*10+s->d[i]-'0';i++;k++;}}
   y=0;JRD(y,4)
   if(i<s->len&&s->d[i]=='-'){i++;mo=0;JRD(mo,2)}
   if(i<s->len&&s->d[i]=='-'){i++;d=0;JRD(d,2)}
   if(i<s->len&&(s->d[i]=='T'||s->d[i]==' ')){i++;h=0;JRD(h,2)if(i<s->len&&s->d[i]==':'){i++;JRD(mi,2)}if(i<s->len&&s->d[i]==':'){i++;JRD(s2,2)}}
   #undef JRD
   t=(double)(jdays(y,mo,d)*86400000LL+h*3600000LL+mi*60000LL+s2*1000LL);}
  else if(args[0].t==JT_O&&args[0].u.o->k==OK_DATE)t=args[0].u.o->ex.u.d;
  else t=jtonum(args[0]);}
 else{
  double v[7]={0,0,1,0,0,0,0};int i;
  for(i=0;i<argc&&i<7;i++)v[i]=jtonum(args[i]);
  {int y=(int)v[0],m=(int)v[1];y+=m>=0?m/12:(m-11)/12;m=((m%12)+12)%12;
   t=(double)jdays(y,m+1,1)*86400000.0+(v[2]-1)*86400000.0+v[3]*3600000.0+v[4]*60000.0+v[5]*1000.0+v[6];}}
 if(jnewcall&&th.t==JT_O){th.u.o->k=OK_DATE;th.u.o->ex=jnum(t);th.u.o->proto=jDateProto;return th;}
 return jmkdate(t);}
JNAT(jf_dateNow){JUNUSED;return jnum((double)jnowunix);}
JNAT(jf_perfNow){JUNUSED;return jnum((double)jnowmono);}
#define JTHISD double t;if(th.t!=JT_O||th.u.o->k!=OK_DATE)return jthrowerr(jErrT,"this is not a Date object.");t=th.u.o->ex.u.d;if(t!=t)return jnum(jNAN());
static void jdparts(double t,int*y,int*m,int*d,int*h,int*mi,int*s,int*ms,int*wd){
 j64 T=(j64)t,days=T/86400000LL,rem=T%86400000LL;
 if(rem<0){rem+=86400000LL;days--;}
 jcivil(days,y,m,d);*h=(int)(rem/3600000);*mi=(int)(rem/60000%60);*s=(int)(rem/1000%60);*ms=(int)(rem%1000);
 *wd=(int)(((days%7)+11)%7);}
#define JDP int y,m,d,h,mi,s,ms,wd;JTHISD jdparts(t,&y,&m,&d,&h,&mi,&s,&ms,&wd);(void)args;(void)argc;
JNAT(jfd_getTime){JTHISD(void)args;(void)argc;return jnum(t);}
JNAT(jfd_getFullYear){JDP return jnum(y);}
JNAT(jfd_getMonth){JDP return jnum(m-1);}
JNAT(jfd_getDate){JDP return jnum(d);}
JNAT(jfd_getDay){JDP return jnum(wd);}
JNAT(jfd_getHours){JDP return jnum(h);}
JNAT(jfd_getMinutes){JDP return jnum(mi);}
JNAT(jfd_getSeconds){JDP return jnum(s);}
JNAT(jfd_getMilliseconds){JDP return jnum(ms);}
JNAT(jfd_toISOString){
 char b[40];JDP
 b[0]=(char)('0'+y/1000%10);b[1]=(char)('0'+y/100%10);b[2]=(char)('0'+y/10%10);b[3]=(char)('0'+y%10);b[4]='-';
 b[5]=(char)('0'+m/10);b[6]=(char)('0'+m%10);b[7]='-';b[8]=(char)('0'+d/10);b[9]=(char)('0'+d%10);b[10]='T';
 b[11]=(char)('0'+h/10);b[12]=(char)('0'+h%10);b[13]=':';b[14]=(char)('0'+mi/10);b[15]=(char)('0'+mi%10);b[16]=':';
 b[17]=(char)('0'+s/10);b[18]=(char)('0'+s%10);b[19]='.';b[20]=(char)('0'+ms/100);b[21]=(char)('0'+ms/10%10);b[22]=(char)('0'+ms%10);b[23]='Z';
 return jsv(b,24);}
JNAT(jfd_toString){return jfd_toISOString(th,args,argc);}
/* console / misc */
#define JLOGSZ 32768
static char jlog[JLOGSZ];static int jlogn;
static void jlogs(const char*s,int n){int i;for(i=0;i<n;i++){if(jlogn>=JLOGSZ-1){int k;for(k=0;k<JLOGSZ/2;k++)jlog[k]=jlog[k+JLOGSZ/2-1];jlogn=JLOGSZ/2-1;}jlog[jlogn++]=s[i];}jlog[jlogn]=0;}
static JStr*jinspect(JV v,int depth){
 if(v.t==JT_S&&depth>0){return jcat(jcat(jcstr("'"),v.u.s),jcstr("'"));}
 if(v.t!=JT_O)return jtos(v);
 if(jiscallable(v))return jcstr("[Function]");
 if(v.u.o->k==OK_ERR||(v.u.o->fl&16))return jtos(v);
 if(v.u.o->k==OK_NODE)return jcstr("[object HTMLElement]");
 if(v.u.o->k==OK_DATE){JV f=jget(v,jcstr("toISOString"));return jtos(jcall(f,v,0,0));}
 if(depth>3)return jcstr(v.u.o->k==OK_ARR?"[Array]":"[Object]");
 {JOb*o=v.u.o;JStr*acc;int i,first=1;
  if(o->k==OK_ARR){
   acc=jcstr("[");
   for(i=0;i<o->na&&i<100;i++){if(!first)acc=jcat(acc,jcstr(", "));first=0;acc=jcat(acc,jinspect(o->it[i],depth+1));}
   return jcat(acc,jcstr("]"));}
  acc=jcstr("{");
  for(i=0;i<o->np;i++){
   if(o->pr[i].fl&1)continue;
   acc=jcat(acc,jcstr(first?" ":", "));first=0;
   acc=jcat(jcat(acc,o->pr[i].k),jcstr(": "));acc=jcat(acc,jinspect(o->pr[i].v,depth+1));}
  return jcat(acc,jcstr(first?"}":" }"));}}
static JV jlogwith(const char*prefix,JV*args,int argc){
 int i;
 jlogs(prefix,0);
 for(i=0;i<argc;i++){JStr*s=jinspect(args[i],0);if(i)jlogs(" ",1);jlogs(s->d,s->len);}
 jlogs("\n",1);return jundef();}
JNAT(jc_log){JUNUSED;return jlogwith("",args,argc);}
JNAT(jc_warn){JUNUSED;jlogs("warn: ",6);return jlogwith("",args,argc);}
JNAT(jc_error){JUNUSED;jlogs("error: ",7);return jlogwith("",args,argc);}
static char jalertq[2048];static int jalertn;
JNAT(jw_alert){JStr*s;JUNUSED;s=jtos(JARG(0));{int i;for(i=0;i<s->len&&jalertn<(int)sizeof jalertq-2;i++)jalertq[jalertn++]=s->d[i];jalertq[jalertn++]=0;}return jundef();}
JNAT(jw_confirm){JUNUSED;return jbool(0);}
JNAT(jw_prompt){JUNUSED;return jnull();}
JNAT(jw_noop){JUNUSED;return jundef();}

/* ===================== regular expressions (backtracking VM) ===================== */
/* Supported: literals, . [classes] \d\w\s\D\W\S \b\B ^ $, groups (capturing, (?:), named (?<n>)), alternation,
 * * + ? {n,m} and lazy forms, backreferences \1-\9, lookahead (?= (?!, flags g i m s y (u accepted).
 * Not supported: lookbehind, unicode property escapes, named backreferences. Case folding is ASCII only. */
enum{RN_CHAR=1,RN_ANY,RN_CLASS,RN_SEQ,RN_ALT,RN_GROUP,RN_REP,RN_BOL,RN_EOL,RN_WB,RN_NWB,RN_BREF,RN_LOOK,RN_EMPTY};
enum{RO_CHAR=1,RO_ANY,RO_CLASS,RO_SPLIT,RO_JMP,RO_SAVE,RO_BOL,RO_EOL,RO_WB,RO_NWB,RO_BREF,RO_LOOK,RO_MATCH,RO_ENDLOOK};
#define RMAXN 700
#define RMAXP 12000
#define RMAXC 3000
#define RMAXCAP 40
#define RF_G 1
#define RF_I 2
#define RF_M 4
#define RF_S 8
#define RF_Y 16
typedef struct{int t,a,b,c,d,nx;}RNode;
static RNode rn[RMAXN];static int rnn;
static int rp[RMAXP];static int rpn;
static int rcl[RMAXC];static int rcn;
static const char*rsrc;static int rslen,rpos,rerr,rncap,rflags;
static int rnew(int t){
 if(rnn>=RMAXN){rerr=1;return 0;}
 rn[rnn].t=t;rn[rnn].a=rn[rnn].b=rn[rnn].c=rn[rnn].d=rn[rnn].nx=0;return rnn++;}
static int rparse_alt(void);
static void rrange(int lo,int hi){if(rcn+2>=RMAXC){rerr=1;return;}rcl[rcn++]=lo;rcl[rcn++]=hi;}
static void rshort(int c){
 switch(c){
  case 'd':rrange('0','9');break;
  case 'D':rrange(0,'0'-1);rrange('9'+1,0x10FFFF);break;
  case 'w':rrange('0','9');rrange('A','Z');rrange('_','_');rrange('a','z');break;
  case 'W':rrange(0,'0'-1);rrange('9'+1,'A'-1);rrange('Z'+1,'_'-1);rrange('`','`');rrange('z'+1,0x10FFFF);break;
  case 's':rrange(9,13);rrange(' ',' ');rrange(0xA0,0xA0);rrange(0x2028,0x2029);break;
  case 'S':rrange(0,8);rrange(14,31);rrange(33,0x9F);rrange(0xA1,0x2027);rrange(0x202A,0x10FFFF);break;}}
static int risshort(int c){return c=='d'||c=='D'||c=='w'||c=='W'||c=='s'||c=='S';}
static int resc(int c){switch(c){case 'n':return'\n';case 't':return'\t';case 'r':return'\r';case 'f':return'\f';case 'v':return'\v';case '0':return 0;}return c;}
static int rhex(int n){int v=0,k;for(k=0;k<n&&rpos<rslen&&hx(rsrc[rpos])>=0;k++)v=v*16+hx(rsrc[rpos++]);return v;}
static int rgetc(void){int l,c=jdec8(rsrc+rpos,rslen-rpos,&l);rpos+=l;return c;}
static int rclassitem(void){ /* one class atom at rpos; returns code point, or -1 if it was a shorthand set */
 if(rsrc[rpos]=='\\'&&rpos+1<rslen){
  int e=(unsigned char)rsrc[rpos+1];rpos+=2;
  if(risshort(e)){rshort(e);return-1;}
  if(e=='x')return rhex(2);
  if(e=='u')return rhex(4);
  if(e=='b')return 8;
  return resc(e);}
 return rgetc();}
static int rparse_class(void){
 int n=rnew(RN_CLASS),start;
 start=rcn;
 if(rcn+2>=RMAXC){rerr=1;return n;}
 rcl[rcn++]=0;rcl[rcn++]=0;
 if(rpos<rslen&&rsrc[rpos]=='^'){rcl[start]=1;rpos++;}
 rn[n].a=start;
 while(rpos<rslen&&rsrc[rpos]!=']'&&!rerr){
  int lo=rclassitem(),hi;
  if(lo<0)continue;
  if(rpos+1<rslen&&rsrc[rpos]=='-'&&rsrc[rpos+1]!=']'){
   rpos++;hi=rclassitem();
   if(hi<0){rrange(lo,lo);rrange('-','-');continue;}
   rrange(lo,hi);}
  else rrange(lo,lo);}
 if(rpos>=rslen)rerr=1;else rpos++;
 rcl[start+1]=(rcn-start-2)/2;
 return n;}
static int rparse_atom(void){
 int c,n;
 if(rpos>=rslen)return rnew(RN_EMPTY);
 c=(unsigned char)rsrc[rpos];
 if(c=='('){
  int cap=-1,look=0,neg=0;
  rpos++;
  if(rpos+1<rslen&&rsrc[rpos]=='?'){
   if(rsrc[rpos+1]==':')rpos+=2;
   else if(rsrc[rpos+1]=='='){rpos+=2;look=1;}
   else if(rsrc[rpos+1]=='!'){rpos+=2;look=1;neg=1;}
   else if(rsrc[rpos+1]=='<'&&rpos+2<rslen&&rsrc[rpos+2]!='='&&rsrc[rpos+2]!='!'){
    rpos+=2;while(rpos<rslen&&rsrc[rpos]!='>')rpos++;rpos++;cap=++rncap;}
   else{rerr=1;return 0;}}
  else cap=++rncap;
  if(rncap>=RMAXCAP){rerr=1;return 0;}
  n=rparse_alt();
  if(rpos>=rslen||rsrc[rpos]!=')'){rerr=1;return 0;}
  rpos++;
  if(look){int l=rnew(RN_LOOK);rn[l].a=n;rn[l].b=neg;return l;}
  if(cap>=0){int g=rnew(RN_GROUP);rn[g].a=n;rn[g].b=cap;return g;}
  return n;}
 if(c=='['){rpos++;return rparse_class();}
 if(c=='.'){rpos++;return rnew(RN_ANY);}
 if(c=='^'){rpos++;return rnew(RN_BOL);}
 if(c=='$'){rpos++;return rnew(RN_EOL);}
 if(c=='\\'&&rpos+1<rslen){
  int e=(unsigned char)rsrc[rpos+1];rpos+=2;
  if(risshort(e)){
   n=rnew(RN_CLASS);
   if(rcn+2>=RMAXC){rerr=1;return n;}
   rn[n].a=rcn;rcl[rcn++]=0;rcl[rcn++]=0;
   {int st=rcn;rshort(e);rcl[rn[n].a+1]=(rcn-st)/2;}
   return n;}
  if(e=='b')return rnew(RN_WB);
  if(e=='B')return rnew(RN_NWB);
  if(e>='1'&&e<='9'){n=rnew(RN_BREF);rn[n].a=e-'0';return n;}
  n=rnew(RN_CHAR);
  rn[n].a=e=='x'?rhex(2):e=='u'?rhex(4):resc(e);
  return n;}
 n=rnew(RN_CHAR);rn[n].a=rgetc();return n;}
static int rparse_rep(void){
 int a=rparse_atom();
 while(rpos<rslen&&!rerr){
  char c=rsrc[rpos];int mn,mx,lazy=0,r;
  if(c=='*'){mn=0;mx=-1;rpos++;}
  else if(c=='+'){mn=1;mx=-1;rpos++;}
  else if(c=='?'){mn=0;mx=1;rpos++;}
  else if(c=='{'){
   int q=rpos+1,v1=0,v2,has=0;
   while(q<rslen&&isdg(rsrc[q])&&v1<2000){v1=v1*10+rsrc[q]-'0';q++;has=1;}
   if(!has)break;
   if(q<rslen&&rsrc[q]==','){
    q++;v2=-1;
    if(q<rslen&&isdg(rsrc[q])){v2=0;while(q<rslen&&isdg(rsrc[q])&&v2<2000){v2=v2*10+rsrc[q]-'0';q++;}}}
   else v2=v1;
   if(q>=rslen||rsrc[q]!='}')break;
   mn=v1;mx=v2;rpos=q+1;}
  else break;
  if(rpos<rslen&&rsrc[rpos]=='?'){lazy=1;rpos++;}
  r=rnew(RN_REP);rn[r].a=a;rn[r].b=mn;rn[r].c=mx;rn[r].d=lazy;a=r;}
 return a;}
static int rparse_seq(void){
 int s=rnew(RN_SEQ),tail=0;
 while(rpos<rslen&&rsrc[rpos]!='|'&&rsrc[rpos]!=')'&&!rerr){
  int a=rparse_rep();
  if(tail)rn[tail].nx=a;else rn[s].a=a;
  tail=a;}
 return s;}
static int rparse_alt(void){
 int first=rparse_seq(),alt,tail;
 if(rpos>=rslen||rsrc[rpos]!='|')return first;
 alt=rnew(RN_ALT);rn[alt].a=first;tail=first;
 while(rpos<rslen&&rsrc[rpos]=='|'&&!rerr){
  int s;rpos++;s=rparse_seq();rn[tail].nx=s;tail=s;}
 return alt;}
static void remit(int v){if(rpn>=RMAXP){rerr=1;return;}rp[rpn++]=v;}
static void rgen(int n){
 int c;
 if(rerr||!n)return;
 switch(rn[n].t){
  case RN_CHAR:remit(RO_CHAR);remit(rn[n].a);break;
  case RN_ANY:remit(RO_ANY);break;
  case RN_CLASS:remit(RO_CLASS);remit(rn[n].a);break;
  case RN_BOL:remit(RO_BOL);break;
  case RN_EOL:remit(RO_EOL);break;
  case RN_WB:remit(RO_WB);break;
  case RN_NWB:remit(RO_NWB);break;
  case RN_BREF:remit(RO_BREF);remit(rn[n].a);break;
  case RN_SEQ:for(c=rn[n].a;c;c=rn[c].nx)rgen(c);break;
  case RN_GROUP:remit(RO_SAVE);remit(2*rn[n].b);rgen(rn[n].a);remit(RO_SAVE);remit(2*rn[n].b+1);break;
  case RN_ALT:{
   int jmps[64],nj=0;
   for(c=rn[n].a;c;c=rn[c].nx){
    if(rn[c].nx){
     int sp;
     remit(RO_SPLIT);remit(rpn+2);sp=rpn;remit(0);
     rgen(c);
     remit(RO_JMP);if(nj<64)jmps[nj++]=rpn;remit(0);
     if(!rerr)rp[sp]=rpn;}
    else rgen(c);}
   while(nj>0&&!rerr)rp[jmps[--nj]]=rpn;
   break;}
  case RN_LOOK:{
   int ep;
   remit(RO_LOOK);remit(rn[n].b);ep=rpn;remit(0);
   rgen(rn[n].a);remit(RO_ENDLOOK);
   if(!rerr)rp[ep]=rpn;
   break;}
  case RN_REP:{
   int i,mn=rn[n].b,mx=rn[n].c,lazy=rn[n].d,patch[600],np=0;
   for(i=0;i<mn&&!rerr;i++)rgen(rn[n].a);
   if(mx<0){
    int l=rpn,sp;
    remit(RO_SPLIT);
    if(lazy){remit(0);sp=rpn;remit(l+3);} /* try exit first */
    else{remit(l+3);sp=rpn;remit(0);}
    rgen(rn[n].a);remit(RO_JMP);remit(l);
    if(!rerr){if(lazy)rp[l+1]=rpn;else rp[sp]=rpn;}}
   else{
    for(i=mn;i<mx&&!rerr&&np<600;i++){
     int l=rpn;
     remit(RO_SPLIT);
     if(lazy){remit(0);patch[np++]=-(rpn)-1;remit(l+3);}
     else{remit(l+3);patch[np++]=rpn;remit(0);}
     rgen(rn[n].a);}
    for(i=0;i<np&&!rerr;i++){if(patch[i]<0)rp[-patch[i]-1]=rpn;else rp[patch[i]]=rpn;}}
   break;}
  default:break;}}
/* ---- matcher ---- */
typedef struct{int pc,sp,idx,old;}RFrm;
#define RSTK 20000
static RFrm rstk[RSTK];static int rsp_;
static int rcaps[2*(RMAXCAP+1)];
static long rsteps;static int rabort;
static const char*rstr;static int rlen;
static int rfold(int c){return(c>='A'&&c<='Z')?c+32:c;}
static int rword(int sp){int c;if(sp<0||sp>=rlen)return 0;c=(unsigned char)rstr[sp];return(c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_';}
static int rclasshit(int cls,int c){
 int neg=rcl[cls],n=rcl[cls+1],i,hit=0;
 for(i=0;i<n;i++)if(c>=rcl[cls+2+2*i]&&c<=rcl[cls+3+2*i]){hit=1;break;}
 if(!hit&&(rflags&RF_I)){
  int f=rfold(c),u=(c>='a'&&c<='z')?c-32:c;
  for(i=0;i<n;i++)if((f>=rcl[cls+2+2*i]&&f<=rcl[cls+3+2*i])||(u>=rcl[cls+2+2*i]&&u<=rcl[cls+3+2*i])){hit=1;break;}}
 return hit!=neg;}
static int rvm(int pc,int sp){
 int base=rsp_;
 for(;;){
  int c,l;
  if(++rsteps>4000000){rabort=1;goto fail;}
  switch(rp[pc]){
   case RO_CHAR:
    if(sp>=rlen)goto fail;
    c=jdec8(rstr+sp,rlen-sp,&l);
    if(!(c==rp[pc+1]||((rflags&RF_I)&&rfold(c)==rfold(rp[pc+1]))))goto fail;
    sp+=l;pc+=2;continue;
   case RO_ANY:
    if(sp>=rlen)goto fail;
    c=jdec8(rstr+sp,rlen-sp,&l);
    if(!(rflags&RF_S)&&(c=='\n'||c=='\r'||c==0x2028||c==0x2029))goto fail;
    sp+=l;pc++;continue;
   case RO_CLASS:
    if(sp>=rlen)goto fail;
    c=jdec8(rstr+sp,rlen-sp,&l);
    if(!rclasshit(rp[pc+1],c))goto fail;
    sp+=l;pc+=2;continue;
   case RO_SPLIT:
    if(rsp_>=RSTK){rabort=1;goto fail;}
    rstk[rsp_].pc=rp[pc+2];rstk[rsp_].sp=sp;rstk[rsp_].idx=-1;rsp_++;
    pc=rp[pc+1];continue;
   case RO_JMP:pc=rp[pc+1];continue;
   case RO_SAVE:
    if(rsp_>=RSTK){rabort=1;goto fail;}
    rstk[rsp_].idx=rp[pc+1];rstk[rsp_].old=rcaps[rp[pc+1]];rsp_++;
    rcaps[rp[pc+1]]=sp;pc+=2;continue;
   case RO_BOL:
    if(sp==0||((rflags&RF_M)&&(rstr[sp-1]=='\n'||rstr[sp-1]=='\r'))){pc++;continue;}
    goto fail;
   case RO_EOL:
    if(sp==rlen||((rflags&RF_M)&&(rstr[sp]=='\n'||rstr[sp]=='\r'))){pc++;continue;}
    goto fail;
   case RO_WB:if(rword(sp-1)!=rword(sp)){pc++;continue;}goto fail;
   case RO_NWB:if(rword(sp-1)==rword(sp)){pc++;continue;}goto fail;
   case RO_BREF:{
    int g=rp[pc+1],a=rcaps[2*g],b=rcaps[2*g+1],k;
    if(a>=0&&b>=a){
     if(sp+(b-a)>rlen)goto fail;
     for(k=0;k<b-a;k++){int x=(unsigned char)rstr[a+k],y=(unsigned char)rstr[sp+k];if(x!=y&&!((rflags&RF_I)&&rfold(x)==rfold(y)))goto fail;}
     sp+=b-a;}
    pc+=2;continue;}
   case RO_LOOK:{
    int neg=rp[pc+1],endpc=rp[pc+2],b2=rsp_,r;
    r=rvm(pc+3,sp);
    if(rabort)goto fail;
    if(r>=0){
     if(neg){while(rsp_>b2){rsp_--;if(rstk[rsp_].idx>=0)rcaps[rstk[rsp_].idx]=rstk[rsp_].old;}goto fail;}
     rsp_=b2>rsp_?rsp_:rsp_; /* keep captures; drop alternatives of the sub-match below */
     {int w=b2;int q;for(q=b2;q<rsp_;q++)if(rstk[q].idx>=0)rstk[w++]=rstk[q];rsp_=w;}
     pc=endpc;continue;}
    if(neg){pc=endpc;continue;}
    goto fail;}
   case RO_ENDLOOK:case RO_MATCH:return sp;
   default:goto fail;}
  fail:
  for(;;){
   if(rsp_<=base)return-1;
   rsp_--;
   if(rstk[rsp_].idx>=0){rcaps[rstk[rsp_].idx]=rstk[rsp_].old;continue;}
   pc=rstk[rsp_].pc;sp=rstk[rsp_].sp;break;}}}
static char rlastp[600];static int rlastl=-1,rlastf=-1,rlastok,rlastn;
static int rx_compile(JStr*pat,int flags){
 int i,r;
 if(pat->len<(int)sizeof rlastp&&rlastl==pat->len&&rlastf==(flags&(RF_I|RF_M|RF_S))){
  for(i=0;i<pat->len&&rlastp[i]==pat->d[i];i++);
  if(i==pat->len){rflags=flags;rncap=rlastn;return rlastok;}}
 rnn=1;rpn=0;rcn=0;rerr=0;rncap=0;rsrc=pat->d;rslen=pat->len;rpos=0;rflags=flags;
 r=rparse_alt();
 if(!rerr&&rpos<rslen)rerr=1;
 remit(RO_SAVE);remit(0);
 rgen(r);
 remit(RO_SAVE);remit(1);remit(RO_MATCH);
 /* the SAVE 0/1 wrapper is emitted before/after: rebuild with proper order */
 rlastok=!rerr;rlastn=rncap;
 if(pat->len<(int)sizeof rlastp){for(i=0;i<pat->len;i++)rlastp[i]=pat->d[i];rlastl=pat->len;rlastf=flags&(RF_I|RF_M|RF_S);}
 else rlastl=-1;
 return rlastok;}
/* ---- RegExp objects and String methods ---- */
static JOb*jRxProto;
static JStr*K_source,*K_lastIndex;
static int jrx_isre(JV v){return v.t==JT_O&&(v.u.o->fl&16);}
static int jrx_flagbits(JStr*f){
 int i,b=0;
 for(i=0;i<f->len;i++){
  int bit;
  switch(f->d[i]){case 'g':bit=RF_G;break;case 'i':bit=RF_I;break;case 'm':bit=RF_M;break;case 's':bit=RF_S;break;case 'y':bit=RF_Y;break;case 'u':case 'd':bit=0;break;default:return-1;}
  b|=bit;}
 return b;}
static JV jmkregex(JStr*pat,JStr*flags){
 int fb=jrx_flagbits(flags);JOb*o;
 if(fb<0)return jthrowerr(jErrS,"Invalid regular expression flags");
 if(!rx_compile(pat,fb))return jthrowerr(jErrS,"Invalid regular expression");
 o=jnewob(OK_PLAIN,jRxProto);o->fl|=16;o->ex=jnum(fb);
 jaddpr(o,K_source,jstrv(pat),1);
 jaddpr(o,jcstr("flags"),jstrv(flags),1);
 jaddpr(o,jcstr("global"),jbool(fb&RF_G),1);
 jaddpr(o,jcstr("ignoreCase"),jbool(fb&RF_I),1);
 jaddpr(o,jcstr("multiline"),jbool(fb&RF_M),1);
 jaddpr(o,K_lastIndex,jnum(0),1);
 return jobj(o);}
static JStr*jrx_pat(JOb*re){JPr*p=jfind(re,K_source);return p&&p->v.t==JT_S?p->v.u.s:&jemptys;}
static int jrx_search(JOb*re,JStr*s,int from,int*caps,int*nc){
 int fb=(int)re->ex.u.d,st,i,l;
 if(!rx_compile(jrx_pat(re),fb))return 0;
 rstr=s->d;rlen=s->len;rsteps=0;rabort=0;
 for(st=from;st<=s->len;){
  for(i=0;i<2*(rncap+1);i++)rcaps[i]=-1;
  rsp_=0;
  if(rvm(0,st)>=0){for(i=0;i<2*(rncap+1);i++)caps[i]=rcaps[i];*nc=rncap;return 1;}
  if(rabort||(fb&RF_Y))return 0;
  if(st>=s->len)break;
  jdec8(s->d+st,s->len-st,&l);st+=l;}
 return 0;}
static JStr*jrx_cap(JStr*s,int*caps,int g){return caps[2*g]<0?0:jnewstr(s->d+caps[2*g],caps[2*g+1]-caps[2*g]);}
static JV jrx_mkres(JStr*s,int*caps,int nc){
 JOb*a=jnewarr(nc+1);int g;
 for(g=0;g<=nc;g++){JStr*c=jrx_cap(s,caps,g);jarrpush(a,c?jstrv(c):jundef());}
 jsetc(a,"index",jnum(jb2cp(s,caps[0])));jsetc(a,"input",jstrv(s));jsetc(a,"groups",jundef());
 {int i;for(i=0;i<a->np;i++)a->pr[i].fl|=1;}
 return jobj(a);}
static JV jrx_exec1(JOb*re,JStr*s,int*matched){
 int fb=(int)re->ex.u.d,caps[2*(RMAXCAP+1)],nc,from=0;JPr*li=jfind(re,K_lastIndex);
 *matched=0;
 if(fb&(RF_G|RF_Y)){double d=li?jtonum(li->v):0;if(d<0||d!=d)d=0;if(d>jcplen(s)){if(li)li->v=jnum(0);return jnull();}from=jcpoff(s,(int)d);}
 if(!jrx_search(re,s,from,caps,&nc)){if((fb&(RF_G|RF_Y))&&li)li->v=jnum(0);return jnull();}
 if((fb&(RF_G|RF_Y))&&li)li->v=jnum(jb2cp(s,caps[1]));
 *matched=1;
 return jrx_mkres(s,caps,nc);}
#define JTHISRX JOb*re;if(!jrx_isre(th))return jthrowerr(jErrT,"Receiver is not a RegExp");re=th.u.o;
JNAT(jf_RegExp){
 JStr*pat,*fl;JV a0=JARG(0);
 if(jrx_isre(a0)){pat=jrx_pat(a0.u.o);{JPr*p=jfind(a0.u.o,jcstr("flags"));fl=p?p->v.u.s:&jemptys;}}
 else{pat=a0.t==JT_U?jcstr("(?:)"):jtos(a0);if(jthrow)return jundef();fl=&jemptys;}
 if(argc>1&&args[1].t!=JT_U){fl=jtos(args[1]);if(jthrow)return jundef();}
 return jmkregex(pat,fl);}
JNAT(jrx_test){JTHISRX{int m;JStr*s=jtos(JARG(0));if(jthrow)return jundef();jrx_exec1(re,s,&m);return jbool(m);}}
JNAT(jrx_exec){JTHISRX{int m;JStr*s=jtos(JARG(0));if(jthrow)return jundef();return jrx_exec1(re,s,&m);}}
JNAT(jrx_toString){JTHISRX{JPr*p=jfind(re,jcstr("flags"));return jstrv(jcat(jcat(jcat(jcstr("/"),jrx_pat(re)),jcstr("/")),p?p->v.u.s:&jemptys));}}
JNAT(jfs_match){
 JTHISS{
  JV a0=JARG(0);JOb*re;
  if(jrx_isre(a0))re=a0.u.o;
  else{JV r=jmkregex(a0.t==JT_U?jcstr("(?:)"):jtos(a0),&jemptys);if(jthrow)return jundef();re=r.u.o;}
  if(!((int)re->ex.u.d&RF_G)){int m;return jrx_exec1(re,s,&m);}
  {JOb*r=jnewarr(8);int from=0,caps[2*(RMAXCAP+1)],nc,l;
   for(;from<=s->len;){
    if(!jrx_search(re,s,from,caps,&nc))break;
    jarrpush(r,jstrv(jrx_cap(s,caps,0)));
    if(caps[1]==caps[0]){if(caps[1]>=s->len)break;jdec8(s->d+caps[1],s->len-caps[1],&l);from=caps[1]+l;}else from=caps[1];}
   {JPr*li=jfind(re,K_lastIndex);if(li)li->v=jnum(0);}
   return r->na?jobj(r):jnull();}}}
JNAT(jfs_matchAll){
 JTHISS{
  JV a0=JARG(0);JOb*re,*r=jnewarr(8);int from=0,caps[2*(RMAXCAP+1)],nc,l;
  if(jrx_isre(a0)){re=a0.u.o;if(!((int)re->ex.u.d&RF_G))return jthrowerr(jErrT,"String.prototype.matchAll called with a non-global RegExp argument");}
  else{JV q=jmkregex(jtos(a0),jcstr("g"));if(jthrow)return jundef();re=q.u.o;}
  for(;from<=s->len;){
   if(!jrx_search(re,s,from,caps,&nc))break;
   jarrpush(r,jrx_mkres(s,caps,nc));
   if(caps[1]==caps[0]){if(caps[1]>=s->len)break;jdec8(s->d+caps[1],s->len-caps[1],&l);from=caps[1]+l;}else from=caps[1];}
  return jobj(r);}}
JNAT(jfs_search){
 JTHISS{
  JV a0=JARG(0);JOb*re;int caps[2*(RMAXCAP+1)],nc;
  if(jrx_isre(a0))re=a0.u.o;else{JV r=jmkregex(a0.t==JT_U?jcstr("(?:)"):jtos(a0),&jemptys);if(jthrow)return jundef();re=r.u.o;}
  if(!jrx_search(re,s,0,caps,&nc))return jnum(-1);
  return jnum(jb2cp(s,caps[0]));}}
static JStr*jrx_expand(JStr*rep,JStr*s,int*caps,int nc){
 JStr*acc=&jemptys;int i=0,st=0;
 while(i<rep->len){
  if(rep->d[i]=='$'&&i+1<rep->len){
   char c=rep->d[i+1];JStr*add=0;int adv=2;
   if(c=='$')add=jcstr("$");
   else if(c=='&')add=jnewstr(s->d+caps[0],caps[1]-caps[0]);
   else if(c=='`')add=jnewstr(s->d,caps[0]);
   else if(c=='\'')add=jnewstr(s->d+caps[1],s->len-caps[1]);
   else if(isdg(c)){
    int g=c-'0';
    if(i+2<rep->len&&isdg(rep->d[i+2])&&g*10+rep->d[i+2]-'0'<=nc){g=g*10+rep->d[i+2]-'0';adv=3;}
    if(g>=1&&g<=nc){JStr*cc=jrx_cap(s,caps,g);add=cc?cc:&jemptys;}}
   if(add){acc=jcat(acc,jnewstr(rep->d+st,i-st));acc=jcat(acc,add);i+=adv;st=i;continue;}}
  i++;}
 return jcat(acc,jnewstr(rep->d+st,rep->len-st));}
static JV jrx_replace(JStr*s,JOb*re,JV rep,int forceall){
 int fb=(int)re->ex.u.d,from=0,last=0,caps[2*(RMAXCAP+1)],nc,g=(fb&RF_G)!=0,l;JSB acc={0,0,0};JStr*rs=0;
 if(forceall&&!g)return jthrowerr(jErrT,"replaceAll must be called with a global RegExp");
 if(!jiscallable(rep)){rs=jtos(rep);if(jthrow)return jundef();}
 for(;from<=s->len;){
  JStr*piece;
  if(!jrx_search(re,s,from,caps,&nc))break;
  jsb_add(&acc,s->d+last,caps[0]-last);
  if(rs)piece=jrx_expand(rs,s,caps,nc);
  else{
   JV av[2+RMAXCAP+2];int k,na=0;
   for(k=0;k<=nc;k++){JStr*cc=jrx_cap(s,caps,k);av[na++]=cc?jstrv(cc):jundef();}
   av[na++]=jnum(jb2cp(s,caps[0]));av[na++]=jstrv(s);
   {JV r=jcall(rep,jundef(),av,na);if(jthrow)return jundef();piece=jtos(r);if(jthrow)return jundef();}}
  jsb_str(&acc,piece);
  last=caps[1];
  if(!g)break;
  if(caps[1]==caps[0]){
   if(caps[1]>=s->len)break;
   jdec8(s->d+caps[1],s->len-caps[1],&l);
   jsb_add(&acc,s->d+caps[1],l);last=caps[1]+l;from=last;}
  else from=caps[1];}
 jsb_add(&acc,s->d+last,s->len-last);
 {JPr*li=jfind(re,K_lastIndex);if(li)li->v=jnum(0);}
 return jstrv(jsb_done(&acc));}
static JV jrx_split(JStr*s,JOb*re,int lim){
 JOb*r=jnewarr(8);int p=0,q=0,caps[2*(RMAXCAP+1)],nc,l;
 if(lim==0)return jobj(r);
 if(!s->len){if(!jrx_search(re,s,0,caps,&nc))jarrpush(r,jstrv(s));return jobj(r);}
 while(q<s->len&&r->na<lim){
  int ms,me,g;
  if(!jrx_search(re,s,q,caps,&nc))break;
  ms=caps[0];me=caps[1];
  if(ms>=s->len)break;
  if(me==ms&&ms==p){jdec8(s->d+q,s->len-q,&l);q+=l;if(q<=ms)q=ms+l;continue;}
  jarrpush(r,jnewstr(s->d+p,ms-p)==&jemptys?jcv(""):jstrv(jnewstr(s->d+p,ms-p)));
  for(g=1;g<=nc&&r->na<lim;g++){JStr*cc=jrx_cap(s,caps,g);jarrpush(r,cc?jstrv(cc):jundef());}
  p=me;
  if(me==ms){jdec8(s->d+me,s->len-me,&l);q=me+l;}else q=me;}
 if(r->na<lim)jarrpush(r,jstrv(jnewstr(s->d+p,s->len-p)));
 return jobj(r);}

/* ===================== DOM bindings ===================== */
static int jdirty,jlevel,jready;
static N jwinn; /* sentinel node standing for `window` in the listener table */
static JOb*jDoc,*jLoc,*jEvProto;
static char jurl[1024];
static char jnavb[1024];static int jnavn;
#define JMAXSCR 64

static char*jextp[JMAXSCR];static int jextn[JMAXSCR];
static char jserb[1<<20];static int jsern;
static void jsa(const char*p,int n){int i;for(i=0;i<n&&jsern<(int)sizeof jserb-1;i++)jserb[jsern++]=p[i];}
static void jsac(const char*p){int n=0;while(p[n])n++;jsa(p,n);}
static S jpers(const char*p,int n){char*b=al(n+1);int i;for(i=0;i<n;i++)b[i]=p[i];b[n]=0;return(S){b,n};}
static S jpersj(JStr*s){return jpers(s->d,s->len);}
static JV jfromS(S s){if(!s.p)return jnull();return jsv(s.p,s.n);}
static JStr*jlow(JStr*s){JStr*r=jnewstr(s->d,s->len);int i;for(i=0;i<r->len;i++)if(r->d[i]>='A'&&r->d[i]<='Z')r->d[i]+=32;return r;}
/* attribute iteration / mutation */
static int jat_next(S a,int*pi,S*key,S*val,int*rs,int*re){
 int i=*pi,j,ka,q;
 while(i<a.n&&(isws(a.p[i])||a.p[i]=='/'))i++;
 if(i>=a.n){*pi=i;return 0;}
 ka=i;while(i<a.n&&!isws(a.p[i])&&a.p[i]!='='&&a.p[i]!='/')i++;
 j=i;*key=(S){a.p+ka,j-ka};
 while(i<a.n&&isws(a.p[i]))i++;
 *val=(S){(char*)"",0};
 if(i<a.n&&a.p[i]=='='){
  i++;while(i<a.n&&isws(a.p[i]))i++;
  if(i<a.n&&(a.p[i]=='"'||a.p[i]=='\'')){q=a.p[i++];val->p=a.p+i;while(i<a.n&&a.p[i]!=q)i++;val->n=(int)(a.p+i-val->p);if(i<a.n)i++;}
  else{val->p=a.p+i;while(i<a.n&&!isws(a.p[i]))i++;val->n=(int)(a.p+i-val->p);}}
 *rs=ka;*re=i;*pi=i;return 1;}
static void jn_sync(N*n){
 S v;
 v=getattr(n,"id");n->id=v.p?v:(S){0,0};
 v=getattr(n,"class");n->cls=v.p?v:(S){0,0};
 v=getattr(n,"style");n->sty=v.p?v:(S){0,0};
 v=getattr(n,"href");
 if(v.p){S c=jpers(v.p,v.n);n->href=dec(c);}else n->href=(S){0,0};}
static void jn_setattr(N*n,S name,S val,int present){
 S a=n->at;int i=0,rs,re,o=0;S k,v;char*b;
 b=al(a.n+name.n+val.n*2+16);
 while(jat_next(a,&i,&k,&v,&rs,&re)){
  if(seqs(k,name))continue;
  b[o++]=' ';{int q;for(q=rs;q<re;q++)b[o++]=a.p[q];}}
 if(present){
  int q,hasq=0,hass=0;
  for(q=0;q<val.n;q++){if(val.p[q]=='"')hasq=1;if(val.p[q]=='\'')hass=1;}
  b[o++]=' ';for(q=0;q<name.n;q++)b[o++]=(char)lc(name.p[q]);
  b[o++]='=';b[o++]=hasq?'\'':'"';
  for(q=0;q<val.n;q++)b[o++]=(hasq&&hass&&val.p[q]=='\'')?'"':val.p[q];
  b[o++]=hasq?'\'':'"';}
 b[o]=0;
 n->at=(S){b,o};jn_sync(n);jdirty=1;}
static int jbattr(N*n,const char*name){return getattr(n,name).p!=0;}
static void jn_setbool(N*n,const char*name,int on){int l=0;while(name[l])l++;jn_setattr(n,(S){(char*)name,l},(S){(char*)"",0},on);}
/* tree */
static int jn_isel(N*n){return n->tag.n>0&&!n->gen;}
static void jn_fix(N*p){
 N*c,*prev=0,*last=0;
 for(c=p->kid;c;c=c->nx){c->pv=0;if(jn_isel(c)){c->pv=prev;prev=c;}last=c;}
 p->lel=prev;p->last=last;}
static void jn_unlink(N*n){
 N*p=n->par,*c;
 if(!p)return;
 if(p->kid==n)p->kid=n->nx;
 else for(c=p->kid;c;c=c->nx)if(c->nx==n){c->nx=n->nx;break;}
 n->par=0;n->nx=0;n->pv=0;
 jn_fix(p);jdirty=1;}
static int jn_contains(N*a,N*b){while(b){if(a==b)return 1;b=b->par;}return 0;}
static void jn_insert(N*p,N*n,N*ref){
 N*prev=0,*c;
 if(n->par)jn_unlink(n);
 n->par=p;n->nx=0;
 if(ref&&ref->par==p){
  for(c=p->kid;c&&c!=ref;c=c->nx)prev=c;
  n->nx=ref;if(prev)prev->nx=n;else p->kid=n;}
 else{
  N*e=p->last;
  if(e&&e->gen==2){
   for(c=p->kid;c&&c!=e;c=c->nx)prev=c;
   n->nx=e;if(prev)prev->nx=n;else p->kid=n;}
  else{if(e)e->nx=n;else p->kid=n;}}
 jn_fix(p);jdirty=1;}
static void jn_insertx(N*p,N*n,N*ref){ /* fragment aware */
 if(n->tag.n==5&&seq(n->tag,"#frag")){
  while(n->kid){N*k=n->kid;while(k&&k->gen)k=k->nx;if(!k)break;jn_insert(p,k,ref);}
  return;}
 jn_insert(p,n,ref);}
static N*jn_create(S tag,int text){
 N*n;
 if(text){n=nn((S){0,0});return n;}
 n=nn(tag);n->at=(S){(char*)"",0};return n;}
static N*jn_clone(N*n,int deep){
 N*c=al(sizeof(N)),*k;
 *c=*n;c->par=c->kid=c->last=c->nx=c->pv=c->lel=0;c->s=0;c->det=0;c->jw=0;
 if(deep)for(k=n->kid;k;k=k->nx){N*kc;if(k->gen)continue;kc=jn_clone(k,1);kc->par=c;if(c->last)c->last->nx=kc;else c->kid=kc;c->last=kc;}
 jn_fix(c);
 return c;}
static void jn_text(N*n){
 N*c;
 if(!n->tag.n){jsa(n->txt.p,n->txt.n);return;}
 for(c=n->kid;c;c=c->nx){if(c->gen)continue;jn_text(c);}}
static void jn_ser(N*n,int outer){
 N*c;
 if(!n->tag.n){
  int i;
  N*p=n->par;
  if(p&&(seq(p->tag,"script")||seq(p->tag,"style"))){jsa(n->txt.p,n->txt.n);return;}
  for(i=0;i<n->txt.n;i++){char ch=n->txt.p[i];if(ch=='&')jsac("&amp;");else if(ch=='<')jsac("&lt;");else if(ch=='>')jsac("&gt;");else jsa(&ch,1);}
  return;}
 if(n->gen)return;
 if(outer&&n!=root){
  jsac("<");jsa(n->tag.p,n->tag.n);
  if(n->at.n){if(n->at.p[0]!=' ')jsac(" ");jsa(n->at.p,n->at.n);}
  jsac(">");
  if(isvoid(n->tag))return;}
 for(c=n->kid;c;c=c->nx)jn_ser(c,1);
 if(outer&&n!=root){jsac("</");jsa(n->tag.p,n->tag.n);jsac(">");}}
static void jn_settext(N*n,S t){
 N*c,*x;
 if(!n->tag.n){n->txt=t;jdirty=1;return;}
 for(c=n->kid;c;){x=c->nx;if(!c->gen)jn_unlink(c);c=x;}
 if(t.n){x=nn((S){0,0});x->txt=t;jn_insert(n,x,0);}
 jdirty=1;}
static void jn_sethtml(N*n,S h){
 N*frag,*c;char*buf;
 buf=al(h.n+1);{int i;for(i=0;i<h.n;i++)buf[i]=h.p[i];}
 for(c=n->kid;c;){N*x=c->nx;if(!c->gen)jn_unlink(c);c=x;}
 {int sv=jrec;jrec=0;frag=parse(buf,h.n);jrec=sv;}
 while(frag->kid){c=frag->kid;jn_unlink(c);jn_insert(n,c,0);}
 jdirty=1;}
static N*jn_find(N*n,const char*tag){N*c,*r;if(n->tag.n&&seq(n->tag,tag))return n;for(c=n->kid;c;c=c->nx){if(c->gen)continue;r=jn_find(c,tag);if(r)return r;}return 0;}
static N*jn_byid(N*n,JStr*id){N*c,*r;if(n->tag.n&&n->id.n==id->len&&id->len){int i;for(i=0;i<id->len&&n->id.p[i]==id->d[i];i++);if(i==id->len)return n;}for(c=n->kid;c;c=c->nx){if(c->gen||!c->tag.n)continue;r=jn_byid(c,id);if(r)return r;}return 0;}
static JOb*jwrapn(N*n);
static JV jwrapv(N*n){if(!n)return jnull();return jobj(jwrapn(n));}
static JOb*jwrapn(N*n){
 JOb*o;
 if(n==root||n==&jwinn)return n==root?jDoc:jGlob;
 while(n->gen&&n->par)n=n->par;
 if(n->jw)return(JOb*)n->jw;
 if(jnw>=JMAXWRAP){jthrowerr(jErrR,"too many DOM wrappers");return&jdmyo;}
 o=jnewob(OK_NODE,jNodeProto);o->dn=n;n->jw=o;jwrap[jnw++]=o;return o;}
static N*jthisn(JV th){
 if(th.t==JT_O){if(th.u.o->k==OK_NODE)return th.u.o->dn;if(th.u.o->k==OK_DOC)return th.u.o->fl==1?&jwinn:root;if(th.u.o==jGlob)return&jwinn;}
 return 0;}
#define JNODE N*n=jthisn(th);if(!n)return jthrowerr(jErrT,"Illegal invocation");
static JOb*jchildren(N*n,int elonly){
 JOb*a=jnewarr(8);N*c;
 for(c=n->kid;c;c=c->nx){if(c->gen)continue;if(elonly&&!c->tag.n)continue;jarrpush(a,jwrapv(c));}
 return a;}
/* selectors */
static int jselrule(S s,Rule*r){
 int i=0,cb,pe=0,sp;
 r->ns=0;r->spec=0;r->pe=0;r->d=(S){0,0};r->mnw=0;r->mxw=1<<30;r->mnh=0;r->mxh=1<<30;
 s=trim(s);
 while(i<s.n){
  cb=0;
  while(i<s.n&&isws(s.p[i]))i++;
  if(i>=s.n)break;
  if(s.p[i]=='>'||s.p[i]=='+'||s.p[i]=='~'){
   if(!r->ns)return 0;
   cb=s.p[i]=='>'?1:s.p[i]=='+'?2:3;i++;
   while(i<s.n&&isws(s.p[i]))i++;}
  if(r->ns>=4||pe)return 0;
  sp=0;
  if(!pcomp(s,&i,&r->c[r->ns],&sp,&pe,0))return 0;
  if(i==0)return 0;
  r->comb[r->ns]=(char)cb;r->ns++;r->spec+=sp;}
 r->pe=pe;
 return r->ns>0&&!pe;}
typedef struct{Rule r[8];int n;}JSelSet;
static JSelSet jsels;
static int jselparse(JStr*sel){
 S parts[8];int np,k;
 np=splitc((S){sel->d,sel->len},parts,8);
 jsels.n=0;
 for(k=0;k<np;k++){if(!jselrule(parts[k],&jsels.r[jsels.n]))return 0;jsels.n++;}
 return jsels.n>0;}
static int jselmatch(N*n){int k;if(!jn_isel(n))return 0;for(k=0;k<jsels.n;k++)if(mtchk(n,&jsels.r[k],jsels.r[k].ns-1))return 1;return 0;}
static N*jqfirst;
static void jqwalk(N*n,JOb*out,int all){
 N*c;
 for(c=n->kid;c;c=c->nx){
  if(!jn_isel(c))continue;
  if(jselmatch(c)){if(all)jarrpush(out,jwrapv(c));else if(!jqfirst){jqfirst=c;return;}}
  jqwalk(c,out,all);
  if(!all&&jqfirst)return;}}
static JV jquery(N*n,JStr*sel,int all){
 int sv=nnot;JOb*out=all?jnewarr(8):0;
 if(!jselparse(sel)){nnot=sv;return jthrowerr(jErrS,"'selector' is not a valid selector");}
 jqfirst=0;jqwalk(n,out,all);
 nnot=sv;
 if(all)return jobj(out);
 return jwrapv(jqfirst);}
static void jtagwalk(N*n,JStr*tag,int star,JOb*out){
 N*c;
 for(c=n->kid;c;c=c->nx){
  if(!jn_isel(c))continue;
  if(star||(c->tag.n==tag->len&&seq(c->tag,tag->d)))jarrpush(out,jwrapv(c));
  jtagwalk(c,tag,star,out);}}
static void jclswalk(N*n,JStr*cl,JOb*out){
 N*c;
 for(c=n->kid;c;c=c->nx){
  if(!jn_isel(c))continue;
  {S t=(S){cl->d,cl->len};if(hc(c->cls,t))jarrpush(out,jwrapv(c));}
  jclswalk(c,cl,out);}}
/* rects */
static void jn_rect(N*n,int*x,int*y,int*w,int*h){
 int i,x0=1<<30,y0=1<<30,x1=-(1<<30),y1=-(1<<30),any=0;
 for(i=0;i<nc;i++){
  N*o=cm[i].on;int d=0;
  while(o&&o!=n&&d<80){o=o->par;d++;}
  if(o!=n)continue;
  if(cm[i].k==3)continue;
  if(cm[i].x<x0)x0=cm[i].x;
  if(cm[i].y<y0)y0=cm[i].y;
  if(cm[i].x+cm[i].w>x1)x1=cm[i].x+cm[i].w;
  if(cm[i].y+cm[i].h>y1)y1=cm[i].y+cm[i].h;
  any=1;}
 if(!any){*x=*y=*w=*h=0;return;}
 *x=x0;*y=y0;*w=x1-x0;*h=y1-y0;}
/* style / dataset / classList helpers */
static void jkebab(JStr*k,char*o,int cap,const char*pre){
 int n=0,i;
 if(jkeqc(k,"cssFloat")){o[0]='f';o[1]='l';o[2]='o';o[3]='a';o[4]='t';o[5]=0;return;}
 for(i=0;pre[i]&&n<cap-2;i++)o[n++]=pre[i];
 for(i=0;i<k->len&&n<cap-3;i++){char c=k->d[i];if(c>='A'&&c<='Z'){o[n++]='-';o[n++]=(char)(c+32);}else o[n++]=c;}
 o[n]=0;}
static S jsty_get(N*n,const char*prop){
 S s=n->sty;int i=0,a=0,d=0;
 for(;i<=s.n;i++){
  if(i<s.n&&s.p[i]=='(')d++;else if(i<s.n&&s.p[i]==')')d--;
  else if(i==s.n||(s.p[i]==';'&&!d)){
   S decl=trim((S){s.p+a,i-a});int c;
   for(c=0;c<decl.n&&decl.p[c]!=':';c++);
   if(c<decl.n){S nm=trim((S){decl.p,c}),vl=trim((S){decl.p+c+1,decl.n-c-1});if(seq(nm,prop))return vl;}
   a=i+1;}}
 return(S){0,0};}
static void jsty_set(N*n,const char*prop,S val){
 S s=n->sty;int i=0,a=0,d=0,o=0,pl=0;char*b;
 while(prop[pl])pl++;
 b=al(s.n+pl+val.n+8);
 for(;i<=s.n;i++){
  if(i<s.n&&s.p[i]=='(')d++;else if(i<s.n&&s.p[i]==')')d--;
  else if(i==s.n||(s.p[i]==';'&&!d)){
   S decl=trim((S){s.p+a,i-a});int c;
   for(c=0;c<decl.n&&decl.p[c]!=':';c++);
   if(c<decl.n&&c>0){S nm=trim((S){decl.p,c});if(!seq(nm,prop)){int q;for(q=0;q<decl.n;q++)b[o++]=decl.p[q];b[o++]=';';}}
   a=i+1;}}
 if(val.n){int q;for(q=0;q<pl;q++)b[o++]=prop[q];b[o++]=':';for(q=0;q<val.n;q++)b[o++]=val.p[q];b[o++]=';';}
 b[o]=0;
 jn_setattr(n,(S){(char*)"style",5},(S){b,o},1);}
static int jcls_set(N*n,JOb*toks){
 char*b;int i,o=0,l=0;
 for(i=0;i<toks->na;i++)l+=toks->it[i].u.s->len+1;
 b=al(l+2);
 for(i=0;i<toks->na;i++){if(i)b[o++]=' ';{int q;JStr*t=toks->it[i].u.s;for(q=0;q<t->len;q++)b[o++]=t->d[q];}}
 b[o]=0;
 jn_setattr(n,(S){(char*)"class",5},(S){b,o},1);return 1;}
static JOb*jcls_toks(N*n){
 JOb*a=jnewarr(4);S c=n->cls;int i=0,s;
 while(i<c.n){while(i<c.n&&isws(c.p[i]))i++;s=i;while(i<c.n&&!isws(c.p[i]))i++;if(i>s)jarrpush(a,jsv(c.p+s,i-s));}
 return a;}

/* ---- DOM property hooks ---- */
static const struct{const char*p,*a;}JREFL[]={{"id","id"},{"className","class"},{"href","href"},{"src","src"},{"title","title"},{"type","type"},{"name","name"},{"placeholder","placeholder"},{"alt","alt"},{"htmlFor","for"},{"lang","lang"},{"rel","rel"},{"target","target"},{"action","action"},{"method","method"},{0,0}};
static const struct{const char*p,*a;}JBOOLP[]={{"disabled","disabled"},{"checked","checked"},{"hidden","hidden"},{"selected","selected"},{"readOnly","readonly"},{"required","required"},{"multiple","multiple"},{"open","open"},{0,0}};
static JV jupper(S t){JStr*s=jnewstr(t.p,t.n);int i;for(i=0;i<s->len;i++)if(s->d[i]>='a'&&s->d[i]<='z')s->d[i]-=32;return jstrv(s);}
static N*jprevsib(N*n){N*c,*p=0;if(!n->par)return 0;for(c=n->par->kid;c&&c!=n;c=c->nx)if(!c->gen)p=c;return p;}
static N*jnextsib(N*n){N*c=n->nx;while(c&&c->gen)c=c->nx;return c;}
static N*jfirstk(N*n,int el){N*c;for(c=n->kid;c;c=c->nx)if(!c->gen&&(!el||c->tag.n))return c;return 0;}
static N*jlastk(N*n,int el){N*c,*r=0;for(c=n->kid;c;c=c->nx)if(!c->gen&&(!el||c->tag.n))r=c;return r;}
static JV jlocpart(const char*which){
 char*u=jurl;int n=0,i,a;
 while(u[n])n++;
 if(which[0]=='h'&&which[1]=='r')return jsv(u,n);
 a=0;for(i=0;i<n&&u[i]!=':';i++);
 if(which[0]=='p'&&which[1]=='r')return jsv(u,i<n?i+1:0);
 if(i<n)i++;
 if(i+1<n&&u[i]=='/'&&u[i+1]=='/'){a=i+2;i=a;while(i<n&&u[i]!='/'&&u[i]!='?'&&u[i]!='#')i++;}
 else a=i=n<i?n:i;
 if(which[0]=='h'&&which[1]=='o')return jsv(u+a,i-a);
 if(which[0]=='o'){int pe=a;{int q=0;while(q<n&&u[q]!=':')q++;return jsv(u,q+1<n?(i):0);}(void)pe;}
 if(which[0]=='p'&&which[1]=='a'){int e=i;while(e<n&&u[e]!='?'&&u[e]!='#')e++;return jsv(u+i,e-i);}
 if(which[0]=='s'){int e=i;while(e<n&&u[e]!='?'&&u[e]!='#')e++;if(e<n&&u[e]=='?'){int f=e;while(f<n&&u[f]!='#')f++;return jsv(u+e,f-e);}return jcv("");}
 if(which[0]=='h'&&which[1]=='a'){int e=i;while(e<n&&u[e]!='#')e++;return jsv(u+e,n-e);}
 return jcv("");}
static void jnavto(JStr*s){int i;for(i=0;i<s->len&&i<(int)sizeof jnavb-1;i++)jnavb[i]=s->d[i];jnavb[i]=0;jnavn=i;}
static int jdomget(JOb*o,JStr*k,JV*out){
 N*n;int i;
#define KY(s) jkeqc(k,s)
 if(o->k==OK_NODE){
  n=o->dn;
  if(KY("nodeType")){*out=jnum(n->tag.n?1:3);return 1;}
  if(KY("nodeName")||KY("tagName")){*out=n->tag.n?jupper(n->tag):jcv("#text");return 1;}
  if(KY("localName")){*out=n->tag.n?jfromS(n->tag):jnull();return 1;}
  for(i=0;JREFL[i].p;i++)if(KY(JREFL[i].p)){S v=getattr(n,JREFL[i].a);*out=v.p?jsv(v.p,v.n):jcv("");return 1;}
  for(i=0;JBOOLP[i].p;i++)if(KY(JBOOLP[i].p)){int on=jbattr(n,JBOOLP[i].a);if(i==7&&n->det&1)on=!on;*out=jbool(on);return 1;}
  if(KY("value")){
   if(seq(n->tag,"textarea")){jsern=0;jn_text(n);*out=jsv(jserb,jsern);return 1;}
   {S v=getattr(n,"value");*out=v.p?jsv(v.p,v.n):jcv("");return 1;}}
  if(KY("textContent")||KY("innerText")||KY("data")||KY("nodeValue")){
   if(!n->tag.n){*out=jsv(n->txt.p,n->txt.n);return 1;}
   if(KY("data")||KY("nodeValue")){*out=jnull();return 1;}
   jsern=0;jn_text(n);*out=jsv(jserb,jsern);return 1;}
  if(KY("innerHTML")){jsern=0;jn_ser(n,0);*out=jsv(jserb,jsern);return 1;}
  if(KY("outerHTML")){jsern=0;jn_ser(n,1);*out=jsv(jserb,jsern);return 1;}
  if(KY("style")){JOb*w=jnewob(OK_STYLE,jStyleProto);w->dn=n;*out=jobj(w);return 1;}
  if(KY("dataset")){JOb*w=jnewob(OK_STYLE,jStyleProto);w->dn=n;w->fl=1;*out=jobj(w);return 1;}
  if(KY("classList")){JOb*w=jnewob(OK_CLS,jClsProto);w->dn=n;*out=jobj(w);return 1;}
  if(KY("parentNode")){*out=jwrapv(n->par);return 1;}
  if(KY("parentElement")){*out=(n->par&&n->par!=root)?jwrapv(n->par):jnull();return 1;}
  if(KY("firstChild")){*out=jwrapv(jfirstk(n,0));return 1;}
  if(KY("lastChild")){*out=jwrapv(jlastk(n,0));return 1;}
  if(KY("firstElementChild")){*out=jwrapv(jfirstk(n,1));return 1;}
  if(KY("lastElementChild")){*out=jwrapv(jlastk(n,1));return 1;}
  if(KY("nextSibling")){*out=jwrapv(jnextsib(n));return 1;}
  if(KY("previousSibling")){*out=jwrapv(jprevsib(n));return 1;}
  if(KY("nextElementSibling")){N*c=jnextsib(n);while(c&&!c->tag.n)c=jnextsib(c);*out=jwrapv(c);return 1;}
  if(KY("previousElementSibling")){*out=jwrapv(n->tag.n?n->pv:0);return 1;}
  if(KY("children")){*out=jobj(jchildren(n,1));return 1;}
  if(KY("childNodes")){*out=jobj(jchildren(n,0));return 1;}
  if(KY("childElementCount")){*out=jnum(jchildren(n,1)->na);return 1;}
  if(KY("ownerDocument")){*out=jobj(jDoc);return 1;}
  if(KY("isConnected")){N*p=n;while(p->par)p=p->par;*out=jbool(p==root);return 1;}
  if(KY("offsetWidth")||KY("clientWidth")||KY("scrollWidth")){int x,y,w,h;jn_rect(n,&x,&y,&w,&h);*out=jnum(w);return 1;}
  if(KY("offsetHeight")||KY("clientHeight")||KY("scrollHeight")){int x,y,w,h;jn_rect(n,&x,&y,&w,&h);*out=jnum(h);return 1;}
  if(KY("offsetLeft")){int x,y,w,h;jn_rect(n,&x,&y,&w,&h);*out=jnum(x);return 1;}
  if(KY("offsetTop")){int x,y,w,h;jn_rect(n,&x,&y,&w,&h);*out=jnum(y);return 1;}
  if(KY("scrollTop")||KY("scrollLeft")){*out=jnum(0);return 1;}
  return 0;}
 if(o->k==OK_DOC&&o->fl==0){
  if(KY("body")){*out=jwrapv(jn_find(root,"body"));return 1;}
  if(KY("head")){*out=jwrapv(jn_find(root,"head"));return 1;}
  if(KY("documentElement")){*out=jwrapv(jn_find(root,"html"));return 1;}
  if(KY("activeElement")){*out=jwrapv(jn_find(root,"body"));return 1;}
  if(KY("title")){*out=ttl.n?jsv(ttl.p,ttl.n):jcv("");return 1;}
  if(KY("readyState")){*out=jcv("complete");return 1;}
  if(KY("cookie")){*out=jcv("");return 1;}
  if(KY("URL")||KY("documentURI")){*out=jcv(jurl);return 1;}
  if(KY("location")){*out=jobj(jLoc);return 1;}
  if(KY("defaultView")){*out=jobj(jGlob);return 1;}
  if(KY("nodeType")){*out=jnum(9);return 1;}
  if(KY("nodeName")){*out=jcv("#document");return 1;}
  if(KY("childNodes")||KY("children")){*out=jobj(jchildren(root,0));return 1;}
  return 0;}
 if(o->k==OK_DOC&&o->fl==1){
  if(KY("href")){*out=jlocpart("href");return 1;}
  if(KY("protocol")){*out=jlocpart("pr");return 1;}
  if(KY("host")||KY("hostname")){*out=jlocpart("ho");return 1;}
  if(KY("origin")){*out=jlocpart("o");return 1;}
  if(KY("pathname")){*out=jlocpart("pa");return 1;}
  if(KY("search")){*out=jlocpart("s");return 1;}
  if(KY("hash")){*out=jlocpart("ha");return 1;}
  return 0;}
 if(o->k==OK_STYLE){
  char buf[96];S v;
  n=o->dn;
  if(!o->fl&&KY("cssText")){*out=jsv(n->sty.p?n->sty.p:"",n->sty.n);return 1;}
  if(KY("length")){*out=jnum(0);return 1;}
  if(KY("setProperty")||KY("getPropertyValue")||KY("removeProperty")||KY("getPropertyPriority"))return 0;
  if(o->fl==1){char nb[112];S a;jkebab(k,buf,90,"");{int q=0,r=0;const char*p="data-";while(p[q])nb[r++]=p[q++];for(q=0;buf[q];q++)nb[r++]=buf[q];nb[r]=0;}a=getattr(n,nb);*out=a.p?jsv(a.p,a.n):jundef();return 1;}
  jkebab(k,buf,90,"");
  v=jsty_get(n,buf);*out=jsv(v.p?v.p:"",v.n);return 1;}
 if(o->k==OK_CLS){
  if(KY("length")){*out=jnum(jcls_toks(o->dn)->na);return 1;}
  if(KY("value")){*out=jsv(o->dn->cls.p?o->dn->cls.p:"",o->dn->cls.n);return 1;}
  return 0;}
#undef KY
 return 0;}
static int jdomset(JOb*o,JStr*k,JV v){
 N*n;int i;
#define KY(s) jkeqc(k,s)
 if(o->k==OK_NODE){
  n=o->dn;
  for(i=0;JREFL[i].p;i++)if(KY(JREFL[i].p)){JStr*s=jtos(v);if(jthrow)return 1;jn_setattr(n,(S){(char*)JREFL[i].a,(int)jcl_(JREFL[i].a)},jpersj(s),1);return 1;}
  for(i=0;JBOOLP[i].p;i++)if(KY(JBOOLP[i].p)){int on=jtruthy(v);if(i==7&&(n->det&1)){n->det=0;}jn_setbool(n,JBOOLP[i].a,on);return 1;}
  if(KY("value")){
   JStr*s=jtos(v);if(jthrow)return 1;
   if(seq(n->tag,"textarea"))jn_settext(n,jpersj(s));
   else jn_setattr(n,(S){(char*)"value",5},jpersj(s),1);
   return 1;}
  if(KY("textContent")||KY("innerText")||KY("data")||KY("nodeValue")){
   JStr*s=(v.t==JT_NUL||v.t==JT_U)?&jemptys:jtos(v);if(jthrow)return 1;
   jn_settext(n,jpersj(s));return 1;}
  if(KY("innerHTML")){JStr*s=jtos(v);if(jthrow)return 1;jn_sethtml(n,jpersj(s));return 1;}
  if(KY("style")){JStr*s=jtos(v);if(jthrow)return 1;jn_setattr(n,(S){(char*)"style",5},jpersj(s),1);return 1;}
  if(KY("scrollTop")||KY("scrollLeft"))return 1;
  return 0;}
 if(o->k==OK_DOC&&o->fl==0){
  if(KY("title")){JStr*s=jtos(v);if(jthrow)return 1;ttl=jpersj(s);jdirty=1;return 1;}
  if(KY("cookie"))return 1;
  return 0;}
 if(o->k==OK_DOC&&o->fl==1){
  if(KY("href")){JStr*s=jtos(v);if(jthrow)return 1;jnavto(s);return 1;}
  return 0;}
 if(o->k==OK_STYLE){
  char buf[96];JStr*s=(v.t==JT_NUL||v.t==JT_U)?&jemptys:jtos(v);
  if(jthrow)return 1;
  n=o->dn;
  if(!o->fl&&KY("cssText")){jn_setattr(n,(S){(char*)"style",5},jpersj(s),1);return 1;}
  if(o->fl==1){char nb[112];int q=0,r=0;const char*p="data-";jkebab(k,buf,90,"");while(p[q])nb[r++]=p[q++];for(q=0;buf[q];q++)nb[r++]=buf[q];nb[r]=0;
   jn_setattr(n,jpers(nb,r),jpersj(s),1);return 1;}
  jkebab(k,buf,90,"");
  jsty_set(n,buf,jpersj(s));return 1;}
#undef KY
 return 0;}

/* ---- DOM natives ---- */
#define JSA(i) jtos(JARG(i))
static JV jN_getAttribute(JV th,JV*args,int argc){JNODE{JStr*k=jtos(JARG(0));char b[96];S v;int i;for(i=0;i<k->len&&i<94;i++)b[i]=k->d[i];b[i]=0;v=getattr(n,b);return v.p?jsv(v.p,v.n):jnull();}}
static JV jN_setAttribute(JV th,JV*args,int argc){JNODE{JStr*k=jtos(JARG(0)),*v=jtos(JARG(1));if(jthrow)return jundef();jn_setattr(n,jpersj(k),jpersj(v),1);return jundef();}}
static JV jN_removeAttribute(JV th,JV*args,int argc){JNODE{JStr*k=jtos(JARG(0));jn_setattr(n,jpersj(k),(S){(char*)"",0},0);return jundef();}}
static JV jN_hasAttribute(JV th,JV*args,int argc){JNODE{JStr*k=jtos(JARG(0));char b[96];int i;for(i=0;i<k->len&&i<94;i++)b[i]=k->d[i];b[i]=0;return jbool(jbattr(n,b));}}
static JV jN_toggleAttribute(JV th,JV*args,int argc){JNODE{JStr*k=jtos(JARG(0));char b[96];int i,has,want;for(i=0;i<k->len&&i<94;i++)b[i]=k->d[i];b[i]=0;has=jbattr(n,b);want=argc>1?jtruthy(args[1]):!has;if(want!=has)jn_setattr(n,jpersj(k),(S){(char*)"",0},want);return jbool(want);}}
static N*jargn(JV v){if(v.t==JT_O&&v.u.o->k==OK_NODE)return v.u.o->dn;if(v.t==JT_O&&v.u.o->k==OK_DOC)return root;return 0;}
static JV jN_appendChild(JV th,JV*args,int argc){JNODE{N*c=jargn(JARG(0));if(!c)return jthrowerr(jErrT,"parameter 1 is not of type 'Node'");if(jn_contains(c,n))return jthrowerr(jErrT,"The new child element contains the parent.");jn_insertx(n,c,0);return JARG(0);}}
static JV jN_insertBefore(JV th,JV*args,int argc){JNODE{N*c=jargn(JARG(0)),*r=jargn(JARG(1));if(!c)return jthrowerr(jErrT,"parameter 1 is not of type 'Node'");if(jn_contains(c,n))return jthrowerr(jErrT,"The new child element contains the parent.");jn_insertx(n,c,r);return JARG(0);}}
static JV jN_removeChild(JV th,JV*args,int argc){JNODE{N*c=jargn(JARG(0));if(!c||c->par!=n)return jthrowerr(jErrT,"The node to be removed is not a child of this node.");jn_unlink(c);return JARG(0);}}
static JV jN_replaceChild(JV th,JV*args,int argc){JNODE{N*c=jargn(JARG(0)),*o=jargn(JARG(1));if(!c||!o||o->par!=n)return jthrowerr(jErrT,"The node to be replaced is not a child of this node.");jn_insertx(n,c,o);if(o->par==n)jn_unlink(o);return JARG(1);}}
static N*jtonode(JV v){
 if(v.t==JT_O&&(v.u.o->k==OK_NODE||v.u.o->k==OK_DOC))return jargn(v);
 {JStr*s=jtos(v);N*t=nn((S){0,0});t->txt=jpersj(s);return t;}}
static JV jN_append(JV th,JV*args,int argc){JNODE{int i;for(i=0;i<argc;i++){N*c=jtonode(args[i]);if(jthrow)return jundef();if(c&&!jn_contains(c,n))jn_insertx(n,c,0);}return jundef();}}
static JV jN_prepend(JV th,JV*args,int argc){JNODE{int i;N*f=jfirstk(n,0);for(i=0;i<argc;i++){N*c=jtonode(args[i]);if(jthrow)return jundef();if(c&&!jn_contains(c,n))jn_insertx(n,c,f);}return jundef();}}
static JV jN_before(JV th,JV*args,int argc){JNODE{int i;if(!n->par)return jundef();for(i=0;i<argc;i++){N*c=jtonode(args[i]);if(jthrow)return jundef();if(c)jn_insertx(n->par,c,n);}return jundef();}}
static JV jN_after(JV th,JV*args,int argc){JNODE{int i;N*nx;if(!n->par)return jundef();nx=jnextsib(n);for(i=0;i<argc;i++){N*c=jtonode(args[i]);if(jthrow)return jundef();if(c)jn_insertx(n->par,c,nx);}return jundef();}}
static JV jN_remove(JV th,JV*args,int argc){JNODE(void)args;(void)argc;if(n->par)jn_unlink(n);return jundef();}
static JV jN_replaceWith(JV th,JV*args,int argc){JNODE{N*c;if(!n->par||argc<1)return jundef();c=jtonode(args[0]);if(jthrow)return jundef();if(c){jn_insertx(n->par,c,n);jn_unlink(n);}return jundef();}}
static JV jN_cloneNode(JV th,JV*args,int argc){JNODE return jwrapv(jn_clone(n,argc>0&&jtruthy(args[0])));}
static JV jN_contains(JV th,JV*args,int argc){JNODE{N*c=jargn(JARG(0));return jbool(c&&jn_contains(n,c));}}
static JV jN_hasChildNodes(JV th,JV*args,int argc){JNODE(void)args;(void)argc;return jbool(jfirstk(n,0)!=0);}
static JV jN_qs(JV th,JV*args,int argc){JNODE{JStr*s=JSA(0);if(jthrow)return jundef();return jquery(n,s,0);}}
static JV jN_qsa(JV th,JV*args,int argc){JNODE{JStr*s=JSA(0);if(jthrow)return jundef();return jquery(n,s,1);}}
static JV jN_matches(JV th,JV*args,int argc){JNODE{JStr*s=JSA(0);int sv=nnot,r;if(jthrow)return jundef();if(!jselparse(s)){nnot=sv;return jthrowerr(jErrS,"'selector' is not a valid selector");}r=jselmatch(n);nnot=sv;return jbool(r);}}
static JV jN_closest(JV th,JV*args,int argc){JNODE{JStr*s=JSA(0);int sv=nnot;N*c;if(jthrow)return jundef();if(!jselparse(s)){nnot=sv;return jthrowerr(jErrS,"'selector' is not a valid selector");}for(c=n;c&&c!=root;c=c->par)if(jselmatch(c)){nnot=sv;return jwrapv(c);}nnot=sv;return jnull();}}
static JV jN_byTag(JV th,JV*args,int argc){JNODE{JStr*t=jlow(JSA(0));JOb*out=jnewarr(8);jtagwalk(n,t,t->len==1&&t->d[0]=='*',out);return jobj(out);}}
static JV jN_byClass(JV th,JV*args,int argc){JNODE{JStr*t=JSA(0);JOb*out=jnewarr(8);jclswalk(n,t,out);return jobj(out);}}
static JV jD_byId(JV th,JV*args,int argc){JNODE{JStr*t=JSA(0);(void)n;return jwrapv(jn_byid(root,t));}}
static JV jD_createElement(JV th,JV*args,int argc){JStr*t=jlow(JSA(0));JUNUSED;{N*e=jn_create(jpersj(t),0);return jwrapv(e);}}
static JV jD_createText(JV th,JV*args,int argc){JStr*t=JSA(0);JUNUSED;{N*e=jn_create((S){0,0},1);e->txt=jpersj(t);return jwrapv(e);}}
static JV jD_createFrag(JV th,JV*args,int argc){JUNUSED;return jwrapv(jn_create((S){(char*)"#frag",5},0));}
static JV jD_write(JV th,JV*args,int argc){JUNUSED;jlogs("warn: document.write is not supported\n",38);return jundef();}
static JV jN_insertAdj(JV th,JV*args,int argc){
 JNODE{
  JStr*pos=jlow(JSA(0)),*h=JSA(1);N*frag,*c;char*buf;
  if(jthrow)return jundef();
  buf=al(h->len+1);{int i;for(i=0;i<h->len;i++)buf[i]=h->d[i];}
  {int sv=jrec;jrec=0;frag=parse(buf,h->len);jrec=sv;}
  if(jkeqc(pos,"beforeend")){while(frag->kid){c=frag->kid;jn_unlink(c);jn_insert(n,c,0);}}
  else if(jkeqc(pos,"afterbegin")){N*f=jfirstk(n,0);while(frag->kid){c=frag->kid;jn_unlink(c);jn_insert(n,c,f);}}
  else if(jkeqc(pos,"beforebegin")&&n->par){while(frag->kid){c=frag->kid;jn_unlink(c);jn_insert(n->par,c,n);}}
  else if(jkeqc(pos,"afterend")&&n->par){N*nx=jnextsib(n);while(frag->kid){c=frag->kid;jn_unlink(c);jn_insert(n->par,c,nx);}}
  jdirty=1;return jundef();}}
static JV jN_rect(JV th,JV*args,int argc){
 JNODE{
  int x,y,w,h;JOb*r=jnewob(OK_PLAIN,jObjProto);
  (void)args;(void)argc;
  jn_rect(n,&x,&y,&w,&h);
  jsetc(r,"x",jnum(x));jsetc(r,"y",jnum(y));jsetc(r,"left",jnum(x));jsetc(r,"top",jnum(y));
  jsetc(r,"width",jnum(w));jsetc(r,"height",jnum(h));jsetc(r,"right",jnum(x+w));jsetc(r,"bottom",jnum(y+h));
  return jobj(r);}}
/* style & classList natives */
static JV jS_setProperty(JV th,JV*args,int argc){
 JStr*k=JSA(0),*v=JSA(1);char b[96];
 if(th.t!=JT_O||th.u.o->k!=OK_STYLE)return jthrowerr(jErrT,"Illegal invocation");
 {int i,n=0;for(i=0;i<k->len&&n<94;i++)b[n++]=k->d[i];b[n]=0;}
 jsty_set(th.u.o->dn,b,jpersj(v));return jundef();}
static JV jS_getPropertyValue(JV th,JV*args,int argc){
 JStr*k=JSA(0);char b[96];S v;
 if(th.t!=JT_O||th.u.o->k!=OK_STYLE)return jthrowerr(jErrT,"Illegal invocation");
 {int i,n=0;for(i=0;i<k->len&&n<94;i++)b[n++]=k->d[i];b[n]=0;}
 v=jsty_get(th.u.o->dn,b);return jsv(v.p?v.p:"",v.n);}
static JV jS_removeProperty(JV th,JV*args,int argc){
 JStr*k=JSA(0);char b[96];S v;JV r;
 if(th.t!=JT_O||th.u.o->k!=OK_STYLE)return jthrowerr(jErrT,"Illegal invocation");
 {int i,n=0;for(i=0;i<k->len&&n<94;i++)b[n++]=k->d[i];b[n]=0;}
 v=jsty_get(th.u.o->dn,b);r=jsv(v.p?v.p:"",v.n);jsty_set(th.u.o->dn,b,(S){0,0});return r;}
static JV jC_add(JV th,JV*args,int argc){
 JOb*t;int i,k;
 if(th.t!=JT_O||th.u.o->k!=OK_CLS)return jthrowerr(jErrT,"Illegal invocation");
 t=jcls_toks(th.u.o->dn);
 for(i=0;i<argc;i++){JStr*s=jtos(args[i]);for(k=0;k<t->na;k++)if(jkeq(t->it[k].u.s,s))break;if(k==t->na&&s->len)jarrpush(t,jstrv(s));}
 jcls_set(th.u.o->dn,t);return jundef();}
static JV jC_remove(JV th,JV*args,int argc){
 JOb*t,*r;int i,k;
 if(th.t!=JT_O||th.u.o->k!=OK_CLS)return jthrowerr(jErrT,"Illegal invocation");
 t=jcls_toks(th.u.o->dn);r=jnewarr(t->na);
 for(i=0;i<t->na;i++){int rm=0;for(k=0;k<argc;k++)if(jkeq(t->it[i].u.s,jtos(args[k])))rm=1;if(!rm)jarrpush(r,t->it[i]);}
 jcls_set(th.u.o->dn,r);return jundef();}
static JV jC_contains(JV th,JV*args,int argc){
 JOb*t;int i;JStr*s=JSA(0);
 if(th.t!=JT_O||th.u.o->k!=OK_CLS)return jthrowerr(jErrT,"Illegal invocation");
 t=jcls_toks(th.u.o->dn);for(i=0;i<t->na;i++)if(jkeq(t->it[i].u.s,s))return jbool(1);return jbool(0);}
static JV jC_toggle(JV th,JV*args,int argc){
 JOb*t,*r;int i,has=0;JStr*s=JSA(0);int want;
 if(th.t!=JT_O||th.u.o->k!=OK_CLS)return jthrowerr(jErrT,"Illegal invocation");
 t=jcls_toks(th.u.o->dn);for(i=0;i<t->na;i++)if(jkeq(t->it[i].u.s,s))has=1;
 want=argc>1&&args[1].t!=JT_U?jtruthy(args[1]):!has;
 if(want&&!has){jarrpush(t,jstrv(s));jcls_set(th.u.o->dn,t);}
 else if(!want&&has){r=jnewarr(t->na);for(i=0;i<t->na;i++)if(!jkeq(t->it[i].u.s,s))jarrpush(r,t->it[i]);jcls_set(th.u.o->dn,r);}
 return jbool(want);}
static JV jC_replace(JV th,JV*args,int argc){
 JOb*t;int i,f=0;JStr*a=JSA(0),*b=JSA(1);
 if(th.t!=JT_O||th.u.o->k!=OK_CLS)return jthrowerr(jErrT,"Illegal invocation");
 t=jcls_toks(th.u.o->dn);for(i=0;i<t->na;i++)if(jkeq(t->it[i].u.s,a)){t->it[i]=jstrv(b);f=1;}
 if(f)jcls_set(th.u.o->dn,t);
 return jbool(f);}
static JV jC_item(JV th,JV*args,int argc){JOb*t;int i=(int)jtonum(JARG(0));if(th.t!=JT_O||th.u.o->k!=OK_CLS)return jundef();t=jcls_toks(th.u.o->dn);return i>=0&&i<t->na?t->it[i]:jnull();}

/* ---- events ---- */
static void jreport(void){
 JStr*m;int ab;
 if(!jthrow&&!jabort)return;
 ab=jabort;jabort=0;jthrow=0;
 {JV e=jexv;
  if(e.t==JT_O&&e.u.o->k==OK_ERR)m=jtos(e);else m=jinspect(e,0);
  if(jthrow){jthrow=0;m=jcstr("error");}}
 (void)ab;
 if(!m->len)m=jcstr("script error");
 jlogs("Uncaught ",9);jlogs(m->d,m->len);
 {char b[24];int n=0;const char*t=" (line ";int i;for(i=0;t[i];i++)b[n++]=t[i];n+=jd2s((double)jline_cur,b+n);b[n++]=')';jlogs(b,n);}
 jlogs("\n",1);
 jabort=0;jthrow=0;jexv=jundef();}
static void jenter(void){
 if(jlevel++==0){jsteps=0;jdepth=0;jthrow=0;jabort=0;jshort=0;jlabel=0;jrv=jundef();
  jsetc(jGlob,"innerWidth",jnum(VW));jsetc(jGlob,"innerHeight",jnum(VH));}}
static void jleave(void){
 if(--jlevel==0){
  jreport();
  if(joom||jtop>(10<<20)){jgc();joom=0;}}}
static void jaddlst(N*nd,JStr*type,JV fn,int cap){
 int i;
 for(i=0;i<jnl;i++)if(jlst[i].nd==nd&&jkeq(jlst[i].type,type)&&jseq(jlst[i].fn,fn)&&jlst[i].cap==cap)return;
 if(jnl>=JMAXLST){jthrowerr(jErrR,"too many event listeners");return;}
 jlst[jnl].nd=nd;jlst[jnl].type=type;jlst[jnl].fn=fn;jlst[jnl].cap=cap;jnl++;}
static JV jN_addEL(JV th,JV*args,int argc){
 JNODE{
  JStr*t=JSA(0);int cap=0;
  if(!jiscallable(JARG(1)))return jundef();
  if(argc>2){if(args[2].t==JT_B)cap=args[2].u.b;else if(args[2].t==JT_O)cap=jtruthy(jget(args[2],jcstr("capture")));}
  jaddlst(n,t,args[1],cap);return jundef();}}
static JV jN_removeEL(JV th,JV*args,int argc){
 JNODE{
  JStr*t=JSA(0);int i,j;
  for(i=0;i<jnl;i++)if(jlst[i].nd==n&&jkeq(jlst[i].type,t)&&jseq(jlst[i].fn,JARG(1))){for(j=i;j<jnl-1;j++)jlst[j]=jlst[j+1];jnl--;break;}
  return jundef();}}
static int jpath(N*t,N**out,int max){int n=0;while(t&&n<max-2){out[n++]=t;t=t->par;}out[n++]=&jwinn;return n;}
static JV jcompile_handler(S code){
 char*buf;int n=0,i;JNd*prog;JV r=jundef();const char*pre="(function(event){",*post="\n})";
 buf=jalloc(code.n+64,BT_STR);if(!buf)return r;
 for(i=0;pre[i];i++)buf[n++]=pre[i];
 for(i=0;i<code.n;i++)buf[n++]=code.p[i];
 for(i=0;post[i];i++)buf[n++]=post[i];
 prog=jparse(buf,n);jhdr(buf)->ty=BT_FREE;jhdr(buf)->mk=0;
 if(jpfail){jlogs("SyntaxError: ",13);jlogs(jperrm,jcl_(jperrm));jlogs(" (event handler attribute)\n",28);return r;}
 if(prog->b&&prog->b->a&&prog->b->a->t==ND_EXPR)r=jev(prog->b->a->a,jGlob);
 if(jthrow){jreport();jthrow=0;return jundef();}
 return r;}
static void jfire(N*target,JStr*type,JOb*ev,int bubbles){
 N*path[200];int np,i,k;JV evv=jobj(ev);JStr*pk=jcstr("_stop");
 char on[48];int q,l=0;JStr*onk;
 on[0]='o';on[1]='n';l=2;for(q=0;q<type->len&&l<46;q++)on[l++]=type->d[q];on[l]=0;onk=jcstr(on);
 jsetown(ev,jcstr("target"),target==&jwinn?jobj(jGlob):jwrapv(target));
 np=jpath(target,path,200);
 for(i=0;i<np;i++){
  N*cur=path[i];JOb*w=cur==&jwinn?jGlob:(cur==root?jDoc:(JOb*)cur->jw);
  if(i>0&&!bubbles)break;
  if(w)jsetown(ev,jcstr("currentTarget"),jobj(w));
  if(cur!=&jwinn&&cur!=root&&cur->tag.n){
   S at=getattr(cur,on);
   if(at.p&&at.n){JOb*cw=jwrapn(cur);if(!jfind(cw,onk)){JV fn=jcompile_handler(at);if(fn.t==JT_O)jsetown(cw,onk,fn);}w=cw;}}
  if(cur==&jwinn&&jkeqc(type,"load")){N*bd=jn_find(root,"body");S at;if(bd&&(at=getattr(bd,"onload")).p&&at.n&&!jfind(jGlob,onk)){JV fn=jcompile_handler(at);if(fn.t==JT_O)jsetown(jGlob,onk,fn);}}
  if(w){JPr*p=jfind(w,onk);if(p&&jiscallable(p->v)){JV a[1],r;a[0]=evv;r=jcall(p->v,jobj(w),a,1);if(jthrow){jreport();jthrow=0;}else if(r.t==JT_B&&!r.u.b)jsetown(ev,jcstr("defaultPrevented"),jbool(1));}}
  for(k=0;k<jnl;k++){
   if(jlst[k].nd==cur&&jkeq(jlst[k].type,type)){
    JV fn=jlst[k].fn,a[1];
    a[0]=evv;jcall(fn,w?jobj(w):jundef(),a,1);
    if(jthrow){if(jabort)return;jreport();jthrow=0;}
    {JPr*sp=jfind(ev,jcstr("_stopi"));if(sp)break;}}}
  {JPr*sp=jfind(ev,pk);if(sp)break;}}
 jdirty=1;}
static JOb*jmkev(const char*type,int bubbles){
 JOb*e=jnewob(OK_PLAIN,jEvProto);
 jsetc(e,"type",jcv(type));jsetc(e,"bubbles",jbool(bubbles));jsetc(e,"defaultPrevented",jbool(0));jsetc(e,"cancelable",jbool(1));
 return e;}
static JV jE_prevent(JV th,JV*args,int argc){JUNUSED;if(th.t==JT_O)jsetc(th.u.o,"defaultPrevented",jbool(1));return jundef();}
static JV jE_stop(JV th,JV*args,int argc){JUNUSED;if(th.t==JT_O){JPr*p;jsetc(th.u.o,"_stop",jbool(1));p=jfind(th.u.o,jcstr("_stop"));if(p)p->fl|=1;}return jundef();}
static JV jE_stopi(JV th,JV*args,int argc){JUNUSED;if(th.t==JT_O){JPr*p;jsetc(th.u.o,"_stop",jbool(1));jsetc(th.u.o,"_stopi",jbool(1));p=jfind(th.u.o,jcstr("_stop"));if(p)p->fl|=1;p=jfind(th.u.o,jcstr("_stopi"));if(p)p->fl|=1;}return jundef();}
static JV jf_Event(JV th,JV*args,int argc){
 JOb*e;JStr*t=JSA(0);int bub=0;
 if(th.t==JT_O&&jnewcall){e=th.u.o;e->proto=jEvProto;}else e=jnewob(OK_PLAIN,jEvProto);
 if(argc>1&&args[1].t==JT_O){bub=jtruthy(jget(args[1],jcstr("bubbles")));jsetc(e,"detail",jget(args[1],jcstr("detail")));}
 jsetc(e,"type",jstrv(t));jsetc(e,"bubbles",jbool(bub));jsetc(e,"defaultPrevented",jbool(0));jsetc(e,"cancelable",jbool(1));
 return jobj(e);}
static JV jN_dispatch(JV th,JV*args,int argc){
 JNODE{
  JV ev=JARG(0);JStr*t;int bub;
  if(ev.t!=JT_O)return jthrowerr(jErrT,"parameter 1 is not of type 'Event'");
  t=jtos(jget(ev,jcstr("type")));bub=jtruthy(jget(ev,jcstr("bubbles")));
  jfire(n,t,ev.u.o,bub);
  return jbool(!jtruthy(jget(ev,jcstr("defaultPrevented"))));}}
static JV jN_click(JV th,JV*args,int argc){JNODE(void)args;(void)argc;{JOb*e=jmkev("click",1);jfire(n,jcstr("click"),e,1);}return jundef();}
/* ---- timers ---- */
static JV jaddtimer(JV*args,int argc,int interval){
 int i,slot=-1;JV fn=JARG(0);double d=jtonum(JARG(1));
 if(!jiscallable(fn))return jthrowerr(jErrT,"callback is not a function");
 if(d!=d||d<0)d=0;
 if(d>2e9)d=2e9;
 for(i=0;i<jnt;i++)if(!jtm[i].live){slot=i;break;}
 if(slot<0){if(jnt>=JMAXTMR)return jthrowerr(jErrR,"too many timers");slot=jnt++;}
 jtm[slot].fn=fn;jtm[slot].id=jtmid++;jtm[slot].iv=interval?(d<4?4:(int)d):0;jtm[slot].live=1;jtm[slot].due=jnowmono+(j64)d;
 jtm[slot].na=0;
 for(i=2;i<argc&&jtm[slot].na<3;i++)jtm[slot].a[jtm[slot].na++]=args[i];
 return jnum(jtm[slot].id);}
JNAT(jw_setTimeout){(void)th;return jaddtimer(args,argc,0);}
JNAT(jw_setInterval){(void)th;return jaddtimer(args,argc,1);}
JNAT(jw_clearTimeout){int i,id=(int)jtonum(JARG(0));(void)th;for(i=0;i<jnt;i++)if(jtm[i].live&&jtm[i].id==id)jtm[i].live=0;return jundef();}
JNAT(jw_raf){JV a[2];(void)th;a[0]=JARG(0);a[1]=jnum(16);return jaddtimer(a,2,0);}
static int jruntimers(j64 now){
 int guard=0,ran=0;
 for(;guard<200;guard++){
  int i,best=-1;JTm t;
  for(i=0;i<jnt;i++)if(jtm[i].live&&jtm[i].due<=now&&(best<0||jtm[i].due<jtm[best].due))best=i;
  if(best<0)break;
  t=jtm[best];
  if(t.iv){jtm[best].due=(t.due+t.iv>now?t.due+t.iv:now+t.iv);}else jtm[best].live=0;
  {JV av[3];int k;for(k=0;k<3;k++)av[k]=t.a[k];
   if(t.fn.t==JT_O&&jtm[best].iv==0&&0){}
   jsteps=0;jdepth=0;
   jcall(t.fn,jobj(jGlob),av,t.na);
   if(jthrow||jabort){jreport();}}
  ran++;}
 return ran;}

/* ---- Map / Set / URI ---- */
static JOb*jMapProto,*jSetProto,*jLocProto,*jStoreProto;
static int jmfind(JOb*m,JV k){int st=(m->fl&4)?2:1,i;for(i=0;i<m->na;i+=st)if(jsamevalue(m->it[i],k))return i;return-1;}
static JOb*jthism(JV th,int fl){if(th.t!=JT_O||!(th.u.o->fl&fl)){jthrowerr(jErrT,"Method called on incompatible receiver");return 0;}return th.u.o;}
JNAT(jf_Map){
 JOb*m;JUNUSED;
 if(th.t!=JT_O||!jnewcall)return jthrowerr(jErrT,"Constructor Map requires 'new'");
 m=th.u.o;m->fl|=4;m->proto=jMapProto;
 if(argc>0&&args[0].t==JT_O&&args[0].u.o->k==OK_ARR){JOb*a=args[0].u.o;int i;for(i=0;i<a->na;i++){JV e=a->it[i];if(e.t==JT_O&&e.u.o->k==OK_ARR&&e.u.o->na>=2){jarrgrow(m,m->na+2);if(m->na+2<=m->ca){int f=jmfind(m,e.u.o->it[0]);if(f>=0)m->it[f+1]=e.u.o->it[1];else{m->it[m->na++]=e.u.o->it[0];m->it[m->na++]=e.u.o->it[1];}}}}}
 return th;}
JNAT(jf_Set){
 JOb*m;JUNUSED;
 if(th.t!=JT_O||!jnewcall)return jthrowerr(jErrT,"Constructor Set requires 'new'");
 m=th.u.o;m->fl|=8;m->proto=jSetProto;
 if(argc>0&&args[0].t==JT_O&&args[0].u.o->k==OK_ARR){JOb*a=args[0].u.o;int i;for(i=0;i<a->na;i++){if(jmfind(m,a->it[i])<0){jarrgrow(m,m->na+1);if(m->na<m->ca)m->it[m->na++]=a->it[i];}}}
 else if(argc>0&&args[0].t==JT_S){JOb*a=jnewarr(8);int i;for(i=0;i<jcplen(args[0].u.s);i++)jarrpush(a,jstrat(args[0].u.s,i));for(i=0;i<a->na;i++)if(jmfind(m,a->it[i])<0){jarrgrow(m,m->na+1);if(m->na<m->ca)m->it[m->na++]=a->it[i];}}
 return th;}
JNAT(jm_get){JOb*m=jthism(th,4);int f;if(!m)return jundef();f=jmfind(m,JARG(0));return f>=0?m->it[f+1]:jundef();}
JNAT(jm_set){JOb*m=jthism(th,4);int f;if(!m)return jundef();f=jmfind(m,JARG(0));if(f>=0)m->it[f+1]=JARG(1);else{jarrgrow(m,m->na+2);if(m->na+2<=m->ca){m->it[m->na++]=JARG(0);m->it[m->na++]=JARG(1);}}return th;}
JNAT(jm_has){JOb*m=jthism(th,12);if(!m)return jundef();return jbool(jmfind(m,JARG(0))>=0);}
JNAT(jm_delete){JOb*m=jthism(th,12);int f,st,i;if(!m)return jundef();st=(m->fl&4)?2:1;f=jmfind(m,JARG(0));if(f<0)return jbool(0);for(i=f;i+st<m->na;i++)m->it[i]=m->it[i+st];m->na-=st;return jbool(1);}
JNAT(jm_clear){JOb*m=jthism(th,12);if(!m)return jundef();m->na=0;(void)args;(void)argc;return jundef();}
JNAT(js_add){JOb*m=jthism(th,8);if(!m)return jundef();if(jmfind(m,JARG(0))<0){jarrgrow(m,m->na+1);if(m->na<m->ca)m->it[m->na++]=JARG(0);}return th;}
JNAT(jm_forEach){
 JOb*m=jthism(th,12),*a;int i;JV f=JARG(0);
 if(!m)return jundef();
 if(!jiscallable(f))return jthrowerr(jErrT,"callback is not a function");
 a=jiterable(th);
 for(i=0;i<a->na;i++){
  JV av[3];
  if(m->fl&4){av[0]=a->it[i].u.o->it[1];av[1]=a->it[i].u.o->it[0];}else{av[0]=a->it[i];av[1]=a->it[i];}
  av[2]=th;jcall(f,jundef(),av,3);if(jthrow)return jundef();}
 return jundef();}
JNAT(jm_keys){JOb*m=jthism(th,12),*r;int i,st;if(!m)return jundef();st=(m->fl&4)?2:1;r=jnewarr(8);for(i=0;i<m->na;i+=st)jarrpush(r,m->it[i]);(void)args;(void)argc;return jobj(r);}
JNAT(jm_values){JOb*m=jthism(th,12),*r;int i,st;if(!m)return jundef();st=(m->fl&4)?2:1;r=jnewarr(8);for(i=0;i<m->na;i+=st)jarrpush(r,m->it[i+st-1]);(void)args;(void)argc;return jobj(r);}
JNAT(jm_entries){JOb*m=jthism(th,12),*r;int i;if(!m)return jundef();(void)args;(void)argc;r=jiterable(th);if(m->fl&8){JOb*q=jnewarr(8);for(i=0;i<r->na;i++){JOb*p=jnewarr(2);jarrpush(p,r->it[i]);jarrpush(p,r->it[i]);jarrpush(q,jobj(p));}return jobj(q);}return jobj(r);}
static int jhex(int c){return"0123456789ABCDEF"[c&15];}
JNAT(jw_encodeURIComponent){
 JStr*s=jtos(JARG(0)),*acc;char*b;int i,n=0;JUNUSED;
 b=jalloc(s->len*3+4,BT_STR);if(!b)return jundef();
 for(i=0;i<s->len;i++){
  unsigned char c=(unsigned char)s->d[i];
  if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='!'||c=='~'||c=='*'||c=='\''||c=='('||c==')')b[n++]=(char)c;
  else{b[n++]='%';b[n++]=(char)jhex(c>>4);b[n++]=(char)jhex(c);}}
 acc=jnewstr(b,n);jhdr(b)->ty=BT_FREE;return jstrv(acc);}
JNAT(jw_decodeURIComponent){
 JStr*s=jtos(JARG(0));char*b;int i,n=0;JV r;JUNUSED;
 b=jalloc(s->len+4,BT_STR);if(!b)return jundef();
 for(i=0;i<s->len;i++){
  if(s->d[i]=='%'&&i+2<s->len&&hx(s->d[i+1])>=0&&hx(s->d[i+2])>=0){b[n++]=(char)(hx(s->d[i+1])*16+hx(s->d[i+2]));i+=2;}
  else b[n++]=s->d[i];}
 r=jsv(b,n);jhdr(b)->ty=BT_FREE;return r;}
/* Storage */
JNAT(jst_getItem){JStr*k=jtos(JARG(0));JPr*p;JUNUSED;if(th.t!=JT_O)return jnull();p=jfind(th.u.o,k);return p?p->v:jnull();}
JNAT(jst_setItem){JStr*k=jtos(JARG(0)),*v=jtos(JARG(1));JUNUSED;if(th.t==JT_O)jsetown(th.u.o,k,jstrv(v));return jundef();}
JNAT(jst_removeItem){JStr*k=jtos(JARG(0));JUNUSED;if(th.t==JT_O)jdelpr(th.u.o,k);return jundef();}
JNAT(jst_clear){JUNUSED;if(th.t==JT_O)th.u.o->np=0;return jundef();}
JNAT(jloc_assign){JUNUSED;jnavto(jtos(JARG(0)));return jundef();}
JNAT(jloc_reload){JUNUSED;jnavto(jcstr(jurl));return jundef();}
JNAT(jw_getComputedStyle){JOb*w;N*n=jargn(JARG(0));JUNUSED;if(!n)return jthrowerr(jErrT,"parameter 1 is not of type 'Element'");w=jnewob(OK_STYLE,jStyleProto);w->dn=n;w->fl=2;return jobj(w);}
JNAT(jw_structuredClone){JV a[1];JStr*s;JV sv=jrv;int sk;JUNUSED;(void)a;s=jstringify(JARG(0),&jemptys,&jemptys,0,&sk);if(jthrow||sk)return jundef();jjp=s->d;jjn=s->len;jji=0;jjerr=0;sv=jjval(0);return sv;}

/* ---- setup ---- */
static JOb*jctor(const char*name,JNF fn,JOb*proto){
 JOb*f=jmknat(name,fn);
 if(proto){jaddpr(f,K_prototype,jobj(proto),1);jaddpr(proto,K_constructor,jobj(f),1);}
 jaddpr(jGlob,jcstr(name),jobj(f),1);return f;}
static void jgset(const char*n,JV v){jaddpr(jGlob,jcstr(n),v,1);}
static JOb*jplain(void){return jnewob(OK_PLAIN,jObjProto);}
static void jconst(JOb*o,const char*n,double d){jaddpr(o,jcstr(n),jnum(d),3);}
static void jinit(void){
 int i;JOb*o,*f;
 jgcok=0;jingc=0;jallocd=0;jthresh=4L<<20;
#ifdef JGC_STRESS
 jthresh=JGC_STRESS;
#endif
 jlimit=JHEAP-(512<<10);
 for(i=0;i<(int)(sizeof jbm/sizeof jbm[0]);i++)jbm[i]=0;
 jtop=0;joom=0;for(i=0;i<130;i++)jfree[i]=0;jbig=0;
 jastp=0;for(i=0;i<JIH;i++)jih[i]=0;
 jnl=jnt=jnw=0;jtmid=1;jlogn=0;jlog[0]=0;jalertn=0;jnavn=0;jlevel=0;jdirty=0;
 jexv=jundef();jrv=jundef();jstorev[0]=jstorev[1]=jundef();
 jthrow=jabort=jshort=jdepth=0;
 for(i=0;i<8;i++)jroot2[i]=0;
 K_length=jintc("length");K_prototype=jintc("prototype");K_constructor=jintc("constructor");K_message=jintc("message");K_name=jintc("name");
 K_toString=jintc("toString");K_source=jintc("source");K_lastIndex=jintc("lastIndex");K_this=jintc("this");K_cls=jintc("%cls");K_args=jintc("arguments");K_valueOf=jintc("valueOf");K_proto=jintc("__proto__");K_stack=jintc("stack");
 jObjProto=jnewob(OK_PLAIN,0);
 jFnProto=jnewob(OK_PLAIN,jObjProto);jArrProto=jnewob(OK_PLAIN,jObjProto);jStrProto=jnewob(OK_PLAIN,jObjProto);
 jNumProto=jnewob(OK_PLAIN,jObjProto);jBoolProto=jnewob(OK_PLAIN,jObjProto);jErrProto=jnewob(OK_PLAIN,jObjProto);
 jDateProto=jnewob(OK_PLAIN,jObjProto);jNodeProto=jnewob(OK_PLAIN,jObjProto);jClsProto=jnewob(OK_PLAIN,jObjProto);
 jStyleProto=jnewob(OK_PLAIN,jObjProto);jDocProto=jnewob(OK_PLAIN,jObjProto);jEvProto=jnewob(OK_PLAIN,jObjProto);
 jRxProto=jnewob(OK_PLAIN,jObjProto);jMapProto=jnewob(OK_PLAIN,jObjProto);jSetProto=jnewob(OK_PLAIN,jObjProto);jLocProto=jnewob(OK_PLAIN,jObjProto);jStoreProto=jnewob(OK_PLAIN,jObjProto);
 jErrT=jnewob(OK_PLAIN,jErrProto);jErrR=jnewob(OK_PLAIN,jErrProto);jErrS=jnewob(OK_PLAIN,jErrProto);jErrF=jnewob(OK_PLAIN,jErrProto);
 jroot2[0]=jMapProto;jroot2[1]=jSetProto;jroot2[2]=jLocProto;jroot2[3]=jStoreProto;jroot2[4]=jEvProto;jroot2[7]=jRxProto;
 jGlob=jnewob(OK_PLAIN,jObjProto);
 jDoc=jnewob(OK_DOC,jDocProto);jDoc->dn=root;
 jLoc=jnewob(OK_DOC,jLocProto);jLoc->fl=1;
 jroot2[5]=jDoc;jroot2[6]=jLoc;
 /* globals */
 jgset("window",jobj(jGlob));jgset("self",jobj(jGlob));jgset("globalThis",jobj(jGlob));jgset("top",jobj(jGlob));jgset("parent",jobj(jGlob));
 jgset("document",jobj(jDoc));jgset("location",jobj(jLoc));
 jaddpr(jGlob,K_this,jobj(jGlob),1);
 jgset("undefined",jundef());jgset("NaN",jnum(jNAN()));jgset("Infinity",jnum(jINF()));
 /* Object */
 f=jctor("Object",jf_Object,jObjProto);
 jdef(f,"keys",jf_keys);jdef(f,"values",jf_values);jdef(f,"entries",jf_entries);jdef(f,"assign",jf_assign);jdef(f,"freeze",jf_freeze);
 jdef(f,"create",jf_create);jdef(f,"getPrototypeOf",jf_getproto);jdef(f,"defineProperty",jf_defprop);jdef(f,"hasOwn",jf_hasOwn);
 jdef(jObjProto,"hasOwnProperty",jfp_hasOwnProperty);jdef(jObjProto,"toString",jfp_objToString);jdef(jObjProto,"valueOf",jfp_valueOf);
 /* Function */
 f=jctor("Function",jw_noop,jFnProto);(void)f;
 jdef(jFnProto,"call",jfp_call);jdef(jFnProto,"apply",jfp_apply);jdef(jFnProto,"bind",jfp_bind);
 /* Errors */
 f=jctor("Error",jf_Error,jErrProto);jdef(jErrProto,"toString",jfp_errToString);
 jaddpr(jErrProto,K_name,jcv("Error"),1);jaddpr(jErrProto,K_message,jcv(""),1);
 f=jctor("TypeError",jf_TypeError,jErrT);jaddpr(jErrT,K_name,jcv("TypeError"),1);
 f=jctor("RangeError",jf_RangeError,jErrR);jaddpr(jErrR,K_name,jcv("RangeError"),1);
 f=jctor("SyntaxError",jf_SyntaxError,jErrS);jaddpr(jErrS,K_name,jcv("SyntaxError"),1);
 f=jctor("ReferenceError",jf_RefError,jErrF);jaddpr(jErrF,K_name,jcv("ReferenceError"),1);
 /* Array */
 f=jctor("Array",jf_Array,jArrProto);jdef(f,"isArray",jf_isArray);jdef(f,"from",jf_from);jdef(f,"of",jf_of);
 o=jArrProto;
 jdef(o,"push",jfa_push);jdef(o,"pop",jfa_pop);jdef(o,"shift",jfa_shift);jdef(o,"unshift",jfa_unshift);jdef(o,"slice",jfa_slice);jdef(o,"splice",jfa_splice);
 jdef(o,"concat",jfa_concat);jdef(o,"join",jfa_join);jdef(o,"toString",jfa_toString);jdef(o,"reverse",jfa_reverse);jdef(o,"indexOf",jfa_indexOf);jdef(o,"lastIndexOf",jfa_lastIndexOf);
 jdef(o,"includes",jfa_includes);jdef(o,"forEach",jfa_forEach);jdef(o,"map",jfa_map);jdef(o,"filter",jfa_filter);jdef(o,"find",jfa_find);jdef(o,"findIndex",jfa_findIndex);
 jdef(o,"some",jfa_some);jdef(o,"every",jfa_every);jdef(o,"reduce",jfa_reduce);jdef(o,"reduceRight",jfa_reduceRight);jdef(o,"findLast",jfa_findLast);jdef(o,"findLastIndex",jfa_findLastIndex);jdef(o,"fill",jfa_fill);jdef(o,"flat",jfa_flat);jdef(o,"sort",jfa_sort);jdef(o,"at",jfa_at);
 /* String */
 f=jctor("String",jf_String,jStrProto);jdef(f,"fromCharCode",jf_fromCharCode);
 o=jStrProto;
 jdef(o,"charAt",jfs_charAt);jdef(o,"charCodeAt",jfs_charCodeAt);jdef(o,"codePointAt",jfs_charCodeAt);jdef(o,"indexOf",jfs_indexOf);jdef(o,"lastIndexOf",jfs_lastIndexOf);
 jdef(o,"includes",jfs_includes);jdef(o,"startsWith",jfs_startsWith);jdef(o,"endsWith",jfs_endsWith);jdef(o,"slice",jfs_slice);jdef(o,"substring",jfs_substring);jdef(o,"substr",jfs_substr);
 jdef(o,"toUpperCase",jfs_upper);jdef(o,"toLowerCase",jfs_lower);jdef(o,"trim",jfs_trim);jdef(o,"trimStart",jfs_trimStart);jdef(o,"trimEnd",jfs_trimEnd);
 jdef(o,"split",jfs_split);jdef(o,"replace",jfs_replace);jdef(o,"replaceAll",jfs_replaceAll);jdef(o,"repeat",jfs_repeat);jdef(o,"padStart",jfs_pad);jdef(o,"padEnd",jfs_padEnd);
 jdef(o,"concat",jfs_concat);jdef(o,"match",jfs_match);jdef(o,"matchAll",jfs_matchAll);jdef(o,"search",jfs_search);jdef(o,"toString",jfs_toString);jdef(o,"valueOf",jfs_toString);jdef(o,"at",jfs_at);jdef(o,"localeCompare",jfs_localeCompare);
 /* Number / Boolean */
 f=jctor("Number",jf_Number,jNumProto);
 jdef(f,"isInteger",jf_nIsInteger);jdef(f,"isFinite",jf_nIsFinite);jdef(f,"isNaN",jf_nIsNaN);jdef(f,"parseFloat",jf_parseFloat);jdef(f,"parseInt",jf_parseInt);
 jconst(f,"MAX_SAFE_INTEGER",9007199254740991.0);jconst(f,"MIN_SAFE_INTEGER",-9007199254740991.0);jconst(f,"EPSILON",2.220446049250313e-16);
 jconst(f,"MAX_VALUE",1.7976931348623157e308);jconst(f,"MIN_VALUE",5e-324);jconst(f,"POSITIVE_INFINITY",jINF());jconst(f,"NEGATIVE_INFINITY",-jINF());jconst(f,"NaN",jNAN());
 jdef(jNumProto,"toString",jfn_toString);jdef(jNumProto,"toFixed",jfn_toFixed);jdef(jNumProto,"toPrecision",jfn_toPrecision);jdef(jNumProto,"toExponential",jfn_toExponential);jdef(jNumProto,"valueOf",jfn_valueOf);jdef(jNumProto,"toLocaleString",jfn_toLocaleString);
 f=jctor("Boolean",jf_Boolean,jBoolProto);(void)f;jdef(jBoolProto,"toString",jfb_toString);jdef(jBoolProto,"valueOf",jfp_valueOf);
 jdef(jGlob,"parseFloat",jf_parseFloat);jdef(jGlob,"parseInt",jf_parseInt);jdef(jGlob,"isNaN",jf_isNaN);jdef(jGlob,"isFinite",jf_isFinite);
 jdef(jGlob,"encodeURIComponent",jw_encodeURIComponent);jdef(jGlob,"decodeURIComponent",jw_decodeURIComponent);
 jdef(jGlob,"encodeURI",jw_encodeURIComponent);jdef(jGlob,"decodeURI",jw_decodeURIComponent);jdef(jGlob,"structuredClone",jw_structuredClone);
 /* Math */
 o=jplain();jgset("Math",jobj(o));
 jconst(o,"PI",JPI);jconst(o,"E",2.718281828459045);jconst(o,"LN2",0.6931471805599453);jconst(o,"LN10",2.302585092994046);jconst(o,"SQRT2",1.4142135623730951);
 jdef(o,"abs",jm_abs);jdef(o,"floor",jm_floor);jdef(o,"ceil",jm_ceil);jdef(o,"round",jm_round);jdef(o,"trunc",jm_trunc);jdef(o,"sign",jm_sign);jdef(o,"sqrt",jm_sqrt);jdef(o,"pow",jm_pow);
 jdef(o,"random",jm_random);jdef(o,"sin",jm_sin);jdef(o,"cos",jm_cos);jdef(o,"tan",jm_tan);jdef(o,"atan",jm_atan);jdef(o,"atan2",jm_atan2);jdef(o,"exp",jm_exp);jdef(o,"log",jm_log);
 jdef(o,"log2",jm_log2);jdef(o,"log10",jm_log10);jdef(o,"hypot",jm_hypot);jdef(o,"max",jm_max);jdef(o,"min",jm_min);
 o=jplain();jgset("JSON",jobj(o));jdef(o,"stringify",jj_stringify);jdef(o,"parse",jj_parse);
 f=jctor("Date",jf_Date,jDateProto);jdef(f,"now",jf_dateNow);
 o=jDateProto;jdef(o,"getTime",jfd_getTime);jdef(o,"valueOf",jfd_getTime);jdef(o,"getFullYear",jfd_getFullYear);jdef(o,"getMonth",jfd_getMonth);jdef(o,"getDate",jfd_getDate);jdef(o,"getDay",jfd_getDay);
 jdef(o,"getHours",jfd_getHours);jdef(o,"getMinutes",jfd_getMinutes);jdef(o,"getSeconds",jfd_getSeconds);jdef(o,"getMilliseconds",jfd_getMilliseconds);
 jdef(o,"getUTCFullYear",jfd_getFullYear);jdef(o,"getUTCMonth",jfd_getMonth);jdef(o,"getUTCDate",jfd_getDate);jdef(o,"getUTCHours",jfd_getHours);jdef(o,"getUTCMinutes",jfd_getMinutes);jdef(o,"getUTCSeconds",jfd_getSeconds);
 jdef(o,"toISOString",jfd_toISOString);jdef(o,"toString",jfd_toString);jdef(o,"toJSON",jfd_toISOString);
 f=jctor("Map",jf_Map,jMapProto);f=jctor("WeakMap",jf_Map,jMapProto);
 jdef(jMapProto,"get",jm_get);jdef(jMapProto,"set",jm_set);jdef(jMapProto,"has",jm_has);jdef(jMapProto,"delete",jm_delete);jdef(jMapProto,"clear",jm_clear);
 jdef(jMapProto,"forEach",jm_forEach);jdef(jMapProto,"keys",jm_keys);jdef(jMapProto,"values",jm_values);jdef(jMapProto,"entries",jm_entries);
 f=jctor("Set",jf_Set,jSetProto);f=jctor("WeakSet",jf_Set,jSetProto);
 jdef(jSetProto,"add",js_add);jdef(jSetProto,"has",jm_has);jdef(jSetProto,"delete",jm_delete);jdef(jSetProto,"clear",jm_clear);
 jdef(jSetProto,"forEach",jm_forEach);jdef(jSetProto,"keys",jm_values);jdef(jSetProto,"values",jm_values);jdef(jSetProto,"entries",jm_entries);
 jctor("RegExp",jf_RegExp,jRxProto);jdef(jRxProto,"test",jrx_test);jdef(jRxProto,"exec",jrx_exec);jdef(jRxProto,"toString",jrx_toString);
 /* console, misc */
 o=jplain();jgset("console",jobj(o));jdef(o,"log",jc_log);jdef(o,"info",jc_log);jdef(o,"debug",jc_log);jdef(o,"warn",jc_warn);jdef(o,"error",jc_error);
 o=jplain();jgset("performance",jobj(o));jdef(o,"now",jf_perfNow);
 o=jplain();jgset("navigator",jobj(o));jsetc(o,"userAgent",jcv("Mozilla/5.0 (compatible; MiniBrowser) MiniWebEngine"));jsetc(o,"language",jcv("en-US"));jsetc(o,"platform",jcv("MiniWin"));jsetc(o,"onLine",jbool(1));
 o=jplain();jgset("screen",jobj(o));jsetc(o,"width",jnum(1024));jsetc(o,"height",jnum(768));
 o=jplain();jgset("history",jobj(o));jdef(o,"pushState",jw_noop);jdef(o,"replaceState",jw_noop);jdef(o,"back",jw_noop);jdef(o,"forward",jw_noop);jsetc(o,"length",jnum(1));
 o=jnewob(OK_PLAIN,jStoreProto);jgset("localStorage",jobj(o));o=jnewob(OK_PLAIN,jStoreProto);jgset("sessionStorage",jobj(o));
 jdef(jStoreProto,"getItem",jst_getItem);jdef(jStoreProto,"setItem",jst_setItem);jdef(jStoreProto,"removeItem",jst_removeItem);jdef(jStoreProto,"clear",jst_clear);
 jdef(jLocProto,"assign",jloc_assign);jdef(jLocProto,"replace",jloc_assign);jdef(jLocProto,"reload",jloc_reload);
 jdef(jGlob,"alert",jw_alert);jdef(jGlob,"confirm",jw_confirm);jdef(jGlob,"prompt",jw_prompt);
 jdef(jGlob,"setTimeout",jw_setTimeout);jdef(jGlob,"setInterval",jw_setInterval);jdef(jGlob,"clearTimeout",jw_clearTimeout);jdef(jGlob,"clearInterval",jw_clearTimeout);
 jdef(jGlob,"requestAnimationFrame",jw_raf);jdef(jGlob,"cancelAnimationFrame",jw_clearTimeout);jdef(jGlob,"queueMicrotask",jw_setTimeout);
 jdef(jGlob,"scrollTo",jw_noop);jdef(jGlob,"scroll",jw_noop);jdef(jGlob,"getComputedStyle",jw_getComputedStyle);
 jdef(jGlob,"addEventListener",jN_addEL);jdef(jGlob,"removeEventListener",jN_removeEL);jdef(jGlob,"dispatchEvent",jN_dispatch);
 jctor("Event",jf_Event,jEvProto);jctor("CustomEvent",jf_Event,jEvProto);
 jdef(jEvProto,"preventDefault",jE_prevent);jdef(jEvProto,"stopPropagation",jE_stop);jdef(jEvProto,"stopImmediatePropagation",jE_stopi);
 /* DOM prototypes */
 o=jNodeProto;
 jdef(o,"getAttribute",jN_getAttribute);jdef(o,"setAttribute",jN_setAttribute);jdef(o,"removeAttribute",jN_removeAttribute);jdef(o,"hasAttribute",jN_hasAttribute);jdef(o,"toggleAttribute",jN_toggleAttribute);
 jdef(o,"appendChild",jN_appendChild);jdef(o,"insertBefore",jN_insertBefore);jdef(o,"removeChild",jN_removeChild);jdef(o,"replaceChild",jN_replaceChild);
 jdef(o,"append",jN_append);jdef(o,"prepend",jN_prepend);jdef(o,"before",jN_before);jdef(o,"after",jN_after);jdef(o,"remove",jN_remove);jdef(o,"replaceWith",jN_replaceWith);
 jdef(o,"cloneNode",jN_cloneNode);jdef(o,"contains",jN_contains);jdef(o,"hasChildNodes",jN_hasChildNodes);
 jdef(o,"querySelector",jN_qs);jdef(o,"querySelectorAll",jN_qsa);jdef(o,"matches",jN_matches);jdef(o,"closest",jN_closest);
 jdef(o,"getElementsByTagName",jN_byTag);jdef(o,"getElementsByClassName",jN_byClass);jdef(o,"insertAdjacentHTML",jN_insertAdj);
 jdef(o,"addEventListener",jN_addEL);jdef(o,"removeEventListener",jN_removeEL);jdef(o,"dispatchEvent",jN_dispatch);jdef(o,"click",jN_click);
 jdef(o,"focus",jw_noop);jdef(o,"blur",jw_noop);jdef(o,"scrollIntoView",jw_noop);jdef(o,"getBoundingClientRect",jN_rect);
 o=jDocProto;
 jdef(o,"getElementById",jD_byId);jdef(o,"createElement",jD_createElement);jdef(o,"createTextNode",jD_createText);jdef(o,"createDocumentFragment",jD_createFrag);
 jdef(o,"querySelector",jN_qs);jdef(o,"querySelectorAll",jN_qsa);jdef(o,"getElementsByTagName",jN_byTag);jdef(o,"getElementsByClassName",jN_byClass);
 jdef(o,"addEventListener",jN_addEL);jdef(o,"removeEventListener",jN_removeEL);jdef(o,"dispatchEvent",jN_dispatch);jdef(o,"write",jD_write);jdef(o,"writeln",jD_write);
 jdef(o,"appendChild",jN_appendChild);jdef(o,"hasFocus",jw_confirm);
 o=jStyleProto;jdef(o,"setProperty",jS_setProperty);jdef(o,"getPropertyValue",jS_getPropertyValue);jdef(o,"removeProperty",jS_removeProperty);
 o=jClsProto;jdef(o,"add",jC_add);jdef(o,"remove",jC_remove);jdef(o,"contains",jC_contains);jdef(o,"toggle",jC_toggle);jdef(o,"replace",jC_replace);jdef(o,"item",jC_item);
 jready=1;jgcok=jstackbase!=0;}

/* ---- host API ---- */
#if defined(__GNUC__)
#define JAPI if(!jlevel){jstackbase=(char*)__builtin_frame_address(0)+256;jgcok=1;}
#else
#define JAPI volatile char jmk_[1];if(!jlevel){jstackbase=(char*)(unsigned long)((unsigned long)jmk_+512);jgcok=1;}
#endif
static int jscript_ok(N*e){
 S t=getattr(e,"type");int i,j;
 const char*ok[]={"javascript","ecmascript","module",0};
 if(!t.p||!t.n)return 1;
 for(j=0;ok[j];j++){int l=0;while(ok[j][l])l++;for(i=0;i+l<=t.n;i++)if(seq((S){t.p+i,l},ok[j]))return 1;}
 return 0;}
static void jrecscript(N*e){
 S sr;
 if(jnscr>=JMAXSCR||!jscript_ok(e))return;
 sr=getattr(e,"src");
 jscr[jnscr].nd=e;jscr[jnscr].ext=0;jscr[jnscr].src=(S){0,0};
 if(sr.p&&sr.n){jscr[jnscr].ext=1;jscr[jnscr].src=dec(sr);}
 jnscr++;}
static void jrunsrc(const char*src,int n,const char*what){
 JNd*prog,*vn;int c;
 jsteps=0;jdepth=0;jthrow=0;jabort=0;
 prog=jparse(src,n);
 if(jpfail){
  char b[160];int k=0,i;const char*t="SyntaxError: ";
  for(i=0;t[i];i++)b[k++]=t[i];
  for(i=0;jperrm[i]&&k<120;i++)b[k++]=jperrm[i];
  b[k++]=' ';b[k++]='(';for(i=0;what[i]&&k<140;i++)b[k++]=what[i];b[k++]=':';k+=jd2s((double)jperrl,b+k);b[k++]=')';b[k++]='\n';
  jlogs(b,k);return;}
 for(vn=prog->c;vn;vn=vn->nx)if(!jfind(jGlob,vn->s))jaddpr(jGlob,vn->s,jundef(),0);
 jhoist(prog->b->a,jGlob);
 c=jexeclist(prog->b->a,jGlob);
 if(c==JC_THROW||jthrow||jabort)jreport();}
int mwe_js_run(void){
 int i,ei=0;JAPI
 if(!jnscr)return 0;
 jinit();
 jenter();
 for(i=0;i<jnscr;i++){
  const char*src=0;int n=0;char nm[16];int k=0;
  nm[k++]='s';nm[k++]='c';nm[k++]='r';nm[k++]='i';nm[k++]='p';nm[k++]='t';nm[k++]=' ';k+=jd2s((double)(i+1),nm+k);nm[k]=0;
  if(jscr[i].ext){if(ei<JMAXSCR){src=jextp[ei];n=jextn[ei];}ei++;if(!src)continue;}
  else{jsern=0;jn_text(jscr[i].nd);src=jserb;n=jsern;}
  if(!n)continue;
  jrunsrc(src,n,nm);
  if(joom||jtop>(2<<20)){jgc();joom=0;}
  }
 {JOb*e=jmkev("DOMContentLoaded",1);jfire(root,jcstr("DOMContentLoaded"),e,1);}
 {JOb*e=jmkev("load",0);jfire(&jwinn,jcstr("load"),e,0);}
 {int d=jdirty;jleave();return d||jdirty;}}
int mwe_js_eval(const char*src,int len){ /* run extra script text (tests / host consoles); 1 = page changed */
 JAPI
 if(!jready)return 0;
 jdirty=0;jenter();jrunsrc(src,len,"eval");jleave();return jdirty;}
const char*mwe_script(int i,int*len){
 int k,e=0;
 for(k=0;k<jnscr;k++)if(jscr[k].ext){if(e==i){*len=jscr[k].src.n;return jscr[k].src.p;}e++;}
 *len=0;return 0;}
void mwe_add_js(int i,char*text,int len){if(i>=0&&i<JMAXSCR){jextp[i]=text;jextn[i]=len;}}
void mwe_js_time(long long unix_ms,long long mono_ms){jnowunix=unix_ms;jnowmono=mono_ms;jrng^=(ju64)unix_ms*2654435761ULL;if(!jrng)jrng=1;}
void mwe_js_url(const char*u){int i;for(i=0;u[i]&&i<(int)sizeof jurl-1;i++)jurl[i]=u[i];jurl[i]=0;}
const char*mwe_js_log(int*len){*len=jlogn;return jlog;}
void mwe_js_log_clear(void){jlogn=0;jlog[0]=0;}
const char*mwe_js_alert(int*len){
 static int pos;int n=0;
 if(!jalertn){pos=0;*len=0;return 0;}
 while(pos+n<jalertn&&jalertq[pos+n])n++;
 {const char*r=jalertq+pos;pos+=n+1;*len=n;if(pos>=jalertn){jalertn=0;pos=0;}return r;}}
const char*mwe_js_nav(int*len){if(!jnavn){*len=0;return 0;}*len=jnavn;jnavn=0;return jnavb;}
int mwe_js_tick(long long mono_ms){
 int d;JAPI
 if(!jready)return 0;
 jnowmono=mono_ms;jdirty=0;
 jenter();jruntimers(mono_ms);d=jdirty;jleave();
 return d||jdirty;}
int mwe_js_active(void){int i;if(!jready)return 0;for(i=0;i<jnt;i++)if(jtm[i].live)return 1;return 0;}
static N*jhit(int x,int y){
 int i;
 for(i=nc-1;i>=0;i--){
  Cmd*c=&cm[i];
  if(!c->on)continue;
  if(x>=c->x&&x<c->x+c->w&&y>=c->y&&y<c->y+c->h){
   N*n=c->on;while(n&&(n->gen||!n->tag.n)&&n->par)n=n->par;
   return n;}}
 return 0;}
static void jevxy(JOb*e,int x,int y){
 jsetc(e,"clientX",jnum(x));jsetc(e,"clientY",jnum(y));jsetc(e,"pageX",jnum(x));jsetc(e,"pageY",jnum(y));jsetc(e,"button",jnum(0));}
static int jprevented(JOb*e){return jtruthy(jget(jobj(e),jcstr("defaultPrevented")));}
int mwe_event_click(int x,int y){
 N*t=jhit(x,y);int fl=0,pv=0;JAPI
 if(!t)return 0;
 if(jready){
  JOb*e;
  jdirty=0;jenter();
  e=jmkev("mousedown",1);jevxy(e,x,y);jfire(t,jcstr("mousedown"),e,1);
  e=jmkev("mouseup",1);jevxy(e,x,y);jfire(t,jcstr("mouseup"),e,1);
  e=jmkev("click",1);jevxy(e,x,y);jfire(t,jcstr("click"),e,1);
  pv=jprevented(e);
  if(jdirty)fl|=1;
  jleave();}
 if(pv)fl|=2;
 else{
  N*in=t;
  while(in&&in!=root&&!(seq(in->tag,"input")))in=in->par;
  if(in&&in!=root){
   S ty=getattr(in,"type");
   if(ty.p&&seq(ty,"checkbox")){jn_setbool(in,"checked",!jbattr(in,"checked"));fl|=1;}
   else if(ty.p&&seq(ty,"radio")){
    S nm=getattr(in,"name");
    if(nm.p&&nm.n){N*stack[400];int sp=0;stack[sp++]=root;while(sp){N*q=stack[--sp],*k;if(q!=in&&q->tag.n&&seq(q->tag,"input")&&seqs(getattr(q,"name"),nm)&&jbattr(q,"checked"))jn_setbool(q,"checked",0);for(k=q->kid;k;k=k->nx)if(k->tag.n&&sp<399)stack[sp++]=k;}}
    jn_setbool(in,"checked",1);fl|=1;}
   if(fl&1&&jready){JOb*e;jenter();e=jmkev("input",1);jfire(in,jcstr("input"),e,1);e=jmkev("change",1);jfire(in,jcstr("change"),e,1);jleave();}}}
 return fl;}
int mwe_event_key(const char*type,const char*key){
 N*t;JOb*e;int fl=0;JAPI
 if(!jready)return 0;
 t=jn_find(root,"body");if(!t)t=root;
 jdirty=0;jenter();
 e=jmkev(type,1);jsetc(e,"key",jcv(key));jsetc(e,"code",jcv(key));
 {int kc=0;if(key[0]&&!key[1]){kc=(unsigned char)key[0];if(kc>='a'&&kc<='z')kc-=32;}else if(!jcl_(key)==0){if(jcl_(key)==5&&key[0]=='E')kc=13;else if(key[0]=='A'&&key[5]=='L')kc=37;else if(key[0]=='A'&&key[5]=='U')kc=38;else if(key[0]=='A'&&key[5]=='R')kc=39;else if(key[0]=='A'&&key[5]=='D')kc=40;else if(key[0]=='E'&&key[1]=='s')kc=27;}
  jsetc(e,"keyCode",jnum(kc));jsetc(e,"which",jnum(kc));}
 jfire(t,jcstr(type),e,1);
 if(jdirty)fl|=1;
 if(jprevented(e))fl|=2;
 jleave();
 return fl;}
