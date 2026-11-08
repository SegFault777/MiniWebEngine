#include <stdio.h>
#include <stdlib.h>
#include "../../mwe_img.h"
int main(int ac,char**av){(void)ac;
 FILE*f=fopen(av[1],"rb");if(!f)return 3;static mu8 buf[1<<24];int n=(int)fread(buf,1,sizeof buf,f);fclose(f);
 int w=0,h=0,fmt=mi_probe(buf,n,&w,&h);
 if(!fmt){printf("UNSUPPORTED\n");return 2;}
 unsigned*px=malloc((size_t)w*h*4);
 if(!mi_decode(fmt,buf,n,px,w,h)){printf("FAIL fmt=%d\n",fmt);return 1;}
 printf("OK %d %d %d\n",fmt,w,h);
 FILE*o=fopen(av[2],"wb");fwrite(px,4,(size_t)w*h,o);fclose(o);return 0;}
