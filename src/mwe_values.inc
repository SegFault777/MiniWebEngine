/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine module: mwe_values
 * lengths, calc()/min()/max()/clamp(), units, colors.
 * Not a standalone translation unit: mwe.c includes the modules in dependency order (unity build). */
/* ---- lengths, calc(), units ---- */
static Len mk(int px,int pc,int k){Len l;l.px=px;l.pc=pc;l.k=k;return l;}
static S es;static int ei,efs,eerr;
static void esk(void){while(ei<es.n&&isws(es.p[ei]))ei++;}
static Len eadd(void);
static int unitmul(const char*u,int un,int*num,int*den){
 static const struct{const char*n;int a,b;}U[]={{"px",1,1},{"pt",4,3},{"pc",16,1},{"in",96,1},{"cm",378,10},{"mm",378,100},{"q",378,400},{"ch",-1,2},{"ex",-1,2},{"em",-1,1},{"rem",-2,1},{"vw",-3,100},{"vh",-4,100},{"vmin",-5,100},{"vmax",-6,100},{0,0,0}};
 int i,k;S s;s.p=(char*)u;s.n=un;
 for(i=0;U[i].n;i++)if(seq(s,U[i].n)){k=U[i].a;*den=U[i].b;
  *num=k==-1?efs:k==-2?rootfs:k==-3?VW:k==-4?VH:k==-5?(VW<VH?VW:VH):k==-6?(VW>VH?VW:VH):k;return 1;}
 return 0;}
static Len efac(void){
 Len r=mk(0,0,2),v[8];int a,neg=0,n,num,den,dg=0;long long iv=0,fr=0,fd=1,v1;S nm;
 esk();
 if(ei>=es.n){eerr=1;return r;}
 if(es.p[ei]=='('){ei++;r=eadd();esk();if(ei<es.n&&es.p[ei]==')')ei++;else eerr=1;return r;}
 if(isa(es.p[ei])){
  a=ei;while(ei<es.n&&isal(es.p[ei]))ei++;
  nm=(S){es.p+a,ei-a};esk();
  if(ei>=es.n||es.p[ei]!='('){eerr=1;return r;}
  ei++;n=0;
  for(;;){v[n++]=eadd();esk();if(ei<es.n&&es.p[ei]==','&&n<8){ei++;continue;}break;}
  if(ei<es.n&&es.p[ei]==')')ei++;else eerr=1;
  if(seq(nm,"calc"))return v[0];
  if(seq(nm,"min")||seq(nm,"max")){
   r=v[0];for(a=1;a<n;a++){if(v[a].pc||r.pc)continue;if(seq(nm,"min")?v[a].px<r.px:v[a].px>r.px)r=v[a];}return r;}
  if(seq(nm,"clamp")&&n==3){
   r=v[1];if(!v[0].pc&&!v[1].pc&&!v[2].pc){if(r.px<v[0].px)r=v[0];if(r.px>v[2].px)r=v[2];}return r;}
  eerr=1;return mk(0,0,2);}
 if(es.p[ei]=='-'){neg=1;ei++;}else if(es.p[ei]=='+')ei++;
 iv=0;
 while(ei<es.n&&isdg(es.p[ei])){if(iv<1000000)iv=iv*10+es.p[ei]-'0';ei++;dg=1;}
 if(ei<es.n&&es.p[ei]=='.'){ei++;while(ei<es.n&&isdg(es.p[ei])){if(fd<1000){fr=fr*10+es.p[ei]-'0';fd*=10;}ei++;dg=1;}}
 if(!dg){eerr=1;return r;}
 v1=iv*1000+fr*1000/fd;if(neg)v1=-v1;
 if(ei<es.n&&es.p[ei]=='%'){ei++;return mk(0,(int)(v1/1000),0);}
 a=ei;while(ei<es.n&&isa(es.p[ei]))ei++;
 if(ei==a)return mk((int)v1,0,3);
 if(!unitmul(es.p+a,ei-a,&num,&den)){eerr=1;return r;}
 v1=v1*num/(den*1000LL);
 if(v1>100000)v1=100000;
 v1=v1<-100000?-100000:v1;
 return mk((int)v1,0,0);}
static Len eterm(void){
 Len a=efac(),b;int op;
 for(;;){
  esk();
  if(ei>=es.n||(es.p[ei]!='*'&&es.p[ei]!='/'))return a;
  op=es.p[ei++];b=efac();
  if(a.k==2||b.k==2){a.k=2;continue;}
  if(op=='*'){
   if(a.k==3&&b.k==3)a.px=(int)((long long)a.px*b.px/1000);
   else if(a.k==3){b.px=(int)((long long)b.px*a.px/1000);b.pc=(int)((long long)b.pc*a.px/1000);a=b;}
   else if(b.k==3){a.px=(int)((long long)a.px*b.px/1000);a.pc=(int)((long long)a.pc*b.px/1000);}
   else a.k=2;}
  else{
   if(b.k!=3||b.px==0)a.k=2;
   else{a.px=(int)((long long)a.px*1000/b.px);a.pc=(int)((long long)a.pc*1000/b.px);}}}}
static Len eadd(void){
 Len a=eterm(),b;int op;
 for(;;){
  esk();
  if(ei>=es.n||(es.p[ei]!='+'&&es.p[ei]!='-'))return a;
  if(es.p[ei]=='-'&&ei+1<es.n&&!isws(es.p[ei+1])&&ei>0&&!isws(es.p[ei-1]))return a; /* "1px-2px" is not a subtraction */
  op=es.p[ei++];b=eterm();
  if(a.k==2||b.k==2||(a.k==3)!=(b.k==3)){a.k=2;continue;}
  a.px+=op=='+'?b.px:-b.px;a.pc+=op=='+'?b.pc:-b.pc;}}
static Len lenv(S v,int fs){
 Len r;
 v=trim(v);
 if(seq(v,"auto"))return mk(0,0,1);
 es=v;ei=0;efs=fs;eerr=0;
 r=eadd();esk();
 if(ei<es.n||eerr)r.k=2;
 return r;}
static int rl(Len l,int base){return l.k==0?l.px+(int)((long long)base*l.pc/100):0;}

/* ---- colors: 0xRRGGBB opaque, ((127-A/2)<<24)|RGB translucent, -1 transparent, -2 invalid, -3 currentColor ---- */
static const char CNN[]="aliceblue\0antiquewhite\0aqua\0aquamarine\0azure\0beige\0bisque\0black\0blanchedalmond\0blue\0blueviolet\0brown\0burlywood\0cadetblue\0chartreuse\0chocolate\0coral\0cornflowerblue\0cornsilk\0crimson\0cyan\0darkblue\0darkcyan\0darkgoldenrod\0darkgray\0darkgreen\0darkgrey\0darkkhaki\0darkmagenta\0darkolivegreen\0darkorange\0darkorchid\0darkred\0darksalmon\0darkseagreen\0darkslateblue\0darkslategray\0darkslategrey\0darkturquoise\0darkviolet\0deeppink\0deepskyblue\0dimgray\0dimgrey\0dodgerblue\0firebrick\0floralwhite\0forestgreen\0fuchsia\0gainsboro\0ghostwhite\0gold\0goldenrod\0gray\0green\0greenyellow\0grey\0honeydew\0hotpink\0indianred\0indigo\0ivory\0khaki\0lavender\0lavenderblush\0lawngreen\0lemonchiffon\0lightblue\0lightcoral\0lightcyan\0lightgoldenrodyellow\0lightgray\0lightgreen\0lightgrey\0lightpink\0lightsalmon\0lightseagreen\0lightskyblue\0lightslategray\0lightslategrey\0lightsteelblue\0lightyellow\0lime\0limegreen\0linen\0magenta\0maroon\0mediumaquamarine\0mediumblue\0mediumorchid\0mediumpurple\0mediumseagreen\0mediumslateblue\0mediumspringgreen\0mediumturquoise\0mediumvioletred\0midnightblue\0mintcream\0mistyrose\0moccasin\0navajowhite\0navy\0oldlace\0olive\0olivedrab\0orange\0orangered\0orchid\0palegoldenrod\0palegreen\0paleturquoise\0palevioletred\0papayawhip\0peachpuff\0peru\0pink\0plum\0powderblue\0purple\0rebeccapurple\0red\0rosybrown\0royalblue\0saddlebrown\0salmon\0sandybrown\0seagreen\0seashell\0sienna\0silver\0skyblue\0slateblue\0slategray\0slategrey\0snow\0springgreen\0steelblue\0tan\0teal\0thistle\0tomato\0turquoise\0violet\0wheat\0white\0whitesmoke\0yellow\0yellowgreen\0";
static const int CNV[]={0xF0F8FF,0xFAEBD7,0x00FFFF,0x7FFFD4,0xF0FFFF,0xF5F5DC,0xFFE4C4,0x000000,0xFFEBCD,0x0000FF,0x8A2BE2,0xA52A2A,0xDEB887,0x5F9EA0,0x7FFF00,0xD2691E,0xFF7F50,0x6495ED,0xFFF8DC,0xDC143C,0x00FFFF,0x00008B,0x008B8B,0xB8860B,0xA9A9A9,0x006400,0xA9A9A9,0xBDB76B,0x8B008B,0x556B2F,0xFF8C00,0x9932CC,0x8B0000,0xE9967A,0x8FBC8F,0x483D8B,0x2F4F4F,0x2F4F4F,0x00CED1,0x9400D3,0xFF1493,0x00BFFF,0x696969,0x696969,0x1E90FF,0xB22222,0xFFFAF0,0x228B22,0xFF00FF,0xDCDCDC,0xF8F8FF,0xFFD700,0xDAA520,0x808080,0x008000,0xADFF2F,0x808080,0xF0FFF0,0xFF69B4,0xCD5C5C,0x4B0082,0xFFFFF0,0xF0E68C,0xE6E6FA,0xFFF0F5,0x7CFC00,0xFFFACD,0xADD8E6,0xF08080,0xE0FFFF,0xFAFAD2,0xD3D3D3,0x90EE90,0xD3D3D3,0xFFB6C1,0xFFA07A,0x20B2AA,0x87CEFA,0x778899,0x778899,0xB0C4DE,0xFFFFE0,0x00FF00,0x32CD32,0xFAF0E6,0xFF00FF,0x800000,0x66CDAA,0x0000CD,0xBA55D3,0x9370DB,0x3CB371,0x7B68EE,0x00FA9A,0x48D1CC,0xC71585,0x191970,0xF5FFFA,0xFFE4E1,0xFFE4B5,0xFFDEAD,0x000080,0xFDF5E6,0x808000,0x6B8E23,0xFFA500,0xFF4500,0xDA70D6,0xEEE8AA,0x98FB98,0xAFEEEE,0xDB7093,0xFFEFD5,0xFFDAB9,0xCD853F,0xFFC0CB,0xDDA0DD,0xB0E0E6,0x800080,0x663399,0xFF0000,0xBC8F8F,0x4169E1,0x8B4513,0xFA8072,0xF4A460,0x2E8B57,0xFFF5EE,0xA0522D,0xC0C0C0,0x87CEEB,0x6A5ACD,0x708090,0x708090,0xFFFAFA,0x00FF7F,0x4682B4,0xD2B48C,0x008080,0xD8BFD8,0xFF6347,0x40E0D0,0xEE82EE,0xF5DEB3,0xFFFFFF,0xF5F5F5,0xFFFF00,0x9ACD32};
#define NCN 148
static int mkc(int r,int g,int b,int a){
 r=r<0?0:r>255?255:r;g=g<0?0:g>255?255:g;b=b<0?0:b>255?255:b;
 if(a<=0)return-1;
 if(a>=254)return(r<<16)|(g<<8)|b;
 return((127-(a>>1))<<24)|(r<<16)|(g<<8)|b; /* 7-bit alpha keeps the value non-negative */}
static int cargs(S v,int*o,int*pcf){ /* numbers in "(...)": o[0..3] value*1000, pcf bit i = percent */
 int i=0,n=0,neg,dg;long long iv,fr,fd;
 while(i<v.n&&v.p[i]!='(')i++;
 i++;*pcf=0;
 while(i<v.n&&n<4){
  while(i<v.n&&(isws(v.p[i])||v.p[i]==','||v.p[i]=='/'))i++;
  if(i>=v.n||v.p[i]==')')break;
  neg=0;iv=fr=0;fd=1;dg=0;
  if(v.p[i]=='-'){neg=1;i++;}else if(v.p[i]=='+')i++;
  while(i<v.n&&isdg(v.p[i])){if(iv<100000)iv=iv*10+v.p[i]-'0';i++;dg=1;}
  if(i<v.n&&v.p[i]=='.'){i++;while(i<v.n&&isdg(v.p[i])){if(fd<1000){fr=fr*10+v.p[i]-'0';fd*=10;}i++;dg=1;}}
  if(!dg)return-1;
  o[n]=(int)((iv*1000+fr*1000/fd)*(neg?-1:1));
  if(i<v.n&&v.p[i]=='%'){*pcf|=1<<n;i++;}
  else while(i<v.n&&isa(v.p[i]))i++; /* deg etc. */
  n++;}
 return n;}
static float fabs_(float x){return x<0?-x:x;}
static float hue(float p,float q,float t){if(t<0)t+=1;if(t>1)t-=1;if(t<1.0f/6)return p+(q-p)*6*t;if(t<0.5f)return q;if(t<2.0f/3)return p+(q-p)*(2.0f/3-t)*6;return p;}
static int clr(S v){
 int i,k,d,r=0,o[4],pf,n;const char*q;
 v=trim(v);
 if(!v.n)return-2;
 if(v.p[0]=='#'){
  for(i=1;i<v.n;i++)if(hx(v.p[i])<0)return-2;
  if(v.n==4||v.n==5){r=0;for(i=1;i<v.n;i++)r=r*16+hx(v.p[i]);
   d=v.n==5?(r&15)*17:255;if(v.n==5)r>>=4;
   return mkc(((r>>8)&15)*17,((r>>4)&15)*17,(r&15)*17,d);}
  if(v.n==7||v.n==9){for(i=1;i<7;i++)r=r*16+hx(v.p[i]);
   d=255;if(v.n==9)d=hx(v.p[7])*16+hx(v.p[8]);
   return mkc(r>>16,(r>>8)&255,r&255,d);}
  return-2;}
 if(starts(v,"rgb")){
  n=cargs(v,o,&pf);if(n<3)return-2;
  for(i=0;i<3;i++)o[i]=(int)((pf>>i&1)?(long long)o[i]*255/100000:o[i]/1000);
  d=255;if(n>3)d=(int)((pf>>3&1)?(long long)o[3]*255/100000:(long long)o[3]*255/1000);
  return mkc(o[0],o[1],o[2],d);}
 if(starts(v,"hsl")){
  float h,s,l,p,qq;
  n=cargs(v,o,&pf);if(n<3)return-2;
  h=(float)o[0]/1000/360;h-=(float)(int)h;if(h<0)h+=1;
  s=(float)o[1]/100000;l=(float)o[2]/100000;if(s>1)s=1;if(s<0)s=0;if(l>1)l=1;if(l<0)l=0;
  d=255;if(n>3)d=(int)((pf>>3&1)?(long long)o[3]*255/100000:(long long)o[3]*255/1000);
  qq=l<0.5f?l*(1+s):l+s-l*s;p=2*l-qq;
  return mkc((int)(hue(p,qq,h+1.0f/3)*255+0.5f),(int)(hue(p,qq,h)*255+0.5f),(int)(hue(p,qq,h-1.0f/3)*255+0.5f),d);}
 if(seq(v,"transparent"))return-1;
 if(seq(v,"currentcolor"))return-3;
 q=CNN;
 for(k=0;k<NCN;k++){if(seq(v,q))return CNV[k];q+=sl(q)+1;}
 (void)fabs_;
 return-2;}
static int calpha(int c){int h;if(c<0)return 0;h=(c>>24)&127;return h?(127-h)*2:255;}
