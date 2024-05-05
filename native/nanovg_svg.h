#ifndef NANOVG_SVG_H
#define NANOVG_SVG_H

#include "nanosvg.h"
#include "nanovg.h"

#ifdef _WIN32
#define NVGSVGAPI __declspec(dllexport)
#else
#define NVGSVGAPI
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

NVGSVGAPI void nvgDrawSVG(NVGcontext *vg, NSVGimage *svg);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NANOVG_SVG_H */
