#include "nanovg_svg.h"

#include <assert.h>
#include <math.h>

// SVG renderer based on old nanosvg report:
// https://github.com/memononen/nanosvg/issues/58

typedef struct Vec
{
	float x;
	float y;
} Vec;

#define VEC_MINUS(dst, a, b) \
	dst.x = a.x - b.x; \
	dst.y = a.y - b.y;

static NVGcolor getNVGColor(unsigned int color)
{
	return nvgRGBA(
		(color >> 0) & 0xff,
		(color >> 8) & 0xff,
		(color >> 16) & 0xff,
		(color >> 24) & 0xff
	);
}

static NVGpaint getPaint(NVGcontext *vg, NSVGpaint *p)
{
	NSVGgradient *g;
	NVGcolor icol, ocol;
	float inverse[6];
	Vec s, e;

	assert(p->type == NSVG_PAINT_LINEAR_GRADIENT || p->type == NSVG_PAINT_RADIAL_GRADIENT);
	g = p->gradient;
	assert(g->nstops >= 1);
	icol = getNVGColor(g->stops[0].color);
	ocol = getNVGColor(g->stops[g->nstops - 1].color);

	nvgTransformInverse(inverse, g->xform);

	// FIXME: Is it always the case that the gradient should be transformed from (0, 0) to (0, 1)?
	nvgTransformPoint(&s.x, &s.y, inverse, 0, 0);
	nvgTransformPoint(&e.x, &e.y, inverse, 0, 1);

	if (p->type == NSVG_PAINT_LINEAR_GRADIENT)
	{
		return nvgLinearGradient(vg, s.x, s.y, e.x, e.y, icol, ocol);
	}
	else
	{
		return nvgRadialGradient(vg, s.x, s.y, 0.0, 160, icol, ocol);
	}
}

static float getLineCrossing(Vec p0, Vec p1, Vec p2, Vec p3)
{
	Vec b, d, e;
	float m;

	VEC_MINUS(b, p2, p0)
	VEC_MINUS(d, p1, p0)
	VEC_MINUS(e, p3, p2)
	m = d.x * e.y - d.y * e.x;

	// Check if lines are parallel, or if either pair of points are equal
	if (fabsf(m) < 1e-6)
	{
		return NAN;
	}
	return -(d.x * b.y - d.y * b.x) / m;
}

void nvgDrawSVG(NVGcontext *vg, NSVGimage *svg)
{
	// Iterate shape linked list
	for (NSVGshape *shape = svg->shapes; shape; shape = shape->next)
	{
		// Visibility
		if (!(shape->flags & NSVG_FLAGS_VISIBLE))
		{
			continue;
		}

		nvgSave(vg);

		// Opacity
		if (shape->opacity < 1.0)
		{
			nvgGlobalAlpha(vg, shape->opacity);
		}

		// Build path
		nvgBeginPath(vg);

		// Iterate path linked list
		for (NSVGpath *path = shape->paths; path; path = path->next)
		{
			nvgMoveTo(vg, path->pts[0], path->pts[1]);
			for (int i = 1; i < path->npts; i += 3)
			{
				float *p = &path->pts[2*i];
				nvgBezierTo(vg, p[0], p[1], p[2], p[3], p[4], p[5]);
				// nvgLineTo(vg, p[4], p[5]);
			}

			// Close path
			if (path->closed)
			{
				nvgClosePath(vg);
			}

			// Compute whether this is a hole or a solid.
			// Assume that no paths are crossing (usually true for normal SVG graphics).
			// Also assume that the topology is the same if we use straight lines rather than Beziers (not always the case but usually true).
			// Using the even-odd fill rule, if we draw a line from a point on the path to a point outside the boundary (e.g. top left) and count the number of times it crosses another path, the parity of this count determines whether the path is a hole (odd) or solid (even).
			int crossings = 0;
			Vec p0, p1;
			p0.x = path->pts[0];
			p0.y = path->pts[1];
			p1.x = path->bounds[0] - 1.0;
			p1.y = path->bounds[1] - 1.0;
			// Iterate all other paths
			for (NSVGpath *path2 = shape->paths; path2; path2 = path2->next)
			{
				if (path2 == path)
				{
					continue;
				}

				// Iterate all lines on the path
				if (path2->npts < 4)
				{
					continue;
				}
				for (int i = 1; i < path2->npts + 3; i += 3)
				{
					float *p = &path2->pts[2*i];
					Vec p2, p3;
					// The previous point
					p2.x = p[-2];
					p2.y = p[-1];
					// The current point
					if (i < path2->npts)
					{
						p3.x = p[4];
						p3.y = p[5];
					}
					else
					{
						p3.x = path2->pts[0];
						p3.y = path2->pts[1];
					}
					float crossing = getLineCrossing(p0, p1, p2, p3);
					float crossing2 = getLineCrossing(p2, p3, p0, p1);
					if (0.0 <= crossing && crossing < 1.0 && 0.0 <= crossing2)
					{
						crossings++;
					}
				}
			}

			if (crossings % 2 == 0)
			{
				nvgPathWinding(vg, NVG_SOLID);
			}
			else
			{
				nvgPathWinding(vg, NVG_HOLE);
			}
		}

		// Fill shape
		if (shape->fill.type)
		{
			switch (shape->fill.type)
			{
				case NSVG_PAINT_COLOR:
				{
					NVGcolor color = getNVGColor(shape->fill.color);
					nvgFillColor(vg, color);
					break;
				}
				case NSVG_PAINT_LINEAR_GRADIENT:
				case NSVG_PAINT_RADIAL_GRADIENT:
				{
					nvgFillPaint(vg, getPaint(vg, &shape->fill));
					break;
				}
			}
			nvgFill(vg);
		}

		// Stroke shape
		if (shape->stroke.type)
		{
			nvgStrokeWidth(vg, shape->strokeWidth);
			// strokeDashOffset, strokeDashArray, strokeDashCount not yet supported
			nvgLineCap(vg, shape->strokeLineCap);
			nvgLineJoin(vg, (int) shape->strokeLineJoin);

			switch (shape->stroke.type)
			{
				case NSVG_PAINT_COLOR:
				{
					NVGcolor color = getNVGColor(shape->stroke.color);
					nvgStrokeColor(vg, color);
					break;
				}
				case NSVG_PAINT_LINEAR_GRADIENT:
				{
					// NSVGgradient *g = shape->stroke.gradient;
					// printf("		lin grad: %f\t%f\n", g->fx, g->fy);
					break;
				}
			}
			nvgStroke(vg);
		}

		nvgRestore(vg);
	}
}
