/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniBrowser v0.2 - Win32 platform layer (window, input, sockets).
 * Only system DLLs are used: user32, gdi32, ws2_32, advapi32 (+kernel32).
 * Build: gcc -std=gnu11 -Os -s -DMWE_LIB -mwindows mwe.c browser/minibrowser.c \
 *            browser/mb_tls.c browser/mb_win32.c -o minibrowser.exe -lws2_32 -lgdi32 -luser32 -ladvapi32 */
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "mb.h"

static HWND hw;
static HDC mem;
static HBITMAP dib,old;
static unsigned *pix;
static int pw,phh,hand;

static void inval(void){if(hw)InvalidateRect(hw,NULL,FALSE);}

/* ---- platform hooks ---- */
void mb_plat_title(const char *s, int n){
 WCHAR w[600];int k;
 if(!hw)return;
 if(n>400)n=400;
 k=MultiByteToWideChar(CP_UTF8,0,s,n,w,500);
 if(k<0)k=0;
 w[k++]=L' ';w[k++]=L'-';w[k++]=L' ';
 w[k++]=L'M';w[k++]=L'i';w[k++]=L'n';w[k++]=L'i';w[k++]=L'B';w[k++]=L'r';w[k++]=L'o';w[k++]=L'w';w[k++]=L's';w[k++]=L'e';w[k++]=L'r';
 w[k]=0;
 SetWindowTextW(hw,w);}
long long mb_plat_ms(void){return (long long)GetTickCount64();}
void mb_plat_alert(const char *s, int n){
 WCHAR w[1200];int k;
 if(n>1000)n=1000;
 k=MultiByteToWideChar(CP_UTF8,0,s,n,w,1100);
 if(k<0)k=0;
 w[k]=0;
 MessageBoxW(hw,w,L"MiniBrowser",MB_OK);}
void mb_plat_log(const char *s, int n){
 char b[1100];int k;
 if(n>1000)n=1000;
 for(k=0;k<n;k++)b[k]=s[k];
 b[k]=0;
 OutputDebugStringA(b);}
void mb_plat_cursor(int h){
 hand=h;
 SetCursor(LoadCursor(NULL,h?IDC_HAND:IDC_ARROW));}
#define MAXS 64
/* RtlGenRandom: the system CSPRNG, exported by advapi32 under this name */
BOOLEAN WINAPI SystemFunction036(PVOID buf, ULONG len);

void mb_plat_random(unsigned char *b, int n){
 if(n>0&&!SystemFunction036(b,(ULONG)n)){ /* never continue with predictable keys */
  ExitProcess(9);}}
long long mb_plat_time(void){
 FILETIME ft;ULARGE_INTEGER u;
 GetSystemTimeAsFileTime(&ft);
 u.LowPart=ft.dwLowDateTime;u.HighPart=ft.dwHighDateTime;
 return (long long)(u.QuadPart/10000000ULL)-11644473600LL;}
/* Non-blocking sockets (v0.10). Name resolution and connect() run in a worker thread per connection; afterwards the
 * socket is switched to non-blocking mode and polled by the core. NOTE: this file could not be compiled or run in the
 * environment v0.10 was developed in (no Windows toolchain) - treat it as unverified and report build errors. */
typedef struct { volatile LONG st; SOCKET fd; int port; char host[256]; } Sk;   /* st: 0 free, 1 connecting, 2 connected, 3 failed, 4 closed while connecting */
static Sk sk2[MAXS];
static CRITICAL_SECTION skcs;
static volatile LONG skinit;
static void skinit_once(void){ if(InterlockedCompareExchange(&skinit,1,0)==0) InitializeCriticalSection(&skcs); }
static DWORD WINAPI skwork(LPVOID a){
 Sk *s=(Sk*)a;struct addrinfo hints,*res=NULL,*ai;char ps[16];SOCKET fd=INVALID_SOCKET;
 wsprintfA(ps,"%d",s->port);
 ZeroMemory(&hints,sizeof hints);
 hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;hints.ai_protocol=IPPROTO_TCP;
 if(getaddrinfo(s->host,ps,&hints,&res)==0){
  for(ai=res;ai;ai=ai->ai_next){
   u_long nb=1;fd_set w,e;struct timeval tv;int r;
   fd=socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol);
   if(fd==INVALID_SOCKET)continue;
   ioctlsocket(fd,FIONBIO,&nb);
   r=connect(fd,ai->ai_addr,(int)ai->ai_addrlen);
   if(r==0)break;
   if(WSAGetLastError()==WSAEWOULDBLOCK){
    FD_ZERO(&w);FD_ZERO(&e);FD_SET(fd,&w);FD_SET(fd,&e);tv.tv_sec=8;tv.tv_usec=0;
    if(select(0,NULL,&w,&e,&tv)>0&&FD_ISSET(fd,&w))break;}
   closesocket(fd);fd=INVALID_SOCKET;}
  freeaddrinfo(res);}
 EnterCriticalSection(&skcs);
 if(s->st==4){if(fd!=INVALID_SOCKET)closesocket(fd);s->st=0;}
 else if(fd!=INVALID_SOCKET){s->fd=fd;s->st=2;}
 else s->st=3;
 LeaveCriticalSection(&skcs);
 return 0;}
int mb_plat_connect(const char *host, int port){
 int i,n=0;HANDLE t;
 skinit_once();
 while(host[n])n++;
 if(n>=(int)sizeof sk2[0].host||port<=0||port>65535)return -1;
 EnterCriticalSection(&skcs);
 for(i=0;i<MAXS&&sk2[i].st;i++);
 if(i==MAXS){LeaveCriticalSection(&skcs);return -1;}
 sk2[i].st=1;sk2[i].fd=INVALID_SOCKET;sk2[i].port=port;
 for(n=0;host[n];n++)sk2[i].host[n]=host[n];
 sk2[i].host[n]=0;
 LeaveCriticalSection(&skcs);
 t=CreateThread(NULL,0,skwork,&sk2[i],0,NULL);
 if(!t){sk2[i].st=0;return -1;}
 CloseHandle(t);
 return i;}
int mb_plat_connect_poll(int h){
 int r;
 if(h<0||h>=MAXS)return -1;
 EnterCriticalSection(&skcs);r=sk2[h].st==2?1:sk2[h].st==1?0:-1;LeaveCriticalSection(&skcs);
 return r;}
int mb_plat_send(int h, const void *b, int n){
 int k;
 if(h<0||h>=MAXS||sk2[h].st!=2)return -1;
 k=send(sk2[h].fd,(const char*)b,n,0);
 if(k==SOCKET_ERROR)return WSAGetLastError()==WSAEWOULDBLOCK?0:-1;
 return k;}
int mb_plat_recv(int h, void *b, int n){
 int k;
 if(h<0||h>=MAXS||sk2[h].st!=2)return -1;
 k=recv(sk2[h].fd,(char*)b,n,0);
 if(k==SOCKET_ERROR)return WSAGetLastError()==WSAEWOULDBLOCK?-2:-1;
 return k;}
void mb_plat_close(int h){
 if(h<0||h>=MAXS)return;
 skinit_once();
 EnterCriticalSection(&skcs);
 if(sk2[h].st==1)sk2[h].st=4;
 else if(sk2[h].st==2||sk2[h].st==3){if(sk2[h].fd!=INVALID_SOCKET)closesocket(sk2[h].fd);sk2[h].fd=INVALID_SOCKET;sk2[h].st=0;}
 LeaveCriticalSection(&skcs);}
void mb_plat_idle(int ms){Sleep((DWORD)ms);}
void *mb_plat_alloc(unsigned long n){return HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(SIZE_T)n);}   /* zeroed; large blocks are committed lazily by the OS */
int mb_plat_readfile(const char *path, char *buf, int cap){   /* UTF-8 path */
 WCHAR w[1024];HANDLE h;DWORD got=0,total=0;
 if(!MultiByteToWideChar(CP_UTF8,0,path,-1,w,1024))return -1;
 h=CreateFileW(w,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
 if(h==INVALID_HANDLE_VALUE)return -1;
 while(total<(DWORD)cap&&ReadFile(h,buf+total,(DWORD)cap-total,&got,NULL)&&got)total+=got;
 CloseHandle(h);return (int)total;}
void mb_plat_free(void *p){if(p)HeapFree(GetProcessHeap(),0,p);}

/* ---- window ---- */
static void mkdib(int w,int h){
 BITMAPINFO bi;
 if(dib){SelectObject(mem,old);DeleteObject(dib);dib=NULL;pix=NULL;}
 if(w<1)w=1;
 if(h<1)h=1;
 ZeroMemory(&bi,sizeof bi);
 bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
 bi.bmiHeader.biWidth=w;
 bi.bmiHeader.biHeight=-h;
 bi.bmiHeader.biPlanes=1;
 bi.bmiHeader.biBitCount=32;
 bi.bmiHeader.biCompression=BI_RGB;
 dib=CreateDIBSection(mem,&bi,DIB_RGB_COLORS,(void**)&pix,NULL,0);
 if(!dib)return;
 old=(HBITMAP)SelectObject(mem,dib);
 pw=w;phh=h;
 mb_resize(w,h);}
/* clipboard (UTF-8 <-> UTF-16). NOTE: not compiled or run in the development environment (no Windows toolchain). */
void mb_plat_clipboard_set(const char *u, int n){
 HGLOBAL g;WCHAR *w;int wn;
 if(n<=0||!OpenClipboard(hw))return;
 wn=MultiByteToWideChar(CP_UTF8,0,u,n,NULL,0);
 g=GlobalAlloc(GMEM_MOVEABLE,(SIZE_T)(wn+1)*sizeof(WCHAR));
 if(g){
  w=(WCHAR*)GlobalLock(g);
  if(w){MultiByteToWideChar(CP_UTF8,0,u,n,w,wn);w[wn]=0;GlobalUnlock(g);EmptyClipboard();if(!SetClipboardData(CF_UNICODETEXT,g))GlobalFree(g);}
  else GlobalFree(g);}
 CloseClipboard();}
int mb_plat_clipboard_get(char *buf, int cap){
 HANDLE h;WCHAR *w;int n=0;
 if(!OpenClipboard(hw))return 0;
 h=GetClipboardData(CF_UNICODETEXT);
 if(h){
  w=(WCHAR*)GlobalLock(h);
  if(w){n=WideCharToMultiByte(CP_UTF8,0,w,-1,buf,cap,NULL,NULL);if(n>0)n--;else n=0;GlobalUnlock(h);}}
 CloseClipboard();
 return n;}
static int vk2k(WPARAM v){
 switch(v){
  case VK_BACK:return MBK_BACKSPACE;case VK_DELETE:return MBK_DELETE;case VK_RETURN:return MBK_ENTER;
  case VK_LEFT:return MBK_LEFT;case VK_RIGHT:return MBK_RIGHT;case VK_HOME:return MBK_HOME;case VK_END:return MBK_END;
  case VK_UP:return MBK_UP;case VK_DOWN:return MBK_DOWN;case VK_PRIOR:return MBK_PGUP;case VK_NEXT:return MBK_PGDN;
  case VK_TAB:return GetKeyState(VK_SHIFT)&0x8000?MBK_SHIFT_TAB:MBK_TAB;case VK_ESCAPE:return MBK_ESC;case VK_F5:return MBK_F5;case VK_F6:return MBK_FOCUS_URL;
  case VK_BROWSER_BACK:return MBK_BACK;case VK_BROWSER_FORWARD:return MBK_FORWARD;}
 return 0;}
static LRESULT CALLBACK wp(HWND h,UINT m,WPARAM w,LPARAM l){
 static unsigned hs;
 unsigned c;int k;
 switch(m){
  case WM_SIZE:
   if(w!=SIZE_MINIMIZED&&mem)mkdib(LOWORD(l),HIWORD(l));
   return 0;
  case WM_PAINT:{
   PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);
   if(pix){mb_draw(pix);BitBlt(dc,0,0,pw,phh,mem,0,0,SRCCOPY);}
   EndPaint(h,&ps);
   return 0;}
  case WM_ERASEBKGND:return 1;
  case WM_TIMER:if(mb_tick())inval();return 0;
  case WM_LBUTTONDOWN:{   /* single / double / triple click by time and distance */
   static DWORD lt;static int lx,ly,nclk;
   int x=(short)LOWORD(l),y=(short)HIWORD(l);DWORD t=GetMessageTime();
   if(nclk>0&&(DWORD)(t-lt)<=GetDoubleClickTime()&&x-lx<=4&&lx-x<=4&&y-ly<=4&&ly-y<=4)nclk=nclk%3+1;else nclk=1;
   lt=t;lx=x;ly=y;
   SetCapture(h);mb_mouse_down(x,y,nclk);inval();return 0;}
  case WM_LBUTTONUP:ReleaseCapture();mb_mouse_up((short)LOWORD(l),(short)HIWORD(l));inval();return 0;
  case WM_MOUSEMOVE:mb_move((short)LOWORD(l),(short)HIWORD(l));if(w&MK_LBUTTON)mb_mouse_drag((short)LOWORD(l),(short)HIWORD(l));inval();return 0;
  case WM_MOUSEWHEEL:mb_wheel((short)HIWORD(w)/WHEEL_DELTA);inval();return 0;
  case WM_SETCURSOR:
   if(LOWORD(l)==HTCLIENT){SetCursor(LoadCursor(NULL,hand?IDC_HAND:IDC_ARROW));return TRUE;}
   break;
  case WM_CHAR:
   c=(unsigned)w;
   if(c>=0xD800&&c<0xDC00){hs=c;return 0;}
   if(c>=0xDC00&&c<0xE000)c=0x10000+((hs-0xD800)<<10)+(c-0xDC00);
   if(c>=32&&c!=127){mb_char((int)c);inval();}
   return 0;
  case WM_KEYDOWN:
   if(GetKeyState(VK_CONTROL)&0x8000){
    int sh=(GetKeyState(VK_SHIFT)&0x8000)?MBMOD_SHIFT:0;
    if(w=='R')mb_key(MBK_F5);
    else if(w=='A'||w=='C'||w=='X'||w=='V'||w=='L')mb_key_ex((int)w+32,MBMOD_CTRL);
    else if((k=vk2k(w))!=0)mb_key_ex(k,MBMOD_CTRL|sh);
    inval();return 0;}
   k=vk2k(w);
   if(k){mb_key_ex(k,(GetKeyState(VK_SHIFT)&0x8000)?MBMOD_SHIFT:0);inval();}
   return 0;
  case WM_SYSKEYDOWN:
   if(w==VK_LEFT){mb_key(MBK_BACK);inval();return 0;}
   if(w==VK_RIGHT){mb_key(MBK_FORWARD);inval();return 0;}
   break;
  case WM_DESTROY:PostQuitMessage(0);return 0;}
 return DefWindowProcW(h,m,w,l);}

int WINAPI WinMain(HINSTANCE hi,HINSTANCE hprev,LPSTR cmd,int show){
 WNDCLASSW wc;MSG msg;WSADATA wd;RECT r;char url[1024];int n=0;
 (void)hprev;(void)show;
 WSAStartup(MAKEWORD(2,2),&wd);
 while(cmd&&(*cmd==' '||*cmd=='"'))cmd++;
 while(cmd&&cmd[n]&&cmd[n]!='"'&&n<1023){url[n]=cmd[n];n++;}
 url[n]=0;
 ZeroMemory(&wc,sizeof wc);
 wc.lpfnWndProc=wp;wc.hInstance=hi;wc.hCursor=LoadCursor(NULL,IDC_ARROW);
 wc.lpszClassName=L"MiniBrowserWnd";
 if(!RegisterClassW(&wc))return 1;
 mem=CreateCompatibleDC(NULL);
 hw=CreateWindowExW(0,L"MiniBrowserWnd",L"MiniBrowser",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
  CW_USEDEFAULT,CW_USEDEFAULT,1100,760,NULL,NULL,hi,NULL);
 if(!hw)return 1;
 SetTimer(hw,1,16,NULL);
 GetClientRect(hw,&r);
 mb_init(url,r.right,r.bottom);
 inval();
 while(GetMessageW(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
 WSACleanup();
 return (int)msg.wParam;}
