/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniBrowser module: mb_input
 * keyboard and mouse input.
 * Not a standalone translation unit: minibrowser.c includes the modules in order (unity build). */
/* ---- input ---- */
static int linkat(int x,int y,char*out){
 const char*h;int n;
 if(x<0||x>=Wd-SBW||y<TB||y>=Ht-SBH)return 0;
 h=mwe_link(x,y-TB+sy,&n);
 if(!h||n<=0||n>=UC)return 0;
 for(y=0;y<n;y++)out[y]=h[y];
 out[n]=0;
 resolve(cur,out,tmp,UC);cpy(out,UC,tmp);return 1;}
void mb_move(int x,int y){
 char u[UC];int h=linkat(x,y,u);
 if(h!=hov){hov=h;mb_plat_cursor(h);}
 if(h)cpy(stt,UC,u);else cpy(stt,UC,"Done");}
void mb_click(int x,int y){
 char u[UC];int chh=Ht-TB-SBH,mx,i,a,b;
 if(secpanel&&!(y<TB&&y>=8&&y<38&&x>=120&&badgew()&&x<120+10+badgew())){secpanel=0;if(y>=TB)return;}
 if(y<TB){
  if(y>=8&&y<38){
   if(x>=8&&x<38){hgo(-1);return;}
   if(x>=44&&x<74){hgo(1);return;}
   if(x>=80&&x<110){go(cur,0);return;}}
  if(x>=120&&x<Wd-10){
   if(badgew()&&x<120+10+badgew()){secpanel=!secpanel;return;}
   if(!uf){uf=1;usel=1;return;}
   usel=0;
   x-=130+badgew()-uoff;ucur=0;
   for(i=0;ub[i];){
    a=i;i++;while((ub[i]&0xC0)==0x80)i++;
    b=mwe_textw(ub,i,14,0);
    if(b>x+(b-mwe_textw(ub,a,14,0))/2)break;
    ucur=i;}
   return;}
  uf=0;return;}
 uf=0;
 if(x>=Wd-SBW&&y<Ht-SBH&&ph>chh){
  mx=ph-chh;sy=(y-TB)*mx/chh;if(sy<0)sy=0;if(sy>mx)sy=mx;return;}
 {int hl=linkat(x,y,u),jr=0;
  if(x<Wd-SBW&&y>=TB&&y<Ht-SBH){
   jr=mwe_event_click(x,y-TB+sy);
   if(jr&1){relayout();settitle();}
   {int l0=nloads;pump();if(nloads!=l0)return;}}
  if(jr&2)return;
  if(hl){if(u[0]=='#')return;go(u,1);}
  else if(x<Wd-SBW&&y>=TB&&y<Ht-SBH&&mwe_click(x,y-TB+sy))relayout();}
 return;}
void mb_wheel(int n){
 int mx=ph-(Ht-TB-SBH);
 sy-=n*60;if(sy>mx)sy=mx;if(sy<0)sy=0;}
static void ins(const char*s,int n){
 int l=len(ub),i;
 if(usel){ub[0]=0;l=0;ucur=0;usel=0;}
 if(l+n>=UC-1)return;
 for(i=l;i>=ucur;i--)ub[i+n]=ub[i];
 for(i=0;i<n;i++)ub[ucur+i]=s[i];
 ucur+=n;}
void mb_char(int c){
 char b[5];int n;
 if(!uf&&c>=32&&c!=127&&ready){
  if(c<0x80){b[0]=(char)c;n=1;}
  else if(c<0x800){b[0]=(char)(0xC0|c>>6);b[1]=(char)(0x80|(c&63));n=2;}
  else if(c<0x10000){b[0]=(char)(0xE0|c>>12);b[1]=(char)(0x80|(c>>6&63));b[2]=(char)(0x80|(c&63));n=3;}
  else{b[0]=(char)(0xF0|c>>18);b[1]=(char)(0x80|(c>>12&63));b[2]=(char)(0x80|(c>>6&63));b[3]=(char)(0x80|(c&63));n=4;}
  b[n]=0;
  if(mwe_event_key("keydown",b)&1){relayout();}
  pump();
  return;}
 if(!uf||c<32||c==127)return;
 if(c<0x80){b[0]=c;n=1;}
 else if(c<0x800){b[0]=0xC0|c>>6;b[1]=0x80|(c&63);n=2;}
 else if(c<0x10000){b[0]=0xE0|c>>12;b[1]=0x80|(c>>6&63);b[2]=0x80|(c&63);n=3;}
 else{b[0]=0xF0|c>>18;b[1]=0x80|(c>>12&63);b[2]=0x80|(c>>6&63);b[3]=0x80|(c&63);n=4;}
 ins(b,n);}
void mb_paste(const char*s,int n){
 char b[UC];int i,o=0;
 if(!uf)return;
 for(i=0;i<n&&o<UC-1;i++)if(s[i]!='\r'&&s[i]!='\n')b[o++]=s[i];
 ins(b,o);}
void mb_key(int k){
 int chh=Ht-TB-SBH,mx=ph-chh,l,i;
 if(secpanel&&k==MBK_ESC){secpanel=0;return;}
 if(k==MBK_FOCUS_URL){uf=1;usel=1;return;}
 if(k==MBK_BACK){hgo(-1);return;}
 if(k==MBK_FORWARD){hgo(1);return;}
 if(k==MBK_F5){go(cur,0);return;}
 if(uf){
  l=len(ub);
  if(k==MBK_ENTER){uf=0;usel=0;ufirst=1;go(ub,1);ufirst=0;}
  else if(k==MBK_ESC){uf=0;usel=0;cpy(ub,UC,cur);ucur=len(ub);}
  else if(k==MBK_BACKSPACE){
   if(usel){ub[0]=0;ucur=0;usel=0;}
   else if(ucur>0){i=ucur-1;while(i>0&&(ub[i]&0xC0)==0x80)i--;
    {int j;for(j=ucur;j<=l;j++)ub[i+j-ucur]=ub[j];}ucur=i;}}
  else if(k==MBK_DELETE){
   if(usel){ub[0]=0;ucur=0;usel=0;}
   else if(ucur<l){i=ucur+1;while(ub[i]&&(ub[i]&0xC0)==0x80)i++;
    {int j;for(j=i;j<=l;j++)ub[ucur+j-i]=ub[j];}}}
  else if(k==MBK_LEFT){usel=0;if(ucur>0){ucur--;while(ucur>0&&(ub[ucur]&0xC0)==0x80)ucur--;}}
  else if(k==MBK_RIGHT){usel=0;if(ucur<l){ucur++;while(ub[ucur]&&(ub[ucur]&0xC0)==0x80)ucur++;}}
  else if(k==MBK_HOME){usel=0;ucur=0;}
  else if(k==MBK_END){usel=0;ucur=l;}
  return;}
 if(ready){
  const char*kn=k==MBK_ENTER?"Enter":k==MBK_LEFT?"ArrowLeft":k==MBK_RIGHT?"ArrowRight":k==MBK_UP?"ArrowUp":k==MBK_DOWN?"ArrowDown":
   k==MBK_ESC?"Escape":k==MBK_BACKSPACE?"Backspace":k==MBK_DELETE?"Delete":k==MBK_HOME?"Home":k==MBK_END?"End":k==MBK_PGUP?"PageUp":k==MBK_PGDN?"PageDown":0;
  if(kn){int r=mwe_event_key("keydown",kn);if(r&1)relayout();pump();if(r&2)return;}}
 if(k==MBK_UP)sy-=48;else if(k==MBK_DOWN)sy+=48;
 else if(k==MBK_PGUP)sy-=chh-40;else if(k==MBK_PGDN)sy+=chh-40;
 else if(k==MBK_HOME)sy=0;else if(k==MBK_END)sy=mx;
 if(sy>mx)sy=mx;
 if(sy<0)sy=0;}
