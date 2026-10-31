/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniBrowser v0.2 - Win32 platform layer (window, input, sockets).
 * Only system DLLs are used: user32, gdi32, ws2_32 (+kernel32).
 * Build: gcc -std=gnu99 -Os -s -DMWE_LIB -mwindows mwe.c browser/minibrowser.c \
 *            browser/mb_win32.c -o minibrowser.exe -lws2_32 -lgdi32 -luser32 */
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
void mb_plat_cursor(int h){
 hand=h;
 SetCursor(LoadCursor(NULL,h?IDC_HAND:IDC_ARROW));}
int mb_plat_http(const char *host, int port, const char *req, int rl, char *out, int cap){
 struct addrinfo hints,*res=NULL,*ai;
 char ps[16];
 SOCKET s=INVALID_SOCKET;
 int n=0,r,k,tm=15000,pl=0;
 char tmp[8];
 if(port<=0||port>65535)port=80;
 k=port;
 do{tmp[pl++]=(char)('0'+k%10);k/=10;}while(k);
 for(r=0;r<pl;r++)ps[r]=tmp[pl-1-r];
 ps[pl]=0;
 ZeroMemory(&hints,sizeof hints);
 hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;hints.ai_protocol=IPPROTO_TCP;
 if(getaddrinfo(host,ps,&hints,&res)!=0)return -1;
 for(ai=res;ai;ai=ai->ai_next){
  s=socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol);
  if(s==INVALID_SOCKET)continue;
  setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,(const char*)&tm,sizeof tm);
  setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,(const char*)&tm,sizeof tm);
  if(connect(s,ai->ai_addr,(int)ai->ai_addrlen)==0)break;
  closesocket(s);s=INVALID_SOCKET;}
 freeaddrinfo(res);
 if(s==INVALID_SOCKET)return -1;
 for(r=0;r<rl;){
  k=send(s,req+r,rl-r,0);
  if(k<=0){closesocket(s);return -1;}
  r+=k;}
 while(n<cap){
  r=recv(s,out+n,cap-n,0);
  if(r<=0)break;
  n+=r;}
 closesocket(s);
 return n>0?n:-1;}

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
static void paste(void){
 HANDLE h;WCHAR *w;char u[2048];int n;
 if(!OpenClipboard(hw))return;
 h=GetClipboardData(CF_UNICODETEXT);
 if(h){
  w=(WCHAR*)GlobalLock(h);
  if(w){
   n=WideCharToMultiByte(CP_UTF8,0,w,-1,u,(int)sizeof u,NULL,NULL);
   if(n>1)mb_paste(u,n-1);
   GlobalUnlock(h);}}
 CloseClipboard();}
static int vk2k(WPARAM v){
 switch(v){
  case VK_BACK:return MBK_BACKSPACE;case VK_DELETE:return MBK_DELETE;case VK_RETURN:return MBK_ENTER;
  case VK_LEFT:return MBK_LEFT;case VK_RIGHT:return MBK_RIGHT;case VK_HOME:return MBK_HOME;case VK_END:return MBK_END;
  case VK_UP:return MBK_UP;case VK_DOWN:return MBK_DOWN;case VK_PRIOR:return MBK_PGUP;case VK_NEXT:return MBK_PGDN;
  case VK_ESCAPE:return MBK_ESC;case VK_F5:return MBK_F5;case VK_F6:return MBK_FOCUS_URL;
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
  case WM_LBUTTONDOWN:mb_click((short)LOWORD(l),(short)HIWORD(l));inval();return 0;
  case WM_MOUSEMOVE:mb_move((short)LOWORD(l),(short)HIWORD(l));inval();return 0;
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
    if(w=='L')mb_key(MBK_FOCUS_URL);else if(w=='R')mb_key(MBK_F5);else if(w=='V')paste();
    inval();return 0;}
   k=vk2k(w);
   if(k){mb_key(k);inval();}
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
 GetClientRect(hw,&r);
 mb_init(url,r.right,r.bottom);
 inval();
 while(GetMessageW(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
 WSACleanup();
 return (int)msg.wParam;}
