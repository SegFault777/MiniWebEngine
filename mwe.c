/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine v0.8
 * Zero-dependency mini HTML5 + CSS3 engine: HTML parse -> selectors/cascade (@media, var(),
 * calc()) -> block / inline / flex / grid / table layout -> anti-aliased vector-font
 * rasterizer (outlines embedded as compact data, see tools/genfont.py) -> pixels.
 *
 * The engine itself calls no libc function (static arena, own string code).
 * Only the optional CLI (default build) uses stdio: fopen fread fwrite fclose.
 *
 * CLI build : cc -Os mwe.c -o mwe              usage: mwe in.html out.bmp [width]
 * Lib build : cc -Os -DMWE_LIB -c mwe.c        API in mwe.h (used by browser/)
 *
 * Source layout (src/, see src/README.md): one translation unit assembled from modules in dependency
 * order, so every function can stay `static` and the build is still a single compiler invocation.
 * Standalone, dependency-free components: src/mwe_img.h (image decoders), src/mwe_font.h (generated
 * font outlines), src/mwe_js.h (JavaScript engine).
 */
#include "src/mwe_types.inc"     /* constants, S/Len/St/N types, forward declarations   */
#include "src/mwe_base.inc"      /* arena, strings, UTF-8, font metrics                  */
#include "src/mwe_values.inc"    /* lengths, calc(), units, colors                       */
#include "src/mwe_decl.inc"      /* style defaults, var(), declaration parser            */
#include "src/mwe_dom.inc"       /* DOM, entities, HTML parser                           */
#include "src/mwe_selectors.inc" /* selectors, rules, @media, matching                   */
#include "src/mwe_cascade.inc"   /* UA stylesheet, presentational hints, generated, cascade */
#include "src/mwe_raster.inc"    /* paint target, blending, vector-font rasterizer       */
#include "src/mwe_images.inc"    /* <img> store and sampling                             */
#include "src/mwe_boxes.inc"     /* box painter: borders, radii, gradients, shadows      */
#include "src/mwe_layout.inc"    /* layout state, measuring, block/inline layout         */
#include "src/mwe_flex.inc"      /* flexbox                                              */
#include "src/mwe_grid.inc"      /* grid                                                 */
#include "src/mwe_table.inc"     /* tables                                               */
#include "src/mwe_abspos.inc"    /* absolute / fixed positioning                         */
#include "src/mwe_api.inc"       /* public API, JavaScript glue, CLI                     */
