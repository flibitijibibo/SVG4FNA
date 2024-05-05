/* SVG4FNA - SVG Container and Renderer for FNA
 *
 * Copyright (c) 2024 Ethan Lee
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software in a
 * product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Ethan "flibitijibibo" Lee <flibitijibibo@flibitijibibo.com>
 *
 */

#include <SDL.h>
#define MOJOSHADER_EFFECT_SUPPORT
#include <mojoshader.h>
#include <FNA3D.h>

#include "svg4fna.c"

#include "../shaders/nanovg.fx.h"

typedef struct FNA3D_TextureUserData
{
	FNA3D_Texture *texture;
	FNA3D_SamplerState samplerState;
	int bpp;
} FNA3D_TextureUserData;

typedef struct FNA3D_UserData
{
	FNA3D_Device *device;

	FNA3D_Effect *effect;
	MOJOSHADER_effect *effectData;
	MOJOSHADER_effectTechnique *techniques[2][4][3];
	MOJOSHADER_effectParam *inverseViewSize;
	MOJOSHADER_effectParam *frag;

	FNA3D_BlendState blendState;
	FNA3D_DepthStencilState depthStencilState;
	FNA3D_RasterizerState rasterizerState;
} FNA3D_UserData;

void CALLBACK_createContext(void* userdata)
{
	FNA3D_UserData *ctx = (FNA3D_UserData*) userdata;

	FNA3D_CreateEffect(
		ctx->device,
		(uint8_t*) g_main,
		SDL_arraysize(g_main),
		&ctx->effect,
		&ctx->effectData
	);
	for (int i = 0; i < ctx->effectData->technique_count; i += 1)
	{
		#define TECHNAME(str, aa, type, texType) \
			if (SDL_strcmp(ctx->effectData->techniques[i].name, #str) == 0) \
			{ \
				ctx->techniques[aa][type][texType] = &ctx->effectData->techniques[i]; \
			}
		TECHNAME(EdgeAA_Gradient_Premultiplied,		0, 0, 0) else
		TECHNAME(EdgeAA_Gradient_Nonpremultiplied,	0, 0, 1) else
		TECHNAME(EdgeAA_Gradient_Alpha,			0, 0, 2) else
		TECHNAME(EdgeAA_Image_Premultiplied,		0, 1, 0) else
		TECHNAME(EdgeAA_Image_Nonpremultiplied,		0, 1, 1) else
		TECHNAME(EdgeAA_Image_Alpha,			0, 1, 2) else
		TECHNAME(EdgeAA_StencilFill_Premultiplied,	0, 2, 0) else
		TECHNAME(EdgeAA_StencilFill_Nonpremultiplied,	0, 2, 1) else
		TECHNAME(EdgeAA_StencilFill_Alpha,		0, 2, 2) else
		TECHNAME(EdgeAA_Tris_Premultiplied,		0, 3, 0) else
		TECHNAME(EdgeAA_Tris_Nonpremultiplied,		0, 3, 1) else
		TECHNAME(EdgeAA_Tris_Alpha,			0, 3, 2) else
		TECHNAME(NoAA_Gradient_Premultiplied,		1, 0, 0) else
		TECHNAME(NoAA_Gradient_Nonpremultiplied,	1, 0, 1) else
		TECHNAME(NoAA_Gradient_Alpha,			1, 0, 2) else
		TECHNAME(NoAA_Image_Premultiplied,		1, 1, 0) else
		TECHNAME(NoAA_Image_Nonpremultiplied,		1, 1, 1) else
		TECHNAME(NoAA_Image_Alpha,			1, 1, 2) else
		TECHNAME(NoAA_StencilFill_Premultiplied,	1, 2, 0) else
		TECHNAME(NoAA_StencilFill_Nonpremultiplied,	1, 2, 1) else
		TECHNAME(NoAA_StencilFill_Alpha,		1, 2, 2) else
		TECHNAME(NoAA_Tris_Premultiplied,		1, 3, 0) else
		TECHNAME(NoAA_Tris_Nonpremultiplied,		1, 3, 1) else
		TECHNAME(NoAA_Tris_Alpha,			1, 3, 2)
		#undef TECHNAME
	}
	for (int i = 0; i < ctx->effectData->param_count; i += 1)
	{
		if (SDL_strcmp(ctx->effectData->params[i].value.name, "inverseViewSize") == 0)
		{
			ctx->inverseViewSize = &ctx->effectData->params[i];
		}
		else if (SDL_strcmp(ctx->effectData->params[i].value.name, "frag") == 0)
		{
			ctx->frag = &ctx->effectData->params[i];
		}
	}
}

void CALLBACK_deleteContext(void* userdata)
{
	FNA3D_UserData *ctx = (FNA3D_UserData*) userdata;

	FNA3D_AddDisposeEffect(ctx->device, ctx->effect);
}

void* CALLBACK_createTexture(void* userdata, int isRGBA, int width, int height, int nearest, int repeatX, int repeatY)
{
	FNA3D_UserData *ctx = (FNA3D_UserData*) userdata;
	FNA3D_TextureUserData *tex = (FNA3D_TextureUserData*) SDL_malloc(sizeof(FNA3D_TextureUserData));

	tex->texture = FNA3D_CreateTexture2D(
		ctx->device,
		(isRGBA) ?
			FNA3D_SURFACEFORMAT_COLOR :
			FNA3D_SURFACEFORMAT_ALPHA8,
		width,
		height,
		1,
		0
	);

	tex->samplerState.filter = nearest ?
		FNA3D_TEXTUREFILTER_POINT :
		FNA3D_TEXTUREFILTER_LINEAR;
	tex->samplerState.addressU = repeatX ?
		FNA3D_TEXTUREADDRESSMODE_WRAP :
		FNA3D_TEXTUREADDRESSMODE_CLAMP;
	tex->samplerState.addressV = repeatY ?
		FNA3D_TEXTUREADDRESSMODE_WRAP :
		FNA3D_TEXTUREADDRESSMODE_CLAMP;
	tex->samplerState.addressW =  FNA3D_TEXTUREADDRESSMODE_CLAMP;
	tex->samplerState.mipMapLevelOfDetailBias = 0.0f;
	tex->samplerState.maxAnisotropy = 4;
	tex->samplerState.maxMipLevel = 0;

	tex->bpp = (isRGBA) ? 4 : 1;

	return tex;
}

void CALLBACK_deleteTexture(void* userdata, void* texture)
{
	FNA3D_UserData *ctx = (FNA3D_UserData*) userdata;
	FNA3D_TextureUserData *tex = (FNA3D_TextureUserData*) texture;

	FNA3D_AddDisposeTexture(
		ctx->device,
		tex->texture
	);
	SDL_free(tex);
}

void CALLBACK_updateTexture(void* userdata, void* texture, int x, int y, int w, int h, void* data)
{
	FNA3D_UserData *ctx = (FNA3D_UserData*) userdata;
	FNA3D_TextureUserData *tex = (FNA3D_TextureUserData*) texture;

	FNA3D_SetTextureData2D(
		ctx->device,
		tex->texture,
		x,
		y,
		w,
		h,
		0,
		data,
		w * h * tex->bpp
	);
}

void* CALLBACK_createVertexBuffer(void* userdata, size_t size)
{
	FNA3D_UserData *ctx = (FNA3D_UserData*) userdata;
	return FNA3D_GenVertexBuffer(
		ctx->device,
		1,
		FNA3D_BUFFERUSAGE_WRITEONLY,
		size
	);
}

void CALLBACK_deleteVertexBuffer(void* userdata, void* buffer)
{
	FNA3D_UserData *ctx = (FNA3D_UserData*) userdata;
	FNA3D_AddDisposeVertexBuffer(ctx->device, (FNA3D_Buffer*) buffer);
}

void CALLBACK_updateVertexBuffer(void* userdata, void* buffer, void* ptr, int count, size_t stride)
{
	FNA3D_UserData *ctx = (FNA3D_UserData*) userdata;
	FNA3D_SetVertexBufferData(
		ctx->device,
		(FNA3D_Buffer*) buffer,
		0,
		ptr,
		count,
		stride,
		stride,
		FNA3D_SETDATAOPTIONS_DISCARD
	);
}

void CALLBACK_updateUniformBuffer(void* userdata, void* uniforms, size_t uniformLength)
{
	FNA3D_UserData *ctx = (FNA3D_UserData*) userdata;
	SDL_memcpy(ctx->frag->value.values, uniforms, uniformLength);
}

void CALLBACK_updateShader(void* userdata, int enableAA, int fillType, int texType)
{
	FNA3D_UserData *ctx = (FNA3D_UserData*) userdata;
	FNA3D_SetEffectTechnique(
		ctx->device,
		ctx->effect,
		ctx->techniques[enableAA][fillType][texType]
	);
}

void CALLBACK_updateSampler(void* userdata, void* texture)
{
	FNA3D_UserData *ctx = (FNA3D_UserData*) userdata;
	FNA3D_TextureUserData *tex = (FNA3D_TextureUserData*) texture;

	FNA3D_VerifySampler(
		ctx->device,
		0,
		tex->texture,
		&tex->samplerState
	);
}

void CALLBACK_setViewport(void* userdata, float width, float height)
{
	FNA3D_UserData *ctx = (FNA3D_UserData*) userdata;
	ctx->inverseViewSize->value.valuesF[0] = 1.0f / width;
	ctx->inverseViewSize->value.valuesF[1] = 1.0f / height;
}
void CALLBACK_resetState(void* userdata)
{
	FNA3D_UserData *ctx = (FNA3D_UserData*) userdata;

	// TODO: Move the constant values to init
	ctx->blendState.colorBlendFunction = FNA3D_BLENDFUNCTION_ADD;
	ctx->blendState.alphaBlendFunction = FNA3D_BLENDFUNCTION_ADD;
	ctx->blendState.colorWriteEnable = FNA3D_COLORWRITECHANNELS_ALL;
	ctx->blendState.colorWriteEnable1 = FNA3D_COLORWRITECHANNELS_ALL;
	ctx->blendState.colorWriteEnable2 = FNA3D_COLORWRITECHANNELS_ALL;
	ctx->blendState.colorWriteEnable3 = FNA3D_COLORWRITECHANNELS_ALL;
	ctx->blendState.blendFactor.r = 0xFF;
	ctx->blendState.blendFactor.g = 0xFF;
	ctx->blendState.blendFactor.b = 0xFF;
	ctx->blendState.blendFactor.a = 0xFF;
	ctx->blendState.multiSampleMask = -1; // AKA 0xFFFFFFFF

	// TODO: Move the constant values to init
	ctx->depthStencilState.depthBufferEnable = 0;
	ctx->depthStencilState.depthBufferWriteEnable = 0;
	ctx->depthStencilState.depthBufferFunction = FNA3D_COMPAREFUNCTION_LESSEQUAL;
	ctx->depthStencilState.stencilEnable = 0;
	ctx->depthStencilState.stencilMask = 0xFF;
	ctx->depthStencilState.stencilWriteMask = 0xFF;
	ctx->depthStencilState.twoSidedStencilMode = 1;
	ctx->depthStencilState.stencilFail = FNA3D_STENCILOPERATION_KEEP;
	ctx->depthStencilState.stencilDepthBufferFail = FNA3D_STENCILOPERATION_KEEP;
	ctx->depthStencilState.stencilPass = FNA3D_STENCILOPERATION_KEEP;
	ctx->depthStencilState.stencilFunction = FNA3D_COMPAREFUNCTION_ALWAYS;
	ctx->depthStencilState.ccwStencilFail = FNA3D_STENCILOPERATION_KEEP;
	ctx->depthStencilState.ccwStencilDepthBufferFail = FNA3D_STENCILOPERATION_KEEP;
	ctx->depthStencilState.ccwStencilPass = FNA3D_STENCILOPERATION_KEEP;
	ctx->depthStencilState.ccwStencilFunction = FNA3D_COMPAREFUNCTION_ALWAYS;
	ctx->depthStencilState.referenceStencil = 0;

	// TODO: Move the constant values to init
	ctx->rasterizerState.fillMode = FNA3D_FILLMODE_SOLID;
	ctx->rasterizerState.cullMode = FNA3D_CULLMODE_CULLCLOCKWISEFACE;
	ctx->rasterizerState.depthBias = 0;
	ctx->rasterizerState.slopeScaleDepthBias = 0;
	ctx->rasterizerState.scissorTestEnable = 0;
	ctx->rasterizerState.multiSampleAntiAlias = 0;
}

void CALLBACK_toggleColorWriteMask(void* userdata, int enabled)
{
	FNA3D_UserData *ctx = (FNA3D_UserData*) userdata;
	ctx->blendState.colorWriteEnable = enabled ?
		FNA3D_COLORWRITECHANNELS_ALL :
		FNA3D_COLORWRITECHANNELS_NONE;
}

static inline FNA3D_Blend nvg2fna(int in)
{
	switch (in)
	{
	case NVG_ZERO:			return FNA3D_BLEND_ZERO;
	case NVG_ONE:			return FNA3D_BLEND_ONE;
	case NVG_SRC_COLOR:		return FNA3D_BLEND_SOURCECOLOR;
	case NVG_ONE_MINUS_SRC_COLOR:	return FNA3D_BLEND_INVERSESOURCECOLOR;
	case NVG_DST_COLOR:		return FNA3D_BLEND_DESTINATIONCOLOR;
	case NVG_ONE_MINUS_DST_COLOR:	return FNA3D_BLEND_INVERSEDESTINATIONCOLOR;
	case NVG_SRC_ALPHA:		return FNA3D_BLEND_SOURCEALPHA;
	case NVG_ONE_MINUS_SRC_ALPHA:	return FNA3D_BLEND_INVERSESOURCEALPHA;
	case NVG_DST_ALPHA:		return FNA3D_BLEND_DESTINATIONALPHA;
	case NVG_ONE_MINUS_DST_ALPHA:	return FNA3D_BLEND_INVERSEDESTINATIONALPHA;
	case NVG_SRC_ALPHA_SATURATE:	return FNA3D_BLEND_SOURCEALPHASATURATION;
	default: return (FNA3D_Blend) -1;
	}
}

void CALLBACK_updateBlendFunction(void* userdata, NVGcompositeOperationState blendOp)
{
	FNA3D_UserData *ctx = (FNA3D_UserData*) userdata;
	ctx->blendState.colorSourceBlend = nvg2fna(blendOp.srcRGB);
	ctx->blendState.colorDestinationBlend = nvg2fna(blendOp.dstRGB);
	ctx->blendState.alphaSourceBlend = nvg2fna(blendOp.srcAlpha);
	ctx->blendState.alphaDestinationBlend = nvg2fna(blendOp.dstAlpha);
	if (	ctx->blendState.colorSourceBlend == -1 ||
		ctx->blendState.colorDestinationBlend == -1 ||
		ctx->blendState.alphaSourceBlend == -1 ||
		ctx->blendState.alphaDestinationBlend == -1	)
	{
		ctx->blendState.colorSourceBlend = FNA3D_BLEND_ONE;
		ctx->blendState.colorDestinationBlend = FNA3D_BLEND_INVERSESOURCEALPHA;
		ctx->blendState.alphaSourceBlend = FNA3D_BLEND_ONE;
		ctx->blendState.alphaDestinationBlend = FNA3D_BLEND_INVERSESOURCEALPHA;
	}
}

void CALLBACK_toggleStencil(void* userdata, int enabled)
{
	FNA3D_UserData *ctx = (FNA3D_UserData*) userdata;
	ctx->depthStencilState.stencilEnable = enabled;
}

void CALLBACK_updateStencilFunction(
	void* userdata,
	nvgStencilCompareFunction stencilFunc,
	nvgStencilOperation stencilFail,
	nvgStencilOperation stencilDepthBufferFail,
	nvgStencilOperation stencilPass,
	nvgStencilOperation ccwStencilFail,
	nvgStencilOperation ccwStencilDepthBufferFail,
	nvgStencilOperation ccwStencilPass
) {
	FNA3D_UserData *ctx = (FNA3D_UserData*) userdata;

	ctx->depthStencilState.stencilFunction = (FNA3D_CompareFunction) stencilFunc;
	ctx->depthStencilState.stencilFail = (FNA3D_StencilOperation) stencilFail;
	ctx->depthStencilState.stencilDepthBufferFail = (FNA3D_StencilOperation) stencilDepthBufferFail;
	ctx->depthStencilState.stencilPass = (FNA3D_StencilOperation) stencilPass;

	ctx->depthStencilState.ccwStencilFunction = (FNA3D_CompareFunction) stencilFunc;
	ctx->depthStencilState.ccwStencilFail = (FNA3D_StencilOperation) ccwStencilFail;
	ctx->depthStencilState.ccwStencilDepthBufferFail = (FNA3D_StencilOperation) ccwStencilDepthBufferFail;
	ctx->depthStencilState.ccwStencilPass = (FNA3D_StencilOperation) ccwStencilPass;
}

void CALLBACK_toggleCullMode(void* userdata, int enabled)
{
	FNA3D_UserData *ctx = (FNA3D_UserData*) userdata;
	ctx->rasterizerState.cullMode = enabled ?
		FNA3D_CULLMODE_CULLCLOCKWISEFACE :
		FNA3D_CULLMODE_NONE;
}

void CALLBACK_applyState(void* userdata, void* vertexBuffer)
{
	FNA3D_UserData *ctx = (FNA3D_UserData*) userdata;

	MOJOSHADER_effectStateChanges changes;
	FNA3D_VertexBufferBinding binding;
	FNA3D_VertexElement elements[2];

	binding.vertexBuffer = (FNA3D_Buffer*) vertexBuffer;
	binding.vertexDeclaration.vertexStride = sizeof(NVGvertex);
	binding.vertexDeclaration.elementCount = 2;
	binding.vertexDeclaration.elements = elements;
	binding.vertexOffset = 0;
	binding.instanceFrequency = 0;
	elements[0].offset = 0;
	elements[0].vertexElementFormat = FNA3D_VERTEXELEMENTFORMAT_VECTOR2;
	elements[0].vertexElementUsage = FNA3D_VERTEXELEMENTUSAGE_POSITION;
	elements[0].usageIndex = 0;
	elements[1].offset = sizeof(float) * 2;
	elements[1].vertexElementFormat = FNA3D_VERTEXELEMENTFORMAT_VECTOR2;
	elements[1].vertexElementUsage = FNA3D_VERTEXELEMENTUSAGE_TEXTURECOORDINATE;
	elements[1].usageIndex = 0;

	FNA3D_SetBlendState(ctx->device, &ctx->blendState);
	FNA3D_SetDepthStencilState(ctx->device, &ctx->depthStencilState);
	FNA3D_ApplyRasterizerState(ctx->device, &ctx->rasterizerState);
	FNA3D_ApplyEffect(ctx->device, ctx->effect, 0, &changes);
	FNA3D_ApplyVertexBufferBindings(
		ctx->device,
		&binding,
		1,
		1, // FIXME: This can be optimized...
		0
	);
}

void CALLBACK_drawPrimitives(void* userdata, int triStrip, int vertexOffset, int vertexCount)
{
	FNA3D_UserData *ctx = (FNA3D_UserData*) userdata;
	FNA3D_PrimitiveType primType;
	int primCount;
	if (triStrip)
	{
		primType = FNA3D_PRIMITIVETYPE_TRIANGLESTRIP;
		primCount = vertexCount - 2;
	}
	else
	{
		primType = FNA3D_PRIMITIVETYPE_TRIANGLELIST;
		primCount = vertexCount / 3;
	}
	FNA3D_DrawPrimitives(
		ctx->device,
		primType,
		vertexOffset,
		primCount
	);
}

// Main loop, finally

int main(int argc, char **argv) {
	NSVGimage *svg = nsvgParseFromFile("23.svg", "px", 96);
	if (svg == NULL) {
		printf("Could not load SVG.\n");
		return -1;
	}

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		SDL_Log("SDL_Init failed");
		return -1;
	}

	SDL_WindowFlags flags = FNA3D_PrepareWindowAttributes() | (
		SDL_WINDOW_RESIZABLE |
		SDL_WINDOW_SHOWN |
		SDL_WINDOW_ALLOW_HIGHDPI
	);

	SDL_Window *window = SDL_CreateWindow(
		"SDL2/FNA3D/NanoVG",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		1024,
		800,
		flags
	);

	if (!window) {
		SDL_Log("SDL_CreateWindow failed");
		return -1;
	}

	int winWidth, winHeight;
	int fbWidth, fbHeight;
	float pxRatio;

	SDL_GetWindowSize(window, &winWidth, &winHeight);
	SDL_GetWindowSizeInPixels(window, &fbWidth, &fbHeight);
	pxRatio = (float)fbWidth / (float)winWidth;

	FNA3D_PresentationParameters presentParams;
	presentParams.backBufferWidth = fbWidth;
	presentParams.backBufferHeight = fbHeight;
	presentParams.backBufferFormat = FNA3D_SURFACEFORMAT_COLOR;
	presentParams.multiSampleCount = 1;
	presentParams.deviceWindowHandle = window;
	presentParams.isFullScreen = 0;
	presentParams.depthStencilFormat = FNA3D_DEPTHFORMAT_D24S8;
	presentParams.presentationInterval = FNA3D_PRESENTINTERVAL_DEFAULT;
	presentParams.displayOrientation = FNA3D_DISPLAYORIENTATION_DEFAULT;
	presentParams.renderTargetUsage = FNA3D_RENDERTARGETUSAGE_DISCARDCONTENTS;

	FNA3D_Device *device = FNA3D_CreateDevice(&presentParams, 1);
	if (device == NULL) {
		SDL_Log("FNA3D_CreateDevice failed");
		return -1;
	}

	FNA3D_Viewport viewport;
	viewport.x = 0;
	viewport.y = 0;
	viewport.w = fbWidth;
	viewport.h = fbHeight;
	viewport.minDepth = 0;
	viewport.maxDepth = 1;
	FNA3D_SetViewport(device, &viewport);

	FNA3D_UserData fna3d;
	SDL_memset(&fna3d, '\0', sizeof(fna3d));
	fna3d.device = device;

	NVGcontext *vg = nvgGpuCreate(
		&fna3d,
		CALLBACK_createContext,
		CALLBACK_deleteContext,
		CALLBACK_createVertexBuffer,
		CALLBACK_deleteVertexBuffer,
		CALLBACK_updateVertexBuffer,
		CALLBACK_createTexture,
		CALLBACK_deleteTexture,
		CALLBACK_updateTexture,
		CALLBACK_updateUniformBuffer,
		CALLBACK_updateShader,
		CALLBACK_updateSampler,
		CALLBACK_setViewport,
		CALLBACK_resetState,
		CALLBACK_toggleColorWriteMask,
		CALLBACK_updateBlendFunction,
		CALLBACK_toggleStencil,
		CALLBACK_updateStencilFunction,
		CALLBACK_toggleCullMode,
		CALLBACK_applyState,
		CALLBACK_drawPrimitives
	);
	if (vg == NULL) {
		SDL_Log("NVGcontext creation failed");
		return -1;
	}

	int running = 1;
	while (running) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				running = 0;
			}
			else if (event.type == SDL_KEYDOWN) {
				switch(event.key.keysym.sym ) {
					case SDLK_ESCAPE:
						running = 0;
						break;
					default:
						break;
				}
			}
			else if (event.type == SDL_WINDOWEVENT) {
				if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
					SDL_GetWindowSize(window, &winWidth, &winHeight);
					SDL_GetWindowSizeInPixels(window, &fbWidth, &fbHeight);
					pxRatio = (float)fbWidth / (float)winWidth;

					presentParams.backBufferWidth = fbWidth;
					presentParams.backBufferHeight = fbHeight;
					FNA3D_ResetBackbuffer(device, &presentParams);

					viewport.w = fbWidth;
					viewport.h = fbHeight;
					FNA3D_SetViewport(device, &viewport);
				}
			}
		}

		FNA3D_Vec4 clearColor;
		clearColor.x = 0;
		clearColor.y = 0;
		clearColor.z = 0;
		clearColor.w = 1;
		FNA3D_Clear(
			device,
			FNA3D_CLEAROPTIONS_TARGET | FNA3D_CLEAROPTIONS_STENCIL,
			&clearColor,
			0.0f,
			0
		);

		nvgBeginFrame(vg, winWidth, winHeight, pxRatio);
		nvgDrawSVG(vg, svg);
		nvgEndFrame(vg);

		FNA3D_SwapBuffers(device, NULL, NULL, window);
	}

	nsvgDelete(svg);
	nvgGpuDelete(vg);
	FNA3D_DestroyDevice(device);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
