#include <stdlib.h>
#include "../../mwe.c"
#include <stdio.h>
static void*al_(unsigned long n){return calloc(1,n);}
int main(void){static char buf[4096];FILE*f=fopen("tests/layout/sticky.html","rb");int n=(int)fread(buf,1,sizeof buf-1,f);static unsigned px[400*200];int sy[]={0,30,100,300,640,700},i,j;
 mwe_set_alloc(al_,free);mwe_load(buf,n);int ph=mwe_layout(400);printf("doc height %d\n",ph);
 for(i=0;i<6;i++){mwe_paint(px,400,0,sy[i],400,200);int top=-1;for(j=0;j<200;j++)if(px[j*400+200]==0xff0000||(px[j*400+200]&0xffffff)==0xff0000){top=j;break;}printf("scroll %3d -> red bar top at viewport y=%d (doc y=%d)\n",sy[i],top,top<0?-1:top+sy[i]);}
 return 0;}
