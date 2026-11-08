/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniBrowser module: mb_str
 * string builder and URL helpers (resolve, scheme, escaping).
 * Not a standalone translation unit: minibrowser.c includes the modules in order (unity build). */
/* ---- tiny string builder ---- */
static char*bd_;static int bn_,bc_;
static void bs(char*d,int cap){bd_=d;bn_=0;bc_=cap;if(cap)d[0]=0;}
static void bn(const char*s,int n){while(n-->0&&*s&&bn_<bc_-1)bd_[bn_++]=*s++;bd_[bn_]=0;}
static void ba(const char*s){bn(s,1<<30);}
static void be(const char*s){ /* HTML-escaped append */
 for(;*s;s++){if(*s=='<')ba("&lt;");else if(*s=='>')ba("&gt;");else if(*s=='&')ba("&amp;");else bn(s,1);}}
static int len(const char*s){int n=0;while(s[n])n++;return n;}
static void cpy(char*d,int cap,const char*s){int i=0;while(s[i]&&i<cap-1){d[i]=s[i];i++;}d[i]=0;}
static int lw(int c){return c>='A'&&c<='Z'?c+32:c;}
static int has(const char*s,const char*p){while(*p){if(lw(*s)!=lw(*p))return 0;s++;p++;}return 1;}
static int hexv(int c){c=lw(c);return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;}
static int dig(int c){return c>='0'&&c<='9';}

/* ---- URLs ---- */
static void dots(char*s){
 char out[UC],tail[UC];int end=0,i=0,o=0,j,k,n;
 while(s[end]&&s[end]!='?'&&s[end]!='#')end++;
 cpy(tail,UC,s+end);
 while(i<end){
  j=i+1;while(j<end&&s[j]!='/')j++;
  n=j-i-1;
  if(n==1&&s[i+1]=='.'){}
  else if(n==2&&s[i+1]=='.'&&s[i+2]=='.'){while(o>0&&out[o-1]!='/')o--;if(o>0)o--;}
  else if(o<UC-2-n){out[o++]='/';for(k=0;k<n;k++)out[o++]=s[i+1+k];}
  i=j;}
 if(!o)out[o++]='/';
 out[o]=0;
 bs(s,UC);ba(out);ba(tail);}
static void resolve(const char*base,const char*ref,char*out,int cap){
 int sch=0,pe,qe,lp,h;char r[UC];
 if(has(ref,"http://")||has(ref,"https://")||has(ref,"file:")||has(ref,"about:")){cpy(out,cap,ref);return;}
 while(base[sch]&&!(base[sch]==':'&&base[sch+1]=='/'&&base[sch+2]=='/'))sch++;
 if(!base[sch]){cpy(out,cap,ref);return;}
 bs(r,sizeof r);
 pe=sch+3;while(base[pe]&&base[pe]!='/'&&base[pe]!='?'&&base[pe]!='#')pe++;
 qe=pe;while(base[qe]&&base[qe]!='?'&&base[qe]!='#')qe++;
 if(ref[0]=='#'){h=0;while(base[h]&&base[h]!='#')h++;bn(base,h);ba(ref);cpy(out,cap,r);return;}
 if(ref[0]=='/'&&ref[1]=='/'){bn(base,sch+1);ba(ref);cpy(out,cap,r);return;}
 bn(base,pe);
 if(ref[0]=='/')ba(ref);
 else if(ref[0]=='?'){bn(base+pe,qe-pe);ba(ref);}
 else{lp=qe;while(lp>pe&&base[lp-1]!='/')lp--;if(lp==pe)ba("/");else bn(base+pe,lp-pe);ba(ref);}
 dots(r+pe);
 cpy(out,cap,r);}
static void norm(const char*in,char*out,int cap){
 char t[UC];int n=0,i;
 while(*in==' ')in++;
 while(in[n]&&n<UC-1){t[n]=in[n];n++;}
 while(n&&t[n-1]==' ')n--;
 t[n]=0;
 if(!n){cpy(out,cap,"about:home");return;}
 if(has(t,"about:")||has(t,"http://")||has(t,"https://")||has(t,"file:")){cpy(out,cap,t);return;}
 bs(out,cap);
 if(((t[0]>='a'&&t[0]<='z')||(t[0]>='A'&&t[0]<='Z'))&&t[1]==':'&&(t[2]=='\\'||t[2]=='/')){
  ba("file:///");for(i=0;t[i];i++){char c=t[i]=='\\'?'/':t[i];bn(&c,1);}return;}
 if(t[0]=='/'||(t[0]=='.'&&(t[1]=='/'||t[1]=='.'))){ba("file://");ba(t);return;}
 ba("http://");ba(t);}
static void fpath(const char*url,char*out,int cap){
 const char*p=url+7;int o=0;
 if(p[0]=='/'&&p[1]&&p[2]==':')p++;
 while(*p&&*p!='?'&&*p!='#'&&o<cap-1){
  if(*p=='%'&&hexv(p[1])>=0&&hexv(p[2])>=0){out[o++]=hexv(p[1])*16+hexv(p[2]);p+=3;}else out[o++]=*p++;}
 out[o]=0;}
