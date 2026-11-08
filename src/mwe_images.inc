/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine module: mwe_images
 * <img> store, image sampling (decoders live in mwe_img.h).
 * Not a standalone translation unit: mwe.c includes the modules in dependency order (unity build). */
/* ---- images: <img src> decoded by mwe_img.h; the host supplies bytes for non-data: URLs ---- */
#define MAXIMG 64
typedef struct{S src;unsigned*px;int w,h,st;}Im;   /* st: 0 pending, 1 decoded, 2 failed */
static Im ims[MAXIMG];static int nims;
static unsigned ipool[1<<22];static int ipn;       /* decoded pixels, 16 MB */
static mu8 idat[1<<22];                              /* data: URI payload */
static int imset(int i,const unsigned char*d,int n){
 int w=0,h=0,f;
 if(i<0||i>=nims)return 0;
 ims[i].st=2;
 f=mi_probe(d,n,&w,&h);
 if(!f||(long long)w*h>(long long)((int)(sizeof ipool/sizeof ipool[0])-ipn))return 0;
 if(!mi_decode(f,d,n,ipool+ipn,w,h))return 0;
 ims[i].px=ipool+ipn;ims[i].w=w;ims[i].h=h;ims[i].st=1;ipn+=w*h;return 1;}
static int b64v(int c){return c>='A'&&c<='Z'?c-'A':c>='a'&&c<='z'?c-'a'+26:c>='0'&&c<='9'?c-'0'+52:c=='+'||c=='-'?62:c=='/'||c=='_'?63:-1;}
static int imdata(int i){   /* data:[mime];base64,PAYLOAD */
 S u=ims[i].src;int k=5,b64=0,o=0,acc=0,nb=0,v;
 while(k<u.n&&u.p[k]!=',')k++;
 if(k>=u.n)return 0;
 {int j;for(j=5;j+6<=k;j++)if(lc(u.p[j])==';'&&lc(u.p[j+1])=='b'&&lc(u.p[j+2])=='a'&&lc(u.p[j+3])=='s'&&lc(u.p[j+4])=='e'&&u.p[j+5]=='6')b64=1;}
 if(!b64)return 0;
 for(k++;k<u.n;k++){
  v=b64v((u.p[k]));
  if(v<0)continue;
  acc=acc<<6|v;nb+=6;
  if(nb>=8){nb-=8;if(o>=(int)sizeof idat)return 0;idat[o++]=(mu8)(acc>>nb);acc&=(1<<nb)-1;}}
 return imset(i,idat,o);}
static int imreg(S src){
 int i,j;
 for(i=0;i<nims;i++){if(ims[i].src.n==src.n){for(j=0;j<src.n&&ims[i].src.p[j]==src.p[j];j++){}if(j==src.n)return i;}}
 if(nims>=MAXIMG)return -1;
 ims[nims].src=src;ims[nims].px=0;ims[nims].w=ims[nims].h=0;ims[nims].st=0;i=nims++;
 if(src.n>5&&lc(src.p[0])=='d'&&lc(src.p[1])=='a'&&lc(src.p[2])=='t'&&lc(src.p[3])=='a'&&src.p[4]==':'){if(!imdata(i))ims[i].st=2;}
 return i;}
static int imfor(N*n){   /* index of the decoded image for an <img>, registering it on first sight; -1 if none (yet) */
 S src=getattr(n,"src");int i;
 if(!src.p||src.n<=0)return -1;
 i=imreg(src);
 return i>=0&&ims[i].st==1?i:-1;}
static void imscan(N*n){N*c;if(n->tag.n==3&&seq(n->tag,"img")){S src=getattr(n,"src");if(src.p&&src.n>0)imreg(src);}for(c=n->kid;c;c=c->nx)imscan(c);}
