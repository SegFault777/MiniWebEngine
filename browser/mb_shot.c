/* SPDX-License-Identifier: GPL-3.0-only */
/* Headless MiniBrowser driver: renders the full browser window to a BMP.
 * Used for CI smoke tests and screenshots (no window system). On Unix it has real sockets,
 * so it can fetch http:// and https:// pages.
 * usage: mb_shot <url|path> out.bmp [width height] [--click X Y] */
#include <stdio.h>
#include "mb.h"
static unsigned px[1600*1200];
static unsigned char rb[1600*3+4];
#if defined(__unix__) || defined(__APPLE__)
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
int mb_plat_open(const char*host,int port){
 struct addrinfo hi,*res=0,*ai;char ps[16];int fd=-1;struct timeval tv;
 memset(&hi,0,sizeof hi);hi.ai_family=AF_UNSPEC;hi.ai_socktype=SOCK_STREAM;
 snprintf(ps,sizeof ps,"%d",port);
 if(getaddrinfo(host,ps,&hi,&res))return -1;
 tv.tv_sec=15;tv.tv_usec=0;
 for(ai=res;ai;ai=ai->ai_next){
  fd=socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol);
  if(fd<0)continue;
  setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof tv);setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof tv);
  if(connect(fd,ai->ai_addr,ai->ai_addrlen)==0)break;
  close(fd);fd=-1;}
 freeaddrinfo(res);return fd;}
int mb_plat_send(int h,const void*b,int n){return (int)send(h,b,(size_t)n,0);}
int mb_plat_recv(int h,void*b,int n){return (int)recv(h,b,(size_t)n,0);}
void mb_plat_close(int h){close(h);}
void mb_plat_random(unsigned char*b,int n){FILE*f=fopen("/dev/urandom","rb");if(!f||(int)fread(b,1,(size_t)n,f)!=n){fprintf(stderr,"no entropy source\n");_exit(9);}fclose(f);}
long long mb_plat_time(void){return (long long)time(0);}
#else
int mb_plat_open(const char*h,int p){(void)h;(void)p;return -1;}
int mb_plat_send(int h,const void*b,int n){(void)h;(void)b;(void)n;return -1;}
int mb_plat_recv(int h,void*b,int n){(void)h;(void)b;(void)n;return -1;}
void mb_plat_close(int h){(void)h;}
void mb_plat_random(unsigned char*b,int n){(void)b;(void)n;}
long long mb_plat_time(void){return 0;}
#endif
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
