#include <math.h>
#include <stdio.h>
#include <ttra.h>
#include <err.h>
#ifndef CPLOT_NO_VERSION_CHECK
#define CPLOT_NO_VERSION_CHECK
#endif
#include "cplot.h"
#include "definitions.h"

static inline void tocanvas(uint32_t *ptr, int value, uint32_t color) {
	int eulav = 255 - value;
	int fg2 = color >> 16 & 0xff,
		fg1 = color >> 8 & 0xff,
		fg0 = color >> 0 & 0xff,
		bg2 = *ptr >> 16 & 0xff,
		bg1 = *ptr >> 8 & 0xff,
		bg0 = *ptr >> 0 & 0xff;
	int c2 = (fg2 * value + bg2 * eulav) / 255,
		c1 = (fg1 * value + bg1 * eulav) / 255,
		c0 = (fg0 * value + bg0 * eulav) / 255;
	*ptr = (color & 0xff<<24) | c2 << 16 | c1 << 8 | c0 << 0;
}

static inline uint32_t from_cmap(const unsigned char *ptr) {
	return
		(ptr[0] << 16 ) |
		(ptr[1] << 8 ) |
		(ptr[2] << 0) |
		(0xff << 24);
}

#include "cplot_colormesh.c"

struct draw_data_args {
	uint32_t *canvas;
	int ystride;
	const unsigned char *bmap;
	int mapw, maph, x, y;
	const int *axis_xywh_outer;
	uint32_t color;

	short *xypixels;
	long x0;
	int len;
	const unsigned char *zlevels, *cmap;
	int reverse_cmap;
};

static inline void draw_datum(struct draw_data_args *ar) {
	if (!ar->bmap) {
		if (0 <= ar->x && ar->x < ar->axis_xywh_outer[2] && 0 <= ar->y && ar->y < ar->axis_xywh_outer[3])
			ar->canvas[(ar->axis_xywh_outer[1] + ar->y) * ar->ystride + ar->axis_xywh_outer[0] + ar->x] = ar->color;
		return;
	}
	int x0_ = ar->axis_xywh_outer[0],       y0_ = ar->axis_xywh_outer[1];
	int x1_ = x0_ + ar->axis_xywh_outer[2], y1_ = y0_ + ar->axis_xywh_outer[3];
	int x0  = x0_ + ar->x - ar->mapw/2,      y0 = y0_ + ar->y - ar->maph/2;
	int j0  = max(0, y0_ - y0);
	int j1  = min(ar->maph, y1_ - y0);
	int i0  = max(0, x0_ - x0);
	int i1  = min(ar->mapw, x1_ - x0);
	uint32_t (*canvas)[ar->ystride] = (void*)ar->canvas;
	const unsigned char (*bmap)[ar->mapw] = (void*)ar->bmap;
	for (int j=j0, y=y0+j0; j<j1; j++, y++)
		for (int i=i0; i<i1; i++)
			tocanvas(canvas[y]+x0+i, bmap[j][i], ar->color);
}

static inline void draw_datum_for_line(struct draw_data_args *ar) {
	int x0_ = ar->axis_xywh_outer[0],       y0_ = ar->axis_xywh_outer[1];
	int x1_ = x0_ + ar->axis_xywh_outer[2], y1_ = y0_ + ar->axis_xywh_outer[3];
	int x0  = ar->x - ar->mapw/2,           y0 = ar->y - ar->maph/2; // x0_ and x1_ has been already taken into account
	int j0  = max(0, y0_ - y0);
	int j1  = min(ar->maph, y1_ - y0);
	int i0  = max(0, x0_ - x0);
	int i1  = min(ar->mapw, x1_ - x0);
	uint32_t (*canvas)[ar->ystride] = (void*)ar->canvas;
	const unsigned char (*bmap)[ar->mapw] = (void*)ar->bmap;
	for (int j=j0, y=y0+j0; j<j1; j++, y++)
		for (int i=i0; i<i1; i++)
			tocanvas(canvas[y]+x0+i, bmap[j][i], ar->color);
}

static inline void draw_datum_cmap(struct draw_data_args *restrict ar) {
	if (!ar->bmap) {
		if (0 <= ar->x && ar->x < ar->axis_xywh_outer[2] && 0 <= ar->y && ar->y < ar->axis_xywh_outer[3])
			ar->canvas[(ar->axis_xywh_outer[1] + ar->y) * ar->ystride + ar->axis_xywh_outer[0] + ar->x] = from_cmap(ar->cmap + 128*3);
		return;
	}
	float colors_per_y = 256.0/ar->maph,
		  colors_per_x = 256.0/ar->mapw;
	for (int j=0; j<ar->maph; j++) {
		int jaxis = ar->y - ar->maph/2 + j;
		if (jaxis < 0 || jaxis >= ar->axis_xywh_outer[3]) continue;
		float ylevel = colors_per_y * j;
		for (int i=0; i<ar->mapw; i++) {
			int iaxis = ar->x - ar->mapw/2 + i;
			if (iaxis < 0 || iaxis >= ar->axis_xywh_outer[2]) continue;
			int value = ar->bmap[j*ar->mapw + i];
			uint32_t *ptr = &ar->canvas[(ar->axis_xywh_outer[1]+jaxis) * ar->ystride + ar->axis_xywh_outer[0] + iaxis];
			int colorind = iroundpos(0.5 * (ylevel + colors_per_x * i));
			uint32_t color = from_cmap(ar->cmap + colorind*3);
			tocanvas(ptr, value, color);
		}
	}
}

/* https://en.wikipedia.org/wiki/Bresenham's_line_algorithm */
/* This method is nice because it uses only integers. */
static void draw_line_bresenham(uint32_t *canvas, int ystride, const int *xy, uint32_t color) {
	int nosteep = Abs(xy[3] - xy[1]) < Abs(xy[2] - xy[0]);
	int backwards = xy[2+!nosteep] < xy[!nosteep]; // m1 < m0
	int m1=xy[2*!backwards+!nosteep], m0=xy[2*backwards+!nosteep],
	n1=xy[2*!backwards+nosteep],  n0=xy[2*backwards+nosteep]; // Δm ≥ Δn

	const int n_add = n1 > n0 ? 1 : -1;
	const int dm = m1 - m0;
	const int dn = n1 > n0 ? n1 - n0 : n0 - n1;
	const int D_add0 = 2 * dn;
	const int D_add1 = 2 * (dn - dm);
	int D = 2*dn - dm;
	if (nosteep) // (m,n) = (x,y)
		for (; m0<=m1; m0++) {
			canvas[n0*ystride + m0] = color;
			n0 += D > 0 ? n_add : 0;
			D  += D > 0 ? D_add1 : D_add0;
		}
	else // (m,n) = (y,x)
		for (; m0<=m1; m0++) {
			canvas[m0*ystride + n0] = color;
			n0 += D > 0 ? n_add : 0;
			D  += D > 0 ? D_add1 : D_add0;
		}
}

#include "cplot_draw_line.c"

/* like draw_line_bresenham, but instead of a dot, each pixel is used as a center for a circle */
static void _draw_line_circle_e(uint32_t *canvas, int ystride, const int *xy, uint32_t color, struct draw_data_args *args) {
	int nosteep = Abs(xy[3] - xy[1]) < Abs(xy[2] - xy[0]);
	int backwards = xy[2+!nosteep] < xy[!nosteep]; // m1 < m0
	int m1=xy[2*!backwards+!nosteep], m0=xy[2*backwards+!nosteep],
	n1=xy[2*!backwards+nosteep],  n0=xy[2*backwards+nosteep]; // Δm ≥ Δn

	const int n_add = n1 > n0 ? 1 : -1;
	const int dm = m1 - m0;
	const int dn = n1 > n0 ? n1 - n0 : n0 - n1;
	const int D_add0 = 2 * dn;
	const int D_add1 = 2 * (dn - dm);
	int D = 2*dn - dm;
	if (nosteep) // (m,n) = (x,y)
		for (; m0<=m1; m0++) {
			args->x = m0;
			args->y = n0;
			draw_datum_for_line(args);
			n0 += D > 0 ? n_add : 0;
			D  += D > 0 ? D_add1 : D_add0;
		}
	else // (m,n) = (y,x)
		for (; m0<=m1; m0++) {
			args->y = m0;
			args->x = n0;
			draw_datum_for_line(args);
			n0 += D > 0 ? n_add : 0;
			D  += D > 0 ? D_add1 : D_add0;
		}
}

struct _cplot_dashed_line_args {
	uint32_t *canvas, color, *colors;
	int ystride, *xy, ithickness, *axis_area, nosteep, patternlength;
	struct cplot_figure *fig;
	float *pattern;
};

struct _cplot_line_args {
	short *xypixels;
	long x0, len;
	uint32_t *canvas;
	const int ystride, *axis_xywh;
	struct cplot_figure *fig;
};

static uint32_t draw_line_bresenham_dashed(struct _cplot_dashed_line_args *args, uint32_t carry) {
	int nosteep		= args->nosteep,
		ystride		= args->ystride;
	struct cplot_figure *fig = args->fig;
	uint32_t *canvas	     = args->canvas,
			 color           = args->color,
			 *colors         = args->colors,
			 colornow;

	int backwards = args->xy[2+!nosteep] < args->xy[!nosteep]; // m1 < m0
	int m1=args->xy[2*!backwards+!nosteep], m0=args->xy[2*backwards+!nosteep],
	n1=args->xy[2*!backwards+nosteep],  n0=args->xy[2*backwards+nosteep]; // Δm ≥ Δn

	const int n_add = n1 > n0 ? 1 : -1;
	const int dm = m1 - m0;
	const int dn = n1 > n0 ? n1 - n0 : n0 - n1;
	const int D_add0 = 2 * dn;
	const int D_add1 = 2 * (dn - dm);
	int D = 2*dn - dm;
	const float coef = dm / sqrt(dm*dm + dn*dn); // m-yksikön pituus = coef * hypotenuusa
	unsigned short ipat = carry>>16, try = (carry & 0xffff) + m0;
	ipat++; // carry:ssä on ipat-1, jotta oletusarvona toimii 0

#define move_forward	\
	n0 += D > 0 ? n_add : 0,	\
	D  += D > 0 ? D_add1 : D_add0
	if (nosteep) { // (m,n) = (x,y)
		while (1) {
			if (colors) colornow = colors[ipat];
			else colornow = color * (ipat % 2 == 0);
			int end = min(m1+1, try);
			if (colornow)
				for (; m0<end; m0++) {
					canvas[n0*ystride + m0] = colornow; // only this line differs
					move_forward;
				}
			else
				for (; m0<end; m0++)
					move_forward;
			if (m0 > m1)
				break;
			ipat = (ipat + 1) % args->patternlength;
			try = m0 + topixels(args->pattern[ipat]*coef, fig);
		}
	}
	else { // (m,n) = (y,x)
		while (1) {
			if (colors) colornow = colors[ipat];
			else colornow = color * (ipat % 2 == 0);
			int end = min(m1+1, try);
			if (colornow)
				for (; m0<end; m0++) {
					canvas[m0*ystride + n0] = colornow; // only this line differs
					move_forward;
				}
			else
				for (; m0<end; m0++)
					move_forward;
			if (m0 > m1)
				break;
			ipat = (ipat + 1) % args->patternlength;
			try = m0 + topixels(args->pattern[ipat]*coef, fig);
		}
	}
	return (ipat-1)<<16 | (try-m0);
#undef move_forward
}

/* anti-aliased line */
static void draw_line_xiaolin(uint32_t *canvas_, int ystride, const int *xy, uint32_t color) {
	uint32_t (*canvas)[ystride] = (void*)canvas_;
	int nosteep = Abs(xy[3] - xy[1]) < Abs(xy[2] - xy[0]);
	int backwards = xy[2+!nosteep] < xy[!nosteep]; // m1 < m0
	int    m1=xy[2*!backwards+!nosteep], m0=xy[2*backwards+!nosteep];
	float fn1=xy[2*!backwards+nosteep], fn0=xy[2*backwards+nosteep];

	const float slope = (fn1 - fn0) / (m1 - m0);

	if (nosteep) { // (m,n) = (x,y)
		for (; m0<=m1; m0++) {
			int n0 = fn0;
			float off = fn0 - n0;
			if (off >= 0.5) {
				canvas [n0 + 1] [m0] = color;
				tocanvas(&canvas[n0][m0], iroundpos((1-off) * 255), color);
			}
			else {
				canvas[n0][m0] = color;
				tocanvas(&canvas[n0+1][m0], iroundpos(off*255), color);
			}
			fn0 += slope;
		}
	}
	else { // (m,n) = (y,x)
		for (; m0<=m1; m0++) {
			int n0 = fn0;
			float off = fn0 - n0;
			if (off >= 0.5) {
				canvas [m0] [n0 + 1] = color;
				tocanvas(&canvas[m0][n0], iroundpos((1-off) * 255), color);
			}
			else {
				canvas[m0][n0] = color;
				tocanvas(&canvas[m0][n0 + 1], iroundpos(off*255), color);
			}
			fn0 += slope;
		}
	}
}

static uint32_t draw_line_xiaolin_dashed(struct _cplot_dashed_line_args *args, uint32_t carry) {
	int nosteep = args->nosteep;
		struct cplot_figure *fig = args->fig;
	uint32_t (*canvas)[args->ystride] = (void*)args->canvas,
			 color = args->color, *colors = args->colors, colornow;

	int backwards = args->xy[2+!nosteep] < args->xy[!nosteep]; // m1 < m0
	int m1=args->xy[2*!backwards+!nosteep], m0=args->xy[2*backwards+!nosteep];
	float fn1=args->xy[2*!backwards+nosteep], fn0=args->xy[2*backwards+nosteep]; // Δm ≥ Δn

	if (m1 == m0)
		return carry;
	int dm = m1 - m0;
	const float dn = fn1 - fn0;
	const float slope = dn / dm;
	const float coef = dm / sqrt(dm*dm + dn*dn); // m-yksikön pituus = coef * hypotenuusa
	unsigned short ipat = carry>>16, m1dash = (carry & 0xffff) + m0;
	ipat++; // carry:ssä on ipat-1, jotta oletusarvona toimii 0

	if (nosteep) { // (m,n) = (x,y)
		while (1) {
			if (colors) colornow = colors[ipat];
			else colornow = color * (ipat % 2 == 0);
			int end = min(m1+1, m1dash);
			for (; m0<end; m0++) {
				int n0 = fn0;
				int level = iroundpos((fn0 - n0) * 255);
				if (colornow) {
					tocanvas(&canvas[n0][m0], 255-level, colornow);	// only these lines differ
					tocanvas(&canvas[n0+1][m0], level, colornow);	// only these lines differ
				}
				fn0 += slope;
			}
			if (m0 > m1)
				break;
			ipat = (ipat + 1) % args->patternlength;
			int add = topixels(args->pattern[ipat]*coef, fig);
			if (add == 0)
				break; // avoid halting with small figure size
			m1dash = m0 + add;
		}
	}
	else { // (m,n) = (y,x)
		while (1) {
			if (colors) colornow = colors[ipat];
			else colornow = color * (ipat % 2 == 0);
			int end = min(m1+1, m1dash);
			for (; m0<end; m0++) {
				int n0 = fn0;
				int level = iroundpos((fn0 - n0) * 255);
				if (colornow) {
					tocanvas(&canvas[m0][n0], 255-level, colornow);	// only these lines differ
					tocanvas(&canvas[m0][n0+1], level, colornow);	// only these lines differ
				}
				fn0 += slope;
			}
			if (m0 > m1)
				break;
			ipat = (ipat + 1) % args->patternlength;
			int add = topixels(args->pattern[ipat]*coef, fig);
			if (add == 0)
				break; // avoid halting with small figure size
			m1dash = m0 + add;
		}
	}
	return (ipat-1)<<16 | (m1dash-m0);
}

static int check_line(int *line, const int *area) {
	if (line[0] == line[2]) {
		if (line[0] < area[0] || line[0] >= area[2])
			return 1;
		update_max(line[0], area[0]);
		update_max(line[2], area[0]);
		update_max(line[1], area[1]);
		update_max(line[3], area[1]);

		update_min(line[0], area[2]-1);
		update_min(line[2], area[2]-1);
		update_min(line[1], area[3]-1);
		update_min(line[3], area[3]-1);
		return 0;
	}

	float slope = (float)(line[3] - line[1]) / (line[2] - line[0]);
	if (line[0] < area[0]) {
		line[0] = area[0];
		line[1] = iround(line[3] - ((line[2]-line[0]) * slope));
	}
	else if (line[0] >= area[2]) {
		line[0] = area[2]-1;
		line[1] = iround(line[3] - ((line[2]-line[0]) * slope));
	}

	if (line[1] < area[1]) {
		if (slope == 0)
			return 1;
		line[1] = area[1];
		if ((line[0] = iround(line[2] - ((line[3]-line[1]) / slope))) < area[0])
			return 1;
	}
	else if (line[1] >= area[3]) {
		if (slope == 0)
			return 1;
		line[1] = area[3]-1;
		if ((line[0] = iround(line[2] - ((line[3]-line[1]) / slope))) < area[0])
			return 1;
	}

	if (line[2] < area[0]) {
		line[2] = area[0];
		line[3] = iround(line[1] + ((line[2]-line[0]) * slope));
	}
	else if (line[2] >= area[2]) {
		line[2] = area[2]-1;
		line[3] = iround(line[1] + ((line[2]-line[0]) * slope));
	}

	if (line[3] < area[1]) {
		if (slope == 0)
			return 1;
		line[3] = area[1];
		if ((line[2] = iround(line[0] + ((line[3]-line[1]) / slope))) < area[2])
			return 1;
	}
	else if (line[3] >= area[3]) {
		if (slope == 0)
			return 1;
		line[3] = area[3]-1;
		if ((line[2] = iround(line[0] + ((line[3]-line[1]) / slope))) < area[2])
			return 1;
	}
	return 0;
}

static void _draw_thick_line_bresenham_xiaolin(uint32_t *canvas, int ystride,
	int xy[4], uint32_t color, int ithickness, int *axis_area, int nosteep)
{
	if (!check_line(xy, axis_area))
		draw_line_xiaolin(canvas, ystride, xy, color);
	xy[nosteep+0]++;
	xy[nosteep+2]++;
	int p = 1;
	for (; p<ithickness-1; p++) {
		if (!check_line(xy, axis_area))
			draw_line_bresenham(canvas, ystride, xy, color);
		xy[nosteep+0]++;
		xy[nosteep+2]++;
	}
	if (p < ithickness && !check_line(xy, axis_area))
		draw_line_xiaolin(canvas, ystride, xy, color);
}

static uint32_t _draw_thick_line_dashed(struct _cplot_dashed_line_args *args, uint32_t carry) {
	uint32_t new_carry = carry;
	if (!check_line(args->xy, args->axis_area))
		new_carry = draw_line_xiaolin_dashed(args, carry);
	args->xy[args->nosteep+0]++;
	args->xy[args->nosteep+2]++;
	int p = 1;
	for (; p<args->ithickness-1; p++) {
		if (!check_line(args->xy, args->axis_area))
			new_carry = draw_line_bresenham_dashed(args, carry);
		args->xy[args->nosteep+0]++;
		args->xy[args->nosteep+2]++;
	}
	if (p < args->ithickness && !check_line(args->xy, args->axis_area))
		new_carry = draw_line_xiaolin_dashed(args, carry);
	return new_carry;
}

static uint32_t draw_line(uint32_t *canvas, int ystride, const int *xy_c, int *area,
	struct cplot_linestyle *style, struct cplot_figure *fig, struct draw_data_args *dotargs, int32_t carry)
{
	if (style->style == cplot_line_future_e) {
		draw_line_cplot(canvas, ystride, xy_c, style->color, tofpixels(style->thickness, fig),
			area[0], area[2], area[1], area[3]);
		return 0;
	}

	int xy[4];
	memcpy(xy, xy_c, sizeof(xy));
	float nthickness = topixels(style->thickness, fig); // initially just thickness,
	int n_ind = Abs(xy[3] - xy[1]) < Abs(xy[2] - xy[0]);
	// m is the direction which is always incremented (x on non-steep lines)
	// n is incremented only sometimes

	/* Vinon viivan leveys vaakasuunnassa on eri.
	   Yhtälö on johdettu kynällä ja paperilla yhdenmuotoisista kolmioista. */
	int dm = xy[2+!n_ind] - xy[!n_ind];
	int dn = xy[2+n_ind] - xy[n_ind];
	if (dm && dn) {
		float n_per_m = (float)dn / dm;
		nthickness *= sqrt(1 + (n_per_m * n_per_m));
	}

	if (nthickness < 1)
		nthickness = 1;
	int inthickness = iroundpos(nthickness);
	switch (style->align) {
		case 0:
			xy[n_ind+0] -= inthickness/2;
			xy[n_ind+2] -= inthickness/2;
			break;
		default:
		case 1:
			break;
		case -1:
			xy[n_ind+0] -= inthickness;
			xy[n_ind+2] -= inthickness;
			break;
	}

	static float __default_dashpattern[] = {1.0/60, 1.0/60};

	switch (style->style) {
		case cplot_line_dashed_e:
			if (!style->pattern) {
				style->pattern = __default_dashpattern;
				style->patternlen = 2;
			}
			if (!style->patternlen)
				style->patternlen = 2;
			struct _cplot_dashed_line_args args = {
				canvas, style->color, style->colors, ystride, xy, inthickness, area, n_ind,
				style->patternlen, fig, style->pattern };
			carry = _draw_thick_line_dashed(&args, carry);
			break;
		case cplot_line_circle_e:
			if (dotargs) {
				_draw_line_circle_e(canvas, ystride, xy, style->color, dotargs);
				break;
			}
			/* run through */
		default:
		case cplot_line_normal_e:
		case cplot_line_bresenham_xiaolin_e:
			_draw_thick_line_bresenham_xiaolin(canvas, ystride, xy, style->color, inthickness, area, n_ind);
		case cplot_line_none_e:
			break;
	}
	return carry;
}

static int put_text(struct ttra *ttra, const char *text, int x, int y, float xalignment, float yalignment, float rot, int area_out[4], int area_only) {
	int wh[2];
	float farea[4], fw, fh;

	ttra_get_textdims_pixels(ttra, text, wh+0, wh+1);
	get_rotated_area(wh[0], wh[1], farea, rot);
	fw = farea[2] - farea[0];
	fh = farea[3] - farea[1];

	area_out[0] = x + round(fw * xalignment);
	area_out[1] = y + round(fh * yalignment);
	area_out[2] = area_out[0] + round(fw);
	area_out[3] = area_out[1] + round(fh);

	if (area_only)
		return 0;

	if (area_out[0] < 0 || area_out[1] < 0)
		return -1;

	if ((int)rot % 400) {
		uint32_t *canvas = ttra->canvas;
		int width0 = ttra->realw;
		int height0 = ttra->realh;

		if (!(ttra->canvas = malloc(wh[0]*wh[1] * sizeof(uint32_t)))) {
			warn("malloc %i * %i * %zu epäonnistui", wh[0], wh[1], sizeof(uint32_t));
			return 1;
		}
		ttra->realw = wh[0];
		ttra->realh = wh[1];
		ttra->clean_line = 1;
		ttra_set_xy0(ttra, 0, 0);
		ttra_print(ttra, text);
		rotate(canvas, area_out[0], area_out[1], width0, height0, ttra->canvas, wh[0], wh[1], rot);

		free(ttra->canvas);
		ttra->canvas = canvas;
		ttra->realw = width0;
		ttra->realh = height0;
		ttra->clean_line = 0;
		return 0;
	}

	ttra_set_xy0(ttra, area_out[0], area_out[1]);
	ttra_print(ttra, text);
	return 0;
}

static void init_plus(unsigned char *to, int tow, int toh) {
	float rside = min(tow, toh) / 15.0;
	float x0 = tow*0.5 - rside,
		  x1 = tow*0.5 + rside;
	float y0 = toh*0.5 - rside,
		  y1 = toh*0.5 + rside;
	for (int j=0; j<toh; j++) {
		float a = y0 - j;
		float b = j+1 - y1;
		float yfrac = 1.0 - a * (a>0) - b * (b>0);
		for (int i=0; i<tow; i++) {
			float a = x0 - i;
			float b = i+1 - x1;
			float xfrac = 1.0 - a * (a>0) - b * (b>0);
			float frac = max(yfrac, xfrac);
			to[j*tow+i] = frac*255 * (frac >= 0);
		}
	}
}

static void init_triangle(unsigned char *to, int tow, int toh) {
	int odd_tow = tow - (tow % 2 == 0);
	int side = min(odd_tow, toh);
	int rectheight = sin(3.14159265/3) * side;
	int y0 = (toh - rectheight) * 0.5;
	float halfw0 = 0.5;
	float halfw1 = side * 0.5;
	/* halfw0 + (rectheight-1) * addw = halfw1 */
	float addw = (halfw1 - halfw0) / (rectheight - 1);
	memset(to, 0, tow*toh);

	int imid = tow/2;
	float ifloat = halfw0;
	for (int j=y0; j<y0+rectheight; j++) {
		int iint = ifloat - 0.5; // without the central pixel
		float lastfrac = ifloat - 0.5 - iint;
		unsigned char lastval = lastfrac * 255;
		if (lastval) {
			to[j*tow + imid - iint - 1] = lastval;
			to[j*tow + imid + iint + 1] = lastval;
		}
		memset(to + j*tow + imid - iint, 255, iint*2+1);
		ifloat += addw;
	}
}

static void init_circle(unsigned char *to, int tow, int toh) {
	const int size = min(tow, toh);
	const int size16 = size * 16;
	const int r = size16 / 2;
	unsigned char (*to16)[size16] = malloc(size16 * size16);
	/* https://en.wikipedia.org/wiki/Midpoint_circle_algorithm#Jesko's_Method */
	/* Unlike in the method, our circle is always even because of the multiplication with an even number */
	int t1 = r/16, // 16 belongs to the method and is unrelated to size16
		x = r,
		y = 0;
	while (x >= y) {
		int x0 = r-x,
			x1 = r+x,
			y0 = r-y,
			y1 = r+y;
		memset(to16[y0], 0, x0);
		memset(to16[y0]+x0, 1, x1-x0);
		memset(to16[y0]+x1, 0, size16-x1);

		memset(to16[y1-1], 0, x0);
		memset(to16[y1-1]+x0, 1, x1-x0);
		memset(to16[y1-1]+x1, 0, size16-x1);

		memset(to16[x0], 0, y0);
		memset(to16[x0]+y0, 1, y1-y0);
		memset(to16[x0]+y1, 0, size16-y1);

		memset(to16[x1-1], 0, y0);
		memset(to16[x1-1]+y0, 1, y1-y0);
		memset(to16[x1-1]+y1, 0, size16-y1);

		t1 += ++y;
		if (t1 >= x)
			t1 -= x--;
	}

	memset(to, 0, tow*toh);
	for (int j=0; j<size16; j++)
		for (int i=0; i<size16; i++)
			to[j/16*tow+i/16] += to16[j][i] && (j%16 || i%16);
	free(to16);
}

/* Only lines commented as 'different' differ from the init_circle function above */
static void init_4star(unsigned char *to, int tow, int toh) {
	const int size = min(tow, toh);
	const int size16 = size * 16;
	const int r = size16 / 2;
	unsigned char (*to16)[size16] = malloc(size16 * size16);
	/* https://en.wikipedia.org/wiki/Midpoint_circle_algorithm#Jesko's_Method */
	/* Unlike in the method, our circle is always even because of the multiplication with an even number */
	int t1 = r/16, // 16 belongs to the method and is unrelated to size16
		x = r,
		y = 0;
	while (x >= y) {
		int x0 = x,				// different
			x1 = size16 - x,	// different
			y0 = y,				// different
			y1 = size16 - y;	// different
		memset(to16[y0], 0, x0);
		memset(to16[y0]+x0, 1, x1-x0);
		memset(to16[y0]+x1, 0, size16-x1);

		memset(to16[y1-1], 0, x0);
		memset(to16[y1-1]+x0, 1, x1-x0);
		memset(to16[y1-1]+x1, 0, size16-x1);

		memset(to16[x0], 0, y0);
		memset(to16[x0]+y0, 1, y1-y0);
		memset(to16[x0]+y1, 0, size16-y1);

		memset(to16[x1-1], 0, y0);
		memset(to16[x1-1]+y0, 1, y1-y0);
		memset(to16[x1-1]+y1, 0, size16-y1);

		t1 += ++y;
		if (t1 >= x)
			t1 -= x--;
	}

	memset(to, 0, tow*toh);
	for (int j=0; j<size16; j++)
		for (int i=0; i<size16; i++)
			to[j/16*tow+i/16] += to16[j][i] && (j%16 || i%16);
	free(to16);
}

void init_literal(unsigned char *bmap, int w, int h, struct cplot_graph *graph) {
	struct ttra *ttra = graph->yxaxis[0]->figure->ttra;
	/* bitmap is probably smaller than fontheight */
	int bw, bh;
	unsigned char *bm;
	ttra_set_fontheight(ttra, 50);
	ttra_get_bitmap(ttra, graph->markerstyle.marker, &bw, &bh);
	ttra_set_fontheight(ttra, h * ttra->fontheight / (float)bh);

	bm = ttra_get_bitmap(ttra, graph->markerstyle.marker, &bw, &bh);
	memset(bmap, 0, w*h);
	int minw = bw < w ? bw : w;
	int minh = bh < h ? bh : h;
	for (int j=0; j<minh; j++)
		memcpy(bmap + j*w, bm + j*bw, minw);
}

static void draw_data_xy(struct draw_data_args *restrict ar) {
	for (int idata=0; idata<ar->len; idata++) {
		ar->x = ar->xypixels[idata*2];
		ar->y = ar->xypixels[idata*2+1];
		draw_datum(ar);
	}
}

static void draw_data_xyc(struct draw_data_args *restrict ar) {
	if (ar->reverse_cmap)
		for (int idata=0; idata<ar->len; idata++) {
			ar->x = ar->xypixels[idata*2];
			ar->y = ar->xypixels[idata*2+1];
			ar->color = from_cmap(ar->cmap + (255-ar->zlevels[idata])*3);
			draw_datum(ar);
		}
	else
		for (int idata=0; idata<ar->len; idata++) {
			ar->x = ar->xypixels[idata*2];
			ar->y = ar->xypixels[idata*2+1];
			ar->color = from_cmap(ar->cmap + ar->zlevels[idata]*3);
			draw_datum(ar);
		}
}

static void draw_data_xyc_list(struct draw_data_args *restrict ar, const uint32_t *colors, int ncolors) {
	long idata0 = ar->x0;
	for (int idata=0; idata<ar->len; idata++) {
		ar->x = ar->xypixels[idata*2];
		ar->y = ar->xypixels[idata*2+1];
		ar->color = colors[(idata0+idata) % ncolors];
		draw_datum(ar);
	}
}

static void make_datapx(short *xypixels, int stride, long istart, long iend,
	double xpix_per_unit, double axis_x0, int addthis, struct cplot_data_container *xdata)
{
	double xstep = xdata->length > 1 ? (xdata->minmax[1] - xdata->minmax[0]) / (xdata->length-1) : 0;
	double x0 = xdata->minmax[0] - axis_x0;
	for (long idata=istart; idata<iend; idata++)
		xypixels[idata*stride] = iroundpos((x0 + (idata)*xstep) * xpix_per_unit) + addthis;
}

/* huomio: datan liittäminen kahden tämän funktion kutsun välillä on toteuttamatta */
static void connect_data_xy(struct _cplot_line_args *restrict args, struct cplot_linestyle *linestyle, struct draw_data_args *dataargs) {
	int axis_area[] = xywh_to_area(args->axis_xywh);

	uint32_t carry = 0;
	for (int i=0; i<args->len-1; i++, args->xypixels += 2) {
		if (args->xypixels[0] == NOT_A_PIXEL) continue;
		if (args->xypixels[1] == NOT_A_PIXEL) continue;
		if (args->xypixels[2] == NOT_A_PIXEL) continue;
		if (args->xypixels[3] == NOT_A_PIXEL) continue;
		int xy[] = {
			args->xypixels[0] + args->axis_xywh[0],
			args->xypixels[1] + args->axis_xywh[1],
			args->xypixels[2] + args->axis_xywh[0],
			args->xypixels[3] + args->axis_xywh[1],
		};
		carry = draw_line(args->canvas, args->ystride, xy, axis_area, linestyle, args->fig, dataargs, carry);
	}
}

static int cplot_visible_marker(const char *str) {
	return str && (unsigned char)*str > ' ';
}

static int cplot_visible_graph(struct cplot_graph *graph) {
	return cplot_visible_marker(graph->markerstyle.marker) || graph->linestyle.style || graph->errstyle.style;
}

static unsigned char* cplot_data_marker_bmap(struct cplot_graph *graph, unsigned char *bmap, int *has_marker, int *width, int *height) {
	if (!graph->markerstyle.marker) {
		*has_marker = 0;
		return NULL;
	}
	typeof(init_literal) *initfun = init_literal;
	*has_marker = 1;
	if (!graph->markerstyle.literal)
		switch (*graph->markerstyle.marker) {
			case ' ':
			case  0 : *has_marker = 0;
			case '.': return NULL;
			case '+': initfun = (void*)init_plus; break;
			case '^': initfun = (void*)init_triangle; break;
			case '*':
			case '4': initfun = (void*)init_4star; break;
			case 'o': initfun = (void*)init_circle; break;
			default: break;
		}

	initfun(bmap, *width, *height, graph);

	if (graph->markerstyle.nofill) {
		int W = *width, H = *height;
		int w = W * graph->markerstyle.nofill,
			h = *height * graph->markerstyle.nofill;
		int x0 = (W - w) / 2,
			y0 = (H - h) / 2;
		unsigned char *bmap1 = malloc(w * h);
		initfun(bmap1, w, h, graph);
		for (int j=0; j<h; j++)
			for (int i=0; i<w; i++)
				bmap[(j+y0)*W + i+x0] -= bmap1[j*w+i];
		free(bmap1);
	}
	return bmap;
}

void cplot_graph_render(struct cplot_graph *graph, uint32_t *canvas, int ystride, struct cplot_figure *fig, long start) {
	if (is_colormesh(graph))
		return cplot_colormesh_render(graph, canvas, ystride, fig, start);
	double yxmin[] = {
		graph->yxaxis[0]->min,
		graph->yxaxis[1]->min,
	};
	double yxdiff[] = {
		graph->yxaxis[0]->max - yxmin[0],
		graph->yxaxis[1]->max - yxmin[1],
	};
	const int *margin = graph->yxaxis[0]->figure->ro_inner_margin;
	const int *xywh0 = graph->yxaxis[0]->figure->ro_inner_xywh;
	int yxlen[] = {xywh0[3]-margin[1]-margin[3], xywh0[2]-margin[0]-margin[2]};
	const long npoints = 1024;
	short xypixels[npoints*2];
	unsigned char zlevels[npoints];

	int width, height, marker;
	width = height = topixels(graph->markerstyle.size, fig);
	unsigned char bmap_buff[width*height];
	unsigned char *bmap = cplot_data_marker_bmap(graph, bmap_buff, &marker, &width, &height);
	/* bmap points to bmap_buff or is NULL */

	unsigned char *linepen_bmap = NULL;
	int linepen_width, linepen_height, helper;
	linepen_width = linepen_height = topixels(graph->linestyle.thickness, fig);
	unsigned char linepen_buff[linepen_width*linepen_height];
	if (graph->linestyle.style == cplot_line_circle_e) {
		struct cplot_graph copy = *graph;
		copy.markerstyle.marker = "o";
		copy.markerstyle.size = graph->linestyle.thickness;
		copy.markerstyle.literal = copy.markerstyle.nofill = 0;
		linepen_bmap = cplot_data_marker_bmap(&copy, linepen_buff, &helper, &linepen_width, &linepen_height);
	}

	int line_thickness = topixels(graph->linestyle.thickness, fig);
	if (line_thickness < 1) line_thickness = 1;

	struct cplot_axis *caxis = graph->yxaxis[2];

	struct draw_data_args data_args = {
		.canvas = canvas,
		.ystride = ystride,
		.bmap = bmap,
		.mapw = width,
		.maph = height,
		.axis_xywh_outer = xywh0,
		.cmap = caxis ? caxis->cmap : NULL,
		.reverse_cmap = caxis ? caxis->reverse_cmap : 0,
		.color = graph->color,
		/*
		 * xypixels, x0, len
		 * zlevels
		 * (x, y), not set in this function
		 */
	};

	struct draw_data_args linepen_dataargs = data_args;
	linepen_dataargs.bmap = linepen_bmap;
	linepen_dataargs.mapw = linepen_width;
	linepen_dataargs.maph = linepen_height;

	struct cplot_data_container
		*xdata = graph->data.list.xdata,
		*ydata = graph->data.list.ydata,
		*zdata = graph->data.list.zdata;

	long length = graph->data.list.ydata->length;
	for (long istart=start; istart<length; ) {
		long iend = min(istart+npoints, length);
		long num = iend - istart;
		if (xdata->data)
			get_datapx[xdata->type](istart, iend, xdata->data, xypixels+0,
				yxmin[1], yxdiff[1], yxlen[1], xdata->stride, 2, margin[0]);
		else {
			double xpix_per_unit = yxlen[1] / yxdiff[1] * graph->yxzstep[1];
			make_datapx(xypixels+0, 2, istart, iend, xpix_per_unit, yxmin[1], margin[0], xdata);
		}
		if (zdata) {
			if (my_isnan(caxis->center))
				get_datalevels[zdata->type](istart, iend, zdata->data, zlevels,
					caxis->min, caxis->max, 255, zdata->stride);
			else
				get_datalevels_with_center[zdata->type](istart, iend, zdata->data, zlevels,
					caxis->min, caxis->center, caxis->max, 255, zdata->stride);
		}
		get_datapx_inv[ydata->type](istart, iend, ydata->data, xypixels+1, yxmin[0], yxdiff[0], yxlen[0], ydata->stride, 2, margin[1]);
		if (graph->linestyle.style) {
			struct _cplot_line_args args = {
				.xypixels = xypixels,
				.x0 = istart,
				.len = num,
				.canvas = canvas,
				.ystride = ystride,
				.axis_xywh = xywh0,
				.fig = fig,
			};
			connect_data_xy(&args, &graph->linestyle, &linepen_dataargs);
		}
		if (marker) {
			data_args.xypixels = xypixels;
			data_args.x0 = istart;
			data_args.len = num;
			data_args.zlevels = zlevels;
			if (zdata)
				draw_data_xyc(&data_args);
			else if (graph->colors)
				draw_data_xyc_list(&data_args, graph->colors, graph->ncolors);
			else
				draw_data_xy(&data_args);
		}

		/* error bars */
		for (int icoord=0; icoord<2; icoord++) {
			struct cplot_data_container *edata = graph->data.arr[arrlen(graph->data.arr)-2+icoord];
			if (!edata)
				continue;
			const int ixcoord = 0;
			short errb[npoints];
			get_datapx_inv[edata->type](istart, iend, edata->data, errb, yxmin[0], yxdiff[0], yxlen[0], edata->stride, 1, margin[1]);
			int area[] = xywh_to_area(xywh0);
			struct cplot_linestyle style = graph->errstyle;
			for (int i=0; i<iend-istart; i++) {
				int line[] = {xypixels[i*2+ixcoord], errb[i], xypixels[i*2+ixcoord], xypixels[i*2+!ixcoord]};
				if (line[0] == NOT_A_PIXEL) continue;
				if (line[1] == NOT_A_PIXEL) continue;
				if (line[2] == NOT_A_PIXEL) continue;
				if (line[3] == NOT_A_PIXEL) continue;
				line[0] += xywh0[0];
				line[2] += xywh0[0];
				line[1] += xywh0[1];
				line[3] += xywh0[1];
				if (graph->colors)
					style.color = graph->colors[(i+istart) % graph->ncolors];
				draw_line(canvas, ystride, line, area, &style, fig, NULL, 0);
				/*if (!check_line(line, area))
				  draw_line_y(canvas, ystride, line, data->errstyle.color);*/
			}
		}
		istart = iend;
	}
}

static void legend_draw_marker(struct cplot_figure *fig, struct cplot_graph *graph,
	uint32_t *canvas, int ystride, int x0, int y0, int text_left)
{
	int width, height, marker;
	width = height = topixels(graph->markerstyle.size, fig);
	unsigned char bmap_buff[width*height];
	unsigned char *bmap = cplot_data_marker_bmap(graph, bmap_buff, &marker, &width, &height);
	int *xywh = graph->yxaxis[0]->figure->ro_inner_xywh;
	x0 -= xywh[0];
	y0 -= xywh[1];
	struct cplot_axis *caxis = graph->yxaxis[2];
	if (graph->linestyle.style) {
		short xypixels[] = {x0 - (text_left+1)/3, y0, x0 + (text_left+1)/3, y0};
		struct _cplot_line_args args = {
			.xypixels = xypixels,
			.x0 = 0, .len = 2,
			.canvas = canvas,
			.ystride = ystride,
			.axis_xywh = xywh,
			.fig = fig,
		};
		connect_data_xy(&args, &graph->linestyle, NULL);
	}
	if (marker) {
		struct draw_data_args args = {
			.x = x0,
			.y = y0,
			.canvas = canvas,
			.ystride = ystride,
			.axis_xywh_outer = xywh,
			.bmap = bmap,
			.mapw = width,
			.maph = height,
			.color = graph->color,
			.cmap = caxis ? caxis->cmap : NULL,
			.reverse_cmap = caxis ? caxis->reverse_cmap : 0,
		};
		if (graph->data.list.zdata)
			draw_datum_cmap(&args);
		else
			draw_datum(&args);
	}
}

void cplot_draw_box(uint32_t *canvas, int ystride, struct cplot_figure *fig, int *area, struct cplot_linestyle *linestyle) {
	int linewidth = topixels(linestyle->thickness, fig);
	struct cplot_linestyle lstyle = *linestyle;
	lstyle.thickness = -1; // becomes 1 for all fig sizes

	{
		int xy[] = {area[0], area[1], area[0], area[3]};
		for (int i=0; i<linewidth; i++) {
			draw_line(canvas, ystride, xy, area, &lstyle, fig, NULL, 0);
			xy[0]++; xy[2]++;
		}
	} {
		int x1 = area[2] - linewidth;
		int xy[] = {x1, area[1], x1, area[3]};
		for (int i=0; i<linewidth; i++) {
			draw_line(canvas, ystride, xy, area, &lstyle, fig, NULL, 0);
			xy[0]++; xy[2]++;
		}
	} {
		int xy[] = {area[0], area[1], area[2], area[1]};
		for (int i=0; i<linewidth; i++) {
			draw_line(canvas, ystride, xy, area, &lstyle, fig, NULL, 0);
			xy[1]++; xy[3]++;
		}
	} {
		int y1 = area[3] - linewidth;
		int xy[] = {area[0], y1, area[2], y1};
		for (int i=0; i<linewidth; i++) {
			draw_line(canvas, ystride, xy, area, &lstyle, fig, NULL, 0);
			xy[1]++; xy[3]++;
		}
	}
}

void cplot_fill_box(uint32_t *canvas, int ystride, const int *restrict area, uint32_t color) {
	for (int j=area[1]; j<area[3]; j++)
		for (int i=area[0]; i<area[2]; i++)
			canvas[j*ystride+i] = color;
}

void cplot_draw_box_xywh(uint32_t *canvas, int ystride, struct cplot_figure *fig, int *xywh, struct cplot_linestyle *linestyle) {
	int area[] = xywh_to_area(xywh);
	cplot_draw_box(canvas, ystride, fig, area, linestyle);
}

void cplot_fill_box_xywh(uint32_t *canvas, int ystride, int *xywh, uint32_t color) {
	int area[] = xywh_to_area(xywh);
	cplot_fill_box(canvas, ystride, area, color);
}

void cplot_legend_draw(struct cplot_figure *fig, uint32_t *canvas, int ystride) {
	if (!fig->legend.visible || no_room_for_legend(fig) || (fig->legend.visible < 0 && fig->legend.ro_place_err))
		return;
	uint32_t fillcolor = fig->background;
	switch (fig->legend.fill) {
		case cplot_fill_color_e:
			fillcolor = fig->legend.fillcolor;
			/* run through */
		case cplot_fill_bg_e:
			cplot_fill_box_xywh(canvas, ystride, fig->legend.ro_xywh, fillcolor);
			/* run through */
		case cplot_no_fill_e:
			cplot_draw_box_xywh(canvas, ystride, fig, fig->legend.ro_xywh, &fig->legend.borderstyle);
			break;
	}

	int leg_x0 = fig->legend.ro_xywh[0];
	int leg_y0 = fig->legend.ro_xywh[1];
	int linewidth = topixels(fig->legend.borderstyle.thickness, fig);
	/* lisään y:hyn +1:n, että kirjaimen ja viivan väliin jää tyhjä pikseli */
	ttra_set_xy0(fig->ttra, leg_x0 + fig->legend.ro_text_left + linewidth, leg_y0 + linewidth + 1);
	int text_left = fig->legend.ro_text_left;
	for (int i=0; i<fig->ngraph; i++) {
		if (!fig->graph[i]->label)
			continue;
		legend_draw_marker(
			fig, fig->graph[i], canvas, ystride,
			leg_x0 + text_left/2,
			fig->legend.ro_xywh[1] + (fig->legend.ro_datay[i] + fig->legend.ro_datay[i+1]) / 2 + linewidth + 1,
			text_left);
		/* drawing a literal marker changes fontheight */
		if (fig->graph[i]->label) {
			set_fontheight(fig, fig->legend.rowheight);
			uint32_t mem = fig->ttra->bg_default;
			fig->ttra->bg_default = fillcolor;
			ttra_printf(fig->ttra, "\033[0m%s\n", fig->graph[i]->label);
			fig->ttra->bg_default = mem;
		}
	}
}
