#ifndef NANOVG_GPU_H
#define NANOVG_GPU_H

#include <stddef.h>
#include "nanovg.h"

#ifdef _WIN32
#define NVGGPUAPI __declspec(dllexport)
#define NVGGPUCALL __cdecl
#else
#define NVGGPUAPI
#define NVGGPUCALL
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum NVGcreateFlags
{
	/* Flag indicating if geometry based anti-aliasing is used (may not be
	 * needed when using MSAA).
	 */
	NVG_ANTIALIAS = 1 << 0,
	/* Flag indicating if strokes should be drawn using stencil buffer. The
	 * rendering will be a little slower, but path overlaps (i.e.
	 * self-intersecting or sharp turns) will be drawn just once.
	 */
	NVG_STENCIL_STROKES = 1 << 1,
	/* Flag indicating that additional debug checks are done. */
	NVG_DEBUG = 1 << 2,
} NVGcreateFlags;

typedef enum nvgStencilCompareFunction
{
	NVG_STENCILCOMPAREFUNCTION_ALWAYS,
	NVG_STENCILCOMPAREFUNCTION_NEVER,
	NVG_STENCILCOMPAREFUNCTION_LESS,
	NVG_STENCILCOMPAREFUNCTION_LESSEQUAL,
	NVG_STENCILCOMPAREFUNCTION_EQUAL,
	NVG_STENCILCOMPAREFUNCTION_GREATEREQUAL,
	NVG_STENCILCOMPAREFUNCTION_GREATER,
	NVG_STENCILCOMPAREFUNCTION_NOTEQUAL
} nvgStencilCompareFunction;

typedef enum nvgStencilOperation
{
	NVG_STENCILOPERATION_KEEP,
	NVG_STENCILOPERATION_ZERO,
	NVG_STENCILOPERATION_REPLACE,
	NVG_STENCILOPERATION_INCREMENT,
	NVG_STENCILOPERATION_DECREMENT,
	NVG_STENCILOPERATION_INCREMENTSATURATION,
	NVG_STENCILOPERATION_DECREMENTSATURATION,
	NVG_STENCILOPERATION_INVERT
} nvgStencilOperation;

typedef void (NVGGPUCALL *nvg_gpu_pfn_createContext)(void* userdata);
typedef void (NVGGPUCALL *nvg_gpu_pfn_deleteContext)(void* userdata);

typedef void* (NVGGPUCALL *nvg_gpu_pfn_createVertexBuffer)(void* userdata, size_t size);
typedef void (NVGGPUCALL *nvg_gpu_pfn_deleteVertexBuffer)(void* userdata, void* buffer);
typedef void (NVGGPUCALL *nvg_gpu_pfn_updateVertexBuffer)(void* userdata, void* buffer, void* ptr, int count, size_t stride);

typedef void* (NVGGPUCALL *nvg_gpu_pfn_createTexture)(void* userdata, int isRGBA, int width, int height, int nearest, int repeatX, int repeatY);
typedef void (NVGGPUCALL *nvg_gpu_pfn_deleteTexture)(void* userdata, void* texture);
typedef void (NVGGPUCALL *nvg_gpu_pfn_updateTexture)(void* userdata, void* texture, int x, int y, int w, int h, void* data);

typedef void (NVGGPUCALL *nvg_gpu_pfn_updateUniformBuffer)(void* userdata, void* uniforms, size_t uniformLength);
typedef void (NVGGPUCALL *nvg_gpu_pfn_updateShader)(void* userdata, int enableAA, int fillType, int texType);
typedef void (NVGGPUCALL *nvg_gpu_pfn_updateSampler)(void* userdata, void* texture);

typedef void (NVGGPUCALL *nvg_gpu_pfn_setViewport)(void* userdata, float width, float height);

typedef void (NVGGPUCALL *nvg_gpu_pfn_resetState)(void* userdata);
typedef void (NVGGPUCALL *nvg_gpu_pfn_toggleColorWriteMask)(void* userdata, int enabled);
typedef void (NVGGPUCALL *nvg_gpu_pfn_updateBlendFunction)(void* userdata, NVGcompositeOperationState blendOp);
typedef void (NVGGPUCALL *nvg_gpu_pfn_toggleStencil)(void* userdata, int enabled);
typedef void (NVGGPUCALL *nvg_gpu_pfn_updateStencilFunction)(
	void* userdata,
	nvgStencilCompareFunction stencilFunc,
	nvgStencilOperation stencilFail,
	nvgStencilOperation stencilDepthBufferFail,
	nvgStencilOperation stencilPass,
	nvgStencilOperation ccwStencilFail,
	nvgStencilOperation ccwStencilDepthBufferFail,
	nvgStencilOperation ccwStencilPass
);
typedef void (NVGGPUCALL *nvg_gpu_pfn_toggleCullMode)(void* userdata, int enabled);
typedef void (NVGGPUCALL *nvg_gpu_pfn_applyState)(void* userdata, void* vertexBuffer);
typedef void (NVGGPUCALL *nvg_gpu_pfn_drawPrimitives)(void* userdata, int triStrip, int vertexOffset, int vertexCount);

NVGGPUAPI NVGcontext* nvgGpuCreate(
	void *userdata,
	nvg_gpu_pfn_createContext createContext,
	nvg_gpu_pfn_deleteContext deleteContext,
	nvg_gpu_pfn_createVertexBuffer createVertexBuffer,
	nvg_gpu_pfn_deleteVertexBuffer deleteVertexBuffer,
	nvg_gpu_pfn_updateVertexBuffer updateVertexBuffer,
	nvg_gpu_pfn_createTexture createTexture,
	nvg_gpu_pfn_deleteTexture deleteTexture,
	nvg_gpu_pfn_updateTexture updateTexture,
	nvg_gpu_pfn_updateUniformBuffer updateUniformBuffer,
	nvg_gpu_pfn_updateShader updateShader,
	nvg_gpu_pfn_updateSampler updateSampler,
	nvg_gpu_pfn_setViewport setViewport,
	nvg_gpu_pfn_resetState resetState,
	nvg_gpu_pfn_toggleColorWriteMask toggleColorWriteMask,
	nvg_gpu_pfn_updateBlendFunction updateBlendFunction,
	nvg_gpu_pfn_toggleStencil toggleStencil,
	nvg_gpu_pfn_updateStencilFunction updateStencilFunction,
	nvg_gpu_pfn_toggleCullMode toggleCullMode,
	nvg_gpu_pfn_applyState applyState,
	nvg_gpu_pfn_drawPrimitives drawPrimitives
);
NVGGPUAPI void nvgGpuDelete(NVGcontext *ctx);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NANOVG_GPU_H */
