/* JS test driver: jt page.html [ops...]   ops: c:X,Y click | t:MS tick | k:key keydown | e:js eval | l relayout+print height */
#define MWE_LIB
#include "../mwe.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char buf[1<<20];
static void flush(void){int n;const char*l=mwe_js_log(&n);if(n)fwrite(l,1,n,stdout);mwe_js_log_clear();
 {const char*a;int al;while((a=mwe_js_alert(&al)))printf("[alert] %s\n",a);}
 {int nl;const char*u=mwe_js_nav(&nl);if(u)printf("[nav] %s\n",u);}}
int main(int ac,char**av){
 FILE*f=fopen(av[1],"rb");int n,i,h;long long now=0;
 if(!f)return 2;
 n=(int)fread(buf,1,sizeof buf-1,f);fclose(f);
 mwe_load(buf,n);mwe_js_time(1767225600000LL,0);mwe_js_url("file:///t/page.html");
 mwe_js_run();h=mwe_layout(800);flush();
 for(i=2;i<ac;i++){
  if(av[i][0]=='c'){int x,y,r;sscanf(av[i]+2,"%d,%d",&x,&y);r=mwe_event_click(x,y);printf("[click %d,%d -> %d]\n",x,y,r);if(r&1)h=mwe_layout(800);}
  else if(av[i][0]=='t'){now+=atoi(av[i]+2);if(mwe_js_tick(now))h=mwe_layout(800);}
  else if(av[i][0]=='k'){int r=mwe_event_key("keydown",av[i]+2);if(r&1)h=mwe_layout(800);}
  else if(av[i][0]=='e'){if(mwe_js_eval(av[i]+2,(int)strlen(av[i]+2)))h=mwe_layout(800);}
  else if(av[i][0]=='l'){h=mwe_layout(800);printf("[h=%d]\n",h);}
  flush();}
 return 0;}
