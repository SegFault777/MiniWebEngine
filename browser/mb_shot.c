/* SPDX-License-Identifier: GPL-3.0-only */
/* Headless MiniBrowser driver: renders the full browser window to a BMP.
 * Used for CI smoke tests and screenshots (no window system, no network).
 * usage: mb_shot <url|path> out.bmp [width height] [--click X Y] */
#include <stdio.h>
#include "mb.h"
static unsigned px[1600*1200];
static unsigned char rb[1600*3+4];
int mb_plat_http(const char*h,int p,const char*r,int rl,char*o,int c){(void)h;(void)p;(void)r;(void)rl;(void)o;(void)c;return -1;}
void mb_plat_title(const char*s,int n){fprintf(stderr,"title: %.*s\n",n,s);}
void mb_plat_cursor(int h){(void)h;}
static void pl(unsigned char*p,int v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
static int num(const char*s){int v=0;while(*s>='0'&&*s<='9')v=v*10+*s++-'0';return v;}
int main(int ac,char**av){
 int w=1000,h=640,i,x,y,rs;unsigned char hd[54]={'B','M'};FILE*f;
 if(ac<3){fprintf(stderr,"usage: mb_shot url out.bmp [w h] [--click X Y]\n");return 1;}
 if(ac>=5&&av[3][0]!='-'){w=num(av[3]);h=num(av[4]);}
 if(w<200||w>1600)w=1000;
 if(h<200||h>1200)h=640;
 mb_init(av[1],w,h);
 for(i=3;i<ac;i++)if(av[i][0]=='-'&&i+2<ac){mb_move(num(av[i+1]),num(av[i+2]));mb_click(num(av[i+1]),num(av[i+2]));}
 mb_draw(px);
 if(!(f=fopen(av[2],"wb")))return 2;
 rs=(w*3+3)&~3;
 pl(hd+2,54+rs*h);pl(hd+10,54);pl(hd+14,40);pl(hd+18,w);pl(hd+22,-h);hd[26]=1;hd[28]=24;pl(hd+34,rs*h);
 fwrite(hd,1,54,f);
 for(y=0;y<h;y++){for(x=0;x<w;x++){unsigned c=px[y*w+x];rb[x*3]=c;rb[x*3+1]=c>>8;rb[x*3+2]=c>>16;}fwrite(rb,1,rs,f);}
 fclose(f);return 0;}
