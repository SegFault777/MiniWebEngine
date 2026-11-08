/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine module: mwe_base
 * arena allocator, strings, UTF-8, font metrics.
 * Not a standalone translation unit: mwe.c includes the modules in dependency order (unity build). */
/* ---- arena (no malloc) ---- */
static long long ar[(1<<24)/8],dmy[64];
static int ap,full;
static void*al(int n){
 char*p;int i;
 n=(n+7)&~7;
 if(ap+n>(int)sizeof ar||n>(int)sizeof dmy*0+(1<<22)){full=1;p=(char*)dmy;n=(int)sizeof dmy;}
 else{p=(char*)ar+ap;ap+=n;}
 for(i=0;i<n;i++)p[i]=0;
 return p;}
static char scr[1<<17],cpool[1<<15];static int scp,cpp; /* per-layout / per-restyle scratch */
static char*sal(int n){char*p;if(scp+n>(int)sizeof scr)return scr;p=scr+scp;scp+=n;return p;}
static int VW=800,VH=800,rootfs=16;

/* ---- strings / utf-8 ---- */
static int lc(int c){return c>='A'&&c<='Z'?c+32:c;}
static int isws(int c){return c==' '||c=='\n'||c=='\t'||c=='\r'||c=='\f';}
static int isa(int c){c=lc(c);return c>='a'&&c<='z';}
static int isal(int c){c=lc(c);return isa(c)||(c>='0'&&c<='9')||c=='-'||c=='_';}
static int isdg(int c){return c>='0'&&c<='9';}
static int sl(const char*s){int n=0;while(s[n])n++;return n;}
static int seq(S a,const char*b){int i=0;for(;i<a.n;i++)if(!b[i]||lc(a.p[i])!=lc(b[i]))return 0;return!b[i];}
static int seqs(S a,S b){int i;if(a.n!=b.n)return 0;for(i=0;i<a.n;i++)if(lc(a.p[i])!=lc(b.p[i]))return 0;return 1;}
static int starts(S a,const char*b){int i=0;for(;b[i];i++)if(i>=a.n||lc(a.p[i])!=lc(b[i]))return 0;return 1;}
static S trim(S s){while(s.n&&isws(*s.p)){s.p++;s.n--;}while(s.n&&isws(s.p[s.n-1]))s.n--;return s;}
static int tk(S v,S*t,int mx){int n=0,i=0,a,pd;while(i<v.n&&n<mx){while(i<v.n&&isws(v.p[i]))i++;if(i>=v.n)break;a=i;pd=0;while(i<v.n&&(pd||!isws(v.p[i]))){if(v.p[i]=='(')pd++;else if(v.p[i]==')')pd--;i++;}t[n].p=v.p+a;t[n].n=i-a;n++;}return n;}
static int splitc(S v,S*t,int mx){int n=0,i=0,a=0,pd=0;if(!v.n)return 0;for(;i<=v.n&&n<mx;i++){if(i<v.n&&v.p[i]=='(')pd++;else if(i<v.n&&v.p[i]==')')pd--;else if(i==v.n||(v.p[i]==','&&!pd)){t[n++]=trim((S){v.p+a,i-a});a=i+1;}}return n;}
/* Strict UTF-8 decoder (RFC 3629 / Unicode "maximal subpart" replacement): overlong forms, surrogates,
 * values above U+10FFFF, stray continuation bytes and truncated sequences all yield U+FFFD, and only the
 * well-formed prefix of a bad sequence is consumed so decoding resynchronizes on the next lead byte. */
static int u8d(const char*p,int n,int*i){
 int c=(u8)p[*i],k,lo=0x80,hi=0xBF,m;
 (*i)++;
 if(c<0x80)return c;
 if(c<0xC2||c>0xF4)return 0xFFFD;
 if(c<0xE0){k=1;c&=0x1F;}
 else if(c<0xF0){k=2;c&=0x0F;if(c==0)lo=0xA0;else if(c==13)hi=0x9F;}
 else{k=3;c&=0x07;if(c==0)lo=0x90;else if(c==4)hi=0x8F;}
 for(m=0;m<k;m++){
  int b;
  if(*i>=n)return 0xFFFD;
  b=(u8)p[*i];
  if(b<lo||b>hi)return 0xFFFD;
  lo=0x80;hi=0xBF;
  c=c<<6|(b&63);(*i)++;}
 return c;}
static int put8(char*p,int c){
 if(c<0x80){p[0]=c;return 1;}
 if(c<0x800){p[0]=0xC0|c>>6;p[1]=0x80|(c&63);return 2;}
 if(c<0x10000){p[0]=0xE0|c>>12;p[1]=0x80|(c>>6&63);p[2]=0x80|(c&63);return 3;}
 p[0]=0xF0|c>>18;p[1]=0x80|(c>>12&63);p[2]=0x80|(c>>6&63);p[3]=0x80|(c&63);return 4;}
static int hx(int c){c=lc(c);return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;}

/* ---- font metrics: tf bit0 bold, bit1 italic, bits2-3 text-transform ---- */
#define ZWG (MF_N+MF_CJ_N)   /* pseudo glyph: no outline, no advance */
static int zwidth(int c){return c==0xAD||(c>=0x200B&&c<=0x200F)||(c>=0x202A&&c<=0x202E)||c==0x2060||c==0xFEFF||(c>=0xFE00&&c<=0xFE0F)||(c>=0x300&&c<=0x36F)||c<32||(c>=0x7F&&c<0xA0);}
static int gi(int cp){
 int lo,hi,m;
 if(cp>=32&&cp<=126)return cp-32;
 if(zwidth(cp))return ZWG;
 lo=95;hi=MF_N-1;
 while(lo<=hi){m=(lo+hi)>>1;if(MF_CP[m]==cp)return m;if(MF_CP[m]<cp)lo=m+1;else hi=m-1;}
#if MF_CJ_N
 lo=0;hi=MF_CJ_N-1;
 while(lo<=hi){m=(lo+hi)>>1;if(MF_CJ_CP[m]==cp)return MF_N+m;if(MF_CJ_CP[m]<cp)lo=m+1;else hi=m-1;}
#endif
 return '?'-32;}
static int ADV(int b,int g){
 if(g>=ZWG)return 0;
#if MF_CJ_N
 if(g>=MF_N)return MF_CJ_ADV[g-MF_N];
#endif
 return MF_ADV[b][g];}
/* line-breaking classes: CJK ideographs/kana/fullwidth break between characters (UAX#14 subset) */
static int iscjk(int c){return (c>=0x2E80&&c<=0xA4CF)||(c>=0xF900&&c<=0xFAFF)||(c>=0xFE30&&c<=0xFE4F)||(c>=0xFF00&&c<=0xFFEF)||(c>=0x20000&&c<=0x3FFFF);}
static int cjopen(int c){return c==0x3008||c==0x300A||c==0x300C||c==0x300E||c==0x3010||c==0x3014||c==0xFF08||c==0xFF3B||c==0xFF5B;}
static int cjclose(int c){return c==0x3001||c==0x3002||c==0x3009||c==0x300B||c==0x300D||c==0x300F||c==0x3011||c==0x3015||c==0xFF01||c==0xFF09||c==0xFF0C||c==0xFF0E||c==0xFF1A||c==0xFF1B||c==0xFF1F||c==0xFF3D||c==0xFF5D;}
/* end of the next unbreakable segment starting at i (within [i,e)) */
static int cjseg(const char*p,int i,int e,int wb){
 int q=i,r,c;
 if(wb==2)return e;
 c=u8d(p,e,&q);
 if(wb==1)return q;
 if(!iscjk(c)){
  for(;;){r=q;if(q>=e)return q;c=u8d(p,e,&q);if(iscjk(c))return r;}}
 if(cjopen(c)){for(;;){r=q;if(q>=e)return q;c=u8d(p,e,&q);if(!cjopen(c)){if(!iscjk(c))return r;break;}}}
 for(;;){r=q;if(q>=e)return q;c=u8d(p,e,&q);if(!cjclose(c))return r;}}
/* simple (1:1) case mapping for ASCII, Latin-1, Latin Extended-A, Greek and Cyrillic */
static int ucase(int c){
 if(c<0x80)return c>='a'&&c<='z'?c-32:c;
 if(c>=0xE0&&c<=0xFE&&c!=0xF7)return c-32;
 if(c==0xFF)return 0x178;
 if(c>=0x100&&c<=0x17F){
  if(c==0x131||c==0x138||c==0x149||c==0x17F||c==0x178)return c;
  if((c>=0x139&&c<=0x148)||(c>=0x179&&c<=0x17E))return(c&1)?c:c-1;
  return(c&1)?c-1:c;}
 if(c>=0x3B1&&c<=0x3C9)return c==0x3C2?0x3A3:c-32;
 if(c>=0x430&&c<=0x44F)return c-32;
 if(c>=0x450&&c<=0x45F)return c-80;
 return c;}
static int lcase(int c){
 if(c<0x80)return c>='A'&&c<='Z'?c+32:c;
 if(c>=0xC0&&c<=0xDE&&c!=0xD7)return c+32;
 if(c==0x178)return 0xFF;
 if(c>=0x100&&c<=0x17F){
  if(c==0x130||c==0x138||c==0x149||c==0x17F)return c;
  if((c>=0x139&&c<=0x148)||(c>=0x179&&c<=0x17E))return(c&1)?c+1:c;
  return(c&1)?c:c+1;}
 if(c>=0x391&&c<=0x3A9&&c!=0x3A2)return c+32;
 if(c>=0x410&&c<=0x42F)return c+32;
 if(c>=0x400&&c<=0x40F)return c+80;
 return c;}
static int tfm(int cp,int tt,int first){
 if(tt==1)return ucase(cp);
 if(tt==2)return lcase(cp);
 if(tt==3&&first)return ucase(cp);
 return cp;}
static int wid(const char*p,int n,int fs,int tf,int ls){
 int i=0,k=0;long long s=0;
 while(i<n){{int g_=gi(tfm(u8d(p,n,&i),tf>>2,k==0));s+=ADV(tf&1,g_);}k++;}
 s=(s*fs+512)>>10;s+=(long long)ls*k;
 return s>100000000?100000000:s<0?0:(int)s;}
