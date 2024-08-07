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

using Microsoft.Xna.Framework;
using Microsoft.Xna.Framework.Input;
using Microsoft.Xna.Framework.Graphics;

class Example : Game
{
	SVG4FNA svgRenderer;
	SVG4FNA.Image svgImage;

	Example() : base()
	{
		// Load this early to allow for setting the window size on startup
		svgImage = new SVG4FNA.Image("example.svg");

		float windowScale = System.Environment.GetEnvironmentVariable(
			"FNA_GRAPHICS_ENABLE_HIGHDPI"
		) == "1" ? 2.0f : 1.0f;
		GraphicsDeviceManager gdm = new GraphicsDeviceManager(this);
		gdm.PreparingDeviceSettings += OnPreparingDeviceSettings;
		gdm.PreferredBackBufferWidth = (int) (svgImage.Width * 2 * windowScale);
		gdm.PreferredBackBufferHeight = (int) (svgImage.Height * 2 * windowScale);
		IsMouseVisible = true;
		Window.AllowUserResizing = true;
	}

	private void OnPreparingDeviceSettings(object sender, PreparingDeviceSettingsEventArgs e)
	{
		e.GraphicsDeviceInformation.PresentationParameters.DepthStencilFormat = DepthFormat.Depth24Stencil8;
	}

	protected override void LoadContent()
	{
		svgRenderer = new SVG4FNA(GraphicsDevice);

		base.LoadContent();
	}

	protected override void UnloadContent()
	{
		if (svgImage != null)
		{
			svgImage.Dispose();
		}
		if (svgRenderer != null)
		{
			svgRenderer.Dispose();
		}

		base.UnloadContent();
	}

	protected override void Draw(GameTime gameTime)
	{
		GraphicsDevice.Clear(Color.CornflowerBlue);

		Rectangle window = Window.ClientBounds;
		svgRenderer.BeginBatch(
			window.Width,
			window.Height,
			GraphicsDevice.PresentationParameters.BackBufferWidth / (float) window.Width,
			1.0f + Mouse.GetState().ScrollWheelValue / 2400.0f
		);
		svgRenderer.Draw(svgImage);
		svgRenderer.Draw(svgImage, svgImage.Width, svgImage.Height);
		svgRenderer.EndBatch();

		base.Draw(gameTime);
	}

	static void Main(string[] args)
	{
		System.Environment.SetEnvironmentVariable(
			"FNA_GRAPHICS_ENABLE_HIGHDPI",
			"1"
		);
		using (Example e = new Example())
		{
			e.Run();
		}
	}
}
