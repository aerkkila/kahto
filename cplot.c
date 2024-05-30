#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ttra.h>
#include <float.h>
#include <cmh_colormaps.h>
#include <waylandhelper.h>
#include "cplot.h"
#include "png.c"

#define min(a,b) ((a) < (b) ? a : (b))
#define max(a,b) ((a) > (b) ? a : (b))
#define update_min(a, b) ((a) = min(a,b))
#define update_max(a, b) ((a) = max(a,b))
#define arrlen(a) (sizeof(a) / sizeof(*(a)))
#define RGB(r, g, b) (0xff<<24 | (r)<<16 | (g)<<8 | (b)<<0)
#define xywh_to_area(xywh) {(xywh)[0], (xywh)[1], (xywh)[2]+(xywh)[0], (xywh)[3]+(xywh)[1]}
#define Abs(a) ((a) < 0 ? -(a) : (a))
#define Sign(a) ((a) < 0 ? -1 : 1 * ((a)>0))

#define default_colormap cmh_jet_e

unsigned cplot_colorscheme[] = {
    0xffe41a1c, 0xff377eb8, 0xff4daf4a, 0xff984ea3,
    0xffff7f00, 0xffffff33, 0xffa65628, 0xfff781bf, 0xff999999
};

int cplot_ncolors = arrlen(cplot_colorscheme);

int cplot_default_width = 1200, cplot_default_height = 1000;

static inline int __attribute__((const)) iround(float f) {
    int a = f;
    return a + (f-a >= 0.5) - (a-f > 0.5);
}

static unsigned fg = 0xff<<24;

static inline int __attribute__((const)) iroundpos(float f) {
    int a = f;
    return a + (f-a >= 0.5);
}

static inline int __attribute__((const)) iround1(float f) {
    int a = iroundpos(f);
    return a + (f != 0 && a == 0);
}

static inline int __attribute__((const)) iceil(float f) {
    int a = f;
    return a + (a != f);
}

static inline cplot_f4si __attribute__((pure)) axis_get_line(struct cplot_axis *axis) {
    switch (axis->direction) {
	case 0: return (cplot_f4si){0, axis->pos, 1, axis->pos};
	case 1: return (cplot_f4si){axis->pos, 0, axis->pos, 1};
	default: __builtin_unreachable();
    }
}

static inline float __attribute__((pure)) get_ticks_overlength(struct cplot_ticks *tk) {
    float f = tk->length * (1 + tk->crossaxis) + tk->axis->pos - 1;
    return f * (f>0);
}

static inline float __attribute__((pure)) get_ticks_underlength(struct cplot_ticks *tk) {
    float f = -(tk->crossaxis*tk->length + tk->axis->pos);
    return f * (f>0);
}

static inline float __attribute__((pure)) get_ticks_overaxislength(struct cplot_ticks *tk) {
    return tk->length * (1 + tk->crossaxis);
}

static inline float __attribute__((pure)) get_ticks_underaxislength(struct cplot_ticks *tk) {
    return -(tk->crossaxis*tk->length);
}

void cplot_get_axislabel_xy(struct cplot_axistext *axistext, int xy[2]);
void cplot_get_legend_dims_chars(struct cplot_axes *axes, int *lines, int *cols);
void cplot_get_legend_dims_px(struct cplot_axes *axes, int *y, int *x, int axesheight);
void cplot_find_empty_rectangle(struct cplot_axes *axes, int rwidth, int rheight, int *xout, int *yout);
void cplot_ticks_draw(struct cplot_ticks *ticks, unsigned *canvas, int axeswidth, int axesheight, int ystride);
void cplot_axistext_draw(struct cplot_axistext *axistext, unsigned *canvas, int axeswidth, int axesheight, int ystride);
static void axis_init_range(struct cplot_axis*);

#include "functions.c"
#include "rotate.c"
#include "rendering.c"
#include "ticker.c"
#include "commit.c"

struct cplot_axis* cplot_axis_new(struct cplot_axes *axes, int x_or_y) {
    struct cplot_axis *axis = calloc(1, sizeof(struct cplot_axis));
    axis->axes = axes;
    if (axes->mem_axis < axes->naxis+1)
	axes->axis = realloc(axes->axis, (axes->mem_axis+=2) * sizeof(void*));
    axes->axis[axes->naxis++] = axis;
    memset(axes->axis + axes->naxis, 0, (axes->mem_axis - axes->naxis) * sizeof(void*));
    axis->linestyle.color = fg;
    axis->linestyle.thickness = 1.0 / 400;
    axis->linestyle.style = 1;
    axis->min = 0;
    axis->max = 1;
    axis->direction = x_or_y != 'x';
    axis->ticks = cplot_ticks_new((void*)axis);
    return axis;
}

struct cplot_axis* cplot_coloraxis_new(struct cplot_axes *axes) {
    struct cplot_axis *caxis = calloc(1, sizeof(struct cplot_axis));
    if (axes->mem_axis < axes->naxis+1)
	axes->axis = realloc(axes->axis, (axes->mem_axis+=2) * sizeof(void*));
    axes->axis[axes->naxis++] = caxis;
    memset(axes->axis + axes->naxis, 0, (axes->mem_axis - axes->naxis) * sizeof(void*));
    axes->last_caxis = caxis;
    caxis->axes = axes;
    caxis->max = 1;
    caxis->cmap = cmh_colormaps[default_colormap].map;
    caxis->direction = 1;
    caxis->outside = 1;
    caxis->pos = 1;
    caxis->po[0] = 1;
    caxis->po[1] = 1.0/30;
    caxis->ticks = cplot_ticks_new((void*)caxis);
    return caxis;
}

struct cplot_ticks* cplot_ticks_new(struct cplot_axis *axis) {
    struct cplot_ticks *ticks = calloc(1, sizeof(struct cplot_ticks));
    ticks->axis = axis;
    ticks->color = fg;
    ticks->ticker.init = cplot_init_ticker_default;
    ticks->ticker.ticks = ticks;
    ticks->length = 1.0 / 80;
    ticks->hvalign_text[0] = -0.5;

    ticks->linestyle.thickness = 1.0 / 1200;
    ticks->linestyle.color = RGB(0, 0, 0);
    ticks->linestyle.style = cplot_line_normal_e;

    ticks->gridstyle.thickness = 1.0 / 1200;
    ticks->gridstyle.color = 0xffcccccc;

    ticks->rowheight = 2.4*ticks->length;
    ticks->have_labels = 1;

    ticks->crossaxis = cplot_automatic;
    ticks->hvalign_text[1] = cplot_automatic;
    ticks->ascending = cplot_automatic;

    ticks->crossaxis1 = cplot_automatic;
    ticks->linestyle1.style = cplot_line_normal_e;
    ticks->linestyle1.thickness = 1.0 / 1200;
    ticks->linestyle1.color = 0xff666666;
    ticks->length1 = 1.0 / 110;

    return ticks;
}

struct cplot_axes* cplot_axes_new() {
    struct cplot_axes *axes = calloc(1, sizeof(struct cplot_axes));
    axes->whatisthis = cplot_axes_e;
    axes->background = -1;

    axes->axis = calloc((axes->mem_axis = 4) + 1, sizeof(void*));
    axes->axis[0] = cplot_axis_new(axes, 'x');
    axes->axis[0]->pos = 1; // bottom x-axis
    axes->axis[0]->ticks->gridstyle.style = cplot_line_normal_e;

    axes->axis[1] = cplot_axis_new(axes, 'y');
    axes->axis[1]->pos = 0; // left y-axis
    axes->axis[1]->ticks->gridstyle.style = cplot_line_normal_e;

    axes->wh[0] = cplot_default_width;
    axes->wh[1] = cplot_default_height;

    axes->ttra = calloc(1, sizeof(struct ttra));
    axes->ttra->fonttype = ttra_sans_e;

    axes->legend.rowheight = 1.0 / 40;
    axes->legend.symbolspace_per_rowheight = 1.25;
    axes->legend.posx = 0;
    axes->legend.posy = 1;
    axes->legend.hvalign[1] = -1;
    axes->legend.automatic_placement = 1;
    axes->legend.borderstyle.thickness = 1.0/500;
    axes->legend.borderstyle.style = cplot_line_normal_e;
    axes->legend.borderstyle.color = 0xff<<24;
    axes->legend.fill = cplot_fill_bg_e;

    return axes;
}

void cplot_layout_put_rows_and_cols(struct cplot_layout *layout, int nrows, int ncols) {
    float height = 1.0 / nrows,
	  width = 1.0 / ncols;
    float x[ncols];
    for (int i=0; i<ncols; i++)
	x[i] = i * width;
    float y0 = 0;
    for (int j=0; j<nrows; j++) {
	float y1 = (j+1) * height;
	for (int i=0; i<ncols; i++) {
	    layout->xywh[j*ncols+i][0] = x[i];
	    layout->xywh[j*ncols+i][1] = y0;
	    layout->xywh[j*ncols+i][2] = width;
	    layout->xywh[j*ncols+i][3] = height;
	}
	y0 = y1;
    }
}

struct cplot_layout* cplot_layout_new(int nrows, int ncols) {
    struct cplot_layout *layout = calloc(1, sizeof(struct cplot_layout));
    layout->whatisthis = cplot_layout_e;
    layout->axes = calloc(ncols*nrows, sizeof(void*));
    layout->xywh = calloc(ncols*nrows, sizeof(float[4]));
    layout->naxes = ncols * nrows;
    layout->wh[0] = cplot_default_width;
    layout->wh[1] = cplot_default_height;
    layout->background = -1;
    cplot_layout_put_rows_and_cols(layout, nrows, ncols);
    return layout;
}

static void get_minmax_for_data(struct cplot_data *data, int yxz) {
    int yxz_other = yxz == 2 ? 1 : !yxz;
    switch (data->yxztype[yxz_other]) {
	case cplot_f4:
	    get_minmax_with_float[data->yxztype[yxz]](
		data->yxzdata[yxz], data->length, data->minmax[yxz], data->yxzdata[yxz_other]);
	    break;
	case cplot_f8:
	    get_minmax_with_double[data->yxztype[yxz]](
		data->yxzdata[yxz], data->length, data->minmax[yxz], data->yxzdata[yxz_other]);
	    break;
	default:
	    get_minmax[data->yxztype[yxz]](data->yxzdata[yxz], data->length, data->minmax[yxz]);
	    break;
    }
}

static void get_min_for_data(struct cplot_data *data, int yxz) {
    int yxz_other = yxz == 2 ? 1 : !yxz;
    switch (data->yxztype[yxz_other]) {
	case cplot_f4:
	    data->minmax[yxz][0] =
		get_min_with_float[data->yxztype[yxz]](
		data->yxzdata[yxz], data->length, data->yxzdata[yxz_other]);
	    break;
	case cplot_f8:
	    data->minmax[yxz][0] =
		get_min_with_double[data->yxztype[yxz]](
		data->yxzdata[yxz], data->length, data->yxzdata[yxz_other]);
	    break;
	default:
	    data->minmax[yxz][0] =
		get_min[data->yxztype[yxz]](data->yxzdata[yxz], data->length);
	    break;
    }
}

static void get_max_for_data(struct cplot_data *data, int yxz) {
    int yxz_other = yxz == 2 ? 1 : !yxz;
    switch (data->yxztype[yxz_other]) {
	case cplot_f4:
	    data->minmax[yxz][1] =
		get_max_with_float[data->yxztype[yxz]](
		data->yxzdata[yxz], data->length, data->yxzdata[yxz_other]);
	    break;
	case cplot_f8:
	    data->minmax[yxz][1] =
		get_max_with_double[data->yxztype[yxz]](
		data->yxzdata[yxz], data->length, data->yxzdata[yxz_other]);
	    break;
	default:
	    data->minmax[yxz][1] =
		get_max[data->yxztype[yxz]](data->yxzdata[yxz], data->length);
	    break;
    }
}

static long get_last_for_data(struct cplot_data *data, int yxz) {
    int yxz_other = yxz == 2 ? 1 : !yxz;
    long i = data->length - 1;
    switch (data->yxztype[yxz_other]) {
	case cplot_f4:
	    float *dt4 = data->yxzdata[yxz_other];
	    for (; i>=0; i--)
		if (!my_isnan_float(dt4[i]))
		    return i;
	    return i;
	case cplot_f8:
	    double *dt8 = data->yxzdata[yxz_other];
	    for (; i>=0; i--)
		if (!my_isnan_double(dt8[i]))
		    return i;
	    return i;
	default:
	    return i;
    }
}

static long get_first_for_data(struct cplot_data *data, int yxz) {
    int yxz_other = yxz == 2 ? 1 : !yxz;
    long i = 0;
    switch (data->yxztype[yxz_other]) {
	case cplot_f4:
	    float *dt4 = data->yxzdata[yxz_other];
	    for (; i<data->length; i++)
		if (!my_isnan_float(dt4[i]))
		    return i;
	    return i;
	case cplot_f8:
	    double *dt8 = data->yxzdata[yxz_other];
	    for (; i<data->length; i++)
		if (!my_isnan_double(dt8[i]))
		    return i;
	    return i;
	default:
	    return i;
    }
}

static void axis_init_range(struct cplot_axis *axis) {
    int yxz = axis->direction == 0;
    axis->min = DBL_MAX;
    axis->max = -DBL_MAX;
    for (int idata=0; idata<axis->axes->ndata; idata++) {
	struct cplot_data *data = axis->axes->data[idata];
	if (data->caxis == axis)
	    yxz = 2;
	else if (data->yxaxis[yxz] != axis)
	    continue;

	if (data->yxztype[yxz] == cplot_notype) {
	    if (!(data->have_minmax[yxz] & cplot_minbit))
		data->minmax[yxz][0] = get_first_for_data(data, yxz);
	    if (!(data->have_minmax[yxz] & cplot_maxbit))
		data->minmax[yxz][1] = get_last_for_data(data, yxz);
	}
	else
	    switch (data->have_minmax[yxz]) {
		case 0:
		    get_minmax_for_data(data, yxz);
		    break;
		case cplot_maxbit:
		    get_min_for_data(data, yxz);
		    break;
		case cplot_minbit:
		    get_max_for_data(data, yxz);
		    break;
		default: break;
	    }

	data->have_minmax[yxz] = cplot_minbit | cplot_maxbit;
	update_min(axis->min, data->minmax[yxz][0]);
	update_max(axis->max, data->minmax[yxz][1]);
    }
    axis->range_isset = cplot_minbit | cplot_maxbit;
}

static int rectangle_nexti(int j, int i, int rwidth, int rheight, const short (*right)[], const short (*down)[], int w, int h) {
    const short (*spaceright)[w] = right;
    const short (*spacedown)[w] = down;
    for (int jj=0; jj<rheight; jj++)
	if (spaceright[j+jj][i] < rwidth)
	    return i + spaceright[j+jj][i] + 1;
    for (int ii=0; ii<rwidth; ii++)
	if (spacedown[j][i+ii] < rheight)
	    return i + ii + 1;
    return -1;
}

static int rectangle_nextj(int j, int i, int rwidth, int rheight, const short (*right)[], const short (*down)[], int w, int h) {
    const short (*spaceright)[w] = right;
    const short (*spacedown)[w] = down;
    for (int ii=0; ii<rwidth; ii++)
	if (spacedown[j][i+ii] < rheight)
	    return j + spacedown[j][i+ii] + 1;
    for (int jj=0; jj<rheight; jj++)
	if (spaceright[j+jj][i] < rwidth)
	    return j + jj + 1;
    return -1;
}

void cplot_find_empty_rectangle(struct cplot_axes *axes, int rwidth, int rheight, int *xout, int *yout) {
    int width = axes->wh[0],
	height = axes->wh[1];
    unsigned (*image)[width] = calloc(width * height, sizeof(unsigned));
    for (int i=0; i<axes->ndata; i++)
	cplot_data_render(axes->data[i], (void*)image, width, height, width);

    int x0 = axes->ro_inner_xywh[0],
	y0 = axes->ro_inner_xywh[1],
	w = axes->ro_inner_xywh[2],
	h = axes->ro_inner_xywh[3];
    int x1 = x0 + w,
	y1 = y0 + h;

    short (*spaceright)[w] = malloc(w*h * sizeof(short));
    for (int j=h-1; j>=0; j--)
	spaceright[j][w-1] = !image[j+y0][x1-1];
    for (int j=h-1; j>=0; j--)
	for (int i=w-2; i>=0; i--)
	    spaceright[j][i] = (spaceright[j][i+1] + 1) * !image[j+y0][i+x0];

    short (*spacedown)[w] = malloc(w*h * sizeof(short));
    for (int i=w-1; i>=0; i--)
	spacedown[h-1][i] = !image[y1-1][i+x0];
    for (int j=h-2; j>=0; j--)
	for (int i=w-1; i>=0; i--)
	    spacedown[j][i] = (spacedown[j+1][i] + 1) * !image[j+y0][i+x0];

    *xout = x0, *yout = y0;
    int jpos, ipos, nextpos;

    /* Corners */
    for (int n=0; n<4; n++) {
	jpos = !!(n/2) * (h - rheight);
	ipos = !!(n%2) * (w - rwidth);
	if (rectangle_nexti(jpos, ipos, rwidth, rheight, spaceright, spacedown, w, h) < 0)
	    goto found;
    }

    /* Edges */
    jpos=0;
    for (ipos=0; ipos<=w-rwidth; ipos=nextpos)
	if ((nextpos = rectangle_nexti(jpos, ipos, rwidth, rheight, spaceright, spacedown, w, h)) < 0)
	    goto found;
    jpos = h - rheight;
    for (ipos=0; ipos<=w-rwidth; ipos=nextpos)
	if ((nextpos = rectangle_nexti(jpos, ipos, rwidth, rheight, spaceright, spacedown, w, h)) < 0)
	    goto found;
    ipos = 0;
    for (jpos=0; jpos<=h-rheight; jpos=nextpos)
	if ((nextpos = rectangle_nextj(jpos, ipos, rwidth, rheight, spaceright, spacedown, w, h)) < 0)
	    goto found;
    ipos = w - rwidth;
    for (jpos=0; jpos<=h-rheight; jpos=nextpos)
	if ((nextpos = rectangle_nextj(jpos, ipos, rwidth, rheight, spaceright, spacedown, w, h)) < 0)
	    goto found;

    /* All positions */
    for (jpos=0; jpos<=h-rheight; jpos++)
	for (ipos=0; ipos<=w-rwidth; ipos=nextpos)
	    if ((nextpos = rectangle_nexti(jpos, ipos, rwidth, rheight, spaceright, spacedown, w, h)) < 0)
		goto found;

    goto end;

found:
    *yout = jpos + y0;
    *xout = ipos + x0;
end:
    free(image);
    free(spacedown);
    free(spaceright);
}

void cplot_ticks_draw(struct cplot_ticks *ticks, unsigned *canvas, int axeswidth, int axesheight, int ystride) {
    char tickbuff[32];
    char *tick = tickbuff;
    struct ttra *ttra = NULL;

    if (ticks->have_labels) {
	ttra = ticks->axis->axes->ttra;
	ttra->canvas = canvas;
	ttra->realw = ystride;
	ttra->realh = axesheight;
	ttra->w = ticks->ro_labelarea[2] - ticks->ro_labelarea[0];
	ttra->h = ticks->ro_labelarea[3] - ticks->ro_labelarea[1];
	ttra->fg_default = ticks->color;
	ttra->bg_default = -1;
	ttra_print(ttra, "\033[0m");
	ttra_set_fontheight(ttra, ticks->rowheight*axesheight);
    }

    int isx = ticks->axis->direction == 0;
    int line_px[4];
    line_px[isx+0] = ticks->ro_lines[0];
    line_px[isx+2] = ticks->ro_lines[1];
    int nticks = ticks->ticker.tickerdata.common.nticks;
    int gridline[4];
    int *xywh = ticks->axis->axes->ro_inner_xywh;
    gridline[isx] = xywh[isx];
    gridline[isx+2] = xywh[isx] + xywh[isx+2];
    int inner_area[] = xywh_to_area(xywh);
    int side = ticks->axis->pos >= 0.5;

    const double axisdatamin = ticks->axis->min;
    const double axisdatalen = ticks->axis->max - axisdatamin;
    for (int itick=0; itick<nticks; itick++) {
	double pos_data = ticks->ticker.get_tick(&ticks->ticker, itick, &tick, 32);
	double pos_rel = (pos_data - axisdatamin) / axisdatalen;
	if (!isx)
	    pos_rel = 1 - pos_rel;
	line_px[!isx] = line_px[!isx+2] = xywh[!isx] + iroundpos(pos_rel * xywh[!isx+2]);
	draw_line(canvas, ystride, line_px, ticks->ro_tot_area, &ticks->linestyle, axesheight, 0);
	int area_text[4] = {0};
	if (ttra && tick[0])
	    put_text(ttra, tick, line_px[side*2], line_px[1+side*2], ticks->hvalign_text[!isx], ticks->hvalign_text[isx], ticks->rotation100, area_text, 0);
	if (ticks->gridstyle.style) {
	    gridline[!isx] = gridline[!isx+2] = line_px[!isx];
	    draw_line(canvas, ystride, gridline, inner_area, &ticks->gridstyle, axesheight, 0);
	}
    }

    if (ticks->ticker.get_maxn_subticks) {
	nticks = ticks->ticker.get_maxn_subticks(&ticks->ticker);
	float posdata[nticks];
	nticks = ticks->ticker.get_subticks(&ticks->ticker, posdata);
	line_px[isx+0] = ticks->ro_lines1[0];
	line_px[isx+2] = ticks->ro_lines1[1];
	for (int i=0; i<nticks; i++) {
	    double pos_rel = (posdata[i] - axisdatamin) / axisdatalen;
	    if (!isx)
		pos_rel = 1 - pos_rel;
	    line_px[!isx] = line_px[!isx+2] = xywh[!isx] + iroundpos(pos_rel * xywh[!isx+2]);
	    draw_line(canvas, ystride, line_px, ticks->ro_tot_area, &ticks->linestyle1, axesheight, 0);
	    if (ticks->gridstyle1.style) {
		gridline[!isx] = gridline[!isx+2] = line_px[!isx];
		draw_line(canvas, ystride, gridline, inner_area, &ticks->gridstyle1, axesheight, 0);
	    }
	}
    }
}

void cplot_get_axislabel_xy(struct cplot_axistext *axistext, int xy[2]) {
    int coord = axistext->axis->direction == 1;
    int *axis_tot_area = axistext->axis->ro_linetick_area;
    int axislength = axis_tot_area[coord+2] - axis_tot_area[coord];
    xy[coord] = iroundpos(axis_tot_area[coord] + axislength * axistext->pos);
    xy[!coord] = axis_tot_area[!coord + 2*(axistext->axis->pos >= 0.5)];
}

void cplot_axistext_draw(struct cplot_axistext *axistext, unsigned *canvas, int axeswidth, int axesheight, int ystride) {
    struct ttra *ttra = axistext->axis->axes->ttra;
    ttra->canvas = canvas;
    ttra->realw = ystride;
    ttra->realh = axesheight;
    ttra->w = axistext->ro_area[2] - axistext->ro_area[0];
    ttra->h = axistext->ro_area[3] - axistext->ro_area[1];
    ttra->fg_default = axistext->axis->linestyle.color;
    ttra->bg_default = -1;
    ttra_print(ttra, "\033[0m");
    ttra_set_fontheight(ttra, axistext->rowheight*axesheight);
    int xy[2];
    int coord = axistext->axis->direction == 1;
    int *axis_tot_area = axistext->axis->ro_linetick_area;
    int axislength = axis_tot_area[coord+2] - axis_tot_area[coord];
    xy[coord] = iroundpos(axis_tot_area[coord] + axislength * axistext->pos);
    xy[!coord] = axis_tot_area[!coord + 2*(axistext->axis->pos >= 0.5)];
    put_text(ttra, axistext->text, xy[0], xy[1], axistext->hvalign[coord], axistext->hvalign[!coord], axistext->rotation100, axistext->ro_area, 0);
}

void cplot_axis_draw(struct cplot_axis *axis, unsigned *canvas, int axeswidth, int axesheight, int ystride) {
    if (axis->direction < 0)
	return;
    int isx = axis->direction == 0;
    int length = axis->ro_area[!isx+2] - axis->ro_area[!isx];

    if (axis->cmap) {
	const unsigned char *cmap = axis->cmap;
	unsigned char rcmap[256*3];
	if (axis->reverse_cmap) {
	    for (int i=0; i<256; i++) {
		rcmap[i*3+0] = cmap[(255-i)*3+0];
		rcmap[i*3+1] = cmap[(255-i)*3+1];
		rcmap[i*3+2] = cmap[(255-i)*3+2];
	    }
	    cmap = rcmap;
	}
	if (!isx)
	    for (int j=axis->ro_area[1]; j<axis->ro_area[3]; j++) {
		long ind0 = j*ystride;
		unsigned color = from_cmap(cmap + (length - (j - axis->ro_area[1])) * 255 / length * 3);
		for (int i=axis->ro_area[0]; i<axis->ro_area[2]; i++)
		    canvas[ind0 + i] = color;
	    }
	else
	    for (int j=axis->ro_area[1]; j<axis->ro_area[3]; j++) {
		long ind0 = j*ystride;
		for (int i=axis->ro_area[0]; i<axis->ro_area[2]; i++)
		    canvas[ind0 + i] = from_cmap(cmap + (i - axis->ro_area[0]) * 255 / length * 3);
	    }
    }

    if (axis->linestyle.style != cplot_line_none_e) {
	float thickness = axis->linestyle.thickness * axesheight;
	int area[4] = xywh_to_area(axis->axes->ro_inner_xywh);
	area[isx+2] += (thickness+1)/2;
	area[isx] -= (thickness+1)/2;
	if (area[isx] < 0) area[isx] = 0;
	if (area[isx+2] > axis->axes->wh[isx])
	    area[isx+2] = axis->axes->wh[isx];

	int WH[] = {axeswidth, axesheight};
	if (area[isx+2] > WH[isx]) area[isx+2] = WH[isx];

	draw_line(canvas, ystride, axis->ro_line, area, &axis->linestyle, axesheight, 0);
    }

    if (axis->ticks)
	cplot_ticks_draw(axis->ticks, canvas, axeswidth, axesheight, ystride);

    for (int i=0; i<axis->ntext; i++)
	cplot_axistext_draw(axis->text[i], canvas, axeswidth, axesheight, ystride);
}

void cplot_axes_render(struct cplot_axes *axes, unsigned *canvas, int ystride) {
    unsigned bg = axes->background;
    for (int j=0; j<axes->wh[1]; j++) {
	unsigned ind0 = j * ystride;
	for (int i=0; i<axes->wh[0]; i++)
	    canvas[ind0+i] = bg;
    }
    for (int i=0; i<axes->naxis; i++)
	cplot_axis_draw(axes->axis[i], canvas, axes->wh[0], axes->wh[1], ystride);
    for (int i=0; i<axes->ndata; i++)
	cplot_data_render(axes->data[i], canvas, axes->wh[0], axes->wh[1], ystride);
    cplot_legend_draw(axes, (struct cplot_drawarea){canvas, axes->wh[0], axes->wh[1], ystride});
}

static void init_datastyle(struct cplot_data *data) {
    if (data->markersize == 0)
	data->markersize = 1.0 / 50;
    if (!data->marker)
	data->marker = "o";
    int next_color = 0;
    if (!data->color) {
	data->color = cplot_colorscheme[(data->yxaxis[0]->axes->icolor) % cplot_ncolors];
	next_color = 1;
    }
    if (!data->linestyle.color) {
	data->linestyle.color = cplot_colorscheme[(data->yxaxis[0]->axes->icolor) % cplot_ncolors];
	next_color = 1;
    }
    data->yxaxis[0]->axes->icolor += next_color;
    if (!data->linestyle.thickness)
	data->linestyle.thickness = 1.0 / 600;
}

void cplot_get_legend_dims_chars(struct cplot_axes *axes, int *lines, int *cols) {
    *lines = *cols = 0;
    for (int i=0; i<axes->ndata; i++)
	if (axes->data[i]->label) {
	    int w, h;
	    ttra_get_textdims_chars(axes->data[i]->label, &w, &h);
	    *lines += h;
	    *cols = max(*cols, w);
	}
}

void cplot_get_legend_dims_px(struct cplot_axes *axes, int *y, int *x, int axesheight) {
    *x = *y = 0;
    int rowh = ttra_set_fontheight(axes->ttra, iroundpos(axes->legend.rowheight * axesheight));
    for (int i=0; i<axes->ndata; i++)
	if (axes->data[i]->label) {
	    int w, h;
	    ttra_get_textdims_pixels(axes->ttra, axes->data[i]->label, &w, &h);
	    *y += h;
	    *x = max(*x, w);
	}
    axes->legend.ro_text_left = iroundpos(axes->legend.symbolspace_per_rowheight * rowh);
    *x += axes->legend.ro_text_left;
    int linewidth = iround1(axes->legend.borderstyle.thickness * axesheight);
    *y += linewidth * 2;
    *x += linewidth * 2;
}

static void add_data(struct cplot_args *args) {
    if (args->axes->mem_data < args->axes->ndata+1)
	args->axes->data = realloc(args->axes->data, (args->axes->mem_data = args->axes->ndata+3) * sizeof(void*));
    struct cplot_data *data;
    args->axes->data[args->axes->ndata++] = data = malloc(sizeof(struct cplot_data));
    memcpy(data, &args->ydata, sizeof(struct cplot_data));
    if (!data->yxaxis[0])
	data->yxaxis[0] = cplot_yaxis0(args->axes);
    if (!data->yxaxis[1])
	data->yxaxis[1] = cplot_xaxis0(args->axes);
    if (data->yxzdata[2] && !data->caxis) {
	if (args->axes->last_caxis)
	    data->caxis = args->axes->last_caxis;
	else
	    data->caxis = cplot_coloraxis_new(args->axes);
	data->caxis->range_isset = 0;
    }
    if (data->caxis) {
	if (data->cmap)
	    data->caxis->cmap = data->cmap;
	else if (data->cmh_enum) {
	    if (data->cmh_enum < 0)
		data->caxis->cmap = cmh_colormaps[-data->cmh_enum].map;
	    else
		data->caxis->cmap = cmh_colormaps[data->cmh_enum].map;
	}
	if (data->cmh_enum < 0)
	    data->caxis->reverse_cmap = 1;
    }
    if (!data->yxzdata[1])
	data->yxaxis[1]->ticks->ticker.integers_only = 1;
    data->yxaxis[0]->range_isset = 0;
    data->yxaxis[1]->range_isset = 0;
    init_datastyle(data);
}

struct cplot_axistext* cplot_add_axistext(struct cplot_axis *axis, struct cplot_axistext *text) {
    if (axis->ntext >= axis->mem_text)
	axis->text = realloc(axis->text, (axis->mem_text = axis->ntext + 2) * sizeof(void*));
    axis->text[axis->ntext++] = text;
    return text;
}

struct cplot_axistext* cplot_axislabel(struct cplot_axis *axis, char *label) {
    struct cplot_axistext *text = malloc(sizeof(struct cplot_axistext));
    *text = (struct cplot_axistext) {
	.text = label,
	.pos = 0.5,
	.hvalign = {-0.5, -1.4 * (axis->pos < 0.5)},
	.rowheight = (axis->ticks ? axis->ticks->rowheight : 2.4/80) * 1.3,
	.axis = axis,
	.rotation100 = 75 * (axis->direction == 1),
	.type = cplot_axistext_label,
    };
    return cplot_add_axistext(axis, text);
}

void cplot_destroy_axis(struct cplot_axis *axis) {
    for (int i=axis->ntext-1; i>=0; i--) {
	if (axis->text[i]->owner)
	    free(axis->text[i]->text);
	free(axis->text[i]);
    }
    free(axis->text);
    free(axis->ticks);
    free(axis);
}

void cplot_destroy_axes(struct cplot_axes *axes) {
    for (int i=0; i<axes->naxis; i++)
	cplot_destroy_axis(axes->axis[i]);
    free(axes->axis);
    ttra_destroy(axes->ttra);
    free(axes->ttra);

    for (int i=0; i<axes->ndata; i++) {
	struct cplot_data *data = axes->data[i];
	for (int j=0; j<3; j++)
	    if (data->owner[j])
		free(data->yxzdata[j]);
	free(data);
    }
    free(axes->data);

    memset(axes, 0, sizeof(*axes));
    free(axes);
}

void cplot_destroy(void *axes_or_layout) {
    struct cplot_layout *layout = axes_or_layout;
    if (layout->whatisthis == cplot_axes_e)
	return cplot_destroy_axes(axes_or_layout);
    for (int i=0; i<layout->naxes; i++)
	if (layout->axes[i])
	    cplot_destroy_axes(layout->axes[i]);
    free(layout->axes);
    free(layout->xywh);
    free(layout);
}

struct cplot_axes* cplot_plot_args(struct cplot_args *args) {
    struct cplot_axes *axes =
	args->axes ? args->axes :
	args->yaxis ? args->yaxis->axes :
	args->xaxis ? args->xaxis->axes :
	cplot_axes_new();
    args->axes = axes;
    if (args->ydata)
	add_data(args);
    return axes;
}

void cplot_axes_draw(struct cplot_axes *axes, unsigned *canvas, int ystride) {
    if (cplot_axes_commit(axes))
	return; // too small window to draw
    cplot_axes_render(axes, canvas+axes->startcanvas, ystride);
}

void cplot_xywh_to_pixels(struct cplot_layout *layout, int islot, int px[4]) {
    px[0] = iroundpos(layout->xywh[islot][0] * layout->wh[0]);
    px[1] = iroundpos(layout->xywh[islot][1] * layout->wh[1]);
    px[2] = iroundpos(layout->wh[0] * layout->xywh[islot][2]);
    px[3] = iroundpos(layout->wh[1] * layout->xywh[islot][3]);
    if (px[0] + px[2] > layout->wh[0])
	px[2] = layout->wh[0] - px[0];
    if (px[1] + px[3] > layout->wh[1])
	px[3] = layout->wh[1] - px[1];
}

void cplot_layout_to_axes(struct cplot_layout *layout) {
    for (int i=0; i<layout->naxes; i++) {
	if (!layout->axes[i])
	    continue;
	int xywh_px[4];
	cplot_xywh_to_pixels(layout, i, xywh_px);
	layout->axes[i]->wh[0] = xywh_px[2];
	layout->axes[i]->wh[1] = xywh_px[3];
	layout->axes[i]->startcanvas = xywh_px[1] * layout->wh[0] + xywh_px[0];
    }
}

void cplot_clear_slot(struct cplot_layout *layout, int islot, unsigned *canvas, int ystride) {
    int xywh[4];
    cplot_xywh_to_pixels(layout, islot, xywh);
    const int area[] = xywh_to_area(xywh);
    unsigned color = layout->background;
    for (int j=area[1]; j<area[3]; j++) {
	int ind0 = j * ystride;
	for (int i=area[0]; i<area[2]; i++)
	    canvas[ind0+i] = color;
    }
}

void cplot_draw(void *vplot, unsigned *canvas, int ystride) {
    if (((struct cplot_axes*)vplot)->whatisthis == cplot_axes_e)
	cplot_axes_draw(vplot, canvas, ystride);
    else {
	struct cplot_layout *layout = vplot;
	cplot_layout_to_axes(layout);
	for (int i=0; i<layout->naxes; i++)
	    if (layout->axes[i])
		cplot_axes_draw(layout->axes[i], canvas, ystride);
	    else
		cplot_clear_slot(layout, i, canvas, ystride);
    }
}

void cplot_show(void *vplot) {
    struct cplot_axes *axes = vplot;
    struct waylandhelper wlh = {
	.xres = axes->wh[0],
	.yres = axes->wh[1],
	.xresmin = 20,
	.yresmin = 20,
    };
    wlh_init(&wlh);
    while (!wlh.stop && wlh_roundtrip(&wlh) >= 0) {
	if (wlh.redraw && wlh.can_redraw) {
	    axes->wh[0] = wlh.xres;
	    axes->wh[1] = wlh.yres;
	    cplot_draw(axes, wlh.data, axes->wh[0]);
	    wlh_commit(&wlh);
	}
	usleep(10000);
    }
    wlh_destroy(&wlh);
}
