#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ttra.h>
#include <float.h>
#include <cmh_colormaps.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#define CPLOT_NO_VERSION_CHECK
#include "cplot.h"

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

static void cplot_fill_u4(uint32_t *canvas, uint32_t color, int w, int h, int ystride) {
	char *ptr = (void*)&color;
	if (ptr[0] == ptr[1] && ptr[0] == ptr[2] && ptr[0] == ptr[3])
		for (int i=0; i<h; i++)
			memset(canvas+i*ystride, ptr[0], w*4);
	else
		for (int i=0; i<h; i++)
			for (int ii=0; ii<w; ii++)
				canvas[i*ystride+ii] = color;
}

int cplot_default_width = 1200, cplot_default_height = 1000;

static inline int __attribute__((const)) iround(float f) {
	int a = f;
	return a + (f-a >= 0.5) - (a-f > 0.5);
}

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

static inline struct cplot_figure* __attribute__((pure)) get_toplevel_figure(struct cplot_figure *figure) {
	while (figure->super) figure = figure->super;
	return figure;
}

static inline int __attribute__((pure)) topixels(float size, struct cplot_figure *figure) {
	switch (figure->topixels_reference) {
		default:
		case cplot_total_height:
			return iroundpos(size * get_toplevel_figure(figure)->wh[1]);
		case cplot_this_height:
			return iroundpos(size * figure->wh[1]);
		case cplot_total_width:
			return iroundpos(size * get_toplevel_figure(figure)->wh[0]);
		case cplot_this_width:
			return iroundpos(size * figure->wh[0]);
	}
}

static inline int intsum_02(const int *a) { return a[0] + a[2]; }

#define no_room_for_legend(figure) ((figure)->legend.ro_place_err < 0)

void cplot_get_legend_dims_chars(struct cplot_figure *figure, int *lines, int *cols);
void cplot_get_legend_dims_px(struct cplot_figure *figure, int *y, int *x);
int cplot_find_empty_rectangle(struct cplot_figure *figure, int rwidth, int rheight, int *xout, int *yout, enum cplot_placement);
void cplot_ticks_draw(struct cplot_ticks *ticks, unsigned *canvas, int figurewidth, int figureheight, int ystride);
void cplot_axistext_draw(struct cplot_axistext *axistext, unsigned *canvas, int figurewidth, int figureheight, int ystride);
void legend_placement(struct cplot_figure *figure);
void texts_placement(struct cplot_figure *figure);

static double __attribute__((unused)) get_time() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec + tv.tv_usec * 1e-6;
}

static int __attribute__((unused)) mkdir_file(const char *restrict name) {
	int len = strlen(name);
	char k1[len+1];
	strcpy(k1, name);
	char k2[len+2];
	if (name[0] == '/')
		k2[0] = '/', len = 1;
	else
		len = 0;
	char *str = strtok(k1, "/"), *str1;
	while (1) {
		if (!(str1 = strtok(NULL, "/")))
			return 0;
		while (*str) k2[len++] = *str++;
		str = str1;
		k2[len++] = '/';
		k2[len] = 0;
		if (mkdir(k2, 0755) && errno != EEXIST)
			return 1;
	}
}

#include "functions.c"
#include "rotate.c"
#include "cplot_rendering.c"
#include "ticker.c"
#include "layout.c"

int __attribute__((pure)) cplot_topixels(float size, struct cplot_figure *figure) {
	return topixels(size, figure);
}

unsigned char __attribute__((malloc))* cplot_make_cmap_from_colorscheme(const unsigned *colorscheme, int ncolors) {
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

unsigned* cplot_make_colorscheme_from_cmap(unsigned *dest, const unsigned char *cmap, int len, int without_ends) {
	int divisor = len - !without_ends;
	float step=1, indf=127.5;
	if (divisor > 0) {
		step = 255.f / (float)divisor;
		indf = !!without_ends * 0.5f*step;
	}
	for (int i=0; i<len; i++) {
		int ind = iroundpos(indf) * 3;
		dest[i] = RGB(cmap[ind], cmap[ind+1], cmap[ind+2]);
		indf += step;
	}
	return dest;
}

struct cplot_axis* cplot_axis_void_new(struct cplot_figure *figure) {
	struct cplot_axis *axis = calloc(1, sizeof(struct cplot_axis));
	axis->figure = figure;
	if (figure->mem_axis < figure->naxis+1)
		figure->axis = realloc(figure->axis, (figure->mem_axis+=2) * sizeof(void*));
	figure->axis[figure->naxis++] = axis;
	memset(figure->axis + figure->naxis, 0, (figure->mem_axis - figure->naxis) * sizeof(void*));
	return axis;
}

long cplot_get_startcanvas(struct cplot_figure *fig, struct cplot_figure *dest, int ystride) {
	long start = 0;
	while (fig != dest) {
		start += fig->ro_corner[1] * ystride + fig->ro_corner[0];
		fig = fig->super;
	}
	return start;
}

struct cplot_axis* cplot_axis_new(struct cplot_figure *figure, int x_or_y, float pos) {
	struct cplot_axis *axis = cplot_axis_void_new(figure);
	axis->linestyle.color = 0xff<<24;
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

struct cplot_axis* cplot_coloraxis_new(struct cplot_figure *figure, int x_or_y) {
	struct cplot_axis *caxis = cplot_axis_void_new(figure);
	figure->last_caxis = caxis;
	caxis->figure = figure;
	caxis->max = 1;
	caxis->cmap = cmh_colormaps[default_colormap].map;
	caxis->direction = x_or_y != 'x';
	caxis->outside = 1;
	caxis->pos = 1;
	caxis->po[0] = 1;
	caxis->po[1] = 1.0/30;
	caxis->ticks = cplot_ticks_new((void*)caxis);
	caxis->center = 0./0.;
	return caxis;
}

struct cplot_ticks* cplot_ticks_new(struct cplot_axis *axis) {
	struct cplot_ticks *ticks = calloc(1, sizeof(struct cplot_ticks));
	int ipar = axis->direction == 1;
	ticks->axis = axis;
	ticks->color = 0xff<<24;
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

struct cplot_figure* cplot_figure_void_new() {
	struct cplot_figure *figure = calloc(1, sizeof(struct cplot_figure));
	figure->background = -1;

	figure->wh[0] = cplot_default_width;
	figure->wh[1] = cplot_default_height;

	figure->ttra = calloc(1, sizeof(struct ttra));
	figure->ttra->fonttype = ttra_sans_e;
	figure->ttra->chop_lines = 1;

	figure->topixels_reference = cplot_this_height;
	figure->colorscheme.colors = cplot_colorschemes[0];
	figure->title.rowheight = 0.05;

	figure->legend.rowheight = 1.0 / 40;
	figure->legend.symbolspace_per_rowheight = 1.25;
	figure->legend.posx = 0;
	figure->legend.posy = 1;
	figure->legend.hvalign[1] = -1;
	figure->legend.placement = cplot_placement_singlemaxdist;
	figure->legend.visible = 1;
	figure->legend.borderstyle.thickness = 1.0/500;
	figure->legend.borderstyle.style = cplot_line_normal_e;
	figure->legend.borderstyle.color = 0xff<<24;
	figure->legend.fill = cplot_fill_bg_e;

	figure->name = "cplotfigure";
	return figure;
}

struct cplot_figure* cplot_figure_new() {
	struct cplot_figure *figure = cplot_figure_void_new();
	figure->axis = calloc((figure->mem_axis = 4) + 1, sizeof(void*));
	figure->axis[0] = cplot_axis_new(figure, 'x', 1);
	figure->axis[0]->ticks->gridstyle.style = cplot_line_normal_e;
	figure->axis[1] = cplot_axis_new(figure, 'y', 0);
	figure->axis[1]->ticks->gridstyle.style = cplot_line_normal_e;
	return figure;
}

struct cplot_figure* cplot_subfigures_put_rows_and_cols(struct cplot_figure *fig, int nrows, int ncols) {
	float height = 1.0 / nrows,
		  width = 1.0 / ncols;
	float x[ncols];
	for (int i=0; i<ncols; i++)
		x[i] = i * width;
	float y0 = 0;
	for (int j=0; j<nrows; j++) {
		float y1 = (j+1) * height;
		for (int i=0; i<ncols; i++) {
			fig->subfigures_xywh[j*ncols+i][0] = x[i];
			fig->subfigures_xywh[j*ncols+i][1] = y0;
			fig->subfigures_xywh[j*ncols+i][2] = width;
			fig->subfigures_xywh[j*ncols+i][3] = height;
		}
		y0 = y1;
	}
	return fig;
}

static void _cplot_make_grid_1d(float (*xywh)[4], int which, int stride, int n, float *arr, float space) {
	float left = 1.0;
	int nfree = 0;
	for (int i=0; i<n; i++) {
		nfree += !arr[i];
		left -= arr[i];
	}
	float size1 = left / (float)nfree;
	float pos = 0;
	for (int i=0; i<n; i++) {
		xywh[i*stride][which+0] = pos;
		pos += xywh[i*stride][which+2] = arr[i] ? arr[i] : size1;
		pos += space;
	}
}

struct cplot_gridargs {
	int nrows, ncols;
	float *width, *height;
	float spacex, spacey;
};

void cplot_make_grid(float (*xywh)[4], struct cplot_gridargs args) {
	_cplot_make_grid_1d(xywh, 0, 1,          args.ncols, args.width,  args.spacex);
	_cplot_make_grid_1d(xywh, 1, args.ncols, args.nrows, args.height, args.spacey);

	/* repeat the xgrid to every row */
	for (int i=1; i<args.nrows; i++)
		for (int ii=0; ii<args.ncols; ii++) {
			xywh[i*args.ncols+ii][0] = xywh[ii][0];
			xywh[i*args.ncols+ii][2] = xywh[ii][2];
		}

	/* repeat the ygrid to every column */
	for (int i=0; i<args.nrows; i++)
		for (int ii=1; ii<args.ncols; ii++) {
			xywh[i*args.ncols+ii][1] = xywh[args.ncols*i][1];
			xywh[i*args.ncols+ii][3] = xywh[args.ncols*i][3];
		}
}

float __attribute__((malloc))* cplot_f4arr(int n, double terminator, ...) {
	va_list args;
	va_start(args);
	float *list = malloc(n * sizeof(float));
	int i;
	for (i=0; i<n; i++) {
		double test = va_arg(args, double);
		if (test == terminator) // test before converting to float
			break;
		list[i] = test;
	}
	memset(list+i, 0, (n-i)*sizeof(float));
	return list;
}

struct cplot_figure* cplot_add_subfigures(struct cplot_figure *fig, int n) {
	int nnew = fig->nsubfigures + n;
	if (fig->memsubfigures < nnew) {
		int nold = fig->memsubfigures;
		fig->subfigures      = realloc(fig->subfigures,      nnew * sizeof(fig->subfigures     [0]));
		fig->subfigures_xywh = realloc(fig->subfigures_xywh, nnew * sizeof(fig->subfigures_xywh[0]));
		memset(fig->subfigures      + nold, 0, (nnew-nold) * sizeof(fig->subfigures     [0]));
		memset(fig->subfigures_xywh + nold, 0, (nnew-nold) * sizeof(fig->subfigures_xywh[0]));
		fig->memsubfigures = nnew;
	}
	fig->nsubfigures = nnew;
	return fig;
}

struct cplot_figure* cplot_subfigures_new(int nrows, int ncols) {
	return cplot_subfigures_put_rows_and_cols(cplot_add_subfigures(cplot_figure_void_new(), nrows*ncols), nrows, ncols);
}

struct cplot_figure* cplot_subfigures_grid_new(int nrows, int ncols, float *yarr, float *xarr, unsigned xyowner) {
	struct cplot_figure *fig = cplot_add_subfigures(cplot_figure_void_new(), nrows*ncols);
	struct cplot_gridargs args = {
		.nrows = nrows,
		.ncols = ncols,
		.width = xarr,
		.height = yarr,
	};
	cplot_make_grid(fig->subfigures_xywh, args);
	if ((xyowner>>0) & 1) free(xarr);
	if ((xyowner>>1) & 1) free(yarr);
	return fig;
}

struct cplot_figure* cplot_subfigures_text_new(const char *_txt) {
	const unsigned char *txt = (typeof(txt))_txt;
	int nrows=0, ncols=0, ipos=0;
	unsigned short xyxy[256][4]; // some are not used but prevent overflow from bad input
	memset(xyxy, 0xff, sizeof(xyxy));
	--txt;
	while (*++txt) {
		switch (*txt) {
			case '\n':
				if (ipos > ncols)
					ncols = ipos;
				if (ipos)
					nrows++;
				ipos = 0;
				continue;
			case ' ':
			case '\t':
				continue;
			default:
				if (xyxy[*txt][0] == 0xffff) {
					xyxy[*txt][0] = xyxy[*txt][2] = ipos;
					xyxy[*txt][1] = xyxy[*txt][3] = nrows;
				}
				else {
					update_min(xyxy[*txt][0], ipos);
					update_max(xyxy[*txt][2], ipos);
					xyxy[*txt][3] = nrows;
				}
				++ipos;
				break;
		}
	}
	if (ipos) nrows++; // did not end in a newline

	int nfig = 0;
	for (int i=0; i<32; i++) {
		if (xyxy[i+'0'][0] == 0xffff)
			continue;
		nfig = i+1;
	}

	struct cplot_figure *fig = cplot_add_subfigures(cplot_figure_void_new(), nfig);
	for (int i=0; i<nfig; i++) {
		if (xyxy[i+'0'][0] == 0xffff)
			continue;
		fig->subfigures_xywh[i][0] = (float)xyxy[i+'0'][0] / (float)ncols;
		fig->subfigures_xywh[i][1] = (float)xyxy[i+'0'][1] / (float)nrows;
		float size;
		size = xyxy[i+'0'][2] - xyxy[i+'0'][0] + 1;
		fig->subfigures_xywh[i][2] = size / (float)ncols;
		size = xyxy[i+'0'][3] - xyxy[i+'0'][1] + 1;
		fig->subfigures_xywh[i][3] = size / (float)nrows;
	}
	return fig;
}

int cplot_next_ifigure_from_coords(struct cplot_figure *fig0, int x, int y) {
	for (int ifig=fig0->nsubfigures-1; ifig>=0; ifig--) {
		struct cplot_figure *fig = fig0->subfigures[ifig];
		if (!fig)
			continue;
		if (x >= fig->ro_corner[0] && x < fig->ro_corner[0] + fig->wh[0] &&
			y >= fig->ro_corner[1] && y < fig->ro_corner[1] + fig->wh[1])
			return ifig;
	}
	return -1;
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
	if (yxz < 2 && (data->err.yx[yxz*2] || data->err.yx[yxz*2+1])) {
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

void cplot_check_dataminmax(struct cplot_data *data, int yxz) {
	unsigned range_isset = data->have_minmax[yxz];
	if (data->yxztype[yxz] == cplot_notype) {
		if (!(range_isset & cplot_minbit))
			data->minmax[yxz][0] = data->yxz0[yxz] + get_first_for_data(data, yxz) * data->yxzstep[yxz];
		if (!(range_isset & cplot_maxbit))
			data->minmax[yxz][1] = data->yxz0[yxz] + get_last_for_data(data, yxz) * data->yxzstep[yxz];
		return;
	}
	switch (range_isset & (cplot_minbit|cplot_maxbit)) {
		case 0:
			get_minmax_for_data(data, yxz);
			data->have_minmax[yxz] |= cplot_minbit|cplot_maxbit;
			break;
		case cplot_maxbit:
			/* data minimum not known yet and axis minimum is not fixed */
			get_min_for_data(data, yxz);
			data->have_minmax[yxz] |= cplot_minbit;
			break;
		case cplot_minbit:
			get_max_for_data(data, yxz);
			data->have_minmax[yxz] |= cplot_maxbit;
			break;
		default:
			__builtin_unreachable();
	}
}

void cplot_make_range(struct cplot_figure *figure) {
	for (int idata=figure->ndata-1; idata>=0; idata--) {
		struct cplot_data *restrict data = figure->data[idata];
		if (!cplot_visible_data(data))
			continue;

		for (int yxz=0; yxz<3; yxz++) {
			struct cplot_axis *axis = yxz < 2 ? data->yxaxis[yxz] : data->caxis;
			if (!axis)
				continue;
			cplot_check_dataminmax(data, yxz);
			if (!(axis->range_isset & cplot_minbit)) {
				if (axis->range_isset & cplot_minbit<<4) // we use this temporarily to mark initialized minmax
					update_min(axis->min, data->minmax[yxz][0]);
				else {
					axis->min = data->minmax[yxz][0];
					axis->range_isset |= cplot_minbit<<4;
				}
			}
			if (!(axis->range_isset & cplot_maxbit)) {
				if (axis->range_isset & cplot_maxbit<<4) // we use this temporarily to mark initialized minmax
					update_max(axis->max, data->minmax[yxz][1]);
				else {
					axis->max = data->minmax[yxz][1];
					axis->range_isset |= cplot_maxbit<<4;
				}
			}
		}
	}

	for (int i=0; i<figure->naxis; i++)
		figure->axis[i]->range_isset = cplot_minbit | cplot_maxbit;
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
int cplot_find_empty_rectangle(struct cplot_figure *figure, int rwidth, int rheight, int *xout, int *yout, enum cplot_placement method) {
	int x0 = figure->ro_inner_xywh[0],
	y0 = figure->ro_inner_xywh[1],
	w = figure->ro_inner_xywh[2],
	h = figure->ro_inner_xywh[3];
	int x1 = x0 + w,
		y1 = y0 + h;
	if (w < rwidth || h < rheight)
		return -1;
	if (!method)
		return 0;
	int retval = 0;

	int width = figure->wh[0],
	height = figure->wh[1];
	unsigned (*image)[width] = calloc(width * height, sizeof(unsigned));
	for (int i=0; i<figure->ndata; i++)
		cplot_data_render(figure->data[i], (void*)image, width, figure, 0);

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

void cplot_ticks_draw(struct cplot_ticks *ticks, unsigned *canvas, int figurewidth, int figureheight, int ystride) {
	char tickbuff[128];
	char *tick = tickbuff;
	struct ttra *ttra = NULL;
	struct cplot_figure *figure = ticks->axis->figure;

	if (ticks->have_labels) {
		ttra = ticks->axis->figure->ttra;
		ttra->canvas = canvas;
		ttra->realw = ystride;
		ttra->realh = figureheight;
		ttra->fg_default = ticks->color;
		ttra->bg_default = -1;
		ttra_print(ttra, "\033[0m");
		ttra_set_fontheight(ttra, topixels(ticks->rowheight, figure));
		ttra->fg_default = 0xff<<24;
	}

	int iort = ticks->axis->direction == 0;
	int isx = iort;
	int line_px[4];
	line_px[iort+0] = ticks->ro_lines[0];
	line_px[iort+2] = ticks->ro_lines[1];
	int nticks = ticks->tickerdata.common.nticks;
	int gridline[4], xywh[4], *margin=ticks->axis->figure->ro_inner_margin, *xywh_out=ticks->axis->figure->ro_inner_xywh;
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
	int tot_area[] = {0, 0, ticks->axis->figure->wh[0], ticks->axis->figure->wh[1]};
	tot_area[!iort] = ticks->axis->ro_line[!iort];
	tot_area[!iort+2] = ticks->axis->ro_line[!iort+2];
	for (int itick=0; itick<nticks; itick++) {
		double pos_data = ticks->get_tick(ticks, itick, &tick, 128);
		double pos_rel = (pos_data - axisdatamin) / axisdatalen;
		if (!isx)
			pos_rel = 1 - pos_rel;
		line_px[!iort] = line_px[!iort+2] = xywh[!iort] + iround(pos_rel * xywh[!iort+2]);
		draw_line(canvas, ystride, line_px, tot_area, &ticks->linestyle, figure, NULL, 0);
		int area_text[4] = {0};
		if (ttra && tick[0])
			put_text(ttra, tick, line_px[side*2], line_px[1+side*2], ticks->xyalign_text[0], ticks->xyalign_text[1], ticks->rotation_grad, area_text, 0);
		if (ticks->gridstyle.style) {
			gridline[!iort] = gridline[!iort+2] = line_px[!iort];
			draw_line(canvas, ystride, gridline, inner_area, &ticks->gridstyle, figure, NULL, 0);
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
			draw_line(canvas, ystride, line_px, tot_area, &ticks->linestyle1, figure, NULL, 0);
			if (ticks->gridstyle1.style) {
				gridline[!iort] = gridline[!iort+2] = line_px[!iort];
				draw_line(canvas, ystride, gridline, inner_area, &ticks->gridstyle1, figure, NULL, 0);
			}
		}
	}
}

void cplot_axistext_draw(struct cplot_axistext *axistext, unsigned *canvas, int figurewidth, int figureheight, int ystride) {
	struct ttra *ttra = axistext->axis->figure->ttra;
	ttra->canvas = canvas;
	ttra->realw = ystride;
	ttra->realh = figureheight;
	ttra->fg_default = axistext->axis->linestyle.color;
	ttra->bg_default = -1;
	ttra_print(ttra, "\033[0m");
	ttra_set_fontheight(ttra, topixels(axistext->rowheight, axistext->axis->figure));
	int *area = axistext->ro_area;
	put_text(ttra, axistext->text, area[0], area[1], 0, 0, axistext->rotation_grad, area, 0);
	ttra->fg_default = 0xff<<24;
}

void cplot_axis_draw(struct cplot_axis *axis, unsigned *canvas, int figurewidth, int figureheight, int ystride) {
	if (!axis || axis->direction < 0)
		return;
	int isx = axis->direction == 0;

	if (axis->cmap) {
		const unsigned char *cmap = axis->cmap;
		int len = axis->ro_area[2+!isx] - axis->ro_area[0+!isx];
		int len1 = axis->ro_area[2+isx] - axis->ro_area[0+isx];
		unsigned char levels[len];
		{
			unsigned short values[len];
			if (!isx) // y-axis goes from bottom to top
				for (int i=0; i<len; i++)
					values[i] = len-1-i;
			else
				for (int i=0; i<len; i++)
					values[i] = i;
			if (my_isnan(axis->center))
				get_datalevels_u2(0, len, values, levels, 0, len-1, 255, 1);
			else {
				double center_rel = axis->center / (axis->max - axis->min);
				double center = len * center_rel;
				get_datalevels_with_center_u2(0, len, values, levels, 0, center, len-1, 255, 1);
			}
		}
		if (axis->reverse_cmap)
			for (int i=0; i<len; i++)
				levels[i] = 255 - levels[i];
		uint32_t (*canvas1)[ystride] = (void*)(canvas + axis->ro_area[1]*ystride + axis->ro_area[0]);
		if (!isx)
			for (int j=0; j<len; j++) {
				unsigned color = from_cmap(cmap + levels[j] * 3);
				for (int i=0; i<len1; i++)
					canvas1[j][i] = color;
			}
		else {
			for (int i=0; i<len; i++)
				canvas1[0][i] = from_cmap(cmap + levels[i] * 3);
			for (int j=1; j<len1; j++)
				memcpy(canvas1[j], canvas1[0], sizeof(canvas1[0]));
		}
	}

	if (axis->linestyle.style != cplot_line_none_e) {
		typeof(axis->ro_area[0]) area[4];
		memcpy(area, axis->ro_area, sizeof(area));
		if (area[isx] < 0) area[isx] = 0;
		if (area[isx+2] > axis->figure->wh[isx])
			area[isx+2] = axis->figure->wh[isx];

		int WH[] = {figurewidth, figureheight};
		if (area[isx+2] > WH[isx]) area[isx+2] = WH[isx];

		if (axis->linestyle.style)
			cplot_fill_box(canvas, ystride, area, axis->linestyle.color);
	}

	if (axis->ticks)
		cplot_ticks_draw(axis->ticks, canvas, figurewidth, figureheight, ystride);

	for (int i=0; i<axis->ntexts; i++)
		cplot_axistext_draw(axis->text[i], canvas, figurewidth, figureheight, ystride);
}

void cplot_figure_render(struct cplot_figure *figure, uint32_t *canvas, int ystride) {
	cplot_fill_u4(canvas, figure->background, figure->wh[0], figure->wh[1], ystride);

	for (int i=0; i<figure->naxis; i++)
		cplot_axis_draw(figure->axis[i], canvas, figure->wh[0], figure->wh[1], ystride);
	for (int i=0; i<figure->ndata; i++)
		cplot_data_render(figure->data[i], canvas, ystride, figure, 0);
	cplot_legend_draw(figure, canvas, ystride);

	struct ttra *ttra = figure->ttra;
	ttra->canvas = canvas;
	ttra->realw = ystride;
	ttra->realh = figure->wh[1];
	ttra->fg_default = 0xff<<24;
	ttra->bg_default = -1;
	ttra_printf(ttra, "\e[0m");
	if (figure->title.text) {
		struct cplot_text *text = &figure->title;
		ttra_set_fontheight(ttra, topixels(text->rowheight, figure));
		put_text(ttra, text->text, text->ro_area[0], text->ro_area[1], 0, 0, text->rotation_grad, text->ro_area, 0);
	}
	for (int i=0; i<figure->ntexts; i++) {
		struct cplot_text *text = figure->texts+i;
		ttra_set_fontheight(ttra, topixels(text->rowheight, figure));
		put_text(ttra, text->text, text->ro_area[0], text->ro_area[1], 0, 0, text->rotation_grad, text->ro_area, 0);
	}

	if (figure->after_drawing)
		figure->after_drawing(figure, canvas, ystride);
}

static void set_icolor(struct cplot_data *data) {
	if (data->icolor != cplot_automatic)
		return;
	struct cplot_figure *figure = data->yxaxis[0]->figure;
	data->icolor = figure->icolor;
	if (data->color)
		return;
	if (!data->markerstyle.color && data->markerstyle.marker && data->markerstyle.marker[0] && !data->yxzdata[2])
		figure->icolor++;
	else if (!data->linestyle.color && data->linestyle.style)
		figure->icolor++;
	else if (!data->errstyle.color && data->errstyle.style && (data->err.list.y0 || data->err.list.x0))
		figure->icolor++;
}

void cplot_get_legend_dims_chars(struct cplot_figure *figure, int *lines, int *cols) {
	*lines = *cols = 0;
	for (int i=0; i<figure->ndata; i++)
		if (figure->data[i]->label) {
			int w, h;
			ttra_get_textdims_chars(figure->data[i]->label, &w, &h);
			*lines += h;
			*cols = max(*cols, w);
		}
}

void cplot_get_legend_dims_px(struct cplot_figure *figure, int *Height, int *Width) {
	struct legend *lg = &figure->legend;
	*Width = *Height = 0;
	int rowh = ttra_set_fontheight(figure->ttra, topixels(lg->rowheight, figure));
	if (lg->ro_datay_len <= figure->ndata) {
		free(lg->ro_datay);
		lg->ro_datay = malloc((lg->ro_datay_len = figure->ndata+1) * sizeof(lg->ro_datay[0]));
	}
	lg->ro_datay[0] = 0;
	for (int i=0; i<figure->ndata; i++) {
		lg->ro_datay[i+1] = lg->ro_datay[i];
		if (figure->data[i]->label) {
			int w, h;
			ttra_get_textdims_pixels(figure->ttra, figure->data[i]->label, &w, &h);
			lg->ro_datay[i+1] = *Height += h;
			*Width = max(*Width, w);
		}
	}
	if (!*Height)
		return;
	figure->legend.ro_text_left = iroundpos(figure->legend.symbolspace_per_rowheight * rowh);
	*Width += figure->legend.ro_text_left;
	int linewidth = topixels(figure->legend.borderstyle.thickness, figure);
	linewidth += linewidth == 0 && figure->legend.borderstyle.thickness != 0; // at least 1 pixel
	*Height += linewidth * 2;
	*Width += linewidth * 2;
	*Height += 1; // tyhjä tila kirjaimen yläreunan ja laatikon väliin
}

void legend_placement(struct cplot_figure *figure) {
	if (!figure->legend.visible)
		return;
	int height, width;
	cplot_get_legend_dims_px(figure, &height, &width);
	if (!height || !width)
		return;
	figure->legend.ro_xywh[2] = width;
	figure->legend.ro_xywh[3] = height;
	figure->legend.ro_xywh[0] =
		figure->ro_inner_xywh[0] + figure->legend.posx * figure->ro_inner_xywh[2] +
		figure->legend.ro_xywh[2] * figure->legend.hvalign[0];
	figure->legend.ro_xywh[1] =
		figure->ro_inner_xywh[1] + figure->legend.posy * figure->ro_inner_xywh[3] +
		figure->legend.ro_xywh[3] * figure->legend.hvalign[1];
	if (!figure->legend.placement)
		return;
	int iplace=0, jplace=0;
	figure->legend.ro_place_err = cplot_find_empty_rectangle(figure, width, height, &iplace, &jplace, figure->legend.placement);
	figure->legend.ro_xywh[0] = iplace;
	figure->legend.ro_xywh[1] = jplace;
}

void texts_placement(struct cplot_figure *figure) {
	for (int i=figure->ntexts-1; i>=0; i--) {
		int *area = figure->texts[i].ro_area;
		struct cplot_text *text = figure->texts+i;
		area[0] = iroundpos(figure->wh[0] * text->xy[0]);
		area[1] = iroundpos(figure->wh[1] * text->xy[1]);
		if (text->rowheight == 0)
			text->rowheight = figure->legend.rowheight;
		if (text->reference == cplot_dataarea_e) {
			area[0] += figure->ro_inner_xywh[0];
			area[1] += figure->ro_inner_xywh[1];
		}
	}
}

static struct cplot_data* add_data(struct cplot_args *args) {
	if (args->figure->mem_data < args->figure->ndata+1)
		args->figure->data = realloc(args->figure->data, (args->figure->mem_data = args->figure->ndata+3) * sizeof(void*));
	struct cplot_data *data;
	args->figure->data[args->figure->ndata++] = data = malloc(sizeof(struct cplot_data));
	memcpy(data, &args->ydata, sizeof(struct cplot_data));

	if (!data->yxaxis[0])
		data->yxaxis[0] = cplot_yaxis0(args->figure);
	if (!data->yxaxis[1])
		data->yxaxis[1] = cplot_xaxis0(args->figure);
	if (data->yxzdata[2] && !data->caxis) {
		if (args->figure->last_caxis)
			data->caxis = args->figure->last_caxis;
		else
			data->caxis = cplot_coloraxis_new(args->figure, 'y');
		data->caxis->range_isset = 0;
	}
	if (data->caxis) {
		if (!my_isnan(args->caxis_center))
			data->caxis->center = args->caxis_center;
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
		data->yxaxis[1]->ticks->integers_only = 1; // Väärin. Akselilla voi olla muutakin dataa.
	data->yxaxis[0]->range_isset = 0;
	data->yxaxis[1]->range_isset = 0;
	set_icolor(data);
	return data;
}

struct cplot_axistext* cplot_add_axistext(struct cplot_axis *axis, struct cplot_axistext *text) {
	if (axis->ntexts >= axis->memtext)
		axis->text = realloc(axis->text, (axis->memtext = axis->ntexts + 2) * sizeof(axis->text[0]));
	axis->text[axis->ntexts++] = text;
	return text;
}

struct cplot_figure* cplot_add_text(struct cplot_figure *figure, struct cplot_text *text) {
	if (figure->ntexts >= figure->memtext)
		figure->texts = realloc(figure->texts, (figure->memtext = figure->ntexts + 2) * sizeof(figure->texts[0]));
	figure->texts[figure->ntexts++] = *text;
	return figure;
}

struct cplot_axistext* cplot_axislabel(struct cplot_axis *axis, char *label) {
	struct cplot_axistext *text = malloc(sizeof(struct cplot_axistext));
	*text = (struct cplot_axistext) {
		.text = label,
			.pos = 0.5,
			.hvalign = {-0.5, -1.2 * (axis->pos < 0.5)},
			.rowheight = (axis->ticks ? axis->ticks->rowheight : 2.4/80) * 1.3,
			.axis = axis,
			.rotation_grad = 300 * (axis->direction == 1),
			.type = cplot_axistext_label,
	};
	return cplot_add_axistext(axis, text);
}

void cplot_ticklabels(struct cplot_axis *axis, char **names, int howmany) {
	axis->ticks->init = cplot_init_ticker_arbitrary_datacoord_enum;
	axis->ticks->tickerdata.arb.labels = names;
	axis->ticks->tickerdata.arb.nticks = howmany;
	axis->ticks->rotation_grad = 300;
}

struct cplot_args* cplot_defaultargs(struct cplot_args *args) {
	*args = (struct cplot_args) {
		__cplot_defaultargs
	};
	return args;
}

struct cplot_args* cplot_default_lineargs(struct cplot_args *args) {
	*args = (struct cplot_args) {
		__cplot_defaultargs,
			cplot_lineargs,
	};
	return args;
}

struct cplot_axis* cplot_remove_ticks(struct cplot_axis *axis) {
	free(axis->ticks);
	axis->ticks = NULL;
	return axis;
}

void cplot_destroy_axis(struct cplot_axis *axis) {
	for (int i=axis->ntexts-1; i>=0; i--) {
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
		free((void*)(intptr_t)data->label);
	for (int i=0; i<4; i++)
		if (data->err_owner[i])
			free(data->err.yx[i]);
	free(data);
}

void cplot_destroy_single(struct cplot_figure *fig) {
	/* Subfigures must be destroyed already or referenced by other pointers. */
	free(fig->subfigures);
	free(fig->subfigures_xywh);

	for (int i=0; i<fig->naxis; i++)
		cplot_destroy_axis(fig->axis[i]);
	free(fig->axis);
	ttra_destroy(fig->ttra);
	free(fig->ttra);
	free(fig->legend.ro_datay);

	for (int i=0; i<fig->ndata; i++)
		cplot_destroy_data(fig->data[i]);
	free(fig->data);

	if (fig->title.owner)
		free((void*)(intptr_t)fig->title.text);

	for (int i=fig->ntexts-1; i>=0; i--)
		if (fig->texts[i].owner)
			free((void*)(intptr_t)fig->texts[i].text);
	free(fig->texts);

	if (fig->colorscheme.owner)
		free(fig->colorscheme.colors);

	memset(fig, 0, sizeof(*fig));
	free(fig);
}

void cplot_destroy(struct cplot_figure *fig) {
	if (!fig)
		return;
	for (int i=0; i<fig->nsubfigures; i++)
		cplot_destroy(fig->subfigures[i]);
	cplot_destroy_single(fig);
}

struct cplot_figure* cplot_plot_args(struct cplot_args *args) {
	if (args->plot_interlazed_y || args->labels) {
		const char **labels = args->labels;
		args->plot_interlazed_y = 0;
		args->labels = NULL;
		if (labels)
			args->label = labels[0];

		struct cplot_figure *figure = cplot_plot_args(args);
		/* Save time by using the same minmax for all interlaced data.
		   This is fine because data->minmax is only a tool for getting axis->minmax. */
		struct cplot_data *data = figure->data[figure->ndata-1];
		struct cplot_data copy = *data;
		copy.length *= copy.yxzstride[0];
		copy.yxzstride[0] = 1;
		for (int yxz=0; yxz<3; yxz++) {
			struct cplot_axis *axis = yxz == 2 ? data->caxis : data->yxaxis[yxz];
			if (!axis)
				continue;
			cplot_check_dataminmax(data, yxz);
			data->have_minmax[yxz] = args->have_minmax[yxz] = copy.have_minmax[yxz];
			memcpy(data->minmax[yxz], copy.minmax[yxz], sizeof(copy.minmax[yxz]));
			memcpy(args->minmax[yxz], copy.minmax[yxz], sizeof(copy.minmax[yxz]));
		}

		args->figure = figure;
		args->figureptr = NULL;
		for (int i=1; i<args->ystride; i++) {
			if (labels)
				args->label = labels[i];
			args->ydata += cplot_sizes[args->ytype];
			cplot_plot_args(args);
		}
		return figure;
	}

	if (args->argsfun)
		args->argsfun(args);
	struct cplot_figure *figure1 = NULL;
	struct cplot_figure **figure = args->figureptr ? args->figureptr : &figure1;
	if (!*figure)
		*figure =
			args->figure ? args->figure :
			args->yaxis ? args->yaxis->figure :
			args->xaxis ? args->xaxis->figure :
			cplot_figure_new();
	args->figure = *figure;
	struct cplot_data *data = add_data(args);

	/* copy if necessary */
	for (int idim=0; idim<3; idim++) {
		if (!(args->yxzowner[idim] < 0))
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

	/* copy err if necessary */
	for (int ierr=0; ierr<4; ierr++) {
		if (args->err_owner[ierr] >= 0)
			continue;
		size_t size = cplot_sizes[data->yxztype[ierr/2]] * data->length; // same type with the main data
		void *old = data->err.yx[ierr];
		if (!(data->err.yx[ierr] = malloc(size)))
			fprintf(stderr, "malloc %zu (%s)\n", size, __func__);
		memcpy(data->err.yx[ierr], old, size);
		data->err_owner[ierr] = 1;
	}

	if (data->labelowner < 0) {
		data->label = strdup(data->label);
		data->labelowner = 1;
	}

	return *figure;
}

void cplot_forward_datacolor(struct cplot_data *data) {
	if (!data->markerstyle.color)
		data->markerstyle.color = data->color;
	if (!data->linestyle.color)
		data->linestyle.color = data->color;
	if (!data->errstyle.color)
		data->errstyle.color = data->color;
}

void set_colors(struct cplot_figure *figure) {
	struct cplot_colorscheme *cs = &figure->colorscheme;
	if (!cs->colors || cs->cmh_enum) {
		cs->ncolors = figure->ndata;
		if (cs->colors && cs->owner)
			free(cs->colors);
		cs->colors = malloc((cs->ncolors+1) * sizeof(unsigned));
		cs->owner = 1;
		cs->colors[cs->ncolors] = 0;
		cplot_make_colorscheme_from_cmap(cs->colors, cmh_colormaps[Abs(cs->cmh_enum)].map, cs->ncolors, cs->without_ends);
		cs->cmh_enum = 0;
	}
	if (cs->ncolors <= 0) {
		cs->ncolors = 0;
		while (cs->colors[cs->ncolors++]); // count the number of colors
	}
	for (int i=0; i<figure->ndata; i++) {
		if (!figure->data[i]->color)
			figure->data[i]->color = cs->colors[figure->data[i]->icolor % cs->ncolors];
		cplot_forward_datacolor(figure->data[i]);
	}
}

void cplot_figure_draw(struct cplot_figure *figure, uint32_t *canvas, int ystride) {
	++figure->draw_counter;
	set_colors(figure);
	if (cplot_figure_layout(figure)) {
		if (figure->fix_too_little_space) {
			figure->fix_too_little_space(figure);
			if (cplot_figure_layout(figure))
				return;
		}
		else
			return;
	}
	cplot_figure_render(figure, canvas, ystride);
	if (figure->revert_fixes)
		figure->revert_fixes(figure);
}

static void subfigures_xywh_to_pixels(struct cplot_figure *fig, int islot, int px[4]) {
	px[0] = iroundpos(fig->subfigures_xywh[islot][0] * fig->ro_inner_xywh[2]) + fig->ro_inner_xywh[0];
	px[1] = iroundpos(fig->subfigures_xywh[islot][1] * fig->ro_inner_xywh[3]) + fig->ro_inner_xywh[1];
	px[2] = iroundpos(fig->ro_inner_xywh[2] * fig->subfigures_xywh[islot][2]);
	px[3] = iroundpos(fig->ro_inner_xywh[3] * fig->subfigures_xywh[islot][3]);
	if (px[0] + px[2] > fig->wh[0])
		px[2] = fig->wh[0] - px[0];
	if (px[1] + px[3] > fig->wh[1])
		px[3] = fig->wh[1] - px[1];
}

void cplot_xywh_to_subfigures(struct cplot_figure *fig) {
	for (int ichild=0; ichild<fig->nsubfigures; ichild++) {
		if (!fig->subfigures[ichild])
			continue;
		int xywh_px[4];
		subfigures_xywh_to_pixels(fig, ichild, xywh_px);
		fig->subfigures[ichild]->wh[0] = xywh_px[2];
		fig->subfigures[ichild]->wh[1] = xywh_px[3];
		fig->subfigures[ichild]->ro_corner[0] = xywh_px[0];
		fig->subfigures[ichild]->ro_corner[1] = xywh_px[1];
		fig->subfigures[ichild]->super = fig;
	}
}

void cplot_clear_data(struct cplot_figure *figure, uint32_t *canvas, int realw) {
	char bg[4];
	uint32_t background = figure->background;
	memcpy(bg, &background, 4);
	int ystart = figure->ro_inner_xywh[1];
	int yend = ystart + figure->ro_inner_xywh[3];
	int xstart = figure->ro_inner_xywh[0];
	int xend = xstart + figure->ro_inner_xywh[2];
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

/* For animation. Selectively copied from draw_ticks. */
void cplot_draw_grid(struct cplot_figure *figure, uint32_t *canvas, int ystride) {
	int naxis = figure->naxis;
	int *xywh = figure->ro_inner_xywh;
	int inner_area[] = xywh_to_area(xywh);
	for (int iaxis=0; iaxis<naxis; iaxis++) {
		struct cplot_axis *axis = figure->axis[iaxis];
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
			draw_line(canvas, ystride, gridline, inner_area, &ticks->gridstyle, figure, NULL, 0);
		}
	}
}

void cplot_draw(struct cplot_figure *fig, uint32_t *canvas, int ystride) {
	cplot_figure_draw(fig, canvas, ystride); // before subfigures to not cover them with background color
	cplot_xywh_to_subfigures(fig);
	if (fig->after_drawing)
		fig->after_drawing(fig, canvas, ystride);
	struct cplot_figure *f;
	for (int i=0; i<fig->nsubfigures; i++)
		if ((f = fig->subfigures[i]))
			cplot_draw(f, canvas + f->ro_corner[1]*ystride + f->ro_corner[0], ystride);
}

static uint32_t* copy_canvas(uint32_t *dest1d, int dest_ystride, uint32_t *src1d, int src_ystride, int *wh) {
	int width = wh[0], height = wh[1];
	uint32_t (*dest)[dest_ystride] = (void*)dest1d,
			 (*src)[src_ystride] = (void*)src1d;
	for (int i=0; i<height; i++)
		memcpy(dest[i], src[i], sizeof(uint32_t)*width);
	return dest1d;
}

static uint32_t __attribute__((malloc,unused))* duplicate_canvas(uint32_t *src1d, int src_ystride, int *wh) {
	uint32_t *copy = malloc(wh[0] * wh[1] * sizeof(uint32_t));
	return copy_canvas(copy, wh[0], src1d, src_ystride, wh);
}

#ifdef HAVE_PNG
#include "png.c"
#else
struct cplot_figure* cplot_write_png_preserve(struct cplot_figure *a, const char *b) {
	fprintf(stderr, "cplot library was compiled without support for writing a png image.\n"
		"Configure and compile again with libpng enabled.\n"
	);
	return a;
}
#endif
void cplot_write_png(struct cplot_figure *fig, const char *name) {
	cplot_destroy(cplot_write_png_preserve(fig, name));
}

#ifdef HAVE_ffmpeg
#include "cplot_video.c"
#else
struct cplot_figure* cplot_write_mp4_preserve(struct cplot_figure *fig, const char *name, float fps) {
	fprintf(stderr, "cplot library was compiled without support for \e[1m%s\e[0m\n", __func__);
	return fig;
}
#endif
void cplot_write_mp4(struct cplot_figure *fig, const char *name, float fps) {
	cplot_destroy(cplot_write_mp4_preserve(fig, name, fps));
}

#ifdef HAVE_wlh
#include "cplot_wayland.c"
#else
struct cplot_figure* cplot_show_preserve_(struct cplot_figure *fig, char *name) {
	fprintf(stderr, "cplot was compiled without support for creating a window\n"
		"Configure and compile again with libwaylandhelper enabled.\n",
	);
	return fig;
}
#endif
void cplot_show_(struct cplot_figure *fig, char *name) {
	cplot_destroy(cplot_show_preserve_(fig, name));
}
