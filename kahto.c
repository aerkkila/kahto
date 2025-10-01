#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ttra.h>
#include <float.h>
#include <cmh_colormaps.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <pthread.h>
#define KAHTO_NO_VERSION_CHECK
#include "kahto.h"

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

const int __kahto_version_in_library = __kahto_version_in_program;

unsigned kahto_colorscheme_00[] = { // color deficient semi friendly, semi dark
	0xff000000, 0xfff20700, 0xff505eff, 0xfff781bf, 0xff108a4f, 0xff66ccff, 0xffffc73a,
	0xffc84eff, 0xffffff33, 0xffa65628, 0xff999999, 0
};
unsigned kahto_colorscheme_01[] = { // color deficient friendly, semi dark
	0xff000000, 0xff505eff, 0xff337538, 0xffc26a77, 0xff56b4e9, 0xffdddd55, 0xff9f4a96, 0
};
unsigned kahto_colorscheme_02[] = { // picked from 12 hue values with some changes
	0xff000000, 0xffff0000, 0xff0000ff, 0xff00b300, 0xffff8000, 0xffff00ff,
	0xff8659b3, 0xff80bfff, 0xff99004d, 0xffb3b300, 0xff4d9991,
	0
};
unsigned *kahto_colorschemes[] = {
	kahto_colorscheme_00,
	kahto_colorscheme_01,
	kahto_colorscheme_02,
};
int kahto_ncolors[] = {
	arrlen(kahto_colorscheme_00) - 1,
	arrlen(kahto_colorscheme_01) - 1,
	arrlen(kahto_colorscheme_02) - 1,
};

static void kahto_fill_u4(uint32_t *canvas, uint32_t color, int w, int h, int ystride) {
	char *ptr = (void*)&color;
	if (ptr[0] == ptr[1] && ptr[0] == ptr[2] && ptr[0] == ptr[3])
		for (int i=0; i<h; i++)
			memset(canvas+i*ystride, ptr[0], w*4);
	else
		for (int i=0; i<h; i++)
			for (int ii=0; ii<w; ii++)
				canvas[i*ystride+ii] = color;
}

int kahto_default_width = 1200, kahto_default_height = 1000;

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

static inline struct kahto_figure* __attribute__((pure)) get_toplevel_figure(struct kahto_figure *figure) {
	while (figure->super) figure = figure->super;
	return figure;
}

static inline float __attribute__((pure)) _tofpixels(float size, struct kahto_figure *figure) {
	switch (figure->topixels_reference) {
		default:
		case kahto_total_height:
			return size * (float)get_toplevel_figure(figure)->wh[1];
		case kahto_this_height:
			return size * (float)figure->wh[1];
		case kahto_total_width:
			return size * (float)get_toplevel_figure(figure)->wh[0];
		case kahto_this_width:
			return size * (float)figure->wh[0];
	}
}

static inline float __attribute__((pure)) tofpixels(float size, struct kahto_figure *figure) {
	if (size >= 1)
		return size;
	return figure->fracsizemul * _tofpixels(size, figure);
}

static inline int __attribute__((pure)) topixels(float size, struct kahto_figure *figure) {
	return iroundpos(tofpixels(size, figure));
}

static int set_fontheight(struct kahto_figure *figure, float size) {
	return ttra_set_fontheight(figure->ttra, topixels(size*figure->fontheightmul, figure));
}

static inline int intsum_02(const int *a) { return a[0] + a[2]; }

static int is_colormesh(struct kahto_graph *g) {
	return
		g->data.list.zdata &&
		g->data.list.xdata &&
		g->data.list.zdata->length >
		g->data.list.xdata->length;
}

void inner_without_margin(int *xywh, struct kahto_figure *fig) {
	int *inner = fig->ro_inner_xywh;
	int *margin = fig->ro_inner_margin;
	xywh[0] = inner[0] + margin[0];
	xywh[1] = inner[1] + margin[1];
	xywh[2] = inner[2] - margin[0] - margin[2];
	xywh[3] = inner[3] - margin[1] - margin[3];
}

#define no_room_for_legend(figure) (!!(figure)->legend.ro_place_err)

void kahto_get_legend_dims_chars(struct kahto_figure *figure, int *lines, int *cols);
void kahto_get_legend_dims_px(struct kahto_figure *figure, int *y, int *x);
int kahto_find_empty_rectangle(struct kahto_figure *figure, int rwidth, int rheight, int *xout, int *yout, enum kahto_placement);
void kahto_ticks_draw(struct kahto_ticks *ticks, unsigned *canvas, int figurewidth, int figureheight, int ystride);
void kahto_axistext_draw(struct kahto_axistext *axistext, unsigned *canvas, int figurewidth, int figureheight, int ystride);
static void legend_placement(struct kahto_figure *figure);
static void texts_placement(struct kahto_figure *figure);

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
#include "kahto_rendering.c"
#include "ticker.c"
#include "layout.c"

int __attribute__((pure)) kahto_topixels(float size, struct kahto_figure *figure) {
	return topixels(size, figure);
}

unsigned char __attribute__((malloc))* kahto_make_cmap_from_colorscheme(const unsigned *colorscheme, int ncolors) {
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

unsigned* kahto_make_colorscheme_from_cmap(unsigned *dest, const unsigned char *cmap, int len, int without_ends) {
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

struct kahto_axis* kahto_axis_void_new(struct kahto_figure *figure) {
	struct kahto_axis *axis = calloc(1, sizeof(struct kahto_axis));
	axis->figure = figure;
	axis->center = 0.0 / 0.0;
	if (figure->mem_axis < figure->naxis+1)
		figure->axis = realloc(figure->axis, (figure->mem_axis+=2) * sizeof(void*));
	figure->axis[figure->naxis++] = axis;
	memset(figure->axis + figure->naxis, 0, (figure->mem_axis - figure->naxis) * sizeof(void*));
	return axis;
}

long kahto_get_startcanvas(struct kahto_figure *fig, struct kahto_figure *dest, int ystride) {
	long start = 0;
	while (fig != dest) {
		start += fig->ro_corner[1] * ystride + fig->ro_corner[0];
		fig = fig->super;
	}
	return start;
}

struct kahto_axis* kahto_axis_new(struct kahto_figure *figure, int x_or_y, float pos) {
	struct kahto_axis *axis = kahto_axis_void_new(figure);
	axis->visible = 1;
	axis->linestyle.color = 0xff<<24;
	axis->linestyle.thickness = 1.0 / 400;
	axis->linestyle.style = 1;
	axis->linestyle.align = pos == 0 ? -1 : pos == 1 ? 1 : 0;
	axis->pos = pos;
	axis->min = 0;
	axis->max = 1;
	axis->direction = x_or_y != 'x';
	axis->ticks = kahto_ticks_new((void*)axis);
	return axis;
}

struct kahto_axis* kahto_coloraxis_new(struct kahto_figure *figure, int x_or_y) {
	struct kahto_axis *caxis = kahto_axis_void_new(figure);
	caxis->visible = 1;
	caxis->max = 1;
	caxis->cmap = cmh_colormaps[default_colormap].map;
	caxis->feature = kahto_color_e;
	caxis->direction = x_or_y != 'x';
	caxis->outside = 1;
	caxis->pos = 1;
	caxis->po[0] = 1;
	caxis->po[1] = 1.0/30;
	caxis->ticks = kahto_ticks_new((void*)caxis);
	return caxis;
}

struct kahto_axis* kahto_featureaxis_new(struct kahto_figure *figure, int x_or_y, enum kahto_feature feature) {
	switch (feature) {
		case kahto_position_e: return kahto_axis_new(figure, x_or_y, 1);
		case kahto_color_e:    return kahto_coloraxis_new(figure, x_or_y);
		default: break;
	}
	struct kahto_axis *axis = kahto_axis_void_new(figure);
	axis->min = 0;
	axis->max = 1;
	axis->direction = x_or_y != 'x';
	axis->feature = feature;
	return axis;
}

struct kahto_ticks* kahto_ticks_new(struct kahto_axis *axis) {
	struct kahto_ticks *ticks = calloc(1, sizeof(struct kahto_ticks));
	int ipar = axis->direction == 1;
	ticks->axis = axis;
	ticks->color = 0xff<<24;
	ticks->init = kahto_init_ticker_default;
	ticks->length = 1.0 / 80;
	ticks->xyalign_text[ipar] = -0.5;
	ticks->xyalign_text[!ipar] = -1 * (axis->pos < 0.5);

	ticks->linestyle.thickness = 1.0 / 1200;
	ticks->linestyle.color = RGB(0, 0, 0);
	ticks->linestyle.style = kahto_line_normal_e;

	ticks->gridstyle.thickness = 1.0 / 1200;
	ticks->gridstyle.color = 0xffcccccc;

	ticks->rowheight = 1./35;
	ticks->visible = 1;
	ticks->have_labels = 1;

	ticks->linestyle1.style = kahto_line_normal_e;
	ticks->linestyle1.thickness = 1.0 / 1200;
	ticks->linestyle1.color = 0xff666666;
	ticks->length1 = 1.0 / 180;

	return ticks;
}

struct ttra* kahto_figure_ttra_new(struct kahto_figure *figure) {
	figure->ttra = calloc(1, sizeof(struct ttra));
	figure->ttra->fonttype = ttra_sans_e;
	figure->ttra->chop_lines = 1;
	figure->ttra_owner = 1;
	return figure->ttra;
}

struct kahto_figure* kahto_figure_init(struct kahto_figure *figure) {
	memset(figure, 0, sizeof(*figure));
	figure->background = -1;

	figure->wh[0] = kahto_default_width;
	figure->wh[1] = kahto_default_height;

	figure->topixels_reference = kahto_this_height;
	figure->colorscheme.colors = kahto_colorschemes[0];
	figure->title.rowheight = 0.05;
	figure->fontheightmul = 1;
	figure->fracsizemul = 1;

	figure->legend.rowheight = 1.0 / 35;
	figure->legend.symbolspace_per_rowheight = 1.25;
	figure->legend.posx = 0;
	figure->legend.posy = 1;
	figure->legend.hvalign[1] = -1;
	figure->legend.placement = kahto_placement_singlemaxdist;
	figure->legend.visible = 1;
	figure->legend.borderstyle.thickness = 1.0/500;
	figure->legend.borderstyle.style = kahto_line_normal_e;
	figure->legend.borderstyle.color = 0xff<<24;
	figure->legend.fill = kahto_fill_bg_e;
	figure->legend.minscale = 0.6;
	figure->legend.scale = 1;

	figure->name = "kahtofigure";
	return figure;
}

struct kahto_figure* kahto_figure_new() {
	return kahto_figure_init(malloc(sizeof(struct kahto_figure)));
}

struct kahto_figure* kahto_subfigures_put_rows_and_cols(struct kahto_figure *fig, int nrows, int ncols) {
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

static void _kahto_make_grid_1d(float (*xywh)[4], int which, int stride, int n, float *arr, float space) {
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

struct kahto_gridargs {
	int nrows, ncols;
	float *width, *height;
	float spacex, spacey;
};

void kahto_make_grid(float (*xywh)[4], struct kahto_gridargs args) {
	_kahto_make_grid_1d(xywh, 0, 1,          args.ncols, args.width,  args.spacex);
	_kahto_make_grid_1d(xywh, 1, args.ncols, args.nrows, args.height, args.spacey);

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

float __attribute__((malloc))* kahto_f4arr(int n, double terminator, ...) {
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

struct kahto_figure* kahto_add_subfigures(struct kahto_figure *fig, int n) {
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

struct kahto_figure* kahto_subfigures_new(int nrows, int ncols) {
	return kahto_subfigures_put_rows_and_cols(kahto_add_subfigures(kahto_figure_new(), nrows*ncols), nrows, ncols);
}

struct kahto_figure* kahto_subfigures_grid_new(int nrows, int ncols, float *yarr, float *xarr, unsigned xyowner) {
	struct kahto_figure *fig = kahto_add_subfigures(kahto_figure_new(), nrows*ncols);
	struct kahto_gridargs args = {
		.nrows = nrows,
		.ncols = ncols,
		.width = xarr,
		.height = yarr,
	};
	kahto_make_grid(fig->subfigures_xywh, args);
	if ((xyowner>>0) & 1) free(xarr);
	if ((xyowner>>1) & 1) free(yarr);
	return fig;
}

struct kahto_figure* kahto_subfigures_text_new(const char *_txt) {
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

	struct kahto_figure *fig = kahto_add_subfigures(kahto_figure_new(), nfig);
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

int kahto_next_ifigure_from_coords(struct kahto_figure *fig0, int x, int y) {
	for (int ifig=fig0->nsubfigures-1; ifig>=0; ifig--) {
		struct kahto_figure *fig = fig0->subfigures[ifig];
		if (!fig)
			continue;
		if (x >= fig->ro_corner[0] && x < fig->ro_corner[0] + fig->wh[0] &&
			y >= fig->ro_corner[1] && y < fig->ro_corner[1] + fig->wh[1])
			return ifig;
	}
	return -1;
}

void kahto_check_dataminmax(struct kahto_data *data, int yxz) {
	unsigned range_isset = data->have_minmax;

	switch (range_isset & (kahto_minbit|kahto_maxbit)) {
		case 0:
			get_minmax[data->type](data->data, data->length, data->minmax, data->stride);
			data->have_minmax |= kahto_minbit|kahto_maxbit;
			break;
		case kahto_maxbit:
			data->minmax[0] = get_min[data->type](data->data, data->length, data->stride);
			data->have_minmax |= kahto_minbit;
			break;
		case kahto_minbit:
			data->minmax[1] = get_max[data->type](data->data, data->length, data->stride);
			data->have_minmax |= kahto_maxbit;
			break;
		case kahto_minbit|kahto_maxbit:
			break;
		default:
			__builtin_unreachable();
	}
}

int find_axis_ind(struct kahto_axis *axis) {
	struct kahto_axis **list = axis->figure->axis;
	int n = axis->figure->naxis;
	for (int i=0; i<n; i++)
		if (list[i] == axis)
			return i;
	return -1;
}

void kahto_make_range(struct kahto_figure *figure) {
	unsigned char minmax = kahto_minbit | kahto_maxbit;
	char used_axis[figure->naxis];
	memset(used_axis, 0, sizeof(used_axis));
	for (int igraph=figure->ngraph-1; igraph>=0; igraph--) {
		struct kahto_graph *restrict graph = figure->graph[igraph];
		if (!kahto_visible_graph(graph))
			continue;

		for (int yxz=0; yxz<arrlen(graph->data.arr); yxz++) {
			struct kahto_data *data = graph->data.arr[yxz];
			if (!data)
				continue;
			struct kahto_axis *axis = yxz < arrlen(graph->yxaxis) ? graph->yxaxis[yxz] : graph->yxaxis[0];
			used_axis[find_axis_ind(axis)] = 1;
			// errordata uses yaxis
			if ((axis->range_isset & minmax) == minmax)
				continue;
			kahto_check_dataminmax(data, yxz);
			if (!(axis->range_isset & kahto_minbit)) {
				if (axis->range_isset & kahto_minbit<<4) // we use this temporarily to mark initialized minmax
					update_min(axis->min, data->minmax[0]);
				else {
					axis->min = data->minmax[0];
					axis->range_isset |= kahto_minbit<<4; // we use this temporarily to mark initialized minmax
				}
			}
			if (!(axis->range_isset & kahto_maxbit)) {
				if (axis->range_isset & kahto_maxbit<<4) // we use this temporarily to mark initialized minmax
					update_max(axis->max, data->minmax[1]);
				else {
					axis->max = data->minmax[1];
					axis->range_isset |= kahto_maxbit<<4; // we use this temporarily to mark initialized minmax
				}
			}
		}
	}

	/* some axis may not be straightly linked to any data, for example countaxis */
	for (int i=0; i<figure->naxis; i++)
		if (used_axis[i])
			figure->axis[i]->range_isset = kahto_minbit | kahto_maxbit;
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
int kahto_find_empty_rectangle(struct kahto_figure *figure, int rwidth, int rheight, int *xout, int *yout, enum kahto_placement method) {
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
	if (!figure->ro_colors_set)
		kahto_set_colors(figure);
	unsigned background = figure->background;
	kahto_fill_u4((void*)image, background, width, height, width);
	for (int i=0; i<figure->ngraph; i++)
		kahto_graph_render(figure->graph[i], (void*)image, width, figure, 0);

	/* including the pointed spot */
	short (*spaceright)[w] = malloc(w*h * sizeof(short));
	for (int j=h-1; j>=0; j--)
		spaceright[j][w-1] = image[j+y0][x1-1] == background;
	for (int j=h-1; j>=0; j--)
		for (int i=w-2; i>=0; i--)
			spaceright[j][i] = (spaceright[j][i+1] + 1) * (image[j+y0][i+x0] == background);

	short (*spacedown)[w] = malloc(w*h * sizeof(short));
	for (int i=w-1; i>=0; i--)
		spacedown[h-1][i] = image[y1-1][i+x0] == background;
	for (int j=h-2; j>=0; j--)
		for (int i=w-1; i>=0; i--)
			spacedown[j][i] = (spacedown[j+1][i] + 1) * (image[j+y0][i+x0] == background);

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
		case kahto_placement_first:
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
		case kahto_placement_singlemaxdist:
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

void kahto_ticks_draw(struct kahto_ticks *ticks, unsigned *canvas, int figurewidth, int figureheight, int ystride) {
	char tickbuff[128];
	char *tick = tickbuff;
	struct ttra *ttra = NULL;
	struct kahto_figure *figure = ticks->axis->figure;

	if (ticks->have_labels) {
		ttra = figure->ttra;
		ttra->canvas = canvas;
		ttra->realw = ystride;
		ttra->realh = figureheight;
		ttra->fg_default = ticks->color;
		ttra->bg_default = -1;
		ttra_print(ttra, "\033[0m");
		set_fontheight(figure, ticks->rowheight);
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

void kahto_axistext_draw(struct kahto_axistext *axistext, unsigned *canvas, int figurewidth, int figureheight, int ystride) {
	struct ttra *ttra = axistext->axis->figure->ttra;
	ttra->canvas = canvas;
	ttra->realw = ystride;
	ttra->realh = figureheight;
	ttra->fg_default = axistext->axis->linestyle.color;
	ttra->bg_default = -1;
	ttra_print(ttra, "\033[0m");
	set_fontheight(axistext->axis->figure, axistext->rowheight);
	int *area = axistext->ro_area;
	put_text(ttra, axistext->text, area[0], area[1], 0, 0, axistext->rotation_grad, area, 0);
	ttra->fg_default = 0xff<<24;
}

void kahto_axis_draw(struct kahto_axis *axis, unsigned *canvas, int figurewidth, int figureheight, int ystride) {
	if (!axis || axis->direction < 0 || !axis->visible)
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

	if (axis->linestyle.style != kahto_line_none_e) {
		typeof(axis->ro_area[0]) area[4];
		memcpy(area, axis->ro_area, sizeof(area));
		if (area[isx] < 0) area[isx] = 0;
		if (area[isx+2] > axis->figure->wh[isx])
			area[isx+2] = axis->figure->wh[isx];

		int WH[] = {figurewidth, figureheight};
		if (area[isx+2] > WH[isx]) area[isx+2] = WH[isx];

		if (axis->linestyle.style)
			kahto_fill_box(canvas, ystride, area, axis->linestyle.color);
	}

	if (axis->ticks)
		kahto_ticks_draw(axis->ticks, canvas, figurewidth, figureheight, ystride);

	for (int i=0; i<axis->ntexts; i++)
		kahto_axistext_draw(axis->text[i], canvas, figurewidth, figureheight, ystride);
}

void kahto_figure_render(struct kahto_figure *figure, uint32_t *canvas, int ystride) {
	if (figure->background)
		kahto_fill_u4(canvas, figure->background, figure->wh[0], figure->wh[1], ystride);
	if (figure->ro_cannot_draw)
		return;
	if (!figure->ro_colors_set)
		kahto_set_colors(figure);

	for (int i=0; i<figure->naxis; i++)
		kahto_axis_draw(figure->axis[i], canvas, figure->wh[0], figure->wh[1], ystride);
	for (int i=0; i<figure->ngraph; i++)
		kahto_graph_render(figure->graph[i], canvas, ystride, figure, 0);
	kahto_legend_draw(figure, canvas, ystride);

	struct ttra *ttra = figure->ttra;
	ttra->canvas = canvas;
	ttra->realw = ystride;
	ttra->realh = figure->wh[1];
	ttra->fg_default = 0xff<<24;
	ttra->bg_default = -1;
	ttra_printf(ttra, "\e[0m");
	if (figure->title.text) {
		struct kahto_text *text = &figure->title;
		set_fontheight(figure, text->rowheight);
		put_text(ttra, text->text, text->ro_area[0], text->ro_area[1], 0, 0, text->rotation_grad, text->ro_area, 0);
	}
	for (int i=0; i<figure->ntexts; i++) {
		struct kahto_text *text = figure->texts+i;
		set_fontheight(figure, text->rowheight);
		put_text(ttra, text->text, text->ro_area[0], text->ro_area[1], 0, 0, text->rotation_grad, text->ro_area, 0);
	}

	if (figure->after_drawing)
		figure->after_drawing(figure, canvas, ystride);
}

static void set_icolor(struct kahto_graph *graph) {
	if (graph->icolor != kahto_automatic) {
		graph->yxaxis[0]->figure->ro_colors_set = 0;
		return;
	}
	struct kahto_figure *figure = graph->yxaxis[0]->figure;
	graph->icolor = figure->icolor;
	if (graph->color)
		return;
	if (!graph->markerstyle.color && graph->markerstyle.marker && graph->markerstyle.marker[0] && !graph->data.list.zdata)
		figure->icolor++;
	else if (!graph->linestyle.color && graph->linestyle.style)
		figure->icolor++;
	else if (!graph->errstyle.color && graph->errstyle.style && (graph->data.list.e0data || graph->data.list.e1data))
		figure->icolor++;
}

void kahto_get_legend_dims_chars(struct kahto_figure *figure, int *lines, int *cols) {
	*lines = *cols = 0;
	for (int i=0; i<figure->ngraph; i++)
		if (figure->graph[i]->label) {
			int w, h;
			ttra_get_textdims_chars(figure->graph[i]->label, &w, &h);
			*lines += h;
			*cols = max(*cols, w);
		}
}

void kahto_get_legend_dims_px(struct kahto_figure *figure, int *Height, int *Width) {
	struct legend *lg = &figure->legend;
	*Width = *Height = 0;
	int rowh = set_fontheight(figure, lg->rowheight);
	if (lg->ro_datay_len <= figure->ngraph) {
		free(lg->ro_datay);
		lg->ro_datay = malloc((lg->ro_datay_len = figure->ngraph+1) * sizeof(lg->ro_datay[0]));
	}
	lg->ro_datay[0] = 0;
	for (int i=0; i<figure->ngraph; i++) {
		lg->ro_datay[i+1] = lg->ro_datay[i];
		if (figure->graph[i]->label) {
			int w, h;
			ttra_get_textdims_pixels(figure->ttra, figure->graph[i]->label, &w, &h);
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

static void legend_placement(struct kahto_figure *figure) {
	if (!figure->legend.visible)
		return;
	float coeff = 1.f / figure->legend.scale;
	figure->legend.borderstyle.thickness *= coeff;
	figure->legend.rowheight *= coeff;
	figure->legend.scale = 1;
try:
	int height, width;
	kahto_get_legend_dims_px(figure, &height, &width);
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
	figure->legend.ro_place_err = kahto_find_empty_rectangle(figure, width, height, &iplace, &jplace, figure->legend.placement);
	if (no_room_for_legend(figure) && figure->legend.scale > figure->legend.minscale) {
		float diff = figure->legend.scale - figure->legend.minscale;
		float newscale;
		if (diff >= 0.1)
			newscale = figure->legend.scale - 0.1;
		else if (diff >= 0.02)
			newscale = figure->legend.scale - 0.02;
		else
			newscale = figure->legend.minscale;
		float coeff = newscale / figure->legend.scale;
		figure->legend.scale = newscale;
		figure->legend.borderstyle.thickness *= coeff;
		figure->legend.rowheight *= coeff;
		goto try;
	}
	figure->legend.ro_xywh[0] = iplace;
	figure->legend.ro_xywh[1] = jplace;
}

static void text_placement(struct kahto_figure *fig, struct kahto_text *text) {
	int *area = text->ro_area;
	if (text->rowheight == 0)
		text->rowheight = fig->legend.rowheight;
	set_fontheight(fig, text->rowheight);
	put_text(fig->ttra, text->text, 0, 0, 0, 0, text->rotation_grad, area, 1); // get textsize
	float move_xy[] = {
		area[2] * text->hvalign[0],
		area[3] * text->hvalign[1],
	};
	int ref_xywh[4];
	switch (text->reference) {
		case kahto_figurearea_e:
			ref_xywh[0] = ref_xywh[1] = 0;
			ref_xywh[2] = fig->wh[0];
			ref_xywh[3] = fig->wh[1];
			break;
		case kahto_dataarea_e:
			memcpy(ref_xywh, fig->ro_inner_xywh, sizeof(ref_xywh));
			break;
		case kahto_dataarea_inner_e:
			memcpy(ref_xywh, fig->ro_inner_xywh, sizeof(ref_xywh));
			ref_xywh[0] += fig->ro_inner_margin[0];
			ref_xywh[1] += fig->ro_inner_margin[1];
			ref_xywh[2] -= fig->ro_inner_margin[0] + fig->ro_inner_margin[2];
			ref_xywh[3] -= fig->ro_inner_margin[1] + fig->ro_inner_margin[3];
			break;
	}
	area[0] = ref_xywh[0] + text->xy[0] * ref_xywh[2] + move_xy[0];
	area[1] = ref_xywh[1] + text->xy[1] * ref_xywh[3] + move_xy[1];
	area[2] += area[0];
	area[3] += area[1];
}

static void texts_placement(struct kahto_figure *fig) {
	for (int i=fig->ntexts-1; i>=0; i--)
		text_placement(fig, fig->texts+i);
}

/* These are probably unnecessary in the graph object */
static void graph_cmap_to_axis(struct kahto_graph *graph, struct kahto_axis *caxis) {
	if (graph->cmap)
		caxis->cmap = graph->cmap;
	else if (graph->cmh_enum) {
		if (graph->cmh_enum < 0)
			caxis->cmap = cmh_colormaps[-graph->cmh_enum].map;
		else
			caxis->cmap = cmh_colormaps[graph->cmh_enum].map;
	}
	if (graph->cmh_enum < 0 && graph->yxaxis[2])
		graph->yxaxis[2]->reverse_cmap = 1;
}

static struct kahto_graph* add_graph(struct kahto_args *args) {
	struct kahto_figure *fig = args->figure;
	if (fig->mem_graph < fig->ngraph+1)
		fig->graph = realloc(fig->graph,
			(fig->mem_graph = fig->ngraph+3) * sizeof(void*));

	struct kahto_graph *graph;
	fig->graph[fig->ngraph++] = graph = calloc(1, sizeof(struct kahto_graph));
	/* copy to kahto_graph the part which is shared with kahto_args */
	memcpy(&graph->yxaxis, &args->yaxis, sizeof(struct kahto_graph) - ((char*)graph->yxaxis - (char*)graph));

	/* find or create a data object for the graph */
#define nth(ptr, n) (&(ptr) + (n))
	for (int iyxz=0; iyxz<arrlen(graph->data.arr); iyxz++) {
		void *thedata = *nth(args->ydata, iyxz);
		if (!thedata && iyxz >= 2)
			continue;

		int *type = nth(args->ytype, iyxz);
		if (*type == kahto_notype && thedata)
			*type = args->ytype; // unspecified type equals ytype, useful with errorbars

		struct kahto_data *data = NULL;
		long length = args->kahto_xlen * args->kahto_ylen; // only with colormesh
		if (length == 0)
			length = args->kahto_len;
		else if (iyxz == 1)
			length = args->kahto_xlen;
		else if (iyxz == 0)
			length = args->kahto_ylen;

		if (!thedata) {
			for (data=fig->data.next; data; data=data->next)
				if (data->data == thedata &&
					data->type == *type &&
					data->length == length &&
					!memcmp(data->minmax, args->minmax[iyxz], sizeof(data->minmax)) &&
					(data->have_minmax & (kahto_minbit|kahto_maxbit)) == (kahto_minbit|kahto_maxbit)
				) {
					graph->data.arr[iyxz] = data;
					goto found; }
		}
		else if (args->yxzowner[iyxz] != -1)
			for (data=fig->data.next; data; data=data->next) {
				if (data->data == thedata &&
					data->type == *type &&
					data->length == length &&
					data->stride == *nth(args->ystride, iyxz)
				) {
					graph->data.arr[iyxz] = data;
					goto found; }
			}

		/* add a new data */
		data = graph->data.arr[iyxz] = calloc(1, sizeof(*data));
		data->data = thedata;
		data->type = *nth(args->ytype, iyxz);
		data->length = length;
		data->stride = *nth(args->ystride, iyxz);
		data->prev = &fig->data;
		data->next = data->prev->next;
		data->prev->next = data;
		if (data->next)
			data->next->prev = data;
		if (!thedata) {
			data->have_minmax = kahto_minbit|kahto_maxbit;
			data->minmax[1] = length-1;
		}

found:
		++data->n_users;
		if (args->yxzowner[iyxz])
			data->owner = args->yxzowner[iyxz];
		if (args->have_minmax[iyxz] & kahto_minbit) {
			data->minmax[0] = args->minmax[iyxz][0];
			data->have_minmax |= kahto_minbit;
		}
		if (args->have_minmax[iyxz] & kahto_maxbit) {
			data->minmax[1] = args->minmax[iyxz][1];
			data->have_minmax |= kahto_maxbit;
		}
	}
#undef nth

	/* add yaxis and xaxis */
	for (int iaxis=0; iaxis<2; iaxis++)
		if (!graph->yxaxis[iaxis]) {
			if (fig->ngraph > 1)
				graph->yxaxis[iaxis] = fig->graph[fig->ngraph-2]->yxaxis[iaxis];
			else {
				graph->yxaxis[iaxis] = kahto_axis_new(fig, 'y'-iaxis, 0+iaxis);
				graph->yxaxis[iaxis]->ticks->gridstyle.style = kahto_line_normal_e;
			}
		}

	/* add featureaxis, if necessary */
	if (graph->data.arr[2] && !graph->yxaxis[2]) {
		for (int i=fig->naxis-1; i>=0; i--)
			if (fig->axis[i]->feature == args->zfeature) {
				graph->yxaxis[2] = fig->axis[i];
				goto found1; }
		graph->yxaxis[2] = kahto_featureaxis_new(fig, 'y', args->zfeature);
found1:
	}

	if (graph->yxaxis[2]) {
		if (!my_isnan(args->caxis_center))
			graph->yxaxis[2]->center = args->caxis_center;
		graph_cmap_to_axis(graph, graph->yxaxis[2]);
	}

	if (graph->markerstyle.count) {
		if (!graph->countaxis)
			graph->countaxis = kahto_featureaxis_new(fig, 'y', kahto_color_e);
		if (!my_isnan(args->caxis_center))
			graph->countaxis->center = args->caxis_center;
		graph_cmap_to_axis(graph, graph->countaxis);
	}

	if (!graph->data.arr[1])
		graph->yxaxis[1]->ticks->integers_only = 1; // Väärin. Akselilla voi olla muutakin dataa.
	for (int i=0; i<arrlen(graph->yxaxis); i++)
		if (graph->yxaxis[i])
			graph->yxaxis[i]->range_isset = 0;
	set_icolor(graph); // tämä pitäisi ohittaa, jos käytetään väriakselia
	return graph;
}

struct kahto_axistext* kahto_add_axistext(struct kahto_axis *axis, struct kahto_axistext *text) {
	if (axis->ntexts >= axis->memtext)
		axis->text = realloc(axis->text, (axis->memtext = axis->ntexts + 2) * sizeof(axis->text[0]));
	axis->text[axis->ntexts++] = text;
	return text;
}

struct kahto_figure* kahto_add_text(struct kahto_figure *figure, struct kahto_text *text) {
	if (figure->ntexts >= figure->memtext)
		figure->texts = realloc(figure->texts, (figure->memtext = figure->ntexts + 2) * sizeof(figure->texts[0]));
	figure->texts[figure->ntexts++] = *text;
	text = figure->texts + figure->ntexts-1;
	if (text->owner < 0) {
		text->text = strdup(text->text);
		text->owner = 1;
	}
	return figure;
}

struct kahto_axistext* kahto_axislabel(struct kahto_axis *axis, char *label) {
	struct kahto_axistext *text = malloc(sizeof(struct kahto_axistext));
	*text = (struct kahto_axistext) {
		.text = label,
		.pos = 0.5,
		.hvalign = {-0.5, -1.2 * (axis->pos < 0.5)},
		.rowheight = (axis->ticks ? axis->ticks->rowheight : 2.4/80) * 1.3,
		.axis = axis,
		.rotation_grad = 300 * (axis->direction == 1),
		.type = kahto_axistext_label,
	};
	return kahto_add_axistext(axis, text);
}

void kahto_ticklabels(struct kahto_axis *axis, char **names, int howmany) {
	axis->ticks->init = kahto_init_ticker_arbitrary_datacoord_enum;
	axis->ticks->tickerdata.arb.labels = names;
	axis->ticks->tickerdata.arb.nticks = howmany;
	axis->ticks->rotation_grad = 300;
}

struct kahto_args* kahto_defaultargs(struct kahto_args *args) {
	*args = (struct kahto_args) {
		__kahto_defaultargs
	};
	return args;
}

struct kahto_args* kahto_default_lineargs(struct kahto_args *args) {
	*args = (struct kahto_args) {
		__kahto_defaultargs,
			kahto_lineargs,
	};
	return args;
}

struct kahto_axis* kahto_remove_ticks(struct kahto_axis *axis) {
	free(axis->ticks);
	axis->ticks = NULL;
	return axis;
}

void kahto_destroy_axis(struct kahto_axis *axis) {
	for (int i=axis->ntexts-1; i>=0; i--) {
		if (axis->text[i]->owner)
			free(axis->text[i]->text);
		free(axis->text[i]);
	}
	free(axis->text);
	free(axis->ticks);
	free(axis);
}

void kahto_data_unlink(struct kahto_data *data) {
	if (!data || --data->n_users)
		return;
	if (data->owner)
		free(data->data);
	data->data = NULL;
	if (data->next)
		data->next->prev = data->prev;
	data->prev->next = data->next;
	free(data);
}

void kahto_destroy_graph(struct kahto_graph *graph) {
	for (int j=0; j<arrlen(graph->data.arr); j++)
		kahto_data_unlink(graph->data.arr[j]);
	if (graph->cmap_owner)
		free(graph->cmap);
	if (graph->labelowner)
		free((void*)(intptr_t)graph->label);
	free(graph);
}

struct kahto_figure* kahto_destroy_single(struct kahto_figure *fig) {
	/* Subfigures must be destroyed already or referenced by other pointers. */
	free(fig->subfigures);
	free(fig->subfigures_xywh);

	for (int i=0; i<fig->naxis; i++)
		kahto_destroy_axis(fig->axis[i]);
	free(fig->axis);
	if (fig->ttra_owner) {
		ttra_destroy(fig->ttra);
		free(fig->ttra);
	}
	free(fig->legend.ro_datay);

	for (int i=0; i<fig->ngraph; i++)
		kahto_destroy_graph(fig->graph[i]);
	free(fig->graph);

	if (fig->title.owner)
		free((void*)(intptr_t)fig->title.text);

	for (int i=fig->ntexts-1; i>=0; i--)
		if (fig->texts[i].owner)
			free((void*)(intptr_t)fig->texts[i].text);
	free(fig->texts);

	if (fig->colorscheme.owner)
		free(fig->colorscheme.colors);

	memset(fig, 0, sizeof(*fig));
	return fig;
}

void kahto_destroy(struct kahto_figure *fig) {
	if (!fig)
		return;
	for (int i=0; i<fig->nsubfigures; i++)
		kahto_destroy(fig->subfigures[i]);
	free(kahto_destroy_single(fig));
}

struct kahto_figure* kahto_clean(struct kahto_figure *fig) {
	if (!fig)
		return fig;
	for (int i=0; i<fig->nsubfigures; i++)
		kahto_destroy(fig->subfigures[i]);
	struct ttra *ttra = fig->ttra;
	fig->ttra_owner = 0;
	int ttra_owner = fig->ttra_owner;
	kahto_destroy_single(fig); // fig not freed
	kahto_figure_init(fig);
	fig->ttra = ttra;
	fig->ttra_owner = ttra_owner;
	return fig;
}

struct kahto_figure* kahto_plot_args(struct kahto_args *args) {
	if (args->argsfun)
		args->argsfun(args);

	if (args->kahto_len < 0)
		args->kahto_len = args->kahto_xlen * args->kahto_ylen;

	if (args->plot_interlazed_y || args->labels) {
		const char **labels = args->labels;
		args->plot_interlazed_y = 0;
		args->labels = NULL;
		if (labels)
			args->label = labels[0];

		struct kahto_figure *figure = kahto_plot_args(args);

		args->figure = figure;
		args->figureptr = NULL;
		for (int i=1; i<args->ystride; i++) {
			if (labels)
				args->label = labels[i];
			args->ydata += kahto_sizes[args->ytype];
			kahto_plot_args(args);
		}
		return figure;
	}

	struct kahto_figure *figure1 = NULL;
	struct kahto_figure **figure = args->figureptr ? args->figureptr : &figure1;
	if (!*figure)
		*figure =
			args->figure ? args->figure :
			args->yaxis ? args->yaxis->figure :
			args->xaxis ? args->xaxis->figure :
			kahto_figure_new();
	args->figure = *figure;
	struct kahto_graph *graph = add_graph(args);

	/* copy if necessary */
	for (int idim=0; idim<arrlen(graph->data.arr); idim++) {
		struct kahto_data *data = graph->data.arr[idim];
		if (!data || data->owner != -1)
			continue;
		size_t size = kahto_sizes[data->type] * data->length;
		void *old = data->data;
		if (!(data->data = malloc(size)))
			fprintf(stderr, "malloc %zu (%s)\n", size, __func__);
		if (data->stride == 1)
			memcpy(data->data, old, size);
		else {
			/* after copying, stride == 1 */
			char *dt = data->data;
			int size = kahto_sizes[data->type];
			int stride = data->stride;
			for (int i=data->length-1; i>=0; i--)
				memcpy(dt+i*size, (char*)old+i*stride*size, size);
			data->stride = 1;
		}
		data->owner = 1;
	}

	if (graph->labelowner < 0) {
		graph->label = strdup(graph->label);
		graph->labelowner = 1;
	}

	return *figure;
}

void kahto_forward_graphcolor(struct kahto_graph *graph) {
	if (!graph->markerstyle.color)
		graph->markerstyle.color = graph->color;
	if (!graph->linestyle.color)
		graph->linestyle.color = graph->color;
	if (!graph->errstyle.color)
		graph->errstyle.color = graph->color;
}

void kahto_set_colors(struct kahto_figure *figure) {
	struct kahto_colorscheme *cs = &figure->colorscheme;
	if (!cs->colors || cs->cmh_enum) {
		cs->ncolors = figure->ngraph;
		if (cs->colors && cs->owner)
			free(cs->colors);
		cs->colors = malloc((cs->ncolors+1) * sizeof(unsigned));
		cs->owner = 1;
		cs->colors[cs->ncolors] = 0;
		kahto_make_colorscheme_from_cmap(cs->colors, cmh_colormaps[Abs(cs->cmh_enum)].map, cs->ncolors, cs->without_ends);
		cs->cmh_enum = 0;
	}
	if (cs->ncolors <= 0) {
		cs->ncolors = 0;
		while (cs->colors[cs->ncolors++]); // count the number of colors
	}
	for (int i=0; i<figure->ngraph; i++) {
		if (!figure->graph[i]->color)
			figure->graph[i]->color = cs->colors[figure->graph[i]->icolor % cs->ncolors];
		kahto_forward_graphcolor(figure->graph[i]);
	}
	figure->ro_colors_set = 1;
}

static void subfigures_xywh_to_pixels(struct kahto_figure *fig, int islot, int px[4]) {
	px[0] = iroundpos(fig->subfigures_xywh[islot][0] * fig->ro_inner_xywh[2]) + fig->ro_inner_xywh[0];
	px[1] = iroundpos(fig->subfigures_xywh[islot][1] * fig->ro_inner_xywh[3]) + fig->ro_inner_xywh[1];
	px[2] = iroundpos(fig->ro_inner_xywh[2] * fig->subfigures_xywh[islot][2]);
	px[3] = iroundpos(fig->ro_inner_xywh[3] * fig->subfigures_xywh[islot][3]);
	if (px[0] + px[2] > fig->wh[0])
		px[2] = fig->wh[0] - px[0];
	if (px[1] + px[3] > fig->wh[1])
		px[3] = fig->wh[1] - px[1];
}

void kahto_xywh_to_subfigures(struct kahto_figure *fig) {
	for (int isub=0; isub<fig->nsubfigures; isub++) {
		if (!fig->subfigures[isub])
			continue;
		int xywh_px[4];
		subfigures_xywh_to_pixels(fig, isub, xywh_px);
		fig->subfigures[isub]->wh[0] = xywh_px[2];
		fig->subfigures[isub]->wh[1] = xywh_px[3];
		fig->subfigures[isub]->ro_corner[0] = xywh_px[0];
		fig->subfigures[isub]->ro_corner[1] = xywh_px[1];
		fig->subfigures[isub]->super = fig;
	}
}

void kahto_clear_data(struct kahto_figure *figure, uint32_t *canvas, int ystride) {
	int ystart = figure->ro_inner_xywh[1];
	int xstart = figure->ro_inner_xywh[0];
	kahto_fill_u4(canvas+ystride*ystart+xstart, figure->background, figure->ro_inner_xywh[2], figure->ro_inner_xywh[3], ystride);
}

/* For animation. Selectively copied from draw_ticks. */
void kahto_draw_grid(struct kahto_figure *figure, uint32_t *canvas, int ystride) {
	int naxis = figure->naxis;
	int *xywh = figure->ro_inner_xywh;
	int inner_area[] = xywh_to_area(xywh);
	for (int iaxis=0; iaxis<naxis; iaxis++) {
		struct kahto_axis *axis = figure->axis[iaxis];
		struct kahto_ticks *ticks = axis->ticks;
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

void kahto_connect_x(struct kahto_figure **figs, int nconnected) {
	int x0 = figs[0]->ro_inner_xywh[0];
	int x1 = x0 + figs[0]->ro_inner_xywh[2];
	for (int i=1; i<nconnected; i++) {
		int *xywh = figs[i]->ro_inner_xywh;
		if (xywh[0] > x0)
			x0 = xywh[0];
		else if (xywh[0] + xywh[2] < x1)
			x1 = xywh[0] + xywh[2];
	}

	for (int i=0; i<nconnected; i++) {
		int *xywh = figs[i]->ro_inner_xywh;
		xywh[0] = x0;
		xywh[2] = x1 - x0;
	}
}

void kahto_layout(struct kahto_figure *fig) {
	/* this figure first because ro_inner_xywh and other things are needed in subfigures */
	if (kahto_figure_layout(fig) && fig->fix_too_little_space) {
		fig->fix_too_little_space(fig);
		kahto_figure_layout(fig);
	}
	/* then subfigures */
	kahto_xywh_to_subfigures(fig);
	for (int i=0; i<fig->nsubfigures; i++)
		if ((fig->subfigures[i]))
			kahto_layout(fig->subfigures[i]);
	if (fig->nconnected_x)
		kahto_connect_x(fig->connected_x, fig->nconnected_x);
}

void kahto_render(struct kahto_figure *fig, uint32_t *canvas, int ystride) {
	kahto_figure_render(fig, canvas, ystride); // before subfigures to not cover them with background color
	struct kahto_figure *f;
	for (int i=0; i<fig->nsubfigures; i++)
		if ((f = fig->subfigures[i]))
			kahto_render(f, canvas + f->ro_corner[1]*ystride + f->ro_corner[0], ystride);
	if (fig->revert_fixes)
		fig->revert_fixes(fig);
}

void kahto_draw(struct kahto_figure *fig, uint32_t *canvas, int ystride) {
	kahto_layout(fig);
	kahto_render(fig, canvas, ystride);
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

static int async_response = 2,
		   async_request = 1,
		   async_step = 3;

static int async_update(struct kahto_async *async, uint32_t *canvas, int ystride) {
	if (!async)
		return 0;
	if (async->_exit == async_request)
		return -1;
	if (async->_lock != async_request)
		return 0;
	async->canvas = canvas;
	async->ystride = ystride;
	async->_lock = async_response;
	while (async->_lock == async_response)
		usleep(3000);
	if (async->_lock == async_step)
		async->_lock = async_request;
	return 1;
}

int kahto_async_lock(struct kahto_async *async) {
	async->_lock = async_request;
	while (async->_lock != async_response) {
		if (!kahto_async_running(async))
			return 1;
		usleep(10);
	}
	return 0;
}

void kahto_async_unlock(struct kahto_async *async) {
	async->_lock = 0;
}

void kahto_async_unlock_step(struct kahto_async *async) {
	async->_lock = async_step;
}

void kahto_async_stop(struct kahto_async *async) {
	async->_exit = async_request;
}

int kahto_async_running(struct kahto_async *async) {
	return async->_exit != async_response;
}

void kahto_async_destroy(struct kahto_async *async) {
	if (kahto_async_running(async))
		kahto_async_stop(async);
	pthread_join(async->_thread, NULL);
	kahto_destroy(async->figure);
	free(async);
}

static void* async_show(void *vargs) {
	struct kahto_async *h = vargs;
	kahto_show_preserve(h->figure);
	h->_exit = async_response;
	return NULL;
}

struct kahto_async* kahto_async_show(struct kahto_figure *fig) {
	struct kahto_async *h = calloc(1, sizeof(*h));
	h->figure = fig;
	fig->async = h;
	pthread_create(&h->_thread, NULL, async_show, h);
	return h;
}

static void* async_write_mp4(void *vargs) {
	struct kahto_async *h = vargs;
	kahto_async_unlock_step(h);
	kahto_write_mp4_preserve(h->figure, NULL, h->_fps);
	h->_exit = async_response;
	return NULL;
}

struct kahto_async* kahto_async_write_mp4(struct kahto_figure *fig, const char *name, float fps) {
	struct kahto_async *h = calloc(1, sizeof(*h));
	h->figure = fig;
	h->_fps = fps;
	if (name)
		fig->name = (void*)(intptr_t)name;
	fig->async = h;
	pthread_create(&h->_thread, NULL, async_write_mp4, h);
	return h;
}

void kahto_async_join(struct kahto_async **async, int n) {
	unsigned char *mask = malloc(n);
	memset(mask, 1, n);
	while (1) {
		usleep(50'000);
		int count = 0;
		for (int i=0; i<n; i++) {
			if (mask[i] && !kahto_async_running(async[i])) {
				kahto_async_destroy(async[i]);
				mask[i] = 0;
			}
			count += mask[i];
		}
		if (!count)
			break;
	}
	free(mask);
}

#ifdef HAVE_PNG
#include "kahto_png.c"
#else
struct kahto_figure* kahto_write_png_preserve(struct kahto_figure *a, const char *b) {
	fprintf(stderr, "kahto library was compiled without support for writing a png image.\n"
		"Configure and compile again with libpng enabled.\n"
	);
	return a;
}
#endif
void kahto_write_png(struct kahto_figure *fig, const char *name) {
	kahto_destroy(kahto_write_png_preserve(fig, name));
}

#ifdef HAVE_ffmpeg
#include "kahto_video.c"
#else
struct kahto_figure* kahto_write_mp4_preserve(struct kahto_figure *fig, const char *name, float fps) {
	fprintf(stderr, "kahto library was compiled without support for \e[1m%s\e[0m\n", __func__);
	return fig;
}
#endif
void kahto_write_mp4(struct kahto_figure *fig, const char *name, float fps) {
	kahto_destroy(kahto_write_mp4_preserve(fig, name, fps));
}

#ifdef HAVE_wlh
#include "kahto_wayland.c"
#else
struct kahto_figure* kahto_show_preserve_(struct kahto_figure *fig, char *name) {
	fprintf(stderr, "kahto was compiled without support for creating a window\n"
		"Configure and compile again with libwaylandhelper enabled.\n"
	);
	return fig;
}
#endif
void kahto_show_(struct kahto_figure *fig, char *name) {
	kahto_destroy(kahto_show_preserve_(fig, name));
}
