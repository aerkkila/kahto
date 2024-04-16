#include <math.h>
#include <stdio.h>
#include <ttra.h>
#include <err.h>
#define using_cplot
#include "cplot.h"

static inline void tocanvas(unsigned *ptr, int value, unsigned color) {
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
    *ptr = *ptr>>24<<24 | c2 << 16 | c1 << 8 | c0 << 0;
}

/* https://en.wikipedia.org/wiki/Bresenham's_line_algorithm */
/* This method is nice because it uses only integers. */
static void draw_line_bresenham(unsigned *canvas, int ystride, const int *xy, unsigned color) {
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
    unsigned *canvas, color;
    int ystride, *xy, ithickness, *axis_area, nosteep, patternlength, axesheight;
    float *pattern;
};

struct _cplot_line_args {
    const short *xypixels;
    long x0, len;
    unsigned *canvas;
    const int ystride, *axis_xywh, axesheight;
};

static unsigned draw_line_bresenham_dashed(struct _cplot_dashed_line_args *args, unsigned carry) {
    int nosteep		= args->nosteep,
	axesheight	= args->axesheight,
	ystride		= args->ystride;
    unsigned *canvas	= args->canvas,
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
	    for (; m0<=end; m0++) {
		if (ipat % 2 == 0)
		    canvas[m0*ystride + n0] = color; // only this line differs
		n0 += D > 0 ? n_add : 0;
		D  += D > 0 ? D_add1 : D_add0;
	    }
	    if (m0 > m1)
		break;
	    ipat = (ipat + 1) % args->patternlength;
	    try = m0 + iroundpos(args->pattern[ipat] * coef);
	}
    }
    return ipat<<16 | (try-m0);
}

/* anti-aliased line */
static void draw_line_xiaolin(unsigned *canvas_, int ystride, const int *xy, unsigned color) {
    unsigned (*canvas)[ystride] = (void*)canvas_;
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

static unsigned draw_line_xiaolin_dashed(struct _cplot_dashed_line_args *args, unsigned carry) {
    int nosteep = args->nosteep,
	axesheight = args->axesheight;
    unsigned (*canvas)[args->ystride] = (void*)args->canvas,
	     color = args->color;

    int backwards = args->xy[2+!nosteep] < args->xy[!nosteep]; // m1 < m0
    int m1=args->xy[2*!backwards+!nosteep], m0=args->xy[2*backwards+!nosteep];
    float fn1=args->xy[2*!backwards+nosteep], fn0=args->xy[2*backwards+nosteep]; // Δm ≥ Δn

    int dm = m1 - m0;
    float dn = fn1 - fn0;
    const float slope = dn / dm;
    const float coef = dm / sqrt(dm*dm + dn*dn); // m-yksikön pituus = coef * hypotenuusa
    unsigned short ipat = carry>>16, try = (carry & 0xffff) + m0;

    if (nosteep) { // (m,n) = (x,y)
	while (1) {
	    int end = min(m1+1, try);
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
	    try = m0 + iroundpos(args->pattern[ipat]*axesheight * coef);
	}
    }
    else { // (m,n) = (y,x)
	while (1) {
	    int end = min(m1+1, try);
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
	    try = m0 + iroundpos(args->pattern[ipat]*axesheight * coef);
	}
    }
    return ipat<<16 | (try-m0);
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

static void _draw_thick_line(unsigned *canvas, int ystride, int xy[4], unsigned color, int ithickness, int *axis_area, int nosteep) {
    if (!check_line(xy, axis_area))
	draw_line_xiaolin(canvas, ystride, xy, color);
    xy[nosteep+0]++;
    xy[nosteep+2]++;
    for (int p=1; p<ithickness-1; p++) {
	if (!check_line(xy, axis_area))
	    draw_line_bresenham(canvas, ystride, xy, color);
	xy[nosteep+0]++;
	xy[nosteep+2]++;
    }
    if (!check_line(xy, axis_area))
	draw_line_xiaolin(canvas, ystride, xy, color);
}

static unsigned _draw_thick_line_dashed(struct _cplot_dashed_line_args *args, unsigned carry) {
    unsigned new_carry = carry;
    if (!check_line(args->xy, args->axis_area))
	new_carry = draw_line_xiaolin_dashed(args, carry);
    args->xy[args->nosteep+0]++;
    args->xy[args->nosteep+2]++;
    for (int p=1; p<args->ithickness-1; p++) {
	if (!check_line(args->xy, args->axis_area))
	    new_carry = draw_line_bresenham_dashed(args, carry);
	args->xy[args->nosteep+0]++;
	args->xy[args->nosteep+2]++;
    }
    if (!check_line(args->xy, args->axis_area))
	new_carry = draw_line_xiaolin_dashed(args, carry);
    return new_carry;
}

static unsigned draw_line(unsigned *canvas, int ystride, const int *xy_c, int *area, struct cplot_linestyle *style, int axesheight, unsigned carry) {
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
    int halfthickness = iroundpos(thickness/2);
    int ithickness = iroundpos(thickness);
    xy[nosteep+0] -= halfthickness;
    xy[nosteep+2] -= halfthickness;

    switch (style->style) {
	case cplot_line_dashed_e:
	    if (!style->pattern) {
		style->pattern = (static float[]){1.0/60, 1.0/60};
		style->patternlen = 2;
	    }
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

static int put_text(struct ttra *ttra, char *text, int x, int y, float xalignment, float yalignment, float rot, int area_out[4], int area_only) {
    int wh[2], x0, y0;

    if ((int)rot % 25)
	fprintf(stderr, "Tekstin pyöräytys on toteutettu vain suorakulmille.\n");

    ttra_get_textdims_pixels(ttra, text, wh+0, wh+1);

    y0 = y + wh[(int)rot%50 == 0] * yalignment; // height if not rotated
    x0 = x + wh[(int)rot%50 != 0] * xalignment; // height if rotated

    int quarter = (int)rot % 50 != 0;
    area_out[0] = x0;
    area_out[1] = y0;
    area_out[2] = x0+wh[quarter];
    area_out[3] = y0+wh[!quarter];
    if (area_only)
	return 0;

    if (y0 < 0 || x0 < 0)
	return -1;

    if ((int)rot % 100) {
	unsigned *canvas = ttra->canvas;
	int width0 = ttra->realw;
	int height0 = ttra->realh;

	if (!(ttra->canvas = malloc(wh[0]*wh[1] * sizeof(unsigned)))) {
	    warn("malloc %i * %i * %zu epäonnistui", wh[0], wh[1], sizeof(unsigned));
	    return 1;
	}
	ttra->realw = wh[0];
	ttra->realh = wh[1];
	ttra->clean_line = 1;
	ttra_set_xy0(ttra, 0, 0);
	ttra_print(ttra, text);
	rotate(canvas+y0*width0 + x0, width0, height0, ttra->canvas, wh[0], wh[1], rot);

	free(ttra->canvas);
	ttra->canvas = canvas;
	ttra->realw = width0;
	ttra->realh = height0;
	ttra->clean_line = 0;
	return 0;
    }

    ttra_set_xy0(ttra, x0, y0);
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

static void init_circle(unsigned char *to, int tow, int toh) {
    int radius = min(tow, toh) / 2;
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

static inline unsigned from_cmap(const unsigned char *ptr) {
    return
	(ptr[0] << 16 ) |
	(ptr[1] << 8 ) |
	(ptr[2] << 0) |
	(0xff << 24);
}

struct draw_data_args {
    unsigned *canvas;
    int ystride;
    const unsigned char *bmap;
    int mapw, maph, x, y;
    const int *axis_xywh;
    unsigned color;

    const short *xypixels;
    long x0;
    int len;
    double xpix_per_unit;
    const unsigned char *zlevels, *cmap;
};

static inline void draw_datum(struct draw_data_args *ar) {
    if (!ar->bmap) {
	if (0 <= ar->x && ar->x < ar->axis_xywh[2] && 0 <= ar->y && ar->y < ar->axis_xywh[3])
	    ar->canvas[(ar->axis_xywh[1] + ar->y) * ar->ystride + ar->axis_xywh[0] + ar->x] = ar->color;
	return;
    }
    for (int j=0; j<ar->maph; j++) {
	int jaxis = ar->y - ar->maph/2 + j;
	if (jaxis < 0 || jaxis >= ar->axis_xywh[3]) continue;
	for (int i=0; i<ar->mapw; i++) {
	    int iaxis = ar->x - ar->mapw/2 + i;
	    if (iaxis < 0 || iaxis >= ar->axis_xywh[2]) continue;
	    int value = ar->bmap[j*ar->mapw + i];
	    unsigned *ptr = &ar->canvas[(ar->axis_xywh[1]+jaxis) * ar->ystride + ar->axis_xywh[0] + iaxis];
	    tocanvas(ptr, value, ar->color);
	}
    }
}

static inline void draw_datum_cmap(struct draw_data_args *restrict ar) {
    if (!ar->bmap) {
	if (0 <= ar->x && ar->x < ar->axis_xywh[2] && 0 <= ar->y && ar->y < ar->axis_xywh[3])
	    ar->canvas[(ar->axis_xywh[1] + ar->y) * ar->ystride + ar->axis_xywh[0] + ar->x] = from_cmap(ar->cmap + 128*3);
	return;
    }
    float colors_per_y = 256.0/ar->maph,
	  colors_per_x = 256.0/ar->mapw;
    for (int j=0; j<ar->maph; j++) {
	int jaxis = ar->y - ar->maph/2 + j;
	if (jaxis < 0 || jaxis >= ar->axis_xywh[3]) continue;
	float ylevel = colors_per_y * j;
	for (int i=0; i<ar->mapw; i++) {
	    int iaxis = ar->x - ar->mapw/2 + i;
	    if (iaxis < 0 || iaxis >= ar->axis_xywh[2]) continue;
	    int value = ar->bmap[j*ar->mapw + i];
	    unsigned *ptr = &ar->canvas[(ar->axis_xywh[1]+jaxis) * ar->ystride + ar->axis_xywh[0] + iaxis];
	    int colorind = iroundpos(0.5 * (ylevel + colors_per_x * i));
	    unsigned color = from_cmap(ar->cmap + colorind*3);
	    tocanvas(ptr, value, color);
	}
    }
}

static void draw_data_y(struct draw_data_args *restrict ar) {
    for (int idata=0; idata<ar->len; idata++) {
	double xd = (ar->x0 + idata) * ar->xpix_per_unit;
	ar->x = iroundpos(xd);
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
    for (int idata=0; idata<ar->len; idata++) {
	ar->x = ar->xypixels[idata*2];
	ar->y = ar->xypixels[idata*2+1];
	ar->color = from_cmap(ar->cmap + ar->zlevels[idata]*3);
	draw_datum(ar);
    }
}

static void draw_data_yc(struct draw_data_args *restrict ar) {
    for (int idata=0; idata<ar->len; idata++) {
	double xd = (ar->x0 + idata) * ar->xpix_per_unit;
	ar->x = iroundpos(xd);
	ar->y = ar->xypixels[idata*2+1];
	ar->color = from_cmap(ar->cmap + ar->zlevels[idata]*3);
	draw_datum(ar);
    }
}

static void connect_data_y(struct _cplot_line_args *restrict args, struct cplot_linestyle *linestyle, double xpix_per_unit) {
    int axis_area[] = xywh_to_area(args->axis_xywh);
    unsigned carry = 0;
    for (int i=0; i<args->len-1; i++) {
	int xy[] = {
	    iroundpos((args->x0+i) * xpix_per_unit) + args->axis_xywh[0],
	    args->xypixels[1] + args->axis_xywh[1],
	    iroundpos((args->x0+i+1) * xpix_per_unit) + args->axis_xywh[0],
	    args->xypixels[3] + args->axis_xywh[1],
	};
	carry = draw_line(args->canvas, args->ystride, xy, axis_area, linestyle, args->axesheight, carry);
	args->xypixels += 2;
    }
}

/* huomio: datan liittäminen kahden tämän funktion kutsun välillä on toteuttamatta */
static void connect_data_xy(struct _cplot_line_args *restrict args, struct cplot_linestyle *linestyle) {
    int axis_area[] = xywh_to_area(args->axis_xywh);

    unsigned carry = 0;
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

static unsigned char* cplot_data_marker_bmap(struct cplot_data *data, unsigned char *bmap, int *has_marker, int *width, int *height) {
    *has_marker = 1;
    if (data->literal_marker) {
	struct ttra *ttra = data->yxaxis[0]->axes->ttra;
	ttra_set_fontheight(ttra, *height);
	bmap = ttra_get_bitmap(ttra, data->marker, width, height);
	/* bitmap is probably smaller than fontheight */
	ttra_set_fontheight(ttra, ttra->fontheight * ttra->fontheight / (float)*height);
	bmap = ttra_get_bitmap(ttra, data->marker, width, height);
    }
    else if (!data->marker)
	*has_marker = 0;
    else switch (*data->marker) {
	case ' ': *has_marker = 0; break;
	case '.': bmap = NULL; break;
	case '+': init_plus(bmap, *width, *height); break;
	default: init_circle(bmap, *width, *height); break;
    }
    return bmap;
}

static void cplot_data_render(struct cplot_data *data, unsigned *canvas, int axeswidth, int axesheight, int ystride) {
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
    const int *xywh = data->yxaxis[0]->axes->ro_inner_xywh;
    int yxlen[] = {xywh[3], xywh[2]};
    const long npoints = 1024;
    short xypixels[npoints*2];
    unsigned char zlevels[npoints];

    int width, height, marker;
    width = height = iroundpos(data->markersize * axesheight);
    unsigned char bmap_buff[width*height];
    unsigned char *bmap = cplot_data_marker_bmap(data, bmap_buff, &marker, &width, &height);

    int line_thickness = iroundpos(data->linestyle.thickness * axesheight);
    if (line_thickness < 1) line_thickness = 1;

    double xpix_per_unit = xywh[2] / yxzdiff[1]; // Used if x is not given.

    struct draw_data_args data_args = {
	.canvas = canvas,
	.ystride = ystride,
	.bmap = bmap,
	.mapw = width,
	.maph = height,
	.axis_xywh = xywh,
	.xpix_per_unit = xpix_per_unit,
	.cmap = data->caxis ? data->caxis->cmap : NULL,
	.color = data->color,
	/*
	 * xypixels, x0, len
	 * zlevels
	 * (x, y), not set in this function
	 */
    };

    for (long istart=0; istart<data->length; ) {
	long iend = min(istart+npoints, data->length);
	if (data->yxzdata[1])
	    get_datapx[data->yxztype[1]](istart, iend, data->yxzdata[1], xypixels+0, yxzmin[1], yxzdiff[1], yxlen[1]);
	if (data->yxzdata[2])
	    get_datalevels[data->yxztype[2]](istart, iend, data->yxzdata[2], zlevels, yxzmin[2], yxzdiff[2], 255);
	get_datapx_inv[data->yxztype[0]](istart, iend, data->yxzdata[0], xypixels+1, yxzmin[0], yxzdiff[0], yxlen[0]);
	if (data->linestyle.style) {
	    struct _cplot_line_args args = {
		.xypixels = xypixels,
		.x0 = istart,
		.len = iend-istart,
		.canvas = canvas,
		.ystride = ystride,
		.axis_xywh = xywh,
		.axesheight = axesheight,
	    };
	    if (data->yxzdata[1])
		connect_data_xy(&args, &data->linestyle);
	    else
		connect_data_y(&args, &data->linestyle, xpix_per_unit);
	}
	if (marker) {
	    data_args.xypixels = xypixels;
	    data_args.x0 = istart;
	    data_args.len = iend-istart;
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
	istart = iend;
    }
}

static void legend_draw_marker(struct cplot_data *data, struct cplot_drawarea area, int x0, int y0, int text_left) {
    int width, height, marker;
    width = height = iroundpos(data->markersize * area.axesheight);
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
	    .axis_xywh = xywh,
	    .bmap = bmap,
	    .mapw = width,
	    .maph = height,
	    .color = data->color,
	    .cmap = data->caxis ? data->caxis->cmap : NULL,
	};
	if (data->yxzdata[2])
	    draw_datum_cmap(&args);
	else
	    draw_datum(&args);
    }
}

void cplot_draw_box(unsigned *canvas, int ystride, int axesheight, int *area, struct cplot_linestyle *linestyle) {
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

void cplot_draw_box_xywh(unsigned *canvas, int ystride, int axesheight, int *xywh, struct cplot_linestyle *linestyle) {
    int area[] = xywh_to_area(xywh);
    cplot_draw_box(canvas, ystride, axesheight, area, linestyle);
}

static void cplot_legend_draw(struct cplot_axes *axes, struct cplot_drawarea area) {
    cplot_draw_box_xywh(area.canvas, area.ystride, area.axesheight, axes->legend.ro_xywh, &axes->legend.borderstyle);
    int leg_x0 = axes->legend.ro_xywh[0];
    int leg_y0 = axes->legend.ro_xywh[1];

    int rowh = ttra_set_fontheight(axes->ttra, iroundpos(axes->legend.rowheight * area.axesheight));
    int linewidth = iroundpos(axes->legend.borderstyle.thickness * area.axesheight);
    ttra_set_xy0(axes->ttra, leg_x0 + axes->legend.ro_text_left + linewidth, leg_y0 + linewidth);
    int text_left = axes->legend.ro_text_left;
    int rownumber = 0;
    for (int i=0; i<axes->ndata; i++) {
	if (!axes->data[i]->label)
	    continue;
	legend_draw_marker(axes->data[i], area, leg_x0 + linewidth + text_left/2, leg_y0 + (rownumber+++0.5)*rowh, text_left);
	/* drawing a literal marker changes fontheight */
	ttra_set_fontheight(axes->ttra, iroundpos(axes->legend.rowheight * area.axesheight));
	if (axes->data[i]->label)
	    ttra_printf(axes->ttra, "%s\n", axes->data[i]->label);
    }
}
