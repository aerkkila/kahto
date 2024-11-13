#include <math.h>
#include <stdio.h>
#include <ttra.h>
#include <err.h>
#ifndef CPLOT_NO_VERSION_CHECK
#define CPLOT_NO_VERSION_CHECK
#endif
#include "cplot.h"

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
    *ptr = color>>24<<24 | c2 << 16 | c1 << 8 | c0 << 0;
}

static void draw_line_x(uint32_t *canvas, int ystride, const int *xy, uint32_t color) {
    int imin = xy[2] < xy[0];
    int x0 = xy[imin*2];
    int x1 = xy[!imin*2];
    for (int i=x0; i<=x1; i++)
	canvas[xy[1]*ystride + i] = color;
}

static void draw_line_y(uint32_t *canvas, int ystride, const int *xy, uint32_t color) {
    int imin = xy[3] < xy[1];
    int y0 = xy[imin*2+1];
    int y1 = xy[!imin*2+1];
    for (int j=y0; j<=y1; j++)
	canvas[j*ystride + xy[2]] = color;
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

struct _cplot_dashed_line_args {
    uint32_t *canvas, color;
    int ystride, *xy, ithickness, *axis_area, nosteep, patternlength, axesheight;
    float *pattern;
};

struct _cplot_line_args {
    const short *xypixels;
    long x0, len;
    uint32_t *canvas;
    const int ystride, *axis_xywh, axesheight;
    double xpix_per_unit, xpos0;
};

static uint32_t draw_line_bresenham_dashed(struct _cplot_dashed_line_args *args, uint32_t carry) {
    int nosteep		= args->nosteep,
	axesheight	= args->axesheight,
	ystride		= args->ystride;
    uint32_t *canvas	= args->canvas,
	     color	= args->color;

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

    if (nosteep) { // (m,n) = (x,y)
	while (1) {
	    int end = min(m1+1, try);
	    for (; m0<end; m0++) {
		if (ipat % 2 == 0)
		    canvas[n0*ystride + m0] = color; // only this line differs
		n0 += D > 0 ? n_add : 0;
		D  += D > 0 ? D_add1 : D_add0;
	    }
	    if (m0 > m1)
		break;
	    ipat = (ipat + 1) % args->patternlength;
	    try = m0 + iroundpos(args->pattern[ipat]*axesheight * coef);
	}
    }
    else { // (m,n) = (y,x)
	while (1) {
	    int end = min(m1+1, try);
	    for (; m0<end; m0++) {
		if (ipat % 2 == 0)
		    canvas[m0*ystride + n0] = color; // only this line differs
		n0 += D > 0 ? n_add : 0;
		D  += D > 0 ? D_add1 : D_add0;
	    }
	    if (m0 > m1)
		break;
	    ipat = (ipat + 1) % args->patternlength;
	    try = m0 + iroundpos(args->pattern[ipat]*axesheight * coef);
	}
    }
    return (ipat-1)<<16 | (try-m0);
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
	    int level = iroundpos((fn0 - n0) * 255);
	    tocanvas(&canvas[n0][m0], 255-level, color);
	    tocanvas(&canvas[n0+1][m0], level, color);
	    fn0 += slope;
	}
    }
    else { // (m,n) = (y,x)
	for (; m0<=m1; m0++) {
	    int n0 = fn0;
	    int level = iroundpos((fn0 - n0) * 255);
	    tocanvas(&canvas[m0][n0], 255-level, color);
	    tocanvas(&canvas[m0][n0+1], level, color);
	    fn0 += slope;
	}
    }
}

static uint32_t draw_line_xiaolin_dashed(struct _cplot_dashed_line_args *args, uint32_t carry) {
    int nosteep = args->nosteep,
	axesheight = args->axesheight;
    uint32_t (*canvas)[args->ystride] = (void*)args->canvas,
	     color = args->color;

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
	    int end = min(m1+1, m1dash);
	    for (; m0<end; m0++) {
		int n0 = fn0;
		int level = iroundpos((fn0 - n0) * 255);
		if (ipat % 2 == 0) {
		    tocanvas(&canvas[n0][m0], 255-level, color);	// only these lines differ
		    tocanvas(&canvas[n0+1][m0], level, color);		// only these lines differ
		}
		fn0 += slope;
	    }
	    if (m0 > m1)
		break;
	    ipat = (ipat + 1) % args->patternlength;
	    int add = iroundpos(args->pattern[ipat]*axesheight * coef);
	    if (add == 0)
		break; // avoid halting with small axesheight
	    m1dash = m0 + add;
	}
    }
    else { // (m,n) = (y,x)
	while (1) {
	    int end = min(m1+1, m1dash);
	    for (; m0<end; m0++) {
		int n0 = fn0;
		int level = iroundpos((fn0 - n0) * 255);
		if (ipat % 2 == 0) {
		    tocanvas(&canvas[m0][n0], 255-level, color);	// only these lines differ
		    tocanvas(&canvas[m0][n0+1], level, color);		// only these lines differ
		}
		fn0 += slope;
	    }
	    if (m0 > m1)
		break;
	    ipat = (ipat + 1) % args->patternlength;
	    int add = iroundpos(args->pattern[ipat]*axesheight * coef);
	    if (add == 0)
		break; // avoid halting with small axesheight
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
	line[0] = iround(line[2] - ((line[3]-line[1]) / slope));
    }
    else if (line[1] >= area[3]) {
	if (slope == 0)
	    return 1;
	line[1] = area[3]-1;
	line[0] = iround(line[2] - ((line[3]-line[1]) / slope));
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
	line[2] = iround(line[0] + ((line[3]-line[1]) / slope));
    }
    else if (line[3] >= area[3]) {
	if (slope == 0)
	    return 1;
	line[3] = area[3]-1;
	line[2] = iround(line[0] + ((line[3]-line[1]) / slope));
    }
    return 0;
}

static void _draw_thick_line(uint32_t *canvas, int ystride, int xy[4], uint32_t color, int ithickness, int *axis_area, int nosteep) {
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

static uint32_t draw_line(uint32_t *canvas, int ystride, const int *xy_c, int *area, struct cplot_linestyle *style, int axesheight, uint32_t carry) {
    int xy[4];
    memcpy(xy, xy_c, sizeof(xy));
    int nosteep = Abs(xy[3] - xy[1]) < Abs(xy[2] - xy[0]);
    float thickness = style->thickness * axesheight;

    /* Vinon viivan leveys vaakasuunnassa on eri.
       Yhtälö on johdettu kynällä ja paperilla yhdenmuotoisista kolmioista. */
    int dy = xy[2+!nosteep] - xy[!nosteep];
    int dx = xy[2+nosteep] - xy[nosteep];
    if (dx && dy) {
	float x_per_y = (float)dx / dy;
	thickness *= sqrt(1 + (x_per_y * x_per_y));
    }

    if (thickness < 1)
	thickness = 1;
    int ithickness = iroundpos(thickness);
    switch (style->align) {
	case 0:
	    xy[nosteep+0] -= ithickness/2;
	    xy[nosteep+2] -= ithickness/2;
	    break;
	default:
	case 1:
	    break;
	case -1:
	    xy[nosteep+0] -= ithickness;
	    xy[nosteep+2] -= ithickness;
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
		canvas, style->color, ystride, xy, ithickness, area, nosteep,
		style->patternlen, axesheight, style->pattern };
	    carry = _draw_thick_line_dashed(&args, carry);
	    break;
	case cplot_line_normal_e:
	    _draw_thick_line(canvas, ystride, xy, style->color, ithickness, area, nosteep);
	    break;
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

    if ((int)rot % 100) {
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

#if 0
static void init_circle(unsigned char *to, int tow, int toh) {
    int radius = (min(tow, toh)-1) / 2;
    memset(to, 0, tow*toh);
    to += radius * tow + radius;
    int radius2 = radius * radius;
    for (int j=0; j<=radius; j++) {
	int j3 = (j+0.5) * (j+0.5);
	int j2 = j * j;
	for (int i=0; i<=radius; i++) {
	    int i3 = (i+0.5) * (i+0.5);
	    int i2 = i * i;
	    int value = j3 + i3 < radius2 ? 255 :
		j3 + i2 < radius2 || j2 + i3 < radius2 ? 128 :
		j2 + i2 < radius2 ? 80 : 0;
	    to[j*tow + i] = to[-j*tow + i] = to[j*tow - i] = to[-j*tow - i] = value;
	}
    }
}

static void init_circle(unsigned char *to, int tow, int toh) {
    float origo[] = {tow/2., toh/2.};
    float radius = min(tow-1, toh-1) / 2.; // -1 because measured at the center of the pixel: -(2*0.5)
    float radius2 = radius * radius;
    /* x = sqrt(r2 - y2) */
    for (int j=0; j<toh; j++) {
	float dy2 = j+0.5 - origo[1];
	dy2 *= dy2;
	float dx = sqrt(radius2 - dy2);
	float xleft = origo[0] - dx;
	float xright = origo[0] + dx;
	int ixleft = xleft; // points at the fractional pixel
	int ixright = xright; // points at the fractional pixel
	memset(to+j*tow, 0, ixleft);
	float part = xleft - ixleft;
	to[j*tow+ixleft] = iroundpos(part*255);
	memset(to+j*tow+ixleft+1, 255, ixright-(ixleft+1));
	part = xright - ixright;
	to[j*tow+ixright] = iroundpos(part*255);
	memset(to+j*tow+ixright+1, 0, tow-(ixright+1));
    }
}
#endif

static void init_circle(unsigned char *to, int tow, int toh) {
    int tow16 = tow*16,
	toh16 = toh*16;
    unsigned char (*to16)[tow16] = malloc(tow16 * toh16);
    int r = (min(tow16, toh16)-1) / 2;
    int t1 = r/16,
	x = r,
	y = 0;
    while (x >= y) {
	int x0 = r-x,
	    x1 = r+x,
	    y0 = r-y,
	    y1 = r+y;
	memset(to16[y0], 0, x0);
	memset(to16[y0]+x0, 1, x1-x0);
	memset(to16[y0]+x1, 0, tow16-x1);

	memset(to16[y1-1], 0, x0);
	memset(to16[y1-1]+x0, 1, x1-x0);
	memset(to16[y1-1]+x1, 0, tow16-x1);

	memset(to16[x0], 0, y0);
	memset(to16[x0]+y0, 1, y1-y0);
	memset(to16[x0]+y1, 0, tow16-y1);

	memset(to16[x1-1], 0, y0);
	memset(to16[x1-1]+y0, 1, y1-y0);
	memset(to16[x1-1]+y1, 0, tow16-y1);

	t1 += ++y;
	if (t1 >= x)
	    t1 -= x--;
    }
    memset(to16[2*r+1], 0, (toh16-(2*r+1)) * sizeof(to16[0]));

    memset(to, 0, tow*toh);
    for (int j=0; j<tow16; j++)
	for (int i=0; i<tow16; i++)
	    to[j/16*tow+i/16] += to16[j][i] && (j%16 || i%16);
    free(to16);
}

static void init_4star(unsigned char *to, int tow, int toh) {
    int tow16 = tow*16,
	toh16 = toh*16;
    unsigned char (*to16)[tow16] = malloc(tow16 * toh16);
    int r = (min(tow16, toh16)-1) / 2;
    int t1 = r/16,
	x = r,
	y = 0;
    int size = 2*r+1;
    while (x >= y) {
	int x0 = x,
	    x1 = size - x,
	    y0 = y,
	    y1 = size - y;
	memset(to16[y0], 0, x0);
	memset(to16[y0]+x0, 1, x1-x0);
	memset(to16[y0]+x1, 0, tow16-x1);

	memset(to16[y1-1], 0, x0);
	memset(to16[y1-1]+x0, 1, x1-x0);
	memset(to16[y1-1]+x1, 0, tow16-x1);

	memset(to16[x0], 0, y0);
	memset(to16[x0]+y0, 1, y1-y0);
	memset(to16[x0]+y1, 0, tow16-y1);

	memset(to16[x1-1], 0, y0);
	memset(to16[x1-1]+y0, 1, y1-y0);
	memset(to16[x1-1]+y1, 0, tow16-y1);

	t1 += ++y;
	if (t1 >= x)
	    t1 -= x--;
    }
    memset(to16[2*r+1], 0, (toh16-(2*r+1)) * sizeof(to16[0]));

    memset(to, 0, tow*toh);
    for (int j=0; j<toh16; j++)
	for (int i=0; i<tow16; i++)
	    to[j/16*tow+i/16] += to16[j][i] && (j%16 || i%16);
    free(to16);
}

static inline uint32_t from_cmap(const unsigned char *ptr) {
    return
	(ptr[0] << 16 ) |
	(ptr[1] << 8 ) |
	(ptr[2] << 0) |
	(0xff << 24);
}

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
    double xpix_per_unit, xpos0; // used if xdata is not given
    const unsigned char *zlevels, *cmap;
    int reverse_cmap;
};

static inline void draw_datum(struct draw_data_args *ar) {
    if (!ar->bmap) {
	if (0 <= ar->x && ar->x < ar->axis_xywh_outer[2] && 0 <= ar->y && ar->y < ar->axis_xywh_outer[3])
	    ar->canvas[(ar->axis_xywh_outer[1] + ar->y) * ar->ystride + ar->axis_xywh_outer[0] + ar->x] = ar->color;
	return;
    }
    for (int j=0; j<ar->maph; j++) {
	int jaxis = ar->y - ar->maph/2 + j;
	if (jaxis < 0 || jaxis >= ar->axis_xywh_outer[3]) continue;
	for (int i=0; i<ar->mapw; i++) {
	    int iaxis = ar->x - ar->mapw/2 + i;
	    if (iaxis < 0 || iaxis >= ar->axis_xywh_outer[2]) continue;
	    int value = ar->bmap[j*ar->mapw + i];
	    uint32_t *ptr = &ar->canvas[(ar->axis_xywh_outer[1]+jaxis) * ar->ystride + ar->axis_xywh_outer[0] + iaxis];
	    tocanvas(ptr, value, ar->color);
	}
    }
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

static void draw_data_y(struct draw_data_args *restrict ar) {
    for (int idata=0; idata<ar->len; idata++) {
	double xd = (ar->x0 + idata) * ar->xpix_per_unit;
	ar->x = ar->xypixels[idata*2+0] = iroundpos(xd) + ar->xpos0;
	ar->y = ar->xypixels[idata*2+1];
	draw_datum(ar);
    }
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

static void draw_data_yc(struct draw_data_args *restrict ar) {
    if (ar->reverse_cmap)
	for (int idata=0; idata<ar->len; idata++) {
	    double xd = (ar->x0 + idata) * ar->xpix_per_unit;
	    ar->x = ar->xypixels[idata*2+0] = iroundpos(xd) + ar->xpos0;
	    ar->y = ar->xypixels[idata*2+1];
	    ar->color = from_cmap(ar->cmap + (255-ar->zlevels[idata])*3);
	    draw_datum(ar);
	}
    else
	for (int idata=0; idata<ar->len; idata++) {
	    double xd = (ar->x0 + idata) * ar->xpix_per_unit;
	    ar->x = ar->xypixels[idata*2+0] = iroundpos(xd) + ar->xpos0;
	    ar->y = ar->xypixels[idata*2+1];
	    ar->color = from_cmap(ar->cmap + ar->zlevels[idata]*3);
	    draw_datum(ar);
	}
}

static void connect_data_y(struct _cplot_line_args *restrict args, struct cplot_linestyle *linestyle) {
    int axis_area[] = xywh_to_area(args->axis_xywh);
    uint32_t carry = 0;
    for (int i=0; i<args->len-1; i++) {
	int xy[] = {
	    iroundpos((args->x0+i) * args->xpix_per_unit) + args->xpos0 + args->axis_xywh[0],
	    args->xypixels[1] + args->axis_xywh[1],
	    iroundpos((args->x0+i+1) * args->xpix_per_unit) + args->xpos0 + args->axis_xywh[0],
	    args->xypixels[3] + args->axis_xywh[1],
	};
	carry = draw_line(args->canvas, args->ystride, xy, axis_area, linestyle, args->axesheight, carry);
	args->xypixels += 2;
    }
}

/* huomio: datan liittäminen kahden tämän funktion kutsun välillä on toteuttamatta */
static void connect_data_xy(struct _cplot_line_args *restrict args, struct cplot_linestyle *linestyle) {
    int axis_area[] = xywh_to_area(args->axis_xywh);

    uint32_t carry = 0;
    for (int i=0; i<args->len-1; i++) {
	int xy[] = {
	    args->xypixels[0] + args->axis_xywh[0],
	    args->xypixels[1] + args->axis_xywh[1],
	    args->xypixels[2] + args->axis_xywh[0],
	    args->xypixels[3] + args->axis_xywh[1],
	};
	carry = draw_line(args->canvas, args->ystride, xy, axis_area, linestyle, args->axesheight, carry);
	args->xypixels += 2;
    }
}

static int cplot_visible_marker(const char *str) {
    return (unsigned char)*str > ' ';
}

static unsigned char* cplot_data_marker_bmap(struct cplot_data *data, unsigned char *bmap, int *has_marker, int *width, int *height) {
    void literal_initfun(unsigned char *bmap, int w, int h) {
	struct ttra *ttra = data->yxaxis[0]->axes->ttra;
	/* bitmap is probably smaller than fontheight */
	int bw, bh;
	unsigned char *bm;
	ttra_set_fontheight(ttra, 50);
	ttra_get_bitmap(ttra, data->markerstyle.marker, &bw, &bh);
	ttra_set_fontheight(ttra, h * ttra->fontheight / (float)bh);

	bm = ttra_get_bitmap(ttra, data->markerstyle.marker, &bw, &bh);
	memset(bmap, 0, w*h);
	int minw = bw < w ? bw : w;
	int minh = bh < h ? bh : h;
	for (int j=0; j<minh; j++)
	    memcpy(bmap + j*w, bm + j*bw, minw);
    }

    *has_marker = 1;
    typeof(init_circle) *initfun = literal_initfun;
    if (!data->markerstyle.marker) {
	*has_marker = 0;
	return NULL;
    }
    else if (!data->markerstyle.literal)
	switch (*data->markerstyle.marker) {
	    case ' ':
	    case  0 : *has_marker = 0;
	    case '.': return NULL;
	    case '+': initfun = init_plus; break;
	    case '^': initfun = init_triangle; break;
	    case '*':
	    case '4': initfun = init_4star; break;
	    case 'o': initfun = init_circle; break;
	    default: break;
	}

    initfun(bmap, *width, *height);

    if (data->markerstyle.nofill) {
	int W = *width, H = *height;
	int w = W * data->markerstyle.nofill,
	    h = *height * data->markerstyle.nofill;
	int x0 = (W - w) / 2,
	    y0 = (H - h) / 2;
	unsigned char *bmap1 = malloc(w * h);
	initfun(bmap1, w, h);
	for (int j=0; j<h; j++)
	    for (int i=0; i<w; i++)
		bmap[(j+y0)*W + i+x0] -= bmap1[j*w+i];
	free(bmap1);
    }
    return bmap;
}

void cplot_data_render(struct cplot_data *data, uint32_t *canvas, int ystride, int axeswidth, int axesheight, long start) {
    double yxzmin[] = {
	data->yxaxis[0]->min,
	data->yxaxis[1]->min,
	data->caxis ? data->caxis->min : 0,
    };
    double yxzdiff[] = {
	data->yxaxis[0]->max - yxzmin[0],
	data->yxaxis[1]->max - yxzmin[1],
	data->caxis ? data->caxis->max - yxzmin[2] : 0,
    };
    const int *margin = data->yxaxis[0]->axes->ro_inner_margin;
    const int *xywh0 = data->yxaxis[0]->axes->ro_inner_xywh;
    int yxlen[] = {xywh0[3]-margin[1]-margin[3], xywh0[2]-margin[0]-margin[2]};
    const long npoints = 1024;
    short xypixels[npoints*2];
    unsigned char zlevels[npoints];

    int width, height, marker;
    width = height = iroundpos(data->markerstyle.size * axesheight);
    unsigned char bmap_buff[width*height];
    unsigned char *bmap = cplot_data_marker_bmap(data, bmap_buff, &marker, &width, &height);

    int line_thickness = iroundpos(data->linestyle.thickness * axesheight);
    if (line_thickness < 1) line_thickness = 1;

    double xpix_per_unit = yxlen[1] / yxzdiff[1]; // Used if x is not given.

    struct draw_data_args data_args = {
	.canvas = canvas,
	.ystride = ystride,
	.bmap = bmap,
	.mapw = width,
	.maph = height,
	.axis_xywh_outer = xywh0,
	.xpix_per_unit = xpix_per_unit,
	.xpos0 = iround((data->yxz0[1]-data->yxaxis[1]->min) * xpix_per_unit) + margin[0],
	.cmap = data->caxis ? data->caxis->cmap : NULL,
	.reverse_cmap = data->caxis ? data->caxis->reverse_cmap : 0,
	.color = data->color,
	/*
	 * xypixels, x0, len
	 * zlevels
	 * (x, y), not set in this function
	 */
    };

    for (long istart=start; istart<data->length; ) {
	long iend = min(istart+npoints, data->length);
	long num = iend - istart;
	if (data->yxzdata[1])
	    get_datapx[data->yxztype[1]](istart, iend, data->yxzdata[1], xypixels+0, yxzmin[1], yxzdiff[1], yxlen[1], data->yxzstride[1], 2, margin[0]);
	if (data->yxzdata[2])
	    get_datalevels[data->yxztype[2]](istart, iend, data->yxzdata[2], zlevels, yxzmin[2], yxzdiff[2], 255, data->yxzstride[2]);
	get_datapx_inv[data->yxztype[0]](istart, iend, data->yxzdata[0], xypixels+1, yxzmin[0], yxzdiff[0], yxlen[0], data->yxzstride[0], 2, margin[1]);
	if (data->linestyle.style) {
	    struct _cplot_line_args args = {
		.xypixels = xypixels,
		.x0 = istart,
		.len = num,
		.canvas = canvas,
		.ystride = ystride,
		.axis_xywh = xywh0,
		.axesheight = axesheight,
		.xpix_per_unit = data_args.xpix_per_unit,
		.xpos0 = data_args.xpos0,
	    };
	    if (data->yxzdata[1])
		connect_data_xy(&args, &data->linestyle);
	    else
		connect_data_y(&args, &data->linestyle);
	}
	if (marker) {
	    data_args.xypixels = xypixels;
	    data_args.x0 = istart;
	    data_args.len = num;
	    data_args.zlevels = zlevels;
	    if (data->yxzdata[1]) {
		if (data->yxzdata[2])
		    draw_data_xyc(&data_args);
		else
		    draw_data_xy(&data_args);
	    }
	    else {
		if (data->yxzdata[2])
		    draw_data_yc(&data_args);
		else
		    draw_data_y(&data_args);
	    }
	}
	for (int icoord=0; icoord<2; icoord++) {
	    if (!data->err.yx[icoord])
		continue;
	    const int ixcoord = 0;
	    short errb[npoints];
	    get_datapx_inv[data->yxztype[0]](istart, iend, data->err.yx[icoord], errb, yxzmin[0], yxzdiff[0], yxlen[0], data->yxzstride[0], 1, margin[1]);
	    int area[] = xywh_to_area(xywh0);
	    for (int i=0; i<iend-istart; i++) {
		int line[] = {xypixels[i*2+ixcoord], errb[i], xypixels[i*2+ixcoord], xypixels[i*2+!ixcoord]};
		line[0] += xywh0[0];
		line[2] += xywh0[0];
		line[1] += xywh0[1];
		line[3] += xywh0[1];
		draw_line(canvas, ystride, line, area, &data->errstyle, axesheight, 0);
		/*if (!check_line(line, area))
		    draw_line_y(canvas, ystride, line, data->errstyle.color);*/
	    }
	}
	istart = iend;
    }
}

static void legend_draw_marker(struct cplot_data *data, struct cplot_drawarea area, int x0, int y0, int text_left) {
    int width, height, marker;
    width = height = iroundpos(data->markerstyle.size * area.axesheight);
    unsigned char bmap_buff[width*height];
    unsigned char *bmap = cplot_data_marker_bmap(data, bmap_buff, &marker, &width, &height);
    int *xywh = data->yxaxis[0]->axes->ro_inner_xywh;
    x0 -= xywh[0];
    y0 -= xywh[1];
    if (data->linestyle.style) {
	short xypixels[] = {x0 - (text_left+1)/3, y0, x0 + (text_left+1)/3, y0};
	struct _cplot_line_args args = {
	    .xypixels = xypixels,
	    .x0 = 0, .len = 2,
	    .canvas = area.canvas,
	    .ystride = area.ystride,
	    .axis_xywh = xywh,
	    .axesheight = area.axesheight,
	};
	connect_data_xy(&args, &data->linestyle);
    }
    if (marker) {
	struct draw_data_args args = {
	    .x = x0,
	    .y = y0,
	    .canvas = area.canvas,
	    .ystride = area.ystride,
	    .axis_xywh_outer = xywh,
	    .bmap = bmap,
	    .mapw = width,
	    .maph = height,
	    .color = data->color,
	    .cmap = data->caxis ? data->caxis->cmap : NULL,
	    .reverse_cmap = data->caxis ? data->caxis->reverse_cmap : 0,
	};
	if (data->yxzdata[2])
	    draw_datum_cmap(&args);
	else
	    draw_datum(&args);
    }
}

void cplot_draw_box(uint32_t *canvas, int ystride, int axesheight, int *area, struct cplot_linestyle *linestyle) {
    int linewidth = iround1(linestyle->thickness * axesheight);
    struct cplot_linestyle lstyle = *linestyle;
    lstyle.thickness = 1.0/axesheight;

    {
	int xy[] = {area[0], area[1], area[0], area[3]};
	for (int i=0; i<linewidth; i++) {
	    draw_line(canvas, ystride, xy, area, &lstyle, axesheight, 0);
	    xy[0]++; xy[2]++;
	}
    } {
	int x1 = area[2] - linewidth;
	int xy[] = {x1, area[1], x1, area[3]};
	for (int i=0; i<linewidth; i++) {
	    draw_line(canvas, ystride, xy, area, &lstyle, axesheight, 0);
	    xy[0]++; xy[2]++;
	}
    } {
	int xy[] = {area[0], area[1], area[2], area[1]};
	for (int i=0; i<linewidth; i++) {
	    draw_line(canvas, ystride, xy, area, &lstyle, axesheight, 0);
	    xy[1]++; xy[3]++;
	}
    } {
	int y1 = area[3] - linewidth;
	int xy[] = {area[0], y1, area[2], y1};
	for (int i=0; i<linewidth; i++) {
	    draw_line(canvas, ystride, xy, area, &lstyle, axesheight, 0);
	    xy[1]++; xy[3]++;
	}
    }
}

void cplot_fill_box(uint32_t *canvas, int ystride, int axesheight, const int *restrict area, uint32_t color) {
    for (int j=area[1]; j<area[3]; j++)
	for (int i=area[0]; i<area[2]; i++)
	    canvas[j*ystride+i] = color;
}

void cplot_draw_box_xywh(uint32_t *canvas, int ystride, int axesheight, int *xywh, struct cplot_linestyle *linestyle) {
    int area[] = xywh_to_area(xywh);
    cplot_draw_box(canvas, ystride, axesheight, area, linestyle);
}

void cplot_fill_box_xywh(uint32_t *canvas, int ystride, int axesheight, int *xywh, uint32_t color) {
    int area[] = xywh_to_area(xywh);
    cplot_fill_box(canvas, ystride, axesheight, area, color);
}

void cplot_legend_draw(struct cplot_axes *axes, struct cplot_drawarea area) {
    if (!axes->legend.visible || no_room_for_legend(axes) || (axes->legend.visible < 0 && !axes->legend.ro_place_err))
	return;
    uint32_t fillcolor = axes->background;
    switch (axes->legend.fill) {
	case cplot_fill_color_e:
	    fillcolor = axes->legend.fillcolor;
	    /* run through */
	case cplot_fill_bg_e:
	    cplot_fill_box_xywh(area.canvas, area.ystride, area.axesheight, axes->legend.ro_xywh, fillcolor);
	    /* run through */
	case cplot_no_fill_e:
	    cplot_draw_box_xywh(area.canvas, area.ystride, area.axesheight, axes->legend.ro_xywh, &axes->legend.borderstyle);
	    break;
    }

    int leg_x0 = axes->legend.ro_xywh[0];
    int leg_y0 = axes->legend.ro_xywh[1];
    int rowh = ttra_set_fontheight(axes->ttra, iroundpos(axes->legend.rowheight * area.axesheight));
    int linewidth = iroundpos(axes->legend.borderstyle.thickness * area.axesheight);
    /* lisään y:hyn +1:n, että kirjaimen ja viivan väliin jää tyhjä pikseli */
    ttra_set_xy0(axes->ttra, leg_x0 + axes->legend.ro_text_left + linewidth, leg_y0 + linewidth + 1);
    int text_left = axes->legend.ro_text_left;
    int rownumber = 0;
    for (int i=0; i<axes->ndata; i++) {
	if (!axes->data[i]->label)
	    continue;
	legend_draw_marker(axes->data[i], area,
	    leg_x0 + linewidth + text_left/2,
	    leg_y0 + linewidth + (rownumber+++0.5)*rowh, text_left);
	/* drawing a literal marker changes fontheight */
	if (axes->data[i]->label) {
	    ttra_set_fontheight(axes->ttra, iroundpos(axes->legend.rowheight * area.axesheight));
	    uint32_t mem = axes->ttra->bg_default;
	    axes->ttra->bg_default = fillcolor;
	    ttra_printf(axes->ttra, "\033[0m%s\n", axes->data[i]->label);
	    axes->ttra->bg_default = mem;
	}
    }
}
