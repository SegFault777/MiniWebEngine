/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniBrowser - portable core, built on MiniWebEngine.
 *
 * NOTE: MiniBrowser is NOT "MiniWeb". MiniWeb is the browser built into the
 * MiniWin operating system. MiniBrowser is a separate, standalone desktop
 * browser that merely shares the "Mini" naming; it is built on MiniWebEngine.
 *
 * This file: URL handling, HTTP/1.1 response parsing, history, UI chrome.
 * No OS calls. File I/O uses stdio only for file:// pages. */
#include <stdio.h>
#include "../mwe.h"
#include "mb.h"

/* Source layout: one translation unit assembled from the modules below (see browser/README.md). */
#include "mb_state.inc"   /* globals: viewport, scroll, history, security state */
#include "mb_str.inc"     /* string builder, URL helpers */
#include "mb_fetch.inc"   /* file/HTTP page sources, HSTS, mixed content */
#include "mb_nav.inc"     /* load pipeline, HTTPS-first, subresources, history */
#include "mb_ui.inc"      /* drawing, lock badge, connection panel */
#include "mb_input.inc"   /* keyboard and mouse */
