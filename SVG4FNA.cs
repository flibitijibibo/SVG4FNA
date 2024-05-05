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

using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

using Microsoft.Xna.Framework;
using Microsoft.Xna.Framework.Graphics;

public class SVG4FNA
{
	#region Private Variables

	private GraphicsDevice device;
	private Effect effect;
	private EffectTechnique[,,] techniques;
	private EffectParameter inverseViewSize;
	private IntPtr frag;

	private DynamicVertexBuffer vertexBuffer;

	private class TextureHandle
	{
		public Texture2D texture;
		public SamplerState samplerState;
		public int bpp;
	}
	private Dictionary<long, TextureHandle> textures;
	private long textureAlias;

	private int enableColorWrite;
	private Blend srcRGB;
	private Blend srcAlpha;
	private Blend dstRGB;
	private Blend dstAlpha;
	private Dictionary<uint, BlendState> blendCache;

	private bool stencilEnable;
	private CompareFunction stencilFunc;
	private StencilOperation stencilFail;
	private StencilOperation stencilDepthBufferFail;
	private StencilOperation stencilPass;
	private StencilOperation ccwStencilFail;
	private StencilOperation ccwStencilDepthBufferFail;
	private StencilOperation ccwStencilPass;
	private Dictionary<uint, DepthStencilState> depthStencilCache;

	private IntPtr svg;
	private IntPtr nvg;

	private nvg_gpu_pfn_createContext createContext;
	private nvg_gpu_pfn_deleteContext deleteContext;
	private nvg_gpu_pfn_createVertexBuffer createVertexBuffer;
	private nvg_gpu_pfn_deleteVertexBuffer deleteVertexBuffer;
	private nvg_gpu_pfn_updateVertexBuffer updateVertexBuffer;
	private nvg_gpu_pfn_createTexture createTexture;
	private nvg_gpu_pfn_deleteTexture deleteTexture;
	private nvg_gpu_pfn_updateTexture updateTexture;
	private nvg_gpu_pfn_updateUniformBuffer updateUniformBuffer;
	private nvg_gpu_pfn_updateShader updateShader;
	private nvg_gpu_pfn_updateSampler updateSampler;
	private nvg_gpu_pfn_setViewport setViewport;
	private nvg_gpu_pfn_resetState resetState;
	private nvg_gpu_pfn_toggleColorWriteMask toggleColorWriteMask;
	private nvg_gpu_pfn_updateBlendFunction updateBlendFunction;
	private nvg_gpu_pfn_toggleStencil toggleStencil;
	private nvg_gpu_pfn_updateStencilFunction updateStencilFunction;
	private nvg_gpu_pfn_toggleCullMode toggleCullMode;
	private nvg_gpu_pfn_applyState applyState;
	private nvg_gpu_pfn_drawPrimitives drawPrimitives;

	#endregion

	#region Public Constructor

	public SVG4FNA(GraphicsDevice device, string svgPath, string units = "px", float dpi = 96.0f)
	{
		this.device = device;

		textures = new Dictionary<long, TextureHandle>();
		textureAlias = 1;

		blendCache = new Dictionary<uint, BlendState>();
		depthStencilCache = new Dictionary<uint, DepthStencilState>();

		svg = nsvgParseFromFile(svgPath, units, dpi);
		if (svg == IntPtr.Zero)
		{
			throw new FileNotFoundException(svgPath);
		}

		createContext = CreateContext;
		deleteContext = DeleteContext;
		createVertexBuffer = CreateVertexBuffer;
		deleteVertexBuffer = DeleteVertexBuffer;
		updateVertexBuffer = UpdateVertexBuffer;
		createTexture = CreateTexture;
		deleteTexture = DeleteTexture;
		updateTexture = UpdateTexture;
		updateUniformBuffer = UpdateUniformBuffer;
		updateShader = UpdateShader;
		updateSampler = UpdateSampler;
		setViewport = SetViewport;
		resetState = ResetState;
		toggleColorWriteMask = ToggleColorWriteMask;
		updateBlendFunction = UpdateBlendFunction;
		toggleStencil = ToggleStencil;
		updateStencilFunction = UpdateStencilFunction;
		toggleCullMode = ToggleCullMode;
		applyState = ApplyState;
		drawPrimitives = DrawPrimitives;

		nvg = nvgGpuCreate(
			IntPtr.Zero,
			createContext,
			deleteContext,
			createVertexBuffer,
			deleteVertexBuffer,
			updateVertexBuffer,
			createTexture,
			deleteTexture,
			updateTexture,
			updateUniformBuffer,
			updateShader,
			updateSampler,
			setViewport,
			resetState,
			toggleColorWriteMask,
			updateBlendFunction,
			toggleStencil,
			updateStencilFunction,
			toggleCullMode,
			applyState,
			drawPrimitives
		);
	}

	#endregion

	#region Public Methods

	public void Dispose()
	{
		nsvgDelete(svg);
		nvgGpuDelete(nvg);

		blendCache.Clear();
		blendCache = null;

		depthStencilCache.Clear();
		depthStencilCache = null;

		textures.Clear();
		textures = null;
	}

	// TODO: Offset, size, stuff like that
	public void Draw(float fbScale)
	{
		nvgBeginFrame(
			nvg,
			device.Viewport.Width,
			device.Viewport.Height,
			fbScale
		);
		nvgDrawSVG(nvg, svg);
		nvgEndFrame(nvg);
	}

	#endregion

	#region Private Callbacks

	private void CreateContext(IntPtr userdata)
	{

		effect = new Effect(device, File.ReadAllBytes("../../../../shaders/nanovg.fxb"));
		inverseViewSize = effect.Parameters["inverseViewSize"];
		frag = (IntPtr) typeof(EffectParameter).GetField(
			"values",
			BindingFlags.Instance | BindingFlags.NonPublic
		).GetValue(effect.Parameters["frag"]);

		techniques = new EffectTechnique[2, 4, 3];
		techniques[0, 0, 0] = effect.Techniques["EdgeAA_Gradient_Premultiplied"];
		techniques[0, 0, 1] = effect.Techniques["EdgeAA_Gradient_Nonpremultiplied"];
		techniques[0, 0, 2] = effect.Techniques["EdgeAA_Gradient_Alpha"];
		techniques[0, 1, 0] = effect.Techniques["EdgeAA_Image_Premultiplied"];
		techniques[0, 1, 1] = effect.Techniques["EdgeAA_Image_Nonpremultiplied"];
		techniques[0, 1, 2] = effect.Techniques["EdgeAA_Image_Alpha"];
		techniques[0, 2, 0] = effect.Techniques["EdgeAA_StencilFill_Premultiplied"];
		techniques[0, 2, 1] = effect.Techniques["EdgeAA_StencilFill_Nonpremultiplied"];
		techniques[0, 2, 2] = effect.Techniques["EdgeAA_StencilFill_Alpha"];
		techniques[0, 3, 0] = effect.Techniques["EdgeAA_Tris_Premultiplied"];
		techniques[0, 3, 1] = effect.Techniques["EdgeAA_Tris_Nonpremultiplied"];
		techniques[0, 3, 2] = effect.Techniques["EdgeAA_Tris_Alpha"];
		techniques[1, 0, 0] = effect.Techniques["NoAA_Gradient_Premultiplied"];
		techniques[1, 0, 1] = effect.Techniques["NoAA_Gradient_Nonpremultiplied"];
		techniques[1, 0, 2] = effect.Techniques["NoAA_Gradient_Alpha"];
		techniques[1, 1, 0] = effect.Techniques["NoAA_Image_Premultiplied"];
		techniques[1, 1, 1] = effect.Techniques["NoAA_Image_Nonpremultiplied"];
		techniques[1, 1, 2] = effect.Techniques["NoAA_Image_Alpha"];
		techniques[1, 2, 0] = effect.Techniques["NoAA_StencilFill_Premultiplied"];
		techniques[1, 2, 1] = effect.Techniques["NoAA_StencilFill_Nonpremultiplied"];
		techniques[1, 2, 2] = effect.Techniques["NoAA_StencilFill_Alpha"];
		techniques[1, 3, 0] = effect.Techniques["NoAA_Tris_Premultiplied"];
		techniques[1, 3, 1] = effect.Techniques["NoAA_Tris_Nonpremultiplied"];
		techniques[1, 3, 2] = effect.Techniques["NoAA_Tris_Alpha"];
	}

	private void DeleteContext(IntPtr userdata)
	{
		if (vertexBuffer != null)
		{
			vertexBuffer.Dispose();
			vertexBuffer = null;
		}
		effect.Dispose();
		inverseViewSize = null;
		frag = IntPtr.Zero;
		techniques = null;
	}

	private IntPtr CreateVertexBuffer(IntPtr userdata, IntPtr size)
	{
		vertexBuffer = new DynamicVertexBuffer(
			device,
			NVGvertex.VertexDeclaration,
			size.ToInt32() / NVGvertex.VertexDeclaration.VertexStride,
			BufferUsage.WriteOnly
		);
		return new IntPtr(1);
	}

	private void DeleteVertexBuffer(IntPtr userdata, IntPtr buffer)
	{
		if (buffer.ToInt64() != 1)
		{
			throw new InvalidOperationException();
		}
		vertexBuffer.Dispose();
		vertexBuffer = null;
	}

	private void UpdateVertexBuffer(IntPtr userdata, IntPtr buffer, IntPtr ptr, int count, IntPtr stride)
	{
		if (buffer.ToInt64() != 1)
		{
			throw new InvalidOperationException();
		}
		vertexBuffer.SetDataPointerEXT(
			0,
			ptr,
			count * stride.ToInt32(),
			SetDataOptions.Discard
		);
	}

	private IntPtr CreateTexture(
		IntPtr userdata,
		int isRGBA,
		int width,
		int height,
		int nearest,
		int repeatX,
		int repeatY
	) {
		TextureHandle textureHandle = new TextureHandle();

		textureHandle.texture = new Texture2D(
			device,
			width,
			height,
			false,
			(isRGBA > 0) ? SurfaceFormat.Color : SurfaceFormat.Alpha8
		);
		textureHandle.samplerState = new SamplerState();
		textureHandle.samplerState.Filter = (nearest > 0) ?
			TextureFilter.Point :
			TextureFilter.Linear;
		textureHandle.samplerState.AddressU = (repeatX > 0) ?
			TextureAddressMode.Wrap :
			TextureAddressMode.Clamp;
		textureHandle.samplerState.AddressV = (repeatY > 0) ?
			TextureAddressMode.Wrap :
			TextureAddressMode.Clamp;
		textureHandle.samplerState.AddressW = TextureAddressMode.Clamp;
		textureHandle.bpp = (isRGBA > 0) ? 4 : 1;

		long alias = textureAlias++;
		textures.Add(alias, textureHandle);
		return new IntPtr(alias);
	}

	private void DeleteTexture(IntPtr userdata, IntPtr texture)
	{
		TextureHandle textureHandle;
		long alias = texture.ToInt64();
		if (textures.TryGetValue(alias, out textureHandle))
		{
			textureHandle.texture.Dispose();
			textures.Remove(alias);
		}
		else
		{
			throw new InvalidOperationException();
		}
	}

	private void UpdateTexture(IntPtr userdata, IntPtr texture, int x, int y, int w, int h, IntPtr data)
	{
		TextureHandle textureHandle;
		if (textures.TryGetValue(texture.ToInt64(), out textureHandle))
		{
			textureHandle.texture.SetDataPointerEXT(
				0,
				new Rectangle(x, y, w, h),
				data,
				w * h * textureHandle.bpp
			);
		}
		else
		{
			throw new InvalidOperationException();
		}
	}

	[DllImport("msvcrt", CallingConvention = CallingConvention.Cdecl)]
	private static extern IntPtr memcpy(IntPtr dst, IntPtr src, IntPtr len);

	private void UpdateUniformBuffer(IntPtr userdata, IntPtr uniforms, IntPtr uniformLength)
	{
		memcpy(frag, uniforms, uniformLength);
	}

	private void UpdateShader(IntPtr userdata, int enableAA, int fillType, int texType)
	{
		effect.CurrentTechnique = techniques[enableAA, fillType, texType];
	}

	private void UpdateSampler(IntPtr userdata, IntPtr texture)
	{
		TextureHandle textureHandle;
		if (textures.TryGetValue(texture.ToInt64(), out textureHandle))
		{
			device.Textures[0] = textureHandle.texture;
			device.SamplerStates[0] = textureHandle.samplerState;
		}
		else
		{
			throw new InvalidOperationException();
		}
	}

	private void SetViewport(IntPtr userdata, float width, float height)
	{
		inverseViewSize.SetValue(new Vector2(1.0f / width, 1.0f / height));
	}

	private void ResetState(IntPtr userdata)
	{
		device.BlendState = BlendState.Opaque;
		device.DepthStencilState = DepthStencilState.None;
		device.RasterizerState = RasterizerState.CullClockwise;
	}

	private void ToggleColorWriteMask(IntPtr userdata, int enabled)
	{
		enableColorWrite = enabled;
	}

	private static Blend nvg2fna(int op)
	{
		switch (op)
		{
		case NVG_ZERO:			return Blend.Zero;
		case NVG_ONE:			return Blend.One;
		case NVG_SRC_COLOR:		return Blend.SourceColor;
		case NVG_ONE_MINUS_SRC_COLOR:	return Blend.InverseSourceColor;
		case NVG_DST_COLOR:		return Blend.DestinationColor;
		case NVG_ONE_MINUS_DST_COLOR:	return Blend.InverseDestinationColor;
		case NVG_SRC_ALPHA:		return Blend.SourceAlpha;
		case NVG_ONE_MINUS_SRC_ALPHA:	return Blend.InverseSourceAlpha;
		case NVG_DST_ALPHA:		return Blend.DestinationAlpha;
		case NVG_ONE_MINUS_DST_ALPHA:	return Blend.InverseDestinationAlpha;
		case NVG_SRC_ALPHA_SATURATE:	return Blend.SourceAlphaSaturation;
		default: throw new InvalidOperationException();
		}
	}

	private void UpdateBlendFunction(IntPtr userdata, NVGcompositeOperationState blendOp)
	{
		srcRGB = nvg2fna(blendOp.srcRGB);
		srcAlpha = nvg2fna(blendOp.srcAlpha);
		dstRGB = nvg2fna(blendOp.dstRGB);
		dstAlpha = nvg2fna(blendOp.dstAlpha);
	}

	private void ToggleStencil(IntPtr userdata, int enabled)
	{
		stencilEnable = (enabled > 0);
	}

	private void UpdateStencilFunction(
		IntPtr userdata,
		nvgStencilCompareFunction stencilFunc,
		nvgStencilOperation stencilFail,
		nvgStencilOperation stencilDepthBufferFail,
		nvgStencilOperation stencilPass,
		nvgStencilOperation ccwStencilFail,
		nvgStencilOperation ccwStencilDepthBufferFail,
		nvgStencilOperation ccwStencilPass
	) {
		this.stencilFunc = (CompareFunction) stencilFunc;
		this.stencilFail = (StencilOperation) stencilFail;
		this.stencilDepthBufferFail = (StencilOperation) stencilDepthBufferFail;
		this.stencilPass = (StencilOperation) stencilPass;
		this.ccwStencilFail = (StencilOperation) ccwStencilFail;
		this.ccwStencilDepthBufferFail = (StencilOperation) ccwStencilDepthBufferFail;
		this.ccwStencilPass = (StencilOperation) ccwStencilPass;
	}

	private void ToggleCullMode(IntPtr userdata, int enabled)
	{
		device.RasterizerState = (enabled > 0) ?
			RasterizerState.CullClockwise :
			RasterizerState.CullNone;
	}

	private void ApplyState(IntPtr userdata, IntPtr buffer)
	{
		if (buffer.ToInt64() != 1)
		{
			throw new InvalidOperationException();
		}

		uint blendPack = (
			((uint) srcRGB << 0) |
			((uint) srcAlpha << 4) |
			((uint) dstRGB << 8) |
			((uint) dstAlpha << 12) |
			((uint) enableColorWrite << 16)
		);
		BlendState blendState;
		if (!blendCache.TryGetValue(blendPack, out blendState))
		{
			blendState = new BlendState();
			blendState.ColorSourceBlend = srcRGB;
			blendState.ColorDestinationBlend = dstRGB;
			blendState.AlphaSourceBlend = srcAlpha;
			blendState.AlphaDestinationBlend = dstAlpha;
			blendState.ColorWriteChannels = (enableColorWrite > 0) ?
				ColorWriteChannels.All :
				ColorWriteChannels.None;

			blendCache.Add(blendPack, blendState);
		}
		device.BlendState = blendState;

		DepthStencilState depthStencilState;
		if (stencilEnable)
		{
			uint stencilPack = (
				((uint) stencilFunc << 0) |
				((uint) stencilFail << 3) |
				((uint) stencilDepthBufferFail << 6) |
				((uint) stencilPass << 9) |
				((uint) ccwStencilFail << 12) |
				((uint) ccwStencilDepthBufferFail << 15) |
				((uint) ccwStencilPass << 18)
			);
			if (!depthStencilCache.TryGetValue(stencilPack, out depthStencilState))
			{
				depthStencilState = new DepthStencilState();
				depthStencilState.DepthBufferEnable = false;
				depthStencilState.DepthBufferWriteEnable = false;
				depthStencilState.DepthBufferFunction = CompareFunction.LessEqual;
				depthStencilState.StencilEnable = true;
				depthStencilState.StencilFunction = stencilFunc;
				depthStencilState.StencilPass = stencilPass;
				depthStencilState.StencilFail = stencilFail;
				depthStencilState.StencilDepthBufferFail = stencilDepthBufferFail;
				depthStencilState.TwoSidedStencilMode = true;
				depthStencilState.CounterClockwiseStencilFunction = stencilFunc;
				depthStencilState.CounterClockwiseStencilFail = ccwStencilFail;
				depthStencilState.CounterClockwiseStencilPass = ccwStencilPass;
				depthStencilState.CounterClockwiseStencilDepthBufferFail = ccwStencilDepthBufferFail;
				depthStencilState.StencilMask = 0xFF;
				depthStencilState.StencilWriteMask = 0xFF;
				depthStencilState.ReferenceStencil = 0;
			}
		}
		else
		{
			depthStencilState = DepthStencilState.None;
		}
		device.DepthStencilState = depthStencilState;

		effect.CurrentTechnique.Passes[0].Apply();
		device.SetVertexBuffer(vertexBuffer);
	}

	private void DrawPrimitives(IntPtr userdata, int triStrip, int vertexOffset, int vertexCount)
	{
		PrimitiveType primType;
		int primCount;
		if (triStrip > 0)
		{
			primType = PrimitiveType.TriangleStrip;
			primCount = vertexCount - 2;
		}
		else
		{
			primType = PrimitiveType.TriangleList;
			primCount = vertexCount / 3;
		}
		device.DrawPrimitives(primType, vertexOffset, primCount);
	}

	#endregion

	#region Native Interop

	[DllImport("libsvg4fna", CallingConvention = CallingConvention.Cdecl)]
	private static extern IntPtr nsvgParseFromFile(
		string filename,
		string units,
		float dpi
	);

	[DllImport("libsvg4fna", CallingConvention = CallingConvention.Cdecl)]
	private static extern void nsvgDelete(IntPtr svg);

	const int NVG_ZERO = 1 << 0;
	const int NVG_ONE = 1<<1;
	const int NVG_SRC_COLOR = 1 << 2;
	const int NVG_ONE_MINUS_SRC_COLOR = 1 << 3;
	const int NVG_DST_COLOR = 1 << 4;
	const int NVG_ONE_MINUS_DST_COLOR = 1 << 5;
	const int NVG_SRC_ALPHA = 1 << 6;
	const int NVG_ONE_MINUS_SRC_ALPHA = 1 << 7;
	const int NVG_DST_ALPHA = 1 << 8;
	const int NVG_ONE_MINUS_DST_ALPHA = 1 << 9;
	const int NVG_SRC_ALPHA_SATURATE = 1 << 10;

	[StructLayout(LayoutKind.Sequential)]
	private struct NVGcompositeOperationState
	{
		public int srcRGB;
		public int dstRGB;
		public int srcAlpha;
		public int dstAlpha;
	}

	private enum nvgStencilCompareFunction
	{
		NVG_STENCILCOMPAREFUNCTION_ALWAYS,
		NVG_STENCILCOMPAREFUNCTION_NEVER,
		NVG_STENCILCOMPAREFUNCTION_LESS,
		NVG_STENCILCOMPAREFUNCTION_LESSEQUAL,
		NVG_STENCILCOMPAREFUNCTION_EQUAL,
		NVG_STENCILCOMPAREFUNCTION_GREATEREQUAL,
		NVG_STENCILCOMPAREFUNCTION_GREATER,
		NVG_STENCILCOMPAREFUNCTION_NOTEQUAL
	}

	private enum nvgStencilOperation
	{
		NVG_STENCILOPERATION_KEEP,
		NVG_STENCILOPERATION_ZERO,
		NVG_STENCILOPERATION_REPLACE,
		NVG_STENCILOPERATION_INCREMENT,
		NVG_STENCILOPERATION_DECREMENT,
		NVG_STENCILOPERATION_INCREMENTSATURATION,
		NVG_STENCILOPERATION_DECREMENTSATURATION,
		NVG_STENCILOPERATION_INVERT
	}

	[StructLayout(LayoutKind.Sequential, Pack = 1)]
	private struct NVGvertex : IVertexType
        {
		VertexDeclaration IVertexType.VertexDeclaration
		{
			get
			{
				return VertexDeclaration;
			}
		}

		public static VertexDeclaration VertexDeclaration = new VertexDeclaration(
			new VertexElement[]
			{
				new VertexElement(
					0,
					VertexElementFormat.Vector2,
					VertexElementUsage.Position,
					0
				),
				new VertexElement(
					8,
					VertexElementFormat.Vector2,
					VertexElementUsage.TextureCoordinate,
					0
				)
			}
		);

		public Vector2 Position;
		public Vector2 TextureCoordinate;
	}

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate void nvg_gpu_pfn_createContext(IntPtr userdata);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate void nvg_gpu_pfn_deleteContext(IntPtr userdata);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate IntPtr nvg_gpu_pfn_createVertexBuffer(IntPtr userdata, IntPtr size);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate void nvg_gpu_pfn_deleteVertexBuffer(IntPtr userdata, IntPtr buffer);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate void nvg_gpu_pfn_updateVertexBuffer(IntPtr userdata, IntPtr buffer, IntPtr ptr, int count, IntPtr stride);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate IntPtr nvg_gpu_pfn_createTexture(IntPtr userdata, int isRGBA, int width, int height, int nearest, int repeatX, int repeatY);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate void nvg_gpu_pfn_deleteTexture(IntPtr userdata, IntPtr texture);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate void nvg_gpu_pfn_updateTexture(IntPtr userdata, IntPtr texture, int x, int y, int w, int h, IntPtr data);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate void nvg_gpu_pfn_updateUniformBuffer(IntPtr userdata, IntPtr uniforms, IntPtr uniformLength);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate void nvg_gpu_pfn_updateShader(IntPtr userdata, int enableAA, int fillType, int texType);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate void nvg_gpu_pfn_updateSampler(IntPtr userdata, IntPtr texture);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate void nvg_gpu_pfn_setViewport(IntPtr userdata, float width, float height);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate void nvg_gpu_pfn_resetState(IntPtr userdata);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate void nvg_gpu_pfn_toggleColorWriteMask(IntPtr userdata, int enabled);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate void nvg_gpu_pfn_updateBlendFunction(IntPtr userdata, NVGcompositeOperationState blendOp);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate void nvg_gpu_pfn_toggleStencil(IntPtr userdata, int enabled);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate void nvg_gpu_pfn_updateStencilFunction(
		IntPtr userdata,
		nvgStencilCompareFunction stencilFunc,
		nvgStencilOperation stencilFail,
		nvgStencilOperation stencilDepthBufferFail,
		nvgStencilOperation stencilPass,
		nvgStencilOperation ccwStencilFail,
		nvgStencilOperation ccwStencilDepthBufferFail,
		nvgStencilOperation ccwStencilPass
	);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate void nvg_gpu_pfn_toggleCullMode(IntPtr userdata, int enabled);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate void nvg_gpu_pfn_applyState(IntPtr userdata, IntPtr vertexBuffer);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate void nvg_gpu_pfn_drawPrimitives(IntPtr userdata, int triStrip, int vertexOffset, int vertexCount);

	[DllImport("libsvg4fna", CallingConvention = CallingConvention.Cdecl)]
	private static extern IntPtr nvgGpuCreate(
		IntPtr userdata,
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

	[DllImport("libsvg4fna", CallingConvention = CallingConvention.Cdecl)]
	private static extern void nvgGpuDelete(IntPtr nvg);

	[DllImport("libsvg4fna", CallingConvention = CallingConvention.Cdecl)]
	private static extern void nvgBeginFrame(IntPtr nvg, float width, float height, float ratio);

	[DllImport("libsvg4fna", CallingConvention = CallingConvention.Cdecl)]
	private static extern void nvgEndFrame(IntPtr nvg);

	[DllImport("libsvg4fna", CallingConvention = CallingConvention.Cdecl)]
	private static extern void nvgDrawSVG(IntPtr nvg, IntPtr svg);

	#endregion
}
