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
	n1=xy[2*!backwards+nosteep],  n0=xy[2*backwards+nosteep];

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

static void draw_thick_line(unsigned *canvas, int ystride, const int *xy_c, unsigned color, float thickness, int *axis_area) {
    int xy[4];
    memcpy(xy, xy_c, sizeof(xy));
    int nosteep = Abs(xy[3] - xy[1]) < Abs(xy[2] - xy[0]);

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

static inline void draw_datum(unsigned *canvas, int ystride,
    const unsigned char *bmap, int mapw, int maph,
    int x, int y, const int *axis_xywh, unsigned color)
{
    if (!bmap) {
	if (0 <= x && x < axis_xywh[2] && 0 <= y && y < axis_xywh[3])
	    canvas[(axis_xywh[1] + y) * ystride + axis_xywh[0] + x] = color;
	return;
    }
    for (int j=0; j<maph; j++) {
	int jaxis = y - maph/2 + j;
	if (jaxis < 0 || jaxis >= axis_xywh[3]) continue;
	for (int i=0; i<mapw; i++) {
	    int iaxis = x - mapw/2 + i;
	    if (iaxis < 0 || iaxis >= axis_xywh[2]) continue;
	    int value = bmap[j*mapw + i];
	    unsigned *ptr = &canvas[(axis_xywh[1]+jaxis) * ystride + axis_xywh[0] + iaxis];
	    tocanvas(ptr, value, color);
	}
    }
}

static void draw_data_x(const short *xypixels, long x0, int len,
    unsigned *canvas, int ystride, const int *axis_xywh,
    unsigned char *bmap, int mapw, int maph, unsigned color, double xpix_per_unit)
{
    for (int idata=0; idata<len; idata++) {
	double xd = (x0 + idata) * xpix_per_unit;
	int x = iroundpos(xd);
	int y = xypixels[idata*2+1];
	draw_datum(canvas, ystride, bmap, mapw, maph, x, y, axis_xywh, color);
    }
}

static void draw_data_xy(const short *xypixels, long x0, int len,
    unsigned *canvas, int ystride, const int *axis_xywh,
    unsigned char *bmap, int mapw, int maph, unsigned color)
{
    for (int idata=0; idata<len; idata++) {
	int x = xypixels[idata*2],
	y = xypixels[idata*2+1];
	draw_datum(canvas, ystride, bmap, mapw, maph, x, y, axis_xywh, color);
    }
}

static void connect_data_x(const short *xypixels, long x0, long len, unsigned *canvas, int ystride, const int *axis_xywh, int thickness, unsigned color, double xpix_per_unit) {
    int axis_area[] = xywh_to_area(axis_xywh);
    for (int i=0; i<len-1; i++) {
	int xy[] = {
	    iroundpos((x0+i) * xpix_per_unit) + axis_xywh[0],
	    xypixels[1] + axis_xywh[1],
	    iroundpos((x0+i+1) * xpix_per_unit) + axis_xywh[0],
	    xypixels[3] + axis_xywh[1],
	};
	draw_thick_line(canvas, ystride, xy, color, thickness, axis_area);
	xypixels += 2;
    }
}

static void connect_data_xy(const short *xypixels, long x0, long len, unsigned *canvas, int ystride, const int *axis_xywh, int thickness, unsigned color) {
    int axis_area[] = xywh_to_area(axis_xywh);
    for (int i=0; i<len-1; i++) {
	int xy[] = {
	    xypixels[0] + axis_xywh[0],
	    xypixels[1] + axis_xywh[1],
	    xypixels[2] + axis_xywh[0],
	    xypixels[3] + axis_xywh[1],
	};
	draw_thick_line(canvas, ystride, xy, color, thickness, axis_area);
	xypixels += 2;
    }
}

static unsigned char* cplot_data_marker_bmap(struct cplot_data *data, unsigned char *bmap, int *has_marker, int *width, int *height) {
    *has_marker = 1;
    if (data->literal_marker) {
	struct ttra *ttra = data->yxaxis[0]->axes->ttra;
	ttra_set_fontheight(ttra, *height);
	bmap = ttra_get_bitmap(ttra, data->marker, width, height);
    }
    else if (!data->marker || data->marker[0] == ' ')
	*has_marker = 0;
    else if (data->marker[0] == '.')
	bmap = NULL;
    else
	init_circle(bmap, *width, *height);
    return bmap;
}

static void cplot_data_render(struct cplot_data *data, unsigned *canvas, int axeswidth, int axesheight, int ystride) {
    double yxmin[] = {data->yxaxis[0]->min, data->yxaxis[1]->min};
    double yxdiff[] = {data->yxaxis[0]->max - yxmin[0], data->yxaxis[1]->max - yxmin[1]};
    const int *xywh = data->yxaxis[0]->axes->ro_inner_xywh;
    int yxlen[] = {xywh[3], xywh[2]};
    const long npoints = 1024;
    short xypixels[npoints*2];

    int width, height, marker;
    width = height = iroundpos(data->markersize * axesheight);
    unsigned char bmap_buff[width*height];
    unsigned char *bmap = cplot_data_marker_bmap(data, bmap_buff, &marker, &width, &height);

    int line_thickness = iroundpos(data->line_thickness * axesheight);
    if (line_thickness < 1) line_thickness = 1;

    double xpix_per_unit = xywh[2] / yxdiff[1]; // Used if x is not given.

    for (long istart=0; istart<data->length; ) {
	long iend = min(istart+npoints, data->length);
	if (data->yxzdata[1])
	    get_datapx[data->yxztype[1]](istart, iend, data->yxzdata[1], xypixels+0, yxmin[1], yxdiff[1], yxlen[1]);
	get_datapx_inv[data->yxztype[0]](istart, iend, data->yxzdata[0], xypixels+1, yxmin[0], yxdiff[0], yxlen[0]);
	if (marker) {
	    if (data->yxzdata[1])
		draw_data_xy(xypixels, istart, iend-istart, canvas, ystride, xywh, bmap, width, height, data->color);
	    else
		draw_data_x(xypixels, istart, iend-istart, canvas, ystride, xywh, bmap, width, height, data->color, xpix_per_unit);
	}
	if (data->linestyle) {
	    if (data->yxzdata[1])
		connect_data_xy(xypixels, istart, iend-istart, canvas, ystride, xywh, line_thickness, data->color);
	    else
		connect_data_x(xypixels, istart, iend-istart, canvas, ystride, xywh, line_thickness, data->color, xpix_per_unit);
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
    if (marker)
	draw_datum(area.canvas, area.ystride, bmap, width, height, x0, y0, xywh, data->color);
    if (data->linestyle) {
	short xypixels[] = {x0 - (text_left+1)/3, y0, x0 + (text_left+1)/3, y0};
	int line_thickness = iroundpos(data->line_thickness * area.axesheight);
	connect_data_xy(xypixels, 0, 2, area.canvas, area.ystride, xywh, line_thickness, data->color);
    }
}

static void cplot_legend(struct cplot_axes *axes, struct cplot_drawarea area) {
    int leg_x0 = axes->legend.ro_xywh[0];
    int leg_y0 = axes->legend.ro_xywh[1];

    int rowh = ttra_set_fontheight(axes->ttra, iroundpos(axes->legend.rowheight * area.axesheight));
    ttra_set_xy0(axes->ttra, leg_x0 + axes->legend.ro_text_left, leg_y0);
    int text_left = axes->legend.ro_text_left;
    for (int i=0; i<axes->ndata; i++) {
	legend_draw_marker(axes->data[i], area, leg_x0 + text_left/2, leg_y0 + (i+0.5)*rowh, text_left);
	if (axes->data[i]->label)
	    ttra_printf(axes->ttra, "%s\n", axes->data[i]->label);
    }
}
