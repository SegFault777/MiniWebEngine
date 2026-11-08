/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniBrowser module: mb_state
 * globals: viewport, scroll, history, security state.
 * Not a standalone translation unit: minibrowser.c includes the modules in order (unity build). */

#define PG (1<<20)
#define TB 44
#define SBH 24
#define SBW 12
#define UC 1024

static char pg[PG+1],raw[1<<21];
static char cur[UC],ub[UC],stt[UC],tmp[UC*2],req[UC*2];
static char hist[32][512];static int hsy[32],hn,hi;
static int Wd,Ht,sy,ph,plen,ucur,uf,usel,uoff,hov,ready,sec; /* sec: 0 local page, 1 http, 2 https (verified) */
static int secpanel,ufirst,quiet,netfail;    /* connection-info panel open; address-bar navigation (HTTPS-first); suppress error pages; last TLS open failed at TCP level */
static mb_tls_info_t pinfo;static char phost[128];  /* TLS details of the page that is showing */
static char hsts[16][96];static char hsub[16];static int nhsts;   /* HTTP Strict Transport Security hosts (this session) */
