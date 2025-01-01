#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ttra.h>
#include <float.h>
#include <cmh_colormaps.h>
#include <sys/time.h>
#include <math.h>
#ifdef HAVE_wlh
#include <waylandhelper.h>
#endif
#define CPLOT_NO_VERSION_CHECK
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

const int __cplot_version_in_library = __cplot_version_in_program;

unsigned cplot_colorscheme_a[] = { // color deficient semi friendly, semi dark
    0xff000000, 0xfff20700, 0xff505eff, 0xfff781bf, 0xff108a4f, 0xff66ccff, 0xffffc73a,
    0xff986eff, 0xffffff33, 0xffa65628, 0xff999999, 0,
};
unsigned cplot_colorscheme_b[] = { // color deficient friendly, semi dark
    0xff000000, 0xff505eff, 0xff337538, 0xffc26a77, /*0xff9f4a96, */0xff56b4e9, 0,
};
unsigned *cplot_colorschemes[] = {
    cplot_colorscheme_a,
    cplot_colorscheme_b,
};
int cplot_ncolors[] = {
    arrlen(cplot_colorscheme_a) - 1,
    arrlen(cplot_colorscheme_b) - 1,
};

unsigned char __attribute__((malloc))* cplot_colorscheme_cmap(unsigned *colorscheme, int ncolors) {
    unsigned char *cmap = malloc(256*3);
    int n = 256 / ncolors;
    for (int i=0; i<ncolors; i++)
	for (int ii=0; ii<n; ii++) {
	    unsigned char *rgb = cmap + i*n*3 + ii*3;
	    rgb[0] = colorscheme[i] >> 16 & 0xff;
	    rgb[1] = colorscheme[i] >> 8 & 0xff;
	    rgb[2] = colorscheme[i] >> 0 & 0xff;
	}
    for (int i=ncolors*n; i<256; i++) {
	unsigned char *rgb = cmap + i*3;
	rgb[0] = colorscheme[ncolors-1] >> 16 & 0xff;
	rgb[1] = colorscheme[ncolors-1] >> 8 & 0xff;
	rgb[2] = colorscheme[ncolors-1] >> 0 & 0xff;
    }
    return cmap;
}

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

#define no_room_for_legend(axes) ((axes)->legend.ro_place_err < 0)
#define topixels(flength, axes) iroundpos((flength) * (axes)->wh[1])

void cplot_get_legend_dims_chars(struct cplot_axes *axes, int *lines, int *cols);
void cplot_get_legend_dims_px(struct cplot_axes *axes, int *y, int *x, int axesheight);
int cplot_find_empty_rectangle(struct cplot_axes *axes, int rwidth, int rheight, int *xout, int *yout, enum cplot_placement);
void cplot_ticks_draw(struct cplot_ticks *ticks, unsigned *canvas, int axeswidth, int axesheight, int ystride);
void cplot_axistext_draw(struct cplot_axistext *axistext, unsigned *canvas, int axeswidth, int axesheight, int ystride);
void cplot_axis_datarange(struct cplot_axis*);

static double __attribute__((unused)) get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

#include "functions.c"
#include "rotate.c"
#include "cplot_rendering.c"
#include "ticker.c"
#include "layout.c"
#ifdef HAVE_ffmpeg
#include "cplot_video.c"
#else
void* cplot_write_mp4(void *axes_or_subplots, const char *name, float fps) {
    fprintf(stderr, "cplot library was compiled without support for %s\n", __func__);
    return axes_or_subplots;
}
#endif

struct cplot_axis* cplot_axis_void_new(struct cplot_axes *axes) {
    struct cplot_axis *axis = calloc(1, sizeof(struct cplot_axis));
    axis->axes = axes;
    if (axes->mem_axis < axes->naxis+1)
	axes->axis = realloc(axes->axis, (axes->mem_axis+=2) * sizeof(void*));
    axes->axis[axes->naxis++] = axis;
    memset(axes->axis + axes->naxis, 0, (axes->mem_axis - axes->naxis) * sizeof(void*));
    return axis;
}

struct cplot_axis* cplot_axis_new(struct cplot_axes *axes, int x_or_y, float pos) {
    struct cplot_axis *axis = cplot_axis_void_new(axes);
    axis->linestyle.color = fg;
    axis->linestyle.thickness = 1.0 / 400;
    axis->linestyle.style = 1;
    axis->linestyle.align = pos == 0 ? -1 : pos == 1 ? 1 : 0;
    axis->pos = pos;
    axis->min = 0;
    axis->max = 1;
    axis->direction = x_or_y != 'x';
    axis->ticks = cplot_ticks_new((void*)axis);
    return axis;
}

struct cplot_axis* cplot_coloraxis_new(struct cplot_axes *axes, int x_or_y) {
    struct cplot_axis *caxis = cplot_axis_void_new(axes);
    axes->last_caxis = caxis;
    caxis->axes = axes;
    caxis->max = 1;
    caxis->cmap = cmh_colormaps[default_colormap].map;
    caxis->direction = x_or_y != 'x';
    caxis->outside = 1;
    caxis->pos = 1;
    caxis->po[0] = 1;
    caxis->po[1] = 1.0/30;
    caxis->ticks = cplot_ticks_new((void*)caxis);
    return caxis;
}

struct cplot_ticks* cplot_ticks_new(struct cplot_axis *axis) {
    struct cplot_ticks *ticks = calloc(1, sizeof(struct cplot_ticks));
    int ipar = axis->direction == 1;
    ticks->axis = axis;
    ticks->color = fg;
    ticks->init = cplot_init_ticker_default;
    ticks->length = 1.0 / 80;
    ticks->xyalign_text[ipar] = -0.5;
    ticks->xyalign_text[!ipar] = -1 * (axis->pos < 0.5);

    ticks->linestyle.thickness = 1.0 / 1200;
    ticks->linestyle.color = RGB(0, 0, 0);
    ticks->linestyle.style = cplot_line_normal_e;

    ticks->gridstyle.thickness = 1.0 / 1200;
    ticks->gridstyle.color = 0xffcccccc;

    ticks->rowheight = 2.4*ticks->length;
    ticks->visible = 1;
    ticks->have_labels = 1;

    ticks->linestyle1.style = cplot_line_normal_e;
    ticks->linestyle1.thickness = 1.0 / 1200;
    ticks->linestyle1.color = 0xff666666;
    ticks->length1 = 1.0 / 180;

    return ticks;
}

struct cplot_axes* cplot_axes_new() {
    struct cplot_axes *axes = calloc(1, sizeof(struct cplot_axes));
    axes->whatisthis = cplot_axes_e;
    axes->background = -1;

    axes->axis = calloc((axes->mem_axis = 4) + 1, sizeof(void*));
    axes->axis[0] = cplot_axis_new(axes, 'x', 1);
    axes->axis[0]->ticks->gridstyle.style = cplot_line_normal_e;

    axes->axis[1] = cplot_axis_new(axes, 'y', 0);
    axes->axis[1]->ticks->gridstyle.style = cplot_line_normal_e;

    axes->wh[0] = cplot_default_width;
    axes->wh[1] = cplot_default_height;

    axes->ttra = calloc(1, sizeof(struct ttra));
    axes->ttra->fonttype = ttra_sans_e;
    axes->ttra->chop_lines = 1;

    axes->colorscheme = cplot_colorschemes[0];
    axes->title.rowheight = 0.05;

    axes->legend.rowheight = 1.0 / 40;
    axes->legend.symbolspace_per_rowheight = 1.25;
    axes->legend.posx = 0;
    axes->legend.posy = 1;
    axes->legend.hvalign[1] = -1;
    axes->legend.placement = cplot_placement_singlemaxdist;
    axes->legend.visible = 1;
    axes->legend.borderstyle.thickness = 1.0/500;
    axes->legend.borderstyle.style = cplot_line_normal_e;
    axes->legend.borderstyle.color = 0xff<<24;
    axes->legend.fill = cplot_fill_bg_e;

    return axes;
}

void cplot_subplots_put_rows_and_cols(struct cplot_subplots *subplots, int nrows, int ncols) {
    float height = 1.0 / nrows,
	  width = 1.0 / ncols;
    float x[ncols];
    for (int i=0; i<ncols; i++)
	x[i] = i * width;
    float y0 = 0;
    for (int j=0; j<nrows; j++) {
	float y1 = (j+1) * height;
	for (int i=0; i<ncols; i++) {
	    subplots->xywh[j*ncols+i][0] = x[i];
	    subplots->xywh[j*ncols+i][1] = y0;
	    subplots->xywh[j*ncols+i][2] = width;
	    subplots->xywh[j*ncols+i][3] = height;
	}
	y0 = y1;
    }
}

struct cplot_subplots* cplot_subplots_new(int nrows, int ncols) {
    struct cplot_subplots *subplots = calloc(1, sizeof(struct cplot_subplots));
    subplots->whatisthis = cplot_subplots_e;
    subplots->axes = calloc(ncols*nrows, sizeof(void*));
    subplots->xywh = calloc(ncols*nrows, sizeof(float[4]));
    subplots->naxes = ncols * nrows;
    subplots->wh[0] = cplot_default_width;
    subplots->wh[1] = cplot_default_height;
    subplots->background = -1;
    cplot_subplots_put_rows_and_cols(subplots, nrows, ncols);
    return subplots;
}

static void get_min_for_data(struct cplot_data *data, int yxz) {
    int yxz_other = yxz == 2 ? 1 : !yxz;
    void *dt = data->yxzdata[yxz];
    if (yxz < 2 && data->err.yx[yxz])
	dt = data->err.yx[yxz]; // lower side of errorbar
    switch (data->yxztype[yxz_other]) {
	case cplot_f4:
	    data->minmax[yxz][0] = get_min_with_float[data->yxztype[yxz]] (dt, data->length, data->yxzdata[yxz_other],
		data->yxzstride[yxz], data->yxzstride[yxz_other]);
	    break;
	case cplot_f8:
	    data->minmax[yxz][0] = get_min_with_double[data->yxztype[yxz]] (dt, data->length, data->yxzdata[yxz_other],
		data->yxzstride[yxz], data->yxzstride[yxz_other]);
	    break;
	default:
	    data->minmax[yxz][0] = get_min[data->yxztype[yxz]] (dt, data->length, data->yxzstride[yxz]);
	    break;
    }
}

static void get_max_for_data(struct cplot_data *data, int yxz) {
    int yxz_other = yxz == 2 ? 1 : !yxz;
    void *dt = data->yxzdata[yxz];
    if (yxz < 2 && data->err.yx[yxz+1])
	dt = data->err.yx[yxz+1]; // higher side of errorbar
    switch (data->yxztype[yxz_other]) {
	case cplot_f4:
	    data->minmax[yxz][1] = get_max_with_float[data->yxztype[yxz]] (dt, data->length, data->yxzdata[yxz_other],
		data->yxzstride[yxz], data->yxzstride[yxz_other]);
	    break;
	case cplot_f8:
	    data->minmax[yxz][1] = get_max_with_double[data->yxztype[yxz]] (dt, data->length, data->yxzdata[yxz_other],
		data->yxzstride[yxz], data->yxzstride[yxz_other]);
	    break;
	default:
	    data->minmax[yxz][1] = get_max[data->yxztype[yxz]] (dt, data->length, data->yxzstride[yxz]);
	    break;
    }
}

static void get_minmax_for_data(struct cplot_data *data, int yxz) {
    int yxz_other = yxz == 2 ? 1 : !yxz;
    /* errorbars are handled in the other functions */
    if (yxz < 2 && (data->err.yx[yxz] || data->err.yx[yxz+1])) {
	get_min_for_data(data, yxz);
	get_max_for_data(data, yxz);
	return;
    }
    switch (data->yxztype[yxz_other]) {
	case cplot_f4:
	    get_minmax_with_float[data->yxztype[yxz]](
		data->yxzdata[yxz], data->length, data->minmax[yxz], data->yxzdata[yxz_other],
		data->yxzstride[yxz], data->yxzstride[yxz_other]);
	    break;
	case cplot_f8:
	    get_minmax_with_double[data->yxztype[yxz]](
		data->yxzdata[yxz], data->length, data->minmax[yxz], data->yxzdata[yxz_other],
		data->yxzstride[yxz], data->yxzstride[yxz_other]);
	    break;
	default:
	    get_minmax[data->yxztype[yxz]](data->yxzdata[yxz], data->length, data->minmax[yxz], data->yxzstride[yxz]);
	    break;
    }
}

static long get_last_for_data(struct cplot_data *data, int yxz) {
    int yxz_other = yxz == 2 ? 1 : !yxz;
    long i = data->length - 1;
    int stride_oth = data->yxzstride[yxz_other];
    switch (data->yxztype[yxz_other]) {
	case cplot_f4:
	    float *dt4 = data->yxzdata[yxz_other];
	    for (; i>=0; i--)
		if (!my_isnan_float(dt4[i*stride_oth]))
		    return i;
	    return i;
	case cplot_f8:
	    double *dt8 = data->yxzdata[yxz_other];
	    for (; i>=0; i--)
		if (!my_isnan_double(dt8[i*stride_oth]))
		    return i;
	    return i;
	default:
	    return i;
    }
}

static long get_first_for_data(struct cplot_data *data, int yxz) {
    int yxz_other = yxz == 2 ? 1 : !yxz;
    long i = 0;
    int stride_oth = data->yxzstride[yxz_other];
    switch (data->yxztype[yxz_other]) {
	case cplot_f4:
	    float *dt4 = data->yxzdata[yxz_other];
	    for (; i<data->length; i++)
		if (!my_isnan_float(dt4[i*stride_oth]))
		    return i;
	    return i;
	case cplot_f8:
	    double *dt8 = data->yxzdata[yxz_other];
	    for (; i<data->length; i++)
		if (!my_isnan_double(dt8[i*stride_oth]))
		    return i;
	    return i;
	default:
	    return i;
    }
}

void cplot_axis_datarange(struct cplot_axis *axis) {
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
		data->minmax[yxz][0] = data->yxz0[yxz] + get_first_for_data(data, yxz) * data->yxzstep[yxz];
	    if (!(data->have_minmax[yxz] & cplot_maxbit))
		data->minmax[yxz][1] = data->yxz0[yxz] + get_last_for_data(data, yxz) * data->yxzstep[yxz];
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
    axis->range_isset |= cplot_minbit | cplot_maxbit;
}

/* Tätä voisi nopeuttaa käymällä löydettyä kohtaa läpi eteenpäin kunnes löytyy vapaa paikka
   samalla tavalla kuin funktiossa rectangle_closest_i. */
/* If rectangle of size (rwidth,rheight) fits to position (i,j), return negative.
   Otherwise return the next i-coordinate (x) to try. */
static int rectangle_nexti(int j, int i, int rwidth, int rheight, const short (*right)[], const short (*down)[], int w, int h) {
    const short (*spaceright)[w] = right;
    const short (*spacedown)[w] = down;
    for (int jj=0; jj<rheight; jj++)
	if (spaceright[j+jj][i] < rwidth)
	    return i + spaceright[j+jj][i] + 1;
    /* is this necessary? */
    for (int ii=0; ii<rwidth; ii++)
	if (spacedown[j][i+ii] < rheight)
	    return i + ii + 1;
    return -1;
}

/* If rectangle of size (rwidth,rheight) fits to position (i,j), return negative.
   Otherwise return the next j-coordinate (y) to try. */
static int rectangle_nextj(int j, int i, int rwidth, int rheight, const short (*right)[], const short (*down)[], int w, int h) {
    const short (*spaceright)[w] = right;
    const short (*spacedown)[w] = down;
    for (int ii=0; ii<rwidth; ii++)
	if (spacedown[j][i+ii] < rheight)
	    return j + spacedown[j][i+ii] + 1;
    /* is this necessary? */
    for (int jj=0; jj<rheight; jj++)
	if (spaceright[j+jj][i] < rwidth)
	    return j + jj + 1;
    return -1;
}

/* Return the negative distance to closest non-free pixel in the i-direction (x) from the left side of the rectangle.
   If the rectangle does not fit, return the next i-coordinate to try. */
static int rectangle_closest_i(int j, int i, int rwidth, int rheight, const short (*right)[], int w) {
    const short (*spaceright)[w] = right;
    int space;
    int closest = w;
    int longest_jump = -1;
    for (int jj=0; jj<rheight; jj++)
	if ((space = spaceright[j+jj][i]) < rwidth) {
	    int jump = space+1;
	    for (; jump+i<w; jump+=space+1)
		if ((space=spaceright[j+jj][i+jump]) >= rwidth)
		    goto jump_found;
	    return w; // no more spots in this direction
jump_found:
	    if (jump > longest_jump)
		longest_jump = jump;
	}
	else if (space < closest)
	    closest = space;
    if (longest_jump >= 0)
	return i + longest_jump;
    return -closest;
}

/* Return the negative distance to closest non-free pixel in the j-direction (y) from the top of the rectangle.
   If the rectangle does not fit, return the next j-coordinate to try. */
static int rectangle_closest_j(int j, int i, int rwidth, int rheight, const short (*down)[], int w, int h) {
    const short (*spacedown)[w] = down;
    int space;
    int closest = h;
    int longest_jump = -1;
    for (int ii=0; ii<rwidth; ii++)
	if ((space = spacedown[j][i+ii]) < rheight) {
	    int jump = space+1;
	    for (; jump+j<h; jump+=space+1)
		if ((space=spacedown[j+jump][i+ii]) >= rheight)
		    goto jump_found;
	    return h; // no more spots in this direction
jump_found:
	    if (jump > longest_jump)
		longest_jump = jump;
	}
	else if (space < closest)
	    closest = space;
    if (longest_jump >= 0)
	return j + longest_jump;
    return -closest;
}

/* Return negative if does not fit to image.
   Return positive if no empty slot is available.
   Return zero on success. */
int cplot_find_empty_rectangle(struct cplot_axes *axes, int rwidth, int rheight, int *xout, int *yout, enum cplot_placement method) {
    int x0 = axes->ro_inner_xywh[0],
	y0 = axes->ro_inner_xywh[1],
	w = axes->ro_inner_xywh[2],
	h = axes->ro_inner_xywh[3];
    int x1 = x0 + w,
	y1 = y0 + h;
    if (w < rwidth || h < rheight)
	return -1;
    if (!method)
	return 0;
    int retval = 0;

    int width = axes->wh[0],
	height = axes->wh[1];
    unsigned (*image)[width] = calloc(width * height, sizeof(unsigned));
    for (int i=0; i<axes->ndata; i++)
	cplot_data_render(axes->data[i], (void*)image, width, width, height, 0);

    /* including the pointed spot */
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
    switch (method) {
	case cplot_placement_first:
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
	    break;
	default:
	case cplot_placement_singlemaxdist:
	    int save[4] = {-1}; // dist, i, j, which direction
	    jpos = 0; // top row
	    for (int itwice=0; itwice<2; itwice++, jpos=h-rheight/*bottom row*/)
		for (ipos=0; ipos<=w-rwidth; )
		    if ((nextpos = rectangle_closest_i(jpos, ipos, rwidth, rheight, spaceright, w)) < 0) {
			nextpos = -nextpos;
			if (nextpos-rwidth > save[0])
			    save[0]=nextpos-rwidth, save[1]=ipos, save[2]=jpos, save[3]=0;
			ipos += nextpos+1;
		    }
		    else
			ipos = nextpos;
	    ipos=0; // left column
	    for (int itwice=0; itwice<2; itwice++, ipos=w-rwidth/*right column*/)
		for (int jpos=0; jpos<=h-rheight; )
		    if ((nextpos = rectangle_closest_j(jpos, ipos, rwidth, rheight, spacedown, w, h)) < 0) {
			nextpos = -nextpos;
			if (nextpos-rheight > save[0])
			    save[0]=nextpos-rheight, save[1]=ipos, save[2]=jpos, save[3]=1;
			jpos += nextpos+1;
		    }
		    else
			jpos = nextpos;
	    if (save[0] >= 0) {
		ipos = save[1] + (save[3]==0) * save[0] / 2;
		jpos = save[2] + (save[3]==1) * save[0] / 2;
		goto found;
	    }
	    break;
    }

    /* All positions */
    for (jpos=0; jpos<=h-rheight; jpos++)
	for (ipos=0; ipos<=w-rwidth; ipos=nextpos)
	    if ((nextpos = rectangle_nexti(jpos, ipos, rwidth, rheight, spaceright, spacedown, w, h)) < 0)
		goto found;

    retval = 1;
    goto end;

found:
    *yout = jpos + y0;
    *xout = ipos + x0;
end:
    free(image);
    free(spacedown);
    free(spaceright);
    return retval;
}

void cplot_ticks_draw(struct cplot_ticks *ticks, unsigned *canvas, int axeswidth, int axesheight, int ystride) {
    char tickbuff[128];
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

    int iort = ticks->axis->direction == 0;
    int isx = iort;
    int line_px[4];
    line_px[iort+0] = ticks->ro_lines[0];
    line_px[iort+2] = ticks->ro_lines[1];
    int nticks = ticks->tickerdata.common.nticks;
    int gridline[4], xywh[4], *margin=ticks->axis->axes->ro_inner_margin, *xywh_out=ticks->axis->axes->ro_inner_xywh;
    memcpy(xywh, xywh_out, sizeof(xywh));
    xywh[0] += margin[0];
    xywh[1] += margin[1];
    xywh[2] -= margin[0] + margin[2];
    xywh[3] -= margin[1] + margin[3];
    gridline[iort] = xywh_out[iort];
    gridline[iort+2] = xywh_out[iort] + xywh_out[iort+2];
    int inner_area[] = xywh_to_area(xywh_out);
    int side = ticks->axis->pos >= 0.5;

    const double axisdatamin = ticks->axis->min;
    const double axisdatalen = ticks->axis->max - axisdatamin;
    int tot_area[] = {0, 0, ticks->axis->axes->wh[0], ticks->axis->axes->wh[1]};
    tot_area[!iort] = ticks->axis->ro_line[!iort];
    tot_area[!iort+2] = ticks->axis->ro_line[!iort+2];
    for (int itick=0; itick<nticks; itick++) {
	double pos_data = ticks->get_tick(ticks, itick, &tick, 128);
	double pos_rel = (pos_data - axisdatamin) / axisdatalen;
	if (!isx)
	    pos_rel = 1 - pos_rel;
	line_px[!iort] = line_px[!iort+2] = xywh[!iort] + iround(pos_rel * xywh[!iort+2]);
	draw_line(canvas, ystride, line_px, tot_area, &ticks->linestyle, axesheight, 0);
	int area_text[4] = {0};
	if (ttra && tick[0])
	    put_text(ttra, tick, line_px[side*2], line_px[1+side*2], ticks->xyalign_text[0], ticks->xyalign_text[1], ticks->rotation100, area_text, 0);
	if (ticks->gridstyle.style) {
	    gridline[!iort] = gridline[!iort+2] = line_px[!iort];
	    draw_line(canvas, ystride, gridline, inner_area, &ticks->gridstyle, axesheight, 0);
	}
    }

    if (ticks->get_maxn_subticks) {
	nticks = ticks->get_maxn_subticks(ticks);
	float posdata[nticks];
	nticks = ticks->get_subticks(ticks, posdata);
	line_px[iort+0] = ticks->ro_lines1[0];
	line_px[iort+2] = ticks->ro_lines1[1];
	for (int i=0; i<nticks; i++) {
	    double pos_rel = (posdata[i] - axisdatamin) / axisdatalen;
	    if (!isx)
		pos_rel = 1 - pos_rel;
	    line_px[!iort] = line_px[!iort+2] = xywh[!iort] + iroundpos(pos_rel * xywh[!iort+2]);
	    draw_line(canvas, ystride, line_px, tot_area, &ticks->linestyle1, axesheight, 0);
	    if (ticks->gridstyle1.style) {
		gridline[!iort] = gridline[!iort+2] = line_px[!iort];
		draw_line(canvas, ystride, gridline, inner_area, &ticks->gridstyle1, axesheight, 0);
	    }
	}
    }
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
    ttra_set_fontheight(ttra, iroundpos(axistext->rowheight*axesheight));
    int *area = axistext->ro_area;
    put_text(ttra, axistext->text, area[0], area[1], 0, 0, axistext->rotation100, area, 0);
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
	typeof(axis->ro_area[0]) area[4];
	memcpy(area, axis->ro_area, sizeof(area));
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

void cplot_axes_render(struct cplot_axes *axes, uint32_t *canvas, int ystride) {
    canvas += axes->startcanvas;
    unsigned bg = axes->background;
    for (int j=0; j<axes->wh[1]; j++) {
	unsigned ind0 = j * ystride;
	for (int i=0; i<axes->wh[0]; i++)
	    canvas[ind0+i] = bg;
    }
    for (int i=0; i<axes->naxis; i++)
	cplot_axis_draw(axes->axis[i], canvas, axes->wh[0], axes->wh[1], ystride);
    for (int i=0; i<axes->ndata; i++)
	cplot_data_render(axes->data[i], canvas, ystride, axes->wh[0], axes->wh[1], 0);
    cplot_legend_draw(axes, (struct cplot_drawarea){canvas, axes->wh[0], axes->wh[1], ystride});

    if (!axes->title.text)
	return;
    struct ttra *ttra = axes->ttra;
    ttra_set_fontheight(ttra, topixels(axes->title.rowheight, axes));
    put_text(ttra, axes->title.text, axes->title.ro_area[0], axes->title.ro_area[1], 0, 0, axes->title.rotation100, axes->title.ro_area, 0);
}

static void set_icolor(struct cplot_data *data) {
    if (data->icolor != cplot_automatic)
	return;
    struct cplot_axes *axes = data->yxaxis[0]->axes;
    data->icolor = axes->icolor;
    if (data->color)
	return;
    if (!data->markerstyle.color && data->markerstyle.marker && data->markerstyle.marker[0] && !data->yxzdata[2])
	axes->icolor++;
    else if (!data->linestyle.color && data->linestyle.style)
	axes->icolor++;
    else if (!data->errstyle.color && data->errstyle.style && (data->err.list.y0 || data->err.list.x0))
	axes->icolor++;
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
    if (!*y)
	return;
    axes->legend.ro_text_left = iroundpos(axes->legend.symbolspace_per_rowheight * rowh);
    *x += axes->legend.ro_text_left;
    int linewidth = iround1(axes->legend.borderstyle.thickness * axesheight);
    *y += linewidth * 2;
    *x += linewidth * 2;
    *y += 1; // tyhjä tila kirjaimen yläreunan ja laatikon väliin
}

static struct cplot_data* add_data(struct cplot_args *args) {
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
	    data->caxis = cplot_coloraxis_new(args->axes, 'y');
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
	data->yxaxis[1]->ticks->integers_only = 1;
    data->yxaxis[0]->range_isset = 0;
    data->yxaxis[1]->range_isset = 0;
    set_icolor(data);
    return data;
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
	.hvalign = {-0.5, -1.2 * (axis->pos < 0.5)},
	.rowheight = (axis->ticks ? axis->ticks->rowheight : 2.4/80) * 1.3,
	.axis = axis,
	.rotation100 = 75 * (axis->direction == 1),
	.type = cplot_axistext_label,
    };
    return cplot_add_axistext(axis, text);
}

void cplot_ticklabels(struct cplot_axis *axis, char **names, int howmany) {
    axis->ticks->init = cplot_init_ticker_arbitrary_datacoord_enum;
    axis->ticks->tickerdata.arb.labels = names;
    axis->ticks->tickerdata.arb.nticks = howmany;
    axis->ticks->rotation100 = 75;
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

void cplot_destroy_data(struct cplot_data *data) {
    for (int j=0; j<3; j++)
	if (data->owner[j])
	    free(data->yxzdata[j]);
    if (data->cmap_owner)
	free(data->cmap);
    if (data->labelowner)
	free(data->label);
    free(data);
}

void cplot_destroy_axes(struct cplot_axes *axes) {
    for (int i=0; i<axes->naxis; i++)
	cplot_destroy_axis(axes->axis[i]);
    free(axes->axis);
    ttra_destroy(axes->ttra);
    free(axes->ttra);

    for (int i=0; i<axes->ndata; i++)
	cplot_destroy_data(axes->data[i]);
    free(axes->data);

    if (axes->title.textowner)
	free(axes->title.text);

    memset(axes, 0, sizeof(*axes));
    free(axes);
}

void cplot_destroy(void *axes_or_subplots) {
    struct cplot_subplots *subplots = axes_or_subplots;
    if (subplots->whatisthis == cplot_axes_e)
	return cplot_destroy_axes(axes_or_subplots);
    for (int i=0; i<subplots->naxes; i++)
	if (subplots->axes[i])
	    cplot_destroy_axes(subplots->axes[i]);
    free(subplots->axes);
    free(subplots->xywh);
    free(subplots);
}

struct cplot_axes* cplot_plot_args(struct cplot_args *args) {
    if (args->argsfun)
	args->argsfun(args);
    struct cplot_axes *axes1 = NULL;
    struct cplot_axes **axes = args->axesptr ? args->axesptr : &axes1;
    if (!*axes)
	*axes =
	    args->axes ? args->axes :
	    args->yaxis ? args->yaxis->axes :
	    args->xaxis ? args->xaxis->axes :
	    cplot_axes_new();
    args->axes = *axes;
    struct cplot_data *data = add_data(args);

    /* copy if necessary */
    for (int idim=0; idim<3; idim++) {
	if (!args->copy[idim] || !data->yxzdata[idim])
	    continue;
	size_t size = cplot_sizes[data->yxztype[idim]] * data->length;
	void *old = data->yxzdata[idim];
	if (!(data->yxzdata[idim] = malloc(size)))
	    fprintf(stderr, "malloc %zu (%s)\n", size, __func__);
	if (data->yxzstride[idim] == 1)
	    memcpy(data->yxzdata[idim], old, size);
	else {
	    /* after copying, stride == 1 */
	    char *dt = data->yxzdata[idim];
	    int size1 = cplot_sizes[data->yxztype[idim]],
		stride = data->yxzstride[idim];
	    for (int i=data->length-1; i>=0; i--)
		memcpy(dt+i*size1, (char*)old+i*stride*size1, size1);
	    data->yxzstride[idim] = 1;
	}
	data->owner[idim] = 1;
    }
    return *axes;
}

void cplot_forward_datacolor(struct cplot_data *data) {
    if (!data->markerstyle.color)
	data->markerstyle.color = data->color;
    if (!data->linestyle.color)
	data->linestyle.color = data->color;
    if (!data->errstyle.color)
	data->errstyle.color = data->color;
}

void set_colors(struct cplot_axes *axes) {
    if (axes->ncolors <= 0) {
	int n = 0;
	while (axes->colorscheme[n++]);
	axes->ncolors = n;
    }
    for (int i=0; i<axes->ndata; i++) {
	if (!axes->data[i]->color)
	    axes->data[i]->color = axes->colorscheme[axes->data[i]->icolor % axes->ncolors];
	cplot_forward_datacolor(axes->data[i]);
    }
}

void cplot_axes_draw(struct cplot_axes *axes, uint32_t *canvas, int ystride) {
    set_colors(axes);
    if (cplot_axes_layout(axes))
	return; // too small window to draw to
    cplot_axes_render(axes, canvas, ystride);
}

void cplot_xywh_to_pixels(struct cplot_subplots *subplots, int islot, int px[4]) {
    px[0] = iroundpos(subplots->xywh[islot][0] * subplots->wh[0]);
    px[1] = iroundpos(subplots->xywh[islot][1] * subplots->wh[1]);
    px[2] = iroundpos(subplots->wh[0] * subplots->xywh[islot][2]);
    px[3] = iroundpos(subplots->wh[1] * subplots->xywh[islot][3]);
    if (px[0] + px[2] > subplots->wh[0])
	px[2] = subplots->wh[0] - px[0];
    if (px[1] + px[3] > subplots->wh[1])
	px[3] = subplots->wh[1] - px[1];
}

void cplot_subplots_to_axes(struct cplot_subplots *subplots) {
    for (int i=0; i<subplots->naxes; i++) {
	if (!subplots->axes[i])
	    continue;
	int xywh_px[4];
	cplot_xywh_to_pixels(subplots, i, xywh_px);
	subplots->axes[i]->wh[0] = xywh_px[2];
	subplots->axes[i]->wh[1] = xywh_px[3];
	subplots->axes[i]->startcanvas = xywh_px[1] * subplots->wh[0] + xywh_px[0];
    }
}

void cplot_clear_slot(struct cplot_subplots *subplots, int islot, unsigned *canvas, int ystride) {
    int xywh[4];
    cplot_xywh_to_pixels(subplots, islot, xywh);
    const int area[] = xywh_to_area(xywh);
    unsigned color = subplots->background;
    for (int j=area[1]; j<area[3]; j++) {
	int ind0 = j * ystride;
	for (int i=area[0]; i<area[2]; i++)
	    canvas[ind0+i] = color;
    }
}

void cplot_clear_data(struct cplot_axes *axes, uint32_t *canvas, int realw) {
    char bg[4];
    uint32_t background = axes->background;
    memcpy(bg, &background, 4);
    int ystart = axes->ro_inner_xywh[1];
    int yend = ystart + axes->ro_inner_xywh[3];
    int xstart = axes->ro_inner_xywh[0];
    int xend = xstart + axes->ro_inner_xywh[2];
    for (int i=1; i<4; i++)
	if (bg[i] != bg[0])
	    goto loop;

    int width = (xend - xstart) * 4;
    for (int i=ystart; i<yend; i++)
	memset(canvas + i * realw + xstart, bg[0], width);
    return;

loop:
    for (int i=ystart; i<yend; i++) {
	int y = i * realw;
	for (int ii=xstart; ii<xend; ii++)
	    canvas[y + ii] = background;
    }
}

/* For animation purposes. Selectively copied from draw_ticks. */
void cplot_draw_grid(struct cplot_axes *axes, uint32_t *canvas, int ystride) {
    int naxis = axes->naxis;
    int *xywh = axes->ro_inner_xywh;
    int inner_area[] = xywh_to_area(xywh);
    for (int iaxis=0; iaxis<naxis; iaxis++) {
	struct cplot_axis *axis = axes->axis[iaxis];
	struct cplot_ticks *ticks = axis->ticks;
	if (!ticks || !ticks->gridstyle.style)
	    continue;
	int nticks = ticks->tickerdata.common.nticks;
	int isx = axis->direction == 0;
	const double axisdatamin = axis->min;
	const double axisdatalen = axis->max - axisdatamin;
	int gridline[4];
	gridline[isx] = xywh[isx];
	gridline[isx+2] = xywh[isx] + xywh[isx+2];
	for (int itick=0; itick<nticks; itick++) {
	    double pos_data = ticks->get_tick(ticks, itick, NULL, 0);
	    double pos_rel = (pos_data - axisdatamin) / axisdatalen;
	    if (!isx)
		pos_rel = 1 - pos_rel;
	    gridline[!isx] = gridline[!isx+2] = xywh[!isx] + iroundpos(pos_rel * xywh[!isx+2]);
	    draw_line(canvas, ystride, gridline, inner_area, &ticks->gridstyle, axes->wh[1], 0);
	}
    }
}

void cplot_draw(void *vplot, uint32_t *canvas, int ystride) {
    if (((struct cplot_axes*)vplot)->whatisthis == cplot_axes_e)
	cplot_axes_draw(vplot, canvas, ystride);
    else {
	struct cplot_subplots *subplots = vplot;
	cplot_subplots_to_axes(subplots);
	for (int i=0; i<subplots->naxes; i++)
	    if (subplots->axes[i])
		cplot_axes_draw(subplots->axes[i], canvas, ystride);
	    else
		cplot_clear_slot(subplots, i, canvas, ystride);
    }
}

void* cplot_show(void *vplot) {
#ifdef HAVE_wlh
    struct cplot_axes *axes_or_subplots = vplot;
    struct waylandhelper wlh = axes_or_subplots->wlh ? *axes_or_subplots->wlh : (struct waylandhelper){
	.xresmin = 20,
	.yresmin = 20,
    };
    wlh.xres = axes_or_subplots->wh[0];
    wlh.yres = axes_or_subplots->wh[1];
    struct waylandhelper *old_wlh = axes_or_subplots->wlh;
    axes_or_subplots->wlh = &wlh;
    wlh_init(&wlh);
    long updatecount = 0;
    double starttime = -1;
    while (!wlh.stop && wlh_roundtrip(&wlh) >= 0) {
	if (wlh.can_redraw) {
	    axes_or_subplots->wh[0] = wlh.xres;
	    axes_or_subplots->wh[1] = wlh.yres;
	    int should_commit = 0;
	    if (wlh.redraw) {
		cplot_draw(axes_or_subplots, wlh.data, axes_or_subplots->wh[0]);
		should_commit++;
	    }
	    if (starttime < 0)
		starttime = get_time();
	    if (axes_or_subplots->update) {
		int ret = axes_or_subplots->update(axes_or_subplots, wlh.data, wlh.xres, updatecount++, get_time() - starttime);
		if (ret < 0)
		    break;
		should_commit += ret;
	    }
	    if (should_commit)
		wlh_commit(&wlh);
	}
	usleep(10000);
    }
    wlh_destroy(&wlh);
    axes_or_subplots->wlh = old_wlh;
#else
    fprintf(stderr, "%s: cplot library was compiled without support for this function.\n"
	"Install waylandhelper and compile again.\n",
	__func__);
#endif
    return vplot;
}
