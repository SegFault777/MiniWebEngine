/* SPDX-License-Identifier: GPL-3.0-only */
/* Headless MiniBrowser driver: renders the full browser window to a BMP.
 * Used for CI smoke tests and screenshots (no window system). On Unix it has real sockets,
 * so it can fetch http:// and https:// pages.
 * usage: mb_shot <url|path> out.bmp [width height] [--click X Y] [--wait MS] [--key C] (options run in order) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mb.h"
static unsigned*px;   /* framebuffer (1600x1200 at most), allocated in main */
static unsigned char rb[1600*3+4];
#if defined(__unix__) || defined(__APPLE__)
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
/* Non-blocking sockets: name resolution and connect() happen in a short-lived worker thread per connection
 * (getaddrinfo cannot be made non-blocking portably); after that the socket is O_NONBLOCK and the core polls it. */
#define NSK 64
typedef struct { int st, fd, port; char host[256]; } Sk;   /* st: 0 free, 1 connecting (worker running), 2 connected, 3 failed, 4 closed while connecting */
static Sk sk[NSK];
static pthread_mutex_t skmu = PTHREAD_MUTEX_INITIALIZER;
static void *skwork(void *a){
 Sk *s=a;struct addrinfo hi,*res=0,*ai;char ps[16];int fd=-1;
 memset(&hi,0,sizeof hi);hi.ai_family=AF_UNSPEC;hi.ai_socktype=SOCK_STREAM;
 snprintf(ps,sizeof ps,"%d",s->port);
 if(!getaddrinfo(s->host,ps,&hi,&res)){
  for(ai=res;ai;ai=ai->ai_next){
   int fl;
   fd=socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol);
   if(fd<0)continue;
   fl=fcntl(fd,F_GETFL,0);fcntl(fd,F_SETFL,fl|O_NONBLOCK);
   if(connect(fd,ai->ai_addr,ai->ai_addrlen)==0)break;
   if(errno==EINPROGRESS){
    struct pollfd p;int err=0;socklen_t el=sizeof err;
    p.fd=fd;p.events=POLLOUT;p.revents=0;
    if(poll(&p,1,8000)>0&&!getsockopt(fd,SOL_SOCKET,SO_ERROR,&err,&el)&&!err)break;}
   close(fd);fd=-1;}
  freeaddrinfo(res);}
 pthread_mutex_lock(&skmu);
 if(s->st==4){if(fd>=0)close(fd);s->st=0;}
 else if(fd>=0){s->fd=fd;s->st=2;}
 else s->st=3;
 pthread_mutex_unlock(&skmu);
 return 0;}
int mb_plat_connect(const char*host,int port){
 int i;pthread_t t;pthread_attr_t at;
 if(strlen(host)>=sizeof sk[0].host)return -1;
 pthread_mutex_lock(&skmu);
 for(i=0;i<NSK&&sk[i].st;i++){}
 if(i==NSK){pthread_mutex_unlock(&skmu);return -1;}
 sk[i].st=1;sk[i].fd=-1;sk[i].port=port;strcpy(sk[i].host,host);
 pthread_mutex_unlock(&skmu);
 pthread_attr_init(&at);pthread_attr_setdetachstate(&at,PTHREAD_CREATE_DETACHED);
 if(pthread_create(&t,&at,skwork,&sk[i])){sk[i].st=0;return -1;}
 return i;}
int mb_plat_connect_poll(int h){
 int r;
 if(h<0||h>=NSK)return -1;
 pthread_mutex_lock(&skmu);r=sk[h].st==2?1:sk[h].st==1?0:-1;pthread_mutex_unlock(&skmu);
 return r;}
int mb_plat_send(int h,const void*b,int n){
 int r;
 if(h<0||h>=NSK||sk[h].st!=2)return -1;
 r=(int)send(sk[h].fd,b,(size_t)n,MSG_NOSIGNAL);
 if(r<0)return errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR?0:-1;
 return r;}
int mb_plat_recv(int h,void*b,int n){
 int r;
 if(h<0||h>=NSK||sk[h].st!=2)return -1;
 r=(int)recv(sk[h].fd,b,(size_t)n,0);
 if(r<0)return errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR?-2:-1;
 return r;}
void mb_plat_close(int h){
 if(h<0||h>=NSK)return;
 pthread_mutex_lock(&skmu);
 if(sk[h].st==1)sk[h].st=4;
 else if(sk[h].st==2||sk[h].st==3){if(sk[h].fd>=0)close(sk[h].fd);sk[h].fd=-1;sk[h].st=0;}
 pthread_mutex_unlock(&skmu);}
void mb_plat_random(unsigned char*b,int n){FILE*f=fopen("/dev/urandom","rb");if(!f||(int)fread(b,1,(size_t)n,f)!=n){fprintf(stderr,"no entropy source\n");_exit(9);}fclose(f);}
long long mb_plat_time(void){return (long long)time(0);}
static void nap_(long us){usleep((useconds_t)us);}
void mb_plat_idle(int ms){nap_(ms*1000L);}
static int fail_after=-1,alloc_n;   /* tests: MB_FAIL_ALLOC=N makes the Nth and later allocations fail */
void*mb_plat_alloc(unsigned long n){if(fail_after<0){const char*e=getenv("MB_FAIL_ALLOC");fail_after=e?atoi(e):1<<30;}if(alloc_n++>=fail_after)return 0;return calloc(1,n);}
void mb_plat_free(void*p){free(p);}
static long long realms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (long long)t.tv_sec*1000+t.tv_nsec/1000000;}
#else
int mb_plat_connect(const char*h,int p){(void)h;(void)p;return -1;}
int mb_plat_connect_poll(int h){(void)h;return -1;}
int mb_plat_send(int h,const void*b,int n){(void)h;(void)b;(void)n;return -1;}
int mb_plat_recv(int h,void*b,int n){(void)h;(void)b;(void)n;return -1;}
void mb_plat_close(int h){(void)h;}
void mb_plat_random(unsigned char*b,int n){(void)b;(void)n;}
long long mb_plat_time(void){return 0;}
static void nap_(long us){(void)us;}
void mb_plat_idle(int ms){(void)ms;}
void*mb_plat_alloc(unsigned long n){return calloc(1,n);}
void mb_plat_free(void*p){free(p);}
static long long realms(void){static long long t;return t+=5;}
#endif
int mb_plat_readfile(const char*path,char*buf,int cap){FILE*f=fopen(path,"rb");int n;if(!f)return -1;n=(int)fread(buf,1,(size_t)cap,f);fclose(f);return n;}
long long mb_plat_ms(void){return realms();}
void mb_plat_alert(const char*s,int n){fprintf(stderr,"alert: %.*s\n",n,s);}
void mb_plat_log(const char*s,int n){fprintf(stderr,"console: %.*s",n,s);}
void mb_plat_title(const char*s,int n){fprintf(stderr,"title: %.*s\n",n,s);}
void mb_plat_cursor(int h){(void)h;}
static void pl(unsigned char*p,int v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
static int num(const char*s){int v=0;while(*s>='0'&&*s<='9')v=v*10+*s++-'0';return v;}
static long long maxtick;static int trace;
static void settle(long long ms){   /* tick like a window loop would until nothing is in flight (or ms passed) */
 long long e=realms()+ms;
 while(realms()<e){long long a=realms(),d;mb_tick();d=realms()-a;if(d>maxtick)maxtick=d;if(!mb_busy())break;nap_(2000);}
 mb_tick();}
int main(int ac,char**av){
 int w=1000,h=640,i,x,y,rs;unsigned char hd[54]={'B','M'};FILE*f;
 if(ac<3){fprintf(stderr,"usage: mb_shot url out.bmp [w h] [--click X Y]\n");return 1;}
 if(ac>=5&&av[3][0]!='-'){w=num(av[3]);h=num(av[4]);}
 if(w<200||w>1600)w=1000;
 if(h<200||h>1200)h=640;
#if defined(__unix__) || defined(__APPLE__)
 signal(SIGPIPE,SIG_IGN);
#endif
 px=(unsigned*)calloc((size_t)w*(size_t)h,4);
 if(!px){fprintf(stderr,"out of memory\n");return 1;}
 mb_init(av[1],w,h);
 {int e=-1;for(i=3;i<ac;i++)if(!strcmp(av[i],"--early")&&i+1<ac)e=num(av[i+1]);
  if(e>=0){long long t=realms()+e;while(realms()<t){mb_tick();nap_(2000);}}   /* snapshot of a half-loaded page */
  else settle(60000);}   /* let the page and its resources finish before the options run */
 for(i=3;i<ac;i++){
  if(av[i][0]=='-'&&av[i][1]=='-'&&av[i][2]=='c'&&i+2<ac){mb_move(num(av[i+1]),num(av[i+2]));mb_click(num(av[i+1]),num(av[i+2]));settle(60000);}
  else if(!strcmp(av[i],"--early")){i++;}
  else if(!strcmp(av[i],"--goto")&&i+1<ac){   /* type a new address while the page is still loading */
   const char*u=av[++i];mb_key(MBK_FOCUS_URL);mb_paste(u,(int)strlen(u));mb_key(MBK_ENTER);settle(60000);}
  else if(!strcmp(av[i],"--wait")&&i+1<ac){long long e=realms()+num(av[i+1]);while(realms()<e){mb_tick();nap_(4000);}}
  else if(!strcmp(av[i],"--key")&&i+1<ac){mb_char(av[i+1][0]);}
  else if(!strcmp(av[i],"--type")&&i+1<ac){const unsigned char*s=(const unsigned char*)av[++i];   /* type UTF-8 text */
   while(*s){int c=*s++;if(c>=0xF0){c=(c&7)<<18|(s[0]&63)<<12|(s[1]&63)<<6|(s[2]&63);s+=3;}else if(c>=0xE0){c=(c&15)<<12|(s[0]&63)<<6|(s[1]&63);s+=2;}else if(c>=0xC0){c=(c&31)<<6|(s[0]&63);s++;}mb_char(c);}
   settle(60000);}
  else if(!strcmp(av[i],"--press")&&i+1<ac){const char*k=av[++i];   /* named key */
   int c=!strcmp(k,"Enter")?MBK_ENTER:!strcmp(k,"Backspace")?MBK_BACKSPACE:!strcmp(k,"Delete")?MBK_DELETE:!strcmp(k,"Left")?MBK_LEFT:!strcmp(k,"Right")?MBK_RIGHT:!strcmp(k,"Home")?MBK_HOME:!strcmp(k,"End")?MBK_END:!strcmp(k,"Up")?MBK_UP:!strcmp(k,"Down")?MBK_DOWN:!strcmp(k,"Tab")?MBK_TAB:MBK_SHIFT_TAB;
   mb_key(c);settle(60000);}
  else if(!strcmp(av[i],"--trace")){trace=1;}}
 if(trace)fprintf(stderr,"trace: %s maxtick=%lldms\n",mb_summary(),maxtick);
 mb_draw(px);
 if(!(f=fopen(av[2],"wb")))return 2;
 rs=(w*3+3)&~3;
 pl(hd+2,54+rs*h);pl(hd+10,54);pl(hd+14,40);pl(hd+18,w);pl(hd+22,-h);hd[26]=1;hd[28]=24;pl(hd+34,rs*h);
 fwrite(hd,1,54,f);
 for(y=0;y<h;y++){for(x=0;x<w;x++){unsigned c=px[y*w+x];rb[x*3]=c;rb[x*3+1]=c>>8;rb[x*3+2]=c>>16;}fwrite(rb,1,rs,f);}
 fclose(f);return 0;}
