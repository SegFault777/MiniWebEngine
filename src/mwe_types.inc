/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine module: mwe_types
 * shared constants and the S / Len / St / N types.
 * Not a standalone translation unit: mwe.c includes the modules in dependency order (unity build). */
#include "../mwe.h"
#include "mwe_font.h"
#include "mwe_img.h"
#ifdef MWE_NO_CJK
#undef MF_CJ_N
#define MF_CJ_N 0
#endif

typedef unsigned char u8;
typedef unsigned int u32;
#define AU (-32768)
#define MC 80000
#define MR 1024
#define IN (1<<20)

typedef struct{char*p;int n;}S;
typedef struct{int px,pc,k;}Len; /* px + pc% ; k: 0 length, 1 auto, 2 invalid, 3 plain number (px = value*1000) */
enum{D_NONE,D_BLOCK,D_INLINE,D_IB,D_X4,D_TABLE,D_ROW,D_CELL,D_LI,D_RG};
typedef struct{
 int disp,fx,gr,col,bg,fs,bold,it,ul,al,tt,ws,wb,ti,of,ls,va,vaa,lh,lst,vis,op,bs,ov,fl,clr,pos,bsp,bcol,fdir,fwrap,jc,ai,as,fgrow,fshr,ord,hascont,gtn,gtauto,gtmin,gcs,
     shx,shy,shb,shs,shc,gk,ga,gn,tsx,tsy,tsc;

 float ar;
 Len m[4],p[4],w,h,mnw,mxw,mnh,mxh,top,rt,bot,lf,rad[4],fb,gx,gy,gt[12];
 int gtk[12],bw[4],bc[4],bst[4],gcol[6],gpos[6],gpx[6];
 S cont;
}St;
typedef struct N N;
struct N{N*par,*kid,*last,*nx,*pv,*lel;S tag,txt,id,cls,sty,href,at;St*s;int det,gen,x0,y0,x1,y1;void*jw;};
static int imfor(N*n);
