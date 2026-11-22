# HANDOVER — MiniWebEngine v0.11

다음 세션이 바로 이어서 작업할 수 있도록 현재 상태, 구조, 빌드·테스트 방법, 함정, 미해결 항목을 정리한 문서입니다.
(작성 시점: v0.11 배포 직후. 산출물: `MiniWebEngine-v0.11.zip`)

## 1. 프로젝트 한 줄 요약

의존성 없는 초소형 브라우저 엔진(C, GPL-3.0-only) + 이를 쓰는 MiniBrowser.
HTML 파싱 → 셀렉터/캐스케이드 → 블록·인라인·플렉스·그리드·테이블 레이아웃 → 벡터 폰트 래스터라이저 → 픽셀. 자체 JavaScript 엔진(트리 워킹), TLS 1.3/1.2 클라이언트, 이미지 디코더(PNG/JPEG/GIF/BMP)를 모두 내장.

## 2. 버전 이력 (이번 대화에서 만든 것)

| 버전 | 핵심 |
|---|---|
| v0.4 | Noto Sans CJK SC(중국어) 임베드, Inter, 저작권 고지(NOTICE.md, OFL, `--licenses`, `about:licenses`), CJK 줄바꿈(kinsoku), `word-break`, `pre-wrap` 수정 |
| v0.5 | 모든 폰트를 Noto Sans(CJK) 계열로 통일, **TLS 1.3** 구현(AES-GCM, X25519, ECDSA/RSA-PSS CertificateVerify, KeyUpdate), TLS 1.2 RSA-PSS |
| v0.6 | 한글(Noto Sans CJK KR, KS X 1001), 엄격한 UTF-8 디코더, `text-indent`, **macOS 빌드 오류 수정**(`fls` 충돌 → `flst`, `brk`/`jnl` 이름 변경, `tests/clash.c`), HTTPS 보안 UI(자물쇠, 연결 패널), HTTPS-first, HSTS, 혼합 콘텐츠 차단 |
| v0.7 | 루트 CA 116개(Mozilla/certifi), **이미지**(`mwe_img.h`), `:is()/:where()`, `aspect-ratio`, `text-shadow` |
| v0.8 | 루트 142개(core 116 + legacy 26), **모듈화**(`src/`, `browser/mb_*.inc`), `object-fit`, `place-*`, 논리 크기 속성, 플렉스 세로 정렬 버그 수정 |
| v0.9 | **JS 현대화**(Promise/async/await/제너레이터/Symbol/Proxy/클래스 필드/태그드 템플릿/fetch/URL 등), `background-image:url()`, 그리드 재작성, CSS 카운터, `text-overflow`, `<picture>/srcset` |

| v0.10 | **비동기 네트워크·리소스 로딩**(요청 상태 머신, 우선순위 큐, 캐시, 취소, 타임아웃), fetch/XHR 실제 비동기 + AbortController, 연결별 TLS 컨텍스트, 논블로킹 소켓, **v0.9 버그 수정(두 번째 스크립트 페이지 JS 파싱 실패: AST 아레나 미초기화)** |
| v0.11 | **폼 입력**(포커스·캐럿·텍스트/textarea 편집·select 순환·GET/POST 제출, `src/mwe_forms.h`), **쿠키**(`browser/mb_cookie.inc`, document.cookie), **z-index/stacking**(Cmd.z + 안정 정렬 `zsort`) |

(v0.5_1은 따로 배포하지 않고 v0.6에 합쳤습니다.)

## 3. 디렉터리 구조

```
mwe.c, mwe.h          엔진 진입점(unity build 집합자)과 공개 API
src/                  엔진 모듈 (src/README.md 에 표)
  mwe_types/base/values/decl/dom/selectors/cascade/raster/images/boxes/layout/flex/grid/table/abspos/api .inc
  mwe_js.h            JS 엔진 코어(렉서·파서·인터프리터·내장·DOM 바인딩)
  mwe_js_async.h      Promise, 마이크로태스크, await 펌프, 제너레이터, Symbol
  mwe_js_proxy.h      Proxy
  mwe_js_web.h        fetch 호스트 콜백 + 프렐류드 포함
  mwe_js_prelude.js   JS로 쓴 웹 API (→ tools/gen_prelude.py → mwe_js_prelude.h, 생성물)
  mwe_img.h           독립 이미지 디코더
  mwe_font.h          생성물(Noto 아웃라인, 약 8MB) — 직접 수정 금지
browser/              MiniBrowser: minibrowser.c + mb_*.inc (mb_net.inc = v0.10 비동기 HTTP 클라이언트), mb_tls.c, mb_shot.c(헤드리스), mb_win32.c
  tls/                TLS/X.509/암호 헤더들, trusted_roots.h(생성물)
roots/                루트 목록·원본 PEM (ROOTS.txt, legacy.pem, mozilla-certifi-*.pem …)
fonts/ NOTICE.md      폰트 라이선스(OFL)와 서드파티 고지
tools/                genfont.py, regen_font.sh, gen_roots.py, gen_prelude.py
tests/                js/(run.sh, *.js/.out, probe.html), img/(run.py, dec.c), roots_check.c, clash.c, jt.c
test*.html            렌더 회귀 테스트 페이지(CI 스모크에서도 사용)
.github/workflows/Build.yml
```

핵심 설계: **엔진은 unity build**(한 번역 단위). 모든 함수가 `static` 이라 이름 충돌이 곧 빌드 오류가 될 수 있습니다(§6).

## 4. 빌드와 테스트

```sh
# 엔진 CLI
cc -std=c99 -Os mwe.c -o mwe                      # 사용: mwe in.html out.bmp [width]   |  mwe --licenses
cc -std=c99 -Os -DMWE_NO_CJK mwe.c -o mwe_small   # CJK/한글 글리프 제외(약 0.3MB)

# 헤드리스 브라우저 (스크린샷)
cc -std=gnu11 -Os -DMWE_LIB mwe.c browser/minibrowser.c browser/mb_tls.c browser/mb_shot.c -lpthread -o mb_shot
./mb_shot URL out.bmp W H [--click X Y] [--wait MS] [--early MS] [--goto URL] [--trace]
#   기본: 페이지·리소스가 다 끝날 때까지 mb_tick 루프(최대 60s) 후 캡처. --early MS: 그 시점 스냅샷(반쯤 로드된 화면), --goto: 로딩 중 주소 입력, --trace: first-render/settled/maxtick/통계 출력. 시험용 -DMB_FAST_TIMEOUTS           # 예: about:roots, about:licenses, file:///…, http://…
#   -DMB_NO_LEGACY_ROOTS : legacy 루트 26개 제외

# 테스트
sh tests/js/run.sh                                # 9개 스위트
python3 tests/img/run.py                          # 이미지 디코더 vs Pillow, 21케이스
cc -std=gnu11 -O1 tests/roots_check.c -o rc && ./rc      # 루트 142개 파싱 확인
cc -std=gnu11 -fsyntax-only -Wall -Wextra tests/clash.c  # libc/BSD 이름 충돌 검사(macOS fls 사고 재발 방지)
PORT=18443 bash browser/tests/tls_e2e.sh          # 로컬 openssl s_server 상대 TLS 17케이스 (v0.10: -lpthread 필요)
bash tests/net/run.sh                             # v0.10/v0.11 네트워크·폼·쿠키 28케이스 (tests/net/server.py: 느린/멈춘/리디렉션/청크 서버, Pillow 필요)
```

**렌더 회귀 방법**: 변경 전 바이너리로 `test*.html` 을 BMP로 렌더해 두고, 변경 후 `cmp`로 바이트 비교(의도한 변경만 달라야 함). v0.9 시점에서 test.html~test9.html 8장은 v0.7 결과와 바이트 동일.

**JS 기대 출력은 Node로 생성**(이 환경에 `node` 있음): `node tests/js/X.js > X.out`. 엔진 출력은 `<script>`로 감싼 HTML을 `mwe`로 돌려 stdout 비교. 단, `await`가 중단되지 않으므로 async 테스트는 **순차 출력만** 써야 Node와 일치합니다.

생성물 갱신:
- JS 프렐류드 수정 후 반드시 `python3 tools/gen_prelude.py` (CI가 `git diff --exit-code` 로 확인).
- 폰트: `tools/regen_font.sh NotoSansCJK-Regular.ttc NotoSansCJK-Bold.ttc [1|2]` (TTC 페이스: SC=2, KR=1).
- 루트: `python3 tools/gen_roots.py roots/mozilla-certifi-2026.10.19.pem --legacy roots/legacy.pem --removed-from browser/tls/trusted_roots.h --version "certifi 2026.10.19"` (openssl 필요).

## 5. 환경 사실 (샌드박스)

- **네트워크 없음**(pip/GitHub 불가). 실제 인터넷 사이트로 테스트한 적 없음 — TLS/HTTPS는 로컬 `openssl s_server`, 페이지 로딩은 `python3 -m http.server` 로만 검증.
- 있는 도구: `node`, `openssl`, Pillow, fonttools, `cc`(gcc/clang), ASan/UBSan.
- 폰트: `/usr/share/fonts/opentype/noto/NotoSansCJK-{Regular,Bold}.ttc`. 라틴 전용 "Noto Sans"/"Noto Sans KR" 단독 파일은 **없음**(→ CJK 페이스에서 Latin/Greek/Cyrillic/한글 추출).
- CA 번들: `/usr/local/lib/python3.12/dist-packages/certifi/cacert.pem`(현재 Mozilla), 구버전은 `/usr/lib/python3/dist-packages/pip/_vendor/certifi/cacert.pem`.
  **시스템 번들(`/etc/ssl/certs/ca-certificates.crt`, Java cacerts)에는 샌드박스 전용 `egress-gateway-ca` 가 섞여 있으니 절대 임베드하지 말 것.**
- macOS/Windows 에서 직접 빌드해 본 적 없음(macOS 오류는 사용자가 CI 로그로 제보 → 수정). `browser/mb_win32.c` 는 v0.3 이후 수정·컴파일 검증 안 함.

## 6. 반드시 알아둘 함정

1. **셸 `pkill -f 패턴`**: 패턴이 명령줄 자체에 들어 있으면 자기 셸을 죽입니다 → `pkill -x openssl` 처럼 `-x` 사용.
2. **이름 충돌**: unity build 이므로 새 전역 이름이 libc/POSIX/BSD 와 겹치면 안 됨(`fls`, `brk`, `jnl`, `index` 등 전례). 추가 후 `tests/clash.c` 실행.
3. **PEM 정규식**: certifi 파싱 시 `re.S` 에서 `.` 이 줄바꿈까지 먹으므로 주석 매칭은 `[^\n]*` 사용(이미 반영).
4. **JS 엔진 내부 규칙**
   - 예외는 전역 `jthrow` 플래그 + `jexv`; 문장 실행기는 `JC_THROW` 코드로 변환. 호스트 진입은 `jenter/jleave`; `jleave` 끝에서 `jmicrorun()` 호출.
   - GC 는 보수적(C 스택 스캔) + 명시적 루트: `jroot2[8]`(가득 참), 그 외 `jgc` 안에서 `jmicroq/junh/jPromProto/jGenProto/jgencur` 를 직접 마킹. 새 전역 객체는 `jgc` 에 추가해야 함.
   - 네이티브 함수의 클로저 데이터: `jcurnat`(현재 호출 중인 네이티브 JOb)를 읽어 `env/ex/pad` 사용. 네이티브 호출 지점 3곳(`jcall`, `jconstruct` 등)에서 설정.
   - `JOb.fl` 비트: 1=숨김(속성), 2=클래스 ctor, 4=Map, 8=Set, 16=RegExp, 32=Promise, 64=Generator, 128=Promise handled. `JOb.k` 에 `OK_PROXY` 추가됨.
   - 키가 `\1@…`(Symbol) 또는 `#…`(private) 인 속성은 `jaddpr` 가 자동으로 비열거 처리.
   - `jdrain` 이름은 GC 가 이미 사용 → 마이크로태스크는 `jmicrorun/jmicrostep`.
5. **JS 프렐류드**는 엔진이 지원하는 문법만 사용해야 함(정규식 named group, BigInt 등 금지). 프렐류드는 스크립트가 있는 페이지에서만 `mwe_js_run` 시작 시 실행.
6. **정적 버퍼**: 이미지 디코드 스크래치 24MB + 픽셀 풀 16MB + data: URI 4MB, JS fetch 응답 4MB, 브라우저 이미지 4MB. 메모리 예산 변경 시 `mwe_img.h`(`MI_SCRATCH`), `mwe_images.inc`(`ipool`), `mwe_js_web.h`(`jfetchbuf`) 확인.
7. **CSS 파서 토큰**: `tk(v,t,8)` 은 공백 기준 분리, 괄호 내부 유지. `"a b"` 같은 따옴표 문자열은 쪼개지므로(grid-template-areas) 원문 `v` 를 직접 스캔.
8. `Len` 구조: `{px,pc,k}` — k==0 길이, k==1 auto, k==3 숫자(px=값×1000), `pc` 는 퍼센트.

## 7. 현재 지원 범위 요약

**JS**: ES2015~2022 대부분 + Promise/async(블로킹 await)/제너레이터(즉시 실행)/Symbol(고유 문자열)/Proxy(5개 트랩)/클래스 필드·private·static block/태그드 템플릿, fetch/XHR(호스트 동기 콜백), URL, AbortController, 옵저버(IntersectionObserver 는 즉시 1회 visible 보고), matchMedia, Intl(en-US), 타입드 배열(일반 배열 기반).
**CSS**: 플렉스, 그리드(areas/lines/spans/auto-fit), 테이블, 절대/고정 위치, var(), calc/min/max/clamp, @media/@supports, `:is/:where/:not/:nth-*` 등, 그라디언트, 그림자, 둥근 모서리, 배경 이미지, object-fit, 카운터, text-overflow, text-shadow, aspect-ratio, 논리 크기.
**HTML**: 대부분의 시맨틱 요소, `<img>`/`<picture>`/srcset, `<details>`, `<template>.content`, 폼 컨트롤(표시 전용).
**네트워크/보안**: TLS 1.3/1.2, 루트 142개(core 116 + legacy 26, `about:roots`), 인증서 항상 검증(우회 없음), HTTPS-first, HSTS(세션), 혼합 콘텐츠 차단, 연결 정보 패널.
**텍스트**: Noto Sans CJK(SC GB2312 + KR KS X 1001 한글 2350자) + Latin/Greek/Cyrillic(374글리프), 굵게는 합성, 기울임 합성.

## 7b. v0.10 비동기 구조 (반드시 읽을 것)

- **플랫폼 API**(`browser/mb.h`): 논블로킹 `mb_plat_connect/connect_poll/send/recv(-2=아직 없음)/close`, `mb_plat_idle`. 유닉스는 `mb_shot.c`(연결마다 워커 스레드가 getaddrinfo+connect), Windows는 `mb_win32.c`(CreateThread, **컴파일 검증 못 함**).
- **TLS**: `tls.h`가 연결별 `tls_ctx_t`(상태+큰 버퍼 3개+TCP 큐 70KB)를 쓰고 `tls_select()`로 전환. `tls_conn`/`TLS_RX_RAW`/`tcp_conn`은 현재 컨텍스트를 가리키는 매크로. 새 TLS 전역 상태를 만들면 반드시 ctx 안에 넣을 것. `mb_tls_*(c)` API는 연결 ID 기반(`mb_tls_begin/step/write/read/end`).
- **`mb_net.inc`**: 슬롯 6개(`Rq`, 버퍼 2MB씩) + 대기 큐 96개(`Qe`). `net_pump(jsonly)`가 `mb_tick`에서 호출(10ms 예산). 종류 K_PAGE/CSS/JS/IMG/FETCH/JSA, 우선순위 0/1/3·2/4·5/6. 리디렉션 5회, 타임아웃(T_CONN/T_HDR/T_STALL/T_TOTAL), 종류별 크기 제한(`capof`), 동일 URL 병합(waiter 4개), 캐시(8MB, GET CSS/JS/IMG), 이미지는 tick당 1장만 디코드.
- **`mb_nav.inc`**: `go()`는 `net_cancel_all()` 후 요청만 큐에 넣고 즉시 반환. HTML 도착 시 `page_ok → finish_commit`(history, mwe_load, relayout, res_discover, img_scan). CSS는 문서 순서대로 `mwe_add_css`, 모든 CSS+비-async JS가 끝나면(실패 포함) `mwe_js_run`, async 스크립트는 이후 `mwe_js_eval`. 뒤로가기 스크롤 복원은 `want_sy`(페이지가 자랄 때까지 재적용).
- **엔진 API 추가**(`mwe.h`): `mwe_js_set_fetch_async/mwe_js_fetch_done`, `mwe_js_set_poll`, `mwe_script_async`, `mwe_image_y`. 프렐류드의 fetch/XHR은 `__fetchStart/__fetchAbort`(콜백 기반), 비동기 호스트가 없으면(CLI) 기존 동기 `__fetch`.
- **await**: 여전히 코루틴이 아님. 대기 중 Promise가 있으면 `jpollcb`(=`js_poll`)로 네트워크만 진행(`net_pump(1)`: FETCH 결과만 전달, 페이지 커밋/이미지 디코드/내비게이션은 보류)하고 타임아웃까지 기다림 → **그동안 창은 응답 없음**. 진짜 해결 = 인터프리터에 코루틴(파이버/CPS) 도입(다음 큰 과제). 재진입 방지: `mb_tick`의 `inpump`, 전달 중 슬롯은 `RS_DELIV`.
- **GC**: `jfcb[64]`(fetch 콜백)가 루트로 `jgc`에서 마킹됨.
- 함정: 이 환경에서 서버 프로세스는 `(setsid nohup ... &)`로 띄워야 도구 호출 사이에 살아남음. 바이너리를 다시 빌드하지 않고 옛 `/tmp/mb_shot`으로 새 옵션을 시험하는 실수 주의.

## 7c. v0.11 구조 메모

- **폼**: 상태 `focn/focpos`(mwe_types.inc). 값은 DOM(input=`value` 속성, textarea=텍스트 자식)에 직접 저장(`jn_setattr/jn_settext`, 키 입력마다 16MB 아레나에서 조금씩 소모). 캐럿은 cascade가 포커스된 컨트롤의 `::value` 텍스트에 `|` 문자를 끼워 넣어 표시(깜박임 없음). textarea는 포커스 중 실제 텍스트를 `walk()`에서 숨기고 사본(캐럿 포함)을 표시. `:focus`는 PS_FOCUS. 브라우저는 `mwe_focus_kind()`>0이면 글자/편집 키를 `mwe_input_text/key`로 보냄(JS keydown이 preventDefault 하면 건너뜀). 제출은 엔진이 `mwe_form_submit()`로 큐잉 → `formpoll()`(mb_nav.inc)이 GET은 `go()`, POST는 `post_nav()`(K_PAGE, 메서드 POST) 호출.
- **쿠키**: `mb_cookie.inc` 세션 항아리 160개, 디스크 저장 없음. `rq_build`가 Cookie 헤더, `rq_headers`가 `cookies_from_headers`. 교차 사이트 판정 = 마지막 두 라벨 비교(공용 접미사 목록 없음).
- **z-index**: `Cmd.z = curz*8192+curz2`. blk()에서 위치 지정 요소가 키(`zi*2+1`, auto=1)를 설정, `zset`이면 stacking context(`curctx`). abspos 큐(`Ab`)에 부모 컨텍스트를 같이 저장. `mwe_layout` 끝의 `zsort()`가 `cm[]`를 안정 병합 정렬(+제자리 순열 적용). 정렬 후에는 cm 인덱스를 저장하는 곳이 없어야 함.
- **v0.10 이후 발견·수정**: `jnd()` 노드 0 초기화(AST 아레나), await 폴링 직후 pad 재확인.
- 시험: `tests/net/run.sh` 28케이스(서버에 /form.html /echo /setck /ck /jsck.html 추가), mb_shot 옵션 `--type TEXT`, `--press Enter|Backspace|Delete|Left|Right|Home|End|Up|Down|Tab`.
- 남은 큰 과제(우선순위): ① `await` 코루틴 ② SVG ③ WebP/AVIF ④ position:sticky / transform / animation·transition ⑤ 텍스트 선택·복사 ⑥ 진짜 드롭다운 ⑦ opacity/transform의 stacking context.

## 8. 알려진 한계 / 미구현 (우선순위 제안)

높음(현대 사이트에 영향 큼):
0. (v0.10에서 해결: 비동기 로딩) / 남은 것: await 코루틴, 진행형 HTML 파싱(수신 중 미리보기), 쿠키
1. 인라인 `<svg>`(현재 빈 박스), `<video>/<audio>/<canvas>/<iframe>` 플레이스홀더
2. (z-index·폼·쿠키는 v0.11에서 해결) `position: sticky`(현재 사실상 static), `transform`(translate/scale 무시됨)
3. `:has()`, `::first-letter/::first-line`, `@font-face`(폴백만), CSS 애니메이션/transition(정적 렌더)
4. 정규식: named groups, lookbehind, `\p{…}`, sticky
5. ES 모듈(`import/export`, `<script type=module>`), BigInt, `WebSocket/Worker`
6. JS 진짜 코루틴(await 중단/제너레이터 지연 실행) — 현재 블로킹/즉시 실행 근사. 완전 해결은 인터프리터를 CPS/명시적 스택으로 바꿔야 함
7. fetch: CORS 미강제, 쿠키 없음, HEAD/바이너리 응답 미지원, 응답 본문은 UTF-8 문자열만
8. 이미지: 프로그레시브 JPEG, WebP/AVIF/SVG, 애니메이션 GIF(첫 프레임만), CSS 배경 URL 은 **페이지 URL 기준**으로 해석(스타일시트 URL 기준 아님)
9. 텍스트: 양방향(아랍/히브리)·복합 문자(인도계, 태국), 분해형 한글 자모, 이모지, KS X 1001 밖 한글, 번체 전용 글자
10. TLS: HelloRetryRequest, PSK/세션 재개, ChaCha20-Poly1305, 클라이언트 인증서(빈 인증서만), P-521 루트
11. 레거시 인코딩(EUC-KR/GBK 등) 디코딩

검증 공백(과장하지 말 것):
- **v0.10**: 실제 인터넷 사이트로 비동기 로더를 시험하지 못함(로컬 서버·openssl s_server만). Windows(`mb_win32.c` 신규 소켓 코드)와 macOS 빌드 미검증. TLS 핸드셰이크 1회가 한 tick을 약 190ms 점유(샌드박스 -O2) → 새 HTTPS 호스트 연결 때 창이 한 번 끊김(해결하려면 암호 연산 분할 필요). 동시 접속 6개 이상 호스트 환경, IPv6 리터럴 호스트는 시험 안 함.
- **실제 인터넷 사이트·React/Vue 번들로 테스트한 적 없음.**
- legacy 루트 26개의 Mozilla 제거 사유는 확인하지 못함(Entrust/AffirmTrust/ePKI 는 distrust-after 라 제외, 만료 임박 제외). 엄격 모드는 `-DMB_NO_LEGACY_ROOTS`.
- Windows 빌드(`mb_win32.c`)와 macOS 전체 빌드는 이 환경에서 미검증. `-Wall -Wextra` 경고 0, ASan/UBSan 퍼징(HTML/JS/이미지 변조 입력 수천 건)은 통과.
- KeyUpdate(TLS 1.3)는 코드만 있고 실서버 테스트 못 함.

## 9. 다음 세션 시작 체크리스트

1. 압축 해제 → `sh tests/js/run.sh`, `python3 tests/img/run.py`, `tests/clash.c` 로 기준선 확인.
2. 변경 전 `test*.html` 렌더를 베이스라인으로 저장(§4 렌더 회귀).
3. 새 기능은 해당 모듈(`src/README.md` 표)에 추가, 필요하면 `mwe_types.inc` 에 St 필드/전방 선언 추가.
4. 새 JS 기능은 `tests/js/*.js` + Node 로 만든 `.out` 추가, 프렐류드 변경 시 `gen_prelude.py`.
5. 버전 올릴 때: `mwe.h`(MWE_VERSION), `browser/mb.h`(MB_VERSION), `mwe.c` 상단 주석, `src/mwe_js.h` 의 버전 문자열, README "What is new", CI 스모크 목록.
6. 마무리: 경고 0, 모든 테스트 통과, ASan 빌드로 `tests/js` 와 변조 입력 퍼징 → zip 패키징 후 사용자에게 파일 제공.

## 10. 사용자와의 작업 방식 (참고)

- 사용자는 한국어로 대화하며 버전 단위(v0.4 → v0.9)로 기능을 요청하고, 매 버전 zip 을 받습니다.
- 요청에 "완벽히", "최신 웹사이트 대응" 같은 표현이 있었지만 현실적으로 불가능한 범위는 **구현한 것 / 하지 못한 것 / 검증하지 못한 것**을 구분해 솔직히 보고하는 방식을 유지했습니다.
- 사용자가 CI 로그(macOS 빌드 실패 등)를 붙여 주면 원인을 찾아 수정하고, 같은 종류 오류의 재발 방지 테스트를 추가했습니다.
- v0.10 요청: 비동기 네트워크(상태 머신, 우선순위 큐, 부분 렌더, Promise 연동, 안정성 기능). 이후 후보 순위: 폼 입력·포커스 → 쿠키·세션 → z-index/stacking → SVG·WebP/AVIF → sticky/transform/animation.
- 마지막 요청: 이 HANDOVER.md 를 갱신해 다음 세션으로 넘어가기.
