#include "nanovg_gpu.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Mostly based on gl2 renderer

typedef enum
{
	RENDERTYPE_NONE,
	RENDERTYPE_FILL,
	RENDERTYPE_CONVEXFILL,
	RENDERTYPE_STROKE,
	RENDERTYPE_TRIANGLES
} RenderType;

typedef enum
{
	RENDERSHADER_FILLGRAD,
	RENDERSHADER_FILLIMG,
	RENDERSHADER_SIMPLE,
	RENDERSHADER_IMG
} RenderShader;

typedef union RenderUniforms
{
	struct
	{
		float scissorMat[12];
		float paintMat[12];
		NVGcolor innerCol;
		NVGcolor outerCol;
		float scissorExt[2];
		float scissorScale[2];
		float extent[2];
		float radius;
		float feather;
		float strokeMult;
		float strokeThr;
		int texType; // FIXME: NOT ACTUALLY A UNIFORM
		int type; // FIXME: NOT ACTUALLY A UNIFORM
	};
	float uniformArray[11][4];
} RenderUniforms;

typedef struct RenderDrawCall
{
	RenderType type;
	int image;
	int pathOffset;
	int pathCount;
	int triangleOffset;
	int triangleCount;
	int uniformOffset;
	NVGcompositeOperationState blendOp;
} RenderDrawCall;

typedef struct RenderPath
{
	int fillOffset;
	int fillCount;
	int strokeOffset;
	int strokeCount;
} RenderPath;

#define NVG_MAX_TEXTURES 128 // FIXME: 128 is arbitrary!

typedef struct RenderTexture
{
	void* userdata;
	int type;
	int flags;
	int width;
	int height;
} RenderTexture;

typedef struct RenderContext
{
	NVGcreateFlags flags;

	// GPU context
	void *userdata;
	nvg_gpu_pfn_createContext createContext;
	nvg_gpu_pfn_deleteContext deleteContext;
	nvg_gpu_pfn_createVertexBuffer createVertexBuffer;
	nvg_gpu_pfn_deleteVertexBuffer deleteVertexBuffer;
	nvg_gpu_pfn_updateVertexBuffer updateVertexBuffer;
	nvg_gpu_pfn_createTexture createTexture;
	nvg_gpu_pfn_deleteTexture deleteTexture;
	nvg_gpu_pfn_updateTexture updateTexture;
	nvg_gpu_pfn_updateUniformBuffer updateUniformBuffer;
	nvg_gpu_pfn_updateShader updateShader;
	nvg_gpu_pfn_updateSampler updateSampler;
	nvg_gpu_pfn_setViewport setViewport;
	nvg_gpu_pfn_resetState resetState;
	nvg_gpu_pfn_toggleColorWriteMask toggleColorWriteMask;
	nvg_gpu_pfn_updateBlendFunction updateBlendFunction;
	nvg_gpu_pfn_toggleStencil toggleStencil;
	nvg_gpu_pfn_updateStencilFunction updateStencilFunction;
	nvg_gpu_pfn_toggleCullMode toggleCullMode;
	nvg_gpu_pfn_applyState applyState;
	nvg_gpu_pfn_drawPrimitives drawPrimitives;

	// GPU resources
	void* vertexBuffer;
	size_t vertexBufferSize;
	RenderTexture textures[NVG_MAX_TEXTURES];

	// Per frame buffers
	RenderDrawCall *calls;
	int ccalls;
	int ncalls;
	RenderPath *paths;
	int cpaths;
	int npaths;
	NVGvertex *verts;
	int cverts;
	int nverts;
	unsigned char *uniforms;
	int cuniforms;
	int nuniforms;
	int fragSize;
} RenderContext;

#define INTERNAL_maxi(x, y) (((x) > (y)) ? (x) : (y))

static RenderDrawCall* INTERNAL_allocCall(RenderContext *gl)
{
	RenderDrawCall *ret = NULL;
	if (gl->ncalls == gl->ccalls)
	{
		RenderDrawCall *calls;
		int ccalls = INTERNAL_maxi(gl->ncalls + 1, 128) + gl->ccalls / 2; // 1.5x Overallocate
		calls = (RenderDrawCall*) realloc(gl->calls, sizeof(RenderDrawCall) * ccalls);
		if (calls == NULL) return NULL;
		gl->calls = calls;
		gl->ccalls = ccalls;
	}
	ret = &gl->calls[gl->ncalls++];
	memset(ret, 0, sizeof(RenderDrawCall));
	return ret;
}

static int INTERNAL_allocPaths(RenderContext *gl, int n)
{
	int ret = 0;
	if (gl->npaths == gl->cpaths)
	{
		RenderPath *paths;
		int cpaths = INTERNAL_maxi(gl->npaths + n, 128) + gl->cpaths / 2; // 1.5x Overallocate
		paths = (RenderPath*) realloc(gl->paths, sizeof(RenderPath) * cpaths);
		if (paths == NULL) return -1;
		gl->paths = paths;
		gl->cpaths = cpaths;
	}
	ret = gl->npaths;
	gl->npaths += n;
	return ret;
}

static int INTERNAL_maxVertCount(const NVGpath* paths, int npaths)
{
	int i, count = 0;
	for (i = 0; i < npaths; i++) {
		if (paths[i].nfill > 0) {
			count += (paths[i].nfill - 2) * 3;
		}
		count += paths[i].nstroke;
	}
	return count;
}

static int INTERNAL_allocVerts(RenderContext* gl, int n)
{
	int ret = 0;
	if (gl->nverts + n > gl->cverts) {
		NVGvertex *verts;
		int cverts = INTERNAL_maxi(gl->nverts + n, 4096) + gl->cverts / 2; // 1.5x Overallocate
		verts = (NVGvertex*) realloc(gl->verts, sizeof(NVGvertex) * cverts);
		if (verts == NULL) return -1;
		gl->verts = verts;
		gl->cverts = cverts;
	}
	ret = gl->nverts;
	gl->nverts += n;
	return ret;
}

static int INTERNAL_allocFragUniforms(RenderContext* gl, int n)
{
	int ret = 0;
	int structSize = gl->fragSize;
	if (gl->nuniforms + n > gl->cuniforms) {
		unsigned char* uniforms;
		int cuniforms = INTERNAL_maxi(gl->nuniforms + n, 128) + gl->cuniforms / 2; // 1.5x Overallocate
		uniforms = (unsigned char*) realloc(gl->uniforms, structSize * cuniforms);
		if (uniforms == NULL) return -1;
		gl->uniforms = uniforms;
		gl->cuniforms = cuniforms;
	}
	ret = gl->nuniforms * structSize;
	gl->nuniforms += n;
	return ret;
}

static RenderTexture* INTERNAL_findTexture(RenderContext *gl, int id)
{
	if (id < 0 || id >= NVG_MAX_TEXTURES) {
		return NULL;
	}
	if (gl->textures[id].userdata == NULL) {
		return NULL;
	}
	return &gl->textures[id];
}

static inline RenderUniforms* INTERNAL_fragUniformPtr(RenderContext* gl, int i)
{
	return (RenderUniforms*) &gl->uniforms[i];
}

static void INTERNAL_setUniforms(RenderContext *gl, int uniformOffset, int image)
{
	RenderTexture *tex = NULL;
	RenderUniforms *frag = INTERNAL_fragUniformPtr(gl, uniformOffset);

	gl->updateUniformBuffer(gl->userdata, frag->uniformArray, sizeof(frag->uniformArray));

	gl->updateShader(
		gl->userdata,
		!!(gl->flags & NVG_ANTIALIAS),
		frag->type,
		frag->texType
	);

	if (image != 0) {
		tex = INTERNAL_findTexture(gl, image);
	}
	if (tex != NULL) {
		gl->updateSampler(gl->userdata, tex->userdata);
	}
}

static inline void INTERNAL_vset(NVGvertex* vtx, float x, float y, float u, float v)
{
	vtx->x = x;
	vtx->y = y;
	vtx->u = u;
	vtx->v = v;
}

static inline NVGcolor INTERNAL_premulColor(NVGcolor c)
{
	c.r *= c.a;
	c.g *= c.a;
	c.b *= c.a;
	return c;
}

static inline void INTERNAL_xformToMat3x4(float* m3, float* t)
{
	m3[0] = t[0];
	m3[1] = t[1];
	m3[2] = 0.0f;
	m3[3] = 0.0f;
	m3[4] = t[2];
	m3[5] = t[3];
	m3[6] = 0.0f;
	m3[7] = 0.0f;
	m3[8] = t[4];
	m3[9] = t[5];
	m3[10] = 1.0f;
	m3[11] = 0.0f;
}

static int INTERNAL_convertPaint(
	RenderContext *gl,
	RenderUniforms *frag,
	NVGpaint *paint,
	NVGscissor *scissor,
	float width,
	float fringe,
	float strokeThr
) {
	RenderTexture *tex = NULL;
	float invxform[6];

	memset(frag, 0, sizeof(*frag));

	frag->innerCol = INTERNAL_premulColor(paint->innerColor);
	frag->outerCol = INTERNAL_premulColor(paint->outerColor);

	if (scissor->extent[0] < -0.5f || scissor->extent[1] < -0.5f) {
		memset(frag->scissorMat, 0, sizeof(frag->scissorMat));
		frag->scissorExt[0] = 1.0f;
		frag->scissorExt[1] = 1.0f;
		frag->scissorScale[0] = 1.0f;
		frag->scissorScale[1] = 1.0f;
	} else {
		nvgTransformInverse(invxform, scissor->xform);
		INTERNAL_xformToMat3x4(frag->scissorMat, invxform);
		frag->scissorExt[0] = scissor->extent[0];
		frag->scissorExt[1] = scissor->extent[1];
		frag->scissorScale[0] = sqrtf(scissor->xform[0]*scissor->xform[0] + scissor->xform[2]*scissor->xform[2]) / fringe;
		frag->scissorScale[1] = sqrtf(scissor->xform[1]*scissor->xform[1] + scissor->xform[3]*scissor->xform[3]) / fringe;
	}

	memcpy(frag->extent, paint->extent, sizeof(frag->extent));
	frag->strokeMult = (width*0.5f + fringe*0.5f) / fringe;
	frag->strokeThr = strokeThr;

	if (paint->image != 0) {
		tex = INTERNAL_findTexture(gl, paint->image);
		if (tex == NULL) return 0;
		if ((tex->flags & NVG_IMAGE_FLIPY) != 0) {
			float m1[6], m2[6];
			nvgTransformTranslate(m1, 0.0f, frag->extent[1] * 0.5f);
			nvgTransformMultiply(m1, paint->xform);
			nvgTransformScale(m2, 1.0f, -1.0f);
			nvgTransformMultiply(m2, m1);
			nvgTransformTranslate(m1, 0.0f, -frag->extent[1] * 0.5f);
			nvgTransformMultiply(m1, m2);
			nvgTransformInverse(invxform, m1);
		} else {
			nvgTransformInverse(invxform, paint->xform);
		}
		frag->type = RENDERSHADER_FILLIMG;

		if (tex->type == NVG_TEXTURE_RGBA) {
			frag->texType = (tex->flags & NVG_IMAGE_PREMULTIPLIED) ? 0 : 1;
		} else {
			frag->texType = 2;
		}
	} else {
		frag->type = RENDERSHADER_FILLGRAD;
		frag->radius = paint->radius;
		frag->feather = paint->feather;
		nvgTransformInverse(invxform, paint->xform);
	}

	INTERNAL_xformToMat3x4(frag->paintMat, invxform);

	return 1;
}

static void INTERNAL_fill(RenderContext *gl, RenderDrawCall *call)
{
	RenderPath *paths = &gl->paths[call->pathOffset];
	int i, npaths = call->pathCount;

	// Draw shapes
	gl->toggleColorWriteMask(gl->userdata, 0);
	gl->toggleStencil(gl->userdata, 1);
	gl->updateStencilFunction(
		gl->userdata,
		NVG_STENCILCOMPAREFUNCTION_ALWAYS,
		NVG_STENCILOPERATION_KEEP,
		NVG_STENCILOPERATION_KEEP,
		NVG_STENCILOPERATION_INCREMENT,
		NVG_STENCILOPERATION_KEEP,
		NVG_STENCILOPERATION_KEEP,
		NVG_STENCILOPERATION_DECREMENT
	);
	gl->toggleCullMode(gl->userdata, 0);

	// set bindpoint for solid loc
	INTERNAL_setUniforms(gl, call->uniformOffset, 0);

	gl->applyState(gl->userdata, gl->vertexBuffer);

	for (i = 0; i < npaths; i++)
		gl->drawPrimitives(
			gl->userdata,
			0,
			paths[i].fillOffset,
			paths[i].fillCount
		);

	// Draw anti-aliased pixels
	gl->toggleColorWriteMask(gl->userdata, 1);
	gl->toggleCullMode(gl->userdata, 1);

	INTERNAL_setUniforms(gl, call->uniformOffset + gl->fragSize, call->image);

	if (gl->flags & NVG_ANTIALIAS) {
		gl->updateStencilFunction(
			gl->userdata,
			NVG_STENCILCOMPAREFUNCTION_EQUAL,
			NVG_STENCILOPERATION_KEEP,
			NVG_STENCILOPERATION_KEEP,
			NVG_STENCILOPERATION_KEEP,
			NVG_STENCILOPERATION_KEEP,
			NVG_STENCILOPERATION_KEEP,
			NVG_STENCILOPERATION_KEEP
		);

		gl->applyState(gl->userdata, gl->vertexBuffer);

		// Draw fringes
		for (i = 0; i < npaths; i++)
			gl->drawPrimitives(
				gl->userdata,
				1,
				paths[i].strokeOffset,
				paths[i].strokeCount
			);
	}

	// Draw fill
	gl->updateStencilFunction(
		gl->userdata,
		NVG_STENCILCOMPAREFUNCTION_NOTEQUAL,
		NVG_STENCILOPERATION_ZERO,
		NVG_STENCILOPERATION_ZERO,
		NVG_STENCILOPERATION_ZERO,
		NVG_STENCILOPERATION_ZERO,
		NVG_STENCILOPERATION_ZERO,
		NVG_STENCILOPERATION_ZERO
	);
	gl->applyState(gl->userdata, gl->vertexBuffer);

	gl->drawPrimitives(
		gl->userdata,
		1,
		call->triangleOffset,
		call->triangleCount
	);

	gl->toggleStencil(gl->userdata, 0);
}

static void INTERNAL_convexFill(RenderContext *gl, RenderDrawCall *call)
{
	RenderPath* paths = &gl->paths[call->pathOffset];
	int i, npaths = call->pathCount;

	INTERNAL_setUniforms(gl, call->uniformOffset, call->image);

	gl->applyState(gl->userdata, gl->vertexBuffer);
	for (i = 0; i < npaths; i++) {
		gl->drawPrimitives(
			gl->userdata,
			0,
			paths[i].fillOffset,
			paths[i].fillCount
		);

		// Draw fringes
		if (paths[i].strokeCount > 0) {
			gl->drawPrimitives(
				gl->userdata,
				1,
				paths[i].strokeOffset,
				paths[i].strokeCount
			);
		}
	}
}

static void INTERNAL_stroke(RenderContext *gl, RenderDrawCall *call)
{
	RenderPath *paths = &gl->paths[call->pathOffset];
	int npaths = call->pathCount, i;

	if (gl->flags & NVG_STENCIL_STROKES) {
		gl->toggleStencil(gl->userdata, 1);

		// Fill the stroke base without overlap
		gl->updateStencilFunction(
			gl->userdata,
			NVG_STENCILCOMPAREFUNCTION_EQUAL,
			NVG_STENCILOPERATION_KEEP,
			NVG_STENCILOPERATION_KEEP,
			NVG_STENCILOPERATION_INCREMENTSATURATION,
			NVG_STENCILOPERATION_KEEP,
			NVG_STENCILOPERATION_KEEP,
			NVG_STENCILOPERATION_INCREMENTSATURATION
		);

		INTERNAL_setUniforms(gl, call->uniformOffset + gl->fragSize, call->image);
		gl->applyState(gl->userdata, gl->vertexBuffer);

		for (i = 0; i < npaths; i++)
			gl->drawPrimitives(
				gl->userdata,
				1,
				paths[i].strokeOffset,
				paths[i].strokeCount
			);

		// Draw anti-aliased pixels.
		INTERNAL_setUniforms(gl, call->uniformOffset, call->image);
		gl->updateStencilFunction(
			gl->userdata,
			NVG_STENCILCOMPAREFUNCTION_EQUAL,
			NVG_STENCILOPERATION_KEEP,
			NVG_STENCILOPERATION_KEEP,
			NVG_STENCILOPERATION_KEEP,
			NVG_STENCILOPERATION_KEEP,
			NVG_STENCILOPERATION_KEEP,
			NVG_STENCILOPERATION_KEEP
		);
		gl->applyState(gl->userdata, gl->vertexBuffer);
		for (i = 0; i < npaths; i++)
			gl->drawPrimitives(
				gl->userdata,
				1,
				paths[i].strokeOffset,
				paths[i].strokeCount
			);

		// Clear stencil buffer.
		gl->toggleColorWriteMask(gl->userdata, 0);
		gl->updateStencilFunction(
			gl->userdata,
			NVG_STENCILCOMPAREFUNCTION_ALWAYS,
			NVG_STENCILOPERATION_ZERO,
			NVG_STENCILOPERATION_ZERO,
			NVG_STENCILOPERATION_ZERO,
			NVG_STENCILOPERATION_ZERO,
			NVG_STENCILOPERATION_ZERO,
			NVG_STENCILOPERATION_ZERO
		);
		gl->applyState(gl->userdata, gl->vertexBuffer);
		for (i = 0; i < npaths; i++)
			gl->drawPrimitives(
				gl->userdata,
				1,
				paths[i].strokeOffset,
				paths[i].strokeCount
			);

		gl->toggleColorWriteMask(gl->userdata, 1);
		gl->toggleStencil(gl->userdata, 0);
	} else {
		INTERNAL_setUniforms(gl, call->uniformOffset, call->image);
		gl->applyState(gl->userdata, gl->vertexBuffer);

		// Draw Strokes
		for (i = 0; i < npaths; i++)
			gl->drawPrimitives(
				gl->userdata,
				1,
				paths[i].strokeOffset,
				paths[i].strokeCount
			);
	}
}

static void INTERNAL_triangles(RenderContext *gl, RenderDrawCall *call)
{
	INTERNAL_setUniforms(gl, call->uniformOffset, call->image);

	gl->drawPrimitives(gl->userdata, 0, call->triangleOffset, call->triangleCount);
}

// GPU implementation of NVGcontext

static int nvg_gpu_renderCreate(void* uptr) {
	RenderContext *ctx = (RenderContext*) uptr;

	ctx->createContext(ctx->userdata);

	int align = 4;
	ctx->fragSize = sizeof(RenderUniforms) + align - sizeof(RenderUniforms) % align;

	return 1;
}

static int nvg_gpu_renderCreateTexture(
	void* uptr,
	int type,
	int w,
	int h,
	int imageFlags,
	const unsigned char* data
) {
	RenderContext *ctx = (RenderContext*) uptr;

	assert(!(imageFlags & NVG_IMAGE_GENERATE_MIPMAPS)); // Unsupported

	RenderTexture *dst = NULL;
	for (int i = 0; i < NVG_MAX_TEXTURES; i += 1) {
		if (ctx->textures[i].userdata == NULL) {
			dst = &ctx->textures[i];
			break;
		}
	}
	if (dst == NULL) {
		return 0;
	}

	dst->type = type;
	dst->flags = imageFlags;
	dst->width = w;
	dst->height = h;
	dst->userdata = ctx->createTexture(
		ctx->userdata,
		(type == NVG_TEXTURE_RGBA),
		w,
		h,
		!!(imageFlags & NVG_IMAGE_NEAREST),
		!!(imageFlags & NVG_IMAGE_REPEATX),
		!!(imageFlags & NVG_IMAGE_REPEATY)
	);
	if (data != NULL)
	{
		ctx->updateTexture(
			ctx->userdata,
			dst->userdata,
			0,
			0,
			w,
			h,
			(void*) data
		);
	}

	return 1;
}

static int nvg_gpu_renderDeleteTexture(void* uptr, int image) {
	RenderContext *ctx = (RenderContext*) uptr;

	if (image < 0 || image >= NVG_MAX_TEXTURES) {
		return 0;
	}

	void *userdata = ctx->textures[image].userdata;
	if (userdata == NULL) {
		return 0;
	}

	ctx->deleteTexture(ctx->userdata, userdata);
	ctx->textures[image].userdata = NULL;
	return 1;
}

static int nvg_gpu_renderUpdateTexture(
	void* uptr,
	int image,
	int x,
	int y,
	int w,
	int h,
	const unsigned char* data
) {
	RenderContext *ctx = (RenderContext*) uptr;

	if (image < 0 || image >= NVG_MAX_TEXTURES) {
		return 0;
	}

	void *userdata = ctx->textures[image].userdata;
	if (userdata == NULL) {
		return 0;
	}

	ctx->updateTexture(
		ctx->userdata,
		userdata,
		x,
		y,
		w,
		h,
		(void*) data
	);
	return 1;
}

static int nvg_gpu_renderGetTextureSize(
	void* uptr,
	int image,
	int* w,
	int* h
) {
	RenderContext *ctx = (RenderContext*) uptr;

	if (image < 0 || image >= NVG_MAX_TEXTURES) {
		return 0;
	}

	if (ctx->textures[image].userdata == NULL) {
		return 0;
	}

	if (w != NULL) {
		*w = ctx->textures[image].width;
	}
	if (h != NULL) {
		*h = ctx->textures[image].height;
	}
	return 1;
}

static void nvg_gpu_renderViewport(void* uptr, float width, float height, float devicePixelRatio) {
	RenderContext *ctx = (RenderContext*) uptr;

	NVG_NOTUSED(devicePixelRatio);

	ctx->setViewport(ctx->userdata, width, height);
}

static void nvg_gpu_renderCancel(void* uptr) {
	RenderContext *ctx = (RenderContext*) uptr;

	ctx->nverts = 0;
	ctx->npaths = 0;
	ctx->ncalls = 0;
	ctx->nuniforms = 0;
}

static void nvg_gpu_renderFlush(void* uptr) {
	RenderContext *ctx = (RenderContext*) uptr;

	if (ctx->ncalls <= 0) {
		goto reset;
	}

	ctx->resetState(ctx->userdata);

	if ((ctx->vertexBuffer == NULL) || (ctx->nverts * sizeof(NVGvertex) > ctx->vertexBufferSize)) {
		if (ctx->vertexBuffer != NULL) {
			ctx->deleteVertexBuffer(ctx->userdata, ctx->vertexBuffer);
		}
		ctx->vertexBufferSize = ctx->nverts * sizeof(NVGvertex);
		ctx->vertexBuffer = ctx->createVertexBuffer(
			ctx->userdata,
			ctx->vertexBufferSize
		);
	}
	ctx->updateVertexBuffer(
		ctx->userdata,
		ctx->vertexBuffer,
		ctx->verts,
		ctx->nverts,
		sizeof(NVGvertex)
	);

	for (int i = 0; i < ctx->ncalls; i++) {
		RenderDrawCall *call = &ctx->calls[i];

		ctx->updateBlendFunction(ctx->userdata, call->blendOp);

		if (call->type == RENDERTYPE_FILL)
			INTERNAL_fill(ctx, call);
		else if (call->type == RENDERTYPE_CONVEXFILL)
			INTERNAL_convexFill(ctx, call);
		else if (call->type == RENDERTYPE_STROKE)
			INTERNAL_stroke(ctx, call);
		else if (call->type == RENDERTYPE_TRIANGLES)
			INTERNAL_triangles(ctx, call);
	}

reset:
	ctx->nverts = 0;
	ctx->npaths = 0;
	ctx->ncalls = 0;
	ctx->nuniforms = 0;
}

static void nvg_gpu_renderFill(
	void* uptr,
	NVGpaint* paint,
	NVGcompositeOperationState compositeOperation,
	NVGscissor* scissor,
	float fringe,
	const float* bounds,
	const NVGpath* paths,
	int npaths
) {
	RenderContext *ctx = (RenderContext*) uptr;

	RenderDrawCall *call = INTERNAL_allocCall(ctx);
	NVGvertex *quad;
	RenderUniforms *frag;
	int i, maxverts, offset;

	if (call == NULL) return;

	call->type = RENDERTYPE_FILL;
	call->triangleCount = 4;
	call->pathOffset = INTERNAL_allocPaths(ctx, npaths);
	if (call->pathOffset == -1) goto error;
	call->pathCount = npaths;
	call->image = paint->image;
	call->blendOp = compositeOperation;

	if (npaths == 1 && paths[0].convex)
	{
		call->type = RENDERTYPE_CONVEXFILL;
		call->triangleCount = 0;	// Bounding box fill quad not needed for convex fill
	}

	// Allocate vertices for all the paths.
	maxverts = INTERNAL_maxVertCount(paths, npaths) + call->triangleCount;
	offset = INTERNAL_allocVerts(ctx, maxverts);
	if (offset == -1) goto error;

	for (i = 0; i < npaths; i++) {
		RenderPath* copy = &ctx->paths[call->pathOffset + i];
		const NVGpath* path = &paths[i];
		memset(copy, 0, sizeof(RenderPath));
		if (path->nfill > 0) {
			copy->fillOffset = offset;
			copy->fillCount = (path->nfill - 2) * 3;

			// Convert fan to list
			NVGvertex *dst = &ctx->verts[offset];
			for (int i = 2; i < path->nfill; i += 1, dst += 3) {
				dst[0] = path->fill[i - 1];
				dst[1] = path->fill[i];
				dst[2] = path->fill[0];
			}
			offset += copy->fillCount;
		}
		if (path->nstroke > 0) {
			copy->strokeOffset = offset;
			copy->strokeCount = path->nstroke;
			memcpy(&ctx->verts[offset], path->stroke, sizeof(NVGvertex) * path->nstroke);
			offset += path->nstroke;
		}
	}

	// Setup uniforms for draw calls
	if (call->type == RENDERTYPE_FILL) {
		// Quad
		call->triangleOffset = offset;
		quad = &ctx->verts[call->triangleOffset];
		INTERNAL_vset(&quad[0], bounds[2], bounds[3], 0.5f, 1.0f);
		INTERNAL_vset(&quad[1], bounds[2], bounds[1], 0.5f, 1.0f);
		INTERNAL_vset(&quad[2], bounds[0], bounds[3], 0.5f, 1.0f);
		INTERNAL_vset(&quad[3], bounds[0], bounds[1], 0.5f, 1.0f);

		call->uniformOffset = INTERNAL_allocFragUniforms(ctx, 2);
		if (call->uniformOffset == -1) goto error;
		// Simple shader for stencil
		frag = INTERNAL_fragUniformPtr(ctx, call->uniformOffset);
		memset(frag, 0, sizeof(*frag));
		frag->strokeThr = -1.0f;
		frag->type = RENDERSHADER_SIMPLE;
		// Fill shader
		INTERNAL_convertPaint(ctx, INTERNAL_fragUniformPtr(ctx, call->uniformOffset + ctx->fragSize), paint, scissor, fringe, fringe, -1.0f);
	} else {
		call->uniformOffset = INTERNAL_allocFragUniforms(ctx, 1);
		if (call->uniformOffset == -1) goto error;
		// Fill shader
		INTERNAL_convertPaint(ctx, INTERNAL_fragUniformPtr(ctx, call->uniformOffset), paint, scissor, fringe, fringe, -1.0f);
	}

	return;

error:
	// We get here if call alloc was ok, but something else is not.
	// Roll back the last call to prevent drawing it.
	if (ctx->ncalls > 0) ctx->ncalls--;
}

static void nvg_gpu_renderStroke(void* uptr,
	NVGpaint* paint,
	NVGcompositeOperationState compositeOperation,
	NVGscissor* scissor,
	float fringe,
	float strokeWidth,
	const NVGpath* paths,
	int npaths
) {
	RenderContext *gl = (RenderContext*) uptr;
	RenderDrawCall *call = INTERNAL_allocCall(gl);
	int i, maxverts, offset;

	if (call == NULL) return;

	call->type = RENDERTYPE_STROKE;
	call->pathOffset = INTERNAL_allocPaths(gl, npaths);
	if (call->pathOffset == -1) goto error;
	call->pathCount = npaths;
	call->image = paint->image;
	call->blendOp = compositeOperation;

	// Allocate vertices for all the paths.
	maxverts = INTERNAL_maxVertCount(paths, npaths);
	offset = INTERNAL_allocVerts(gl, maxverts);
	if (offset == -1) goto error;

	for (i = 0; i < npaths; i++) {
		RenderPath* copy = &gl->paths[call->pathOffset + i];
		const NVGpath* path = &paths[i];
		memset(copy, 0, sizeof(RenderPath));
		if (path->nstroke) {
			copy->strokeOffset = offset;
			copy->strokeCount = path->nstroke;
			memcpy(&gl->verts[offset], path->stroke, sizeof(NVGvertex) * path->nstroke);
			offset += path->nstroke;
		}
	}

	if (gl->flags & NVG_STENCIL_STROKES) {
		// Fill shader
		call->uniformOffset = INTERNAL_allocFragUniforms(gl, 2);
		if (call->uniformOffset == -1) goto error;

		INTERNAL_convertPaint(gl, INTERNAL_fragUniformPtr(gl, call->uniformOffset), paint, scissor, strokeWidth, fringe, -1.0f);
		INTERNAL_convertPaint(gl, INTERNAL_fragUniformPtr(gl, call->uniformOffset + gl->fragSize), paint, scissor, strokeWidth, fringe, 1.0f - 0.5f/255.0f);

	} else {
		// Fill shader
		call->uniformOffset = INTERNAL_allocFragUniforms(gl, 1);
		if (call->uniformOffset == -1) goto error;
		INTERNAL_convertPaint(gl, INTERNAL_fragUniformPtr(gl, call->uniformOffset), paint, scissor, strokeWidth, fringe, -1.0f);
	}

	return;

error:
	// We get here if call alloc was ok, but something else is not.
	// Roll back the last call to prevent drawing it.
	if (gl->ncalls > 0) gl->ncalls--;
}

static void nvg_gpu_renderTriangles(void* uptr,
	NVGpaint* paint,
	NVGcompositeOperationState compositeOperation,
	NVGscissor* scissor,
	const NVGvertex* verts,
	int nverts,
	float fringe
) {
	RenderContext *gl = (RenderContext*) uptr;
	RenderDrawCall *call = INTERNAL_allocCall(gl);
	RenderUniforms *frag;

	if (call == NULL) return;

	call->type = RENDERTYPE_TRIANGLES;
	call->image = paint->image;
	call->blendOp = compositeOperation;

	// Allocate vertices for all the paths.
	call->triangleOffset = INTERNAL_allocVerts(gl, nverts);
	if (call->triangleOffset == -1) goto error;
	call->triangleCount = nverts;

	memcpy(&gl->verts[call->triangleOffset], verts, sizeof(NVGvertex) * nverts);

	// Fill shader
	call->uniformOffset = INTERNAL_allocFragUniforms(gl, 1);
	if (call->uniformOffset == -1) goto error;
	frag = INTERNAL_fragUniformPtr(gl, call->uniformOffset);
	INTERNAL_convertPaint(gl, frag, paint, scissor, 1.0f, fringe, -1.0f);
	frag->type = RENDERSHADER_IMG;

	return;

error:
	// We get here if call alloc was ok, but something else is not.
	// Roll back the last call to prevent drawing it.
	if (gl->ncalls > 0) gl->ncalls--;
}

static void nvg_gpu_renderDelete(void* uptr) {

	RenderContext *ctx = (RenderContext*) uptr;

	for (int i = 0; i < NVG_MAX_TEXTURES; i += 1) {
		if (ctx->textures[i].userdata != NULL) {
			ctx->deleteTexture(
				ctx->userdata,
				ctx->textures[i].userdata
			);
			ctx->textures[i].userdata = NULL;
		}
	}

	if (ctx->vertexBuffer != NULL) {
		ctx->deleteVertexBuffer(ctx->userdata, ctx->vertexBuffer);
	}

	ctx->deleteContext(ctx->userdata);

	free(ctx->paths);
	free(ctx->verts);
	free(ctx->uniforms);
	free(ctx->calls);

	free(uptr);
}

NVGcontext* nvgGpuCreate(
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
) {
	NVGparams nvgParams;

	RenderContext *ctx = (RenderContext*) malloc(sizeof(RenderContext));
	memset(ctx, '\0', sizeof(RenderContext));
	ctx->flags = NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG;
	ctx->userdata = userdata;
	ctx->createContext = createContext;
	ctx->deleteContext = deleteContext;
	ctx->createVertexBuffer = createVertexBuffer;
	ctx->deleteVertexBuffer = deleteVertexBuffer;
	ctx->updateVertexBuffer = updateVertexBuffer;
	ctx->createTexture = createTexture;
	ctx->deleteTexture = deleteTexture;
	ctx->updateTexture = updateTexture;
	ctx->updateUniformBuffer = updateUniformBuffer;
	ctx->updateShader = updateShader;
	ctx->updateSampler = updateSampler;
	ctx->setViewport = setViewport;
	ctx->resetState = resetState;
	ctx->toggleColorWriteMask = toggleColorWriteMask;
	ctx->updateBlendFunction = updateBlendFunction;
	ctx->toggleStencil = toggleStencil;
	ctx->updateStencilFunction = updateStencilFunction;
	ctx->toggleCullMode = toggleCullMode;
	ctx->applyState = applyState;
	ctx->drawPrimitives = drawPrimitives;

	memset(&nvgParams, 0, sizeof(nvgParams));
	nvgParams.renderCreate = nvg_gpu_renderCreate;
	nvgParams.renderCreateTexture = nvg_gpu_renderCreateTexture;
	nvgParams.renderDeleteTexture = nvg_gpu_renderDeleteTexture;
	nvgParams.renderUpdateTexture = nvg_gpu_renderUpdateTexture;
	nvgParams.renderGetTextureSize = nvg_gpu_renderGetTextureSize;
	nvgParams.renderViewport = nvg_gpu_renderViewport;
	nvgParams.renderCancel = nvg_gpu_renderCancel;
	nvgParams.renderFlush = nvg_gpu_renderFlush;
	nvgParams.renderFill = nvg_gpu_renderFill;
	nvgParams.renderStroke = nvg_gpu_renderStroke;
	nvgParams.renderTriangles = nvg_gpu_renderTriangles;
	nvgParams.renderDelete = nvg_gpu_renderDelete;
	nvgParams.userPtr = ctx;
	nvgParams.edgeAntiAlias = ctx->flags & NVG_ANTIALIAS ? 1 : 0;

	return nvgCreateInternal(&nvgParams);
}

void nvgGpuDelete(NVGcontext *ctx)
{
	nvgDeleteInternal(ctx);
}
