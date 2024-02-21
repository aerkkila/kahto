#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ttra.h>
#include <float.h>

#define using_cplot
#include "cplot.h"

#define min(a,b) ((a) < (b) ? a : (b))
#define max(a,b) ((a) > (b) ? a : (b))
#define update_min(a, b) ((a) = min(a,b))
#define update_max(a, b) ((a) = max(a,b))
#define arrlen(a) (sizeof(a) / sizeof(*(a)))
#define RGB(r, g, b) (0xff<<24 | (r)<<16 | (g)<<8 | (b)<<0)
#define xywh_to_area(xywh) {xywh[0], xywh[1], xywh[2]+xywh[0], xywh[3]+xywh[1]}

static inline int iround(float f) {
    int a = f;
    return a + (f-a >= 0.5) - (a-f > 0.5);
}

static unsigned fg = 0xff<<24;

static inline int iroundpos(float f) {
    int a = f;
    return a + (f-a >= 0.5);
}

static inline int iceil(float f) {
    int a = f;
    return a + (a != f);
}

#include "data_to_pixels.c"
#include "rotate.c"
#include "rendering.c"
#include "ticker.c"
#include "wayland_helper/waylandhelper.h"

struct $axis* cplot_axis_alloc(struct $axes *axes) {
    struct $axis *axis = calloc(1, sizeof(struct $axis));
    axis->axes = axes;
    axes->naxis++;
    axis->color = fg;
    axis->thickness = 1.0 / 400;
    axis->min = 0;
    axis->max = 1;
    return axis;
}

struct $ticks* cplot_ticks_alloc(struct $axis *axis, int have_labels) {
    struct $ticks *ticks = calloc(1, sizeof(struct $ticks));
    ticks->axis = axis;
    ticks->color = fg;
    ticks->ticker.init = cplot_init_ticker_default;
    ticks->length = 1.0 / 80;
    ticks->thickness = -1; // same as axis
    ticks->hvalign_text[0] = -0.5;
    ticks->grid_on = 1;
    ticks->grid_pen.thickness = 1.0/1200;
    ticks->grid_pen.color = RGB(100, 100, 100);

    if (have_labels) {
	ticks->ttra = calloc(1, sizeof(struct ttra));
	ttra_init(ticks->ttra);
	ticks->rowheight = 2.4*ticks->length;
    }

    if (axis->pos >= 0.5) {
	ticks->crossaxis = 0;
	ticks->hvalign_text[1] = 0;
	ticks->ascending = 1;
    }
    else {
	ticks->crossaxis = -1;
	ticks->hvalign_text[1] = -1;
	ticks->ascending = 0;
    }

    return ticks;
}

void cplot_add_axistext(struct $axis *axis, struct $axistext *text) {
    if (axis->ntext >= axis->mem_text)
	axis->text = realloc(axis->text, (axis->mem_text = axis->ntext + 2) * sizeof(void*));
    axis->text[axis->ntext++] = text;
}

struct $axes* cplot_axes_alloc() {
    struct $axes *axes = calloc(1, sizeof(struct $axes));
    axes->borders = ($f4si){0, 0, 1, 1};
    axes->background = -1;

    axes->axis = calloc((axes->mem_axis = 4) + 1, sizeof(void*));
    axes->axis[0] = cplot_axis_alloc(axes);
    axes->axis[0]->x_or_y = 'x'; // bottom x-axis
    axes->axis[0]->pos = 1;
    axes->axis[0]->ticks[0] = cplot_ticks_alloc(axes->axis[0], 1);
    axes->axis[0]->nticks++;

    axes->axis[1] = cplot_axis_alloc(axes);
    axes->axis[1]->x_or_y = 'y'; // left y-axis
    axes->axis[1]->pos = 0;
    axes->axis[1]->ticks[0] = cplot_ticks_alloc(axes->axis[1], 1);
    axes->axis[1]->nticks++;

    axes->ttra = calloc(1, sizeof(struct ttra));
    ttra_init(axes->ttra);

    return axes;
}

static void init_datastyle(struct $data *data) {
    if (data->markersize == 0)
	data->markersize = 1.0 / 150;
    if (!data->marker)
	data->marker = "o";
    if (!data->color)
	data->color = RGB(0, 70, 185);
}

static void add_data(struct cplot_args *args) {
    if (args->axes->mem_data < args->axes->ndata+1)
	args->axes->data = realloc(args->axes->data, (args->axes->mem_data = args->axes->ndata+3) * sizeof(void*));
    struct $data *data;
    args->axes->data[args->axes->ndata++] = data = malloc(sizeof(struct $data));
    memcpy(data, &args->ydata, sizeof(struct $data));
    if (!data->yxaxis[0])
	data->yxaxis[0] = $yaxis0(args->axes);
    if (!data->yxaxis[1])
	data->yxaxis[1] = $xaxis0(args->axes);
    data->yxaxis[0]->range_isset = 0;
    data->yxaxis[1]->range_isset = 0;
    init_datastyle(data);
}

struct $axes* $plot_args(struct cplot_args *args) {
    struct $axes *axes = args->axes ? args->axes : cplot_axes_alloc();
    args->axes = axes;
    if (args->ydata)
	add_data(args);
    return axes;
}

void cplot_ticks_draw(struct $ticks *ticks, unsigned *canvas, int axeswidth, int axesheight, int ystride, const int axis_xywh[4]) {
    float min = ticks->axis->min;
    float max = ticks->axis->max;
    int nticks = ticks->ticker.init(&ticks->ticker, min, max);
    float thickness = ticks->thickness < 0 ? ticks->axis->thickness * -ticks->thickness : ticks->thickness;
    thickness *= axesheight;
    if (thickness > 1e-9 && thickness < 1)
	thickness = 1;

    char tick[32];
    if (ticks->ttra) {
	ticks->ttra->canvas = canvas;
	ticks->ttra->realw = axeswidth;
	ticks->ttra->realh = axesheight;
	ticks->ttra->w = axeswidth; // turha?
	ticks->ttra->h = axesheight; // turha?
	ticks->ttra->fg_default = ticks->color;
	ticks->ttra->bg_default = -1;
	ttra_print(ticks->ttra, "\033[0m");
	ttra_set_fontheight(ticks->ttra, ticks->rowheight*axesheight);
    }

    int isx = ticks->axis->x_or_y == 'x';
    int line_px[4];
    line_px[isx] = axis_xywh[isx] +
	iroundpos(ticks->axis->pos * axis_xywh[isx+2] + ticks->crossaxis * ticks->length * axesheight);
    line_px[isx+2] = axis_xywh[isx] +
	iroundpos(ticks->axis->pos * axis_xywh[isx+2] + (ticks->crossaxis+1) * ticks->length * axesheight);

    ticks->ro_lines[0] = line_px[isx];
    ticks->ro_lines[1] = line_px[isx+2];
    int minmaxpos[2];
    memcpy(ticks->ro_labelarea, ticks->axis->ro_line, sizeof(ticks->ro_labelarea));

    double axisdiff = ticks->axis->max - ticks->axis->min;
    int axis_area[] = xywh_to_area(axis_xywh);

    /* Silmukan kääntäminen muuttaisi minmaxpos-muuttujaa. */
    for (int itick=0; itick<nticks; itick++) {
	double pos_data = ticks->ticker.get_tick(&ticks->ticker, itick, tick, 32);
	double pos_rel = (pos_data - ticks->axis->min) / axisdiff;
	if (!isx)
	    pos_rel = 1 - pos_rel;
	line_px[!isx] = line_px[!isx+2] = minmaxpos[itick!=0] = axis_xywh[!isx] + iroundpos(pos_rel * axis_xywh[!isx+2]);
	draw_thick_line_bresenham(canvas, ystride, line_px, ticks->color, thickness, axis_area);
	int area_text[4] = {0};
	if (ticks->ttra && tick[0])
	    if (put_text(ticks->ttra, tick, line_px[0], line_px[3], ticks->hvalign_text[!isx], ticks->hvalign_text[isx], 0, area_text) >= 0) { // successful geometry
		update_min(ticks->ro_labelarea[0], area_text[0]);
		update_min(ticks->ro_labelarea[1], area_text[1]);
		update_max(ticks->ro_labelarea[2], area_text[2]);
		update_max(ticks->ro_labelarea[3], area_text[3]);
	    }
	if (ticks->grid_on) {
	    int gridline[4];
	    gridline[!isx] = gridline[!isx+2] = line_px[!isx];
	    int *xywh = ticks->axis->axes->ro_inner_xywh;
	    gridline[isx] = xywh[isx];
	    gridline[isx+2] = xywh[isx] + xywh[isx+2];
	    draw_thick_line_bresenham(canvas, axeswidth, gridline, ticks->grid_pen.color, ticks->grid_pen.thickness * axesheight, axis_area);
	}
    }

    ticks->ro_tot_area[isx+0] = min(ticks->ro_labelarea[isx+0], ticks->ro_lines[0]);
    ticks->ro_tot_area[isx+2] = max(ticks->ro_labelarea[isx+2], ticks->ro_lines[1]);
    ticks->ro_tot_area[!isx+0] = min(ticks->ro_labelarea[!isx+0], minmaxpos[0]);
    ticks->ro_tot_area[!isx+2] = max(ticks->ro_labelarea[!isx+2], minmaxpos[1]);

    struct $axis *a = ticks->axis;
    update_min(a->ro_tick_area[0], ticks->ro_tot_area[0]);
    update_min(a->ro_tick_area[1], ticks->ro_tot_area[1]);
    update_max(a->ro_tick_area[2], ticks->ro_tot_area[2]);
    update_max(a->ro_tick_area[3], ticks->ro_tot_area[3]);
}

void cplot_axistext_draw(struct $axistext *axistext, unsigned *canvas, int axeswidth, int axesheight, int ystride) {
    struct ttra *ttra = axistext->axis->axes->ttra;
    ttra->canvas = canvas;
    ttra->realw = axeswidth;
    ttra->realh = axesheight;
    ttra->fg_default = axistext->axis->color;
    ttra->bg_default = -1;
    ttra_print(ttra, "\033[0m");
    ttra_set_fontheight(ttra, axistext->rowheight*axesheight);
    int xy[2];
    int coord = axistext->axis->x_or_y == 'y';
    int *axis_tot_area = axistext->axis->ro_tot_area;
    int axislength = axis_tot_area[coord+2] - axis_tot_area[coord];
    xy[coord] = iroundpos(axis_tot_area[coord] + axislength * axistext->pos);
    xy[!coord] = axis_tot_area[!coord + 2*(axistext->axis->pos >= 0.5)];
    put_text(ttra, axistext->text, xy[0], xy[1], axistext->hvalign[coord], axistext->hvalign[!coord], axistext->rotation100, axistext->ro_area);
}

static inline $f4si axis_get_line(struct $axis *axis) {
    switch (axis->x_or_y) {
	case 'x': return ($f4si){0, axis->pos, 1, axis->pos};
	case 'y': return ($f4si){axis->pos, 0, axis->pos, 1};
	default: __builtin_unreachable();
    }
}

void cplot_axis_draw(struct $axis *axis, unsigned *canvas, int axeswidth, int axesheight, int ystride, const int axis_xywh[4]) {
    float thickness = axis->thickness * axesheight;
    if (thickness > 1e-9 && thickness < 1)
	thickness = 1;
    $f4si line = axis_get_line(axis);

    {
	int tmp[] = {
	    axis_xywh[0] + line[0] * (axis_xywh[2]-1),
	    axis_xywh[1] + line[1] * (axis_xywh[3]-1),
	    axis_xywh[0] + line[2] * (axis_xywh[2]-1),
	    axis_xywh[1] + line[3] * (axis_xywh[3]-1),
	};
	memcpy(axis->ro_line, tmp, sizeof(tmp));
    }
    int axis_area[] = xywh_to_area(axis_xywh);
    draw_thick_line_bresenham(canvas, ystride, axis->ro_line, axis->color, thickness, axis_area);

    axis->ro_tick_area[0] = axis->ro_tick_area[2] = axis->ro_line[0];
    axis->ro_tick_area[1] = axis->ro_tick_area[3] = axis->ro_line[1];
    for (int i=0; i<axis->nticks; i++)
	cplot_ticks_draw(axis->ticks[i], canvas, axeswidth, axesheight, ystride, axis_xywh);

    axis->ro_tot_area[0] = min(axis->ro_line[0], axis->ro_tick_area[0]);
    axis->ro_tot_area[1] = min(axis->ro_line[1], axis->ro_tick_area[1]);
    axis->ro_tot_area[2] = max(axis->ro_line[2], axis->ro_tick_area[2]);
    axis->ro_tot_area[3] = max(axis->ro_line[3], axis->ro_tick_area[3]);

    for (int i=0; i<axis->ntext; i++)
	cplot_axistext_draw(axis->text[i], canvas, axeswidth, axesheight, ystride);
}

static inline float get_ticks_overlength(struct $ticks *tk) {
    float f = tk->length * (1 + tk->crossaxis) + tk->axis->pos - 1;
    return f * (f>0);
}

static inline float get_ticks_underlength(struct $ticks *tk) {
    float f = -(tk->crossaxis*tk->length + tk->axis->pos);
    return f * (f>0);
}

static void get_axislabel_limits(struct $axis *axis, int axeswidth, int axesheight, float lim2inout[2]) {
    struct ttra *ttra = axis->axes->ttra;
    int coord = axis->x_or_y == 'x';
    float old[2];
    memcpy(old, lim2inout, sizeof(old));
    for (int itext=0; itext<axis->ntext; itext++) {
	struct $axistext *text = axis->text[itext];
	int wh[2];
	ttra_set_fontheight(ttra, text->rowheight * axesheight);
	ttra_get_textdims_pixels(ttra, text->text, wh+0, wh+1);
	float frac = wh[coord + ((int)text->rotation100 % 50 == 25)] / (float)axesheight;
	int side = axis->pos >= 0.5;
	lim2inout[side] = max(old[side]+frac, lim2inout[side]);
    }
}

/* Akselia kohtisuoraan */
static void get_ticklabel_limits(struct $axis *axis, int axeswidth, int axesheight, float lim2out[2]) {
    memset(lim2out, 0, 2*sizeof(float));
    float min = axis->min,
	  max = axis->max;
    for (int i=0; i<axis->nticks; i++) {
	struct $ticks *tk = axis->ticks[i];
	float lim2[] = {get_ticks_underlength(tk), get_ticks_overlength(tk)};
	if (!tk->ticker.init)
	    goto endloop;

	ttra_set_fontheight(tk->ttra, tk->rowheight * axesheight);
	int nlabels = tk->ticker.init(&tk->ticker, min, max);
	char out[128];
	int maxcols=0, maxrows=0;
	int width, height;
	for (int i=0; i<nlabels; i++) {
	    tk->ticker.get_tick(&tk->ticker, i, out, 128);
	    ttra_get_textdims_chars(out, &width, &height);
	    if (height > maxrows)
		maxrows = height;
	    if (width > maxcols)
		maxcols = width;
	}

	/* voinee siirtää silmukan ulkopuolelle */
	switch (axis->x_or_y) {
	    case 'x':
		lim2[tk->ascending] += (float)maxrows * tk->ttra->fontheight / axesheight; // alignment?
		break;
	    case 'y':
		lim2[tk->ascending] += (float)maxcols * tk->ttra->fontwidth / axesheight;
		break;
	}

endloop:
	lim2out[0] = max(lim2[0], lim2out[0]);
	lim2out[1] = max(lim2[1], lim2out[1]);
    }
    get_axislabel_limits(axis, axeswidth, axesheight, lim2out);
}

/* Akselin suuntaan */
static void get_ticklabel_limits_round2(struct $axis *axis, int axeswidth, int axesheight, int axis_xywh[4]) {
    double axisdiff = axis->max - axis->min;
    for (int iticks=0; iticks<axis->nticks; iticks++) {
	struct $ticks *tk = axis->ticks[iticks];
	if (!tk->ticker.init)
	    continue;
	float min = axis->min,
	      max = axis->max;

	int nlabels = tk->ticker.init(&tk->ticker, min, max);
	char out[128];
	int coord = axis->x_or_y == 'y';
	for (int i=0; i<nlabels; i++) {
	    int wh[2];
	    double pos_data = tk->ticker.get_tick(&tk->ticker, i, out, 128);
	    double pos_rel = (pos_data - axis->min) / axisdiff;
	    int pos_px = axis_xywh[coord] + iroundpos(pos_rel * (axis_xywh[coord+2]-1));
	    ttra_get_textdims_pixels(tk->ttra, out, wh+0, wh+1);
	    /* lower side */
	    int edge = pos_px + wh[coord] * tk->hvalign_text[0];
	    if (edge < 0) {
		axis_xywh[coord] += -edge;
		axis_xywh[coord+2] -= -edge;
		pos_px = axis_xywh[coord] + iroundpos(pos_rel * (axis_xywh[coord+2]-1));
	    }
	    /* higher side */
	    edge = pos_px + wh[coord] * (1 + tk->hvalign_text[0]);
	    int WH[] = {axeswidth, axesheight};
	    if (edge >= WH[coord])
		axis_xywh[coord+2] = (WH[coord] - axis_xywh[coord] - wh[coord] * (1 + tk->hvalign_text[0])) / pos_rel;
	}
    }
}

static void axis_update_range(struct $axis *axis) {
    int isx = axis->x_or_y == 'x';
    axis->min = DBL_MAX;
    axis->max = -DBL_MIN;
    for (int idata=0; idata<axis->axes->ndata; idata++) {
	struct $data *data = axis->axes->data[idata];
	if (data->yxaxis[isx] != axis)
	    continue;

	if (data->yxztype[isx] == cplot_notype)
	    switch (data->have_minmax[isx]) {
		case 0: data->minmax[isx][0] = 0;
			data->minmax[isx][1] = data->length-1;
			break;
		case maxbit: data->minmax[isx][0] = 0; break;
		case minbit: data->minmax[isx][1] = data->length-1; break;
		default: break;
	    }
	else
	    switch (data->have_minmax[isx]) {
		case 0:
		    get_minmax[data->yxztype[isx]](data->yxzdata[isx], data->length, data->minmax[isx]);
		    break;
		case maxbit:
		    data->minmax[isx][0] = get_min[data->yxztype[isx]](data->yxzdata[isx], data->length);
		    break;
		case minbit:
		    data->minmax[isx][1] = get_max[data->yxztype[isx]](data->yxzdata[isx], data->length);
		    break;
		default: break;
	    }

	data->have_minmax[isx] = minbit | maxbit;
	if (!(axis->range_isset & minbit))
	    update_min(axis->min, data->minmax[isx][0]);
	if (!(axis->range_isset & maxbit))
	    update_max(axis->max, data->minmax[isx][1]);
    }
    axis->range_isset = minbit | maxbit;
}

void $axes_draw(struct $axes *axes, unsigned *canvas, int axeswidth, int axesheight, int ystride) {
    unsigned bg = axes->background;
    for (int j=0; j<axesheight; j++) {
	unsigned ind0 = j * ystride;
	for (int i=0; i<axeswidth; i++)
	    canvas[ind0+i] = bg;
    }

    $f4si overgoing = {0};
    for (int iaxis=0; iaxis<axes->naxis; iaxis++) {
	if (axes->axis[iaxis]->range_isset != (minbit | maxbit))
	    axis_update_range(axes->axis[iaxis]);

	float over[2];
	get_ticklabel_limits(axes->axis[iaxis], axeswidth, axesheight, over);
	int coord = axes->axis[iaxis]->x_or_y == 'x'; // ticks are orthogonal to this
	overgoing[coord+0]  = max(overgoing[coord+0], over[0]);
	overgoing[coord+2]  = max(overgoing[coord+2], over[1]);
    }

    overgoing *= (float)axesheight; // to pixels
    int *axis_xywh = axes->ro_inner_xywh;
    axis_xywh[0] = iceil(overgoing[0]);
    axis_xywh[1] = iceil(overgoing[1]);
    axis_xywh[2] = axeswidth - axis_xywh[0] - iceil(overgoing[2]);
    axis_xywh[3] = axesheight - axis_xywh[1] - iceil(overgoing[3]);

    for (int iaxis=0; iaxis<axes->naxis; iaxis++)
	get_ticklabel_limits_round2(axes->axis[iaxis], axeswidth, axesheight, axis_xywh);

    for (int i=0; i<axes->naxis; i++)
	cplot_axis_draw(axes->axis[i], canvas, axeswidth, axesheight, ystride, axis_xywh);

    for (int i=0; i<axes->ndata; i++)
	cplot_data_draw(axes->data[i], canvas, axeswidth, axesheight, ystride, axis_xywh);
}

void $axislabel(struct $axis *axis, char *label) {
    struct $axistext *text = malloc(sizeof(struct $axistext));
    *text = (struct $axistext) {
	.text = label,
	.pos = 0.5,
	.hvalign = {-0.5, -1 * (axis->pos < 0.5)},
	.rowheight = (axis->nticks ? axis->ticks[0]->rowheight : 2.4/80) * 1.3,
	.axis = axis,
	.rotation100 = 25 * (axis->x_or_y == 'y'),
    };
    cplot_add_axistext(axis, text);
}

void $free_axis(struct $axis *axis) {
    for (int i=axis->ntext-1; i>=0; i--) {
	if (axis->text[i]->owner)
	    free(axis->text[i]->text);
	free(axis->text[i]);
    }
    free(axis->text);
    for (int i=0; i<axis->nticks; i++) {
	if (axis->ticks[i]->ttra) {
	    ttra_destroy(axis->ticks[i]->ttra);
	    free(axis->ticks[i]->ttra);
	}
	free(axis->ticks[i]);
    }
    memset(axis, 0, sizeof(*axis));
    free(axis);
}

void $free(struct $axes *axes) {
    for (int i=0; i<axes->naxis; i++)
	$free_axis(axes->axis[i]);
    free(axes->axis);
    //free(axes->text);
    ttra_destroy(axes->ttra);
    free(axes->ttra);

    for (int i=0; i<axes->ndata; i++) {
	struct $data *data = axes->data[i];
	for (int j=0; j<3; j++)
	    if (data->owner[j])
		free(data->yxzdata[j]);
	free(data);
    }
    free(axes->data);

    memset(axes, 0, sizeof(*axes));
    free(axes);
}

void $show(struct $axes *axes) {
    struct waylandhelper wlh = {0};
    wlh_init(&wlh);
    while (!wlh.stop && wlh_roundtrip(&wlh) >= 0) {
	if (wlh.redraw && wlh.can_redraw) {
	    $axes_draw(axes, wlh.data, wlh.xres, wlh.yres, wlh.xres);
	    wlh_commit(&wlh);
	}
	usleep(10000);
    }
    wlh_destroy(&wlh);
}
