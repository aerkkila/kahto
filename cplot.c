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
#define xywh_to_area(xywh) {(xywh)[0], (xywh)[1], (xywh)[2]+(xywh)[0], (xywh)[3]+(xywh)[1]}

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

static inline $f4si axis_get_line(struct $axis *axis) {
    switch (axis->x_or_y) {
	case 'x': return ($f4si){0, axis->pos, 1, axis->pos};
	case 'y': return ($f4si){axis->pos, 0, axis->pos, 1};
	default: __builtin_unreachable();
    }
}

static inline float get_ticks_overlength(struct $ticks *tk) {
    float f = tk->length * (1 + tk->crossaxis) + tk->axis->pos - 1;
    return f * (f>0);
}

static inline float get_ticks_underlength(struct $ticks *tk) {
    float f = -(tk->crossaxis*tk->length + tk->axis->pos);
    return f * (f>0);
}

void cplot_get_axislabel_xy(struct $axistext *axistext, int xy[2]);
void cplot_get_legend_dims(struct $axes *axes, int *lines, int *cols);
void cplot_get_legend_dims_px(struct $axes *axes, int *y, int *x, int axesheight);


#include "functions.c"
#include "rotate.c"
#include "rendering.c"
#include "ticker.c"
#include "commit.c"
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

struct $ticks* cplot_ticks_alloc(struct $axis *axis) {
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

    ticks->rowheight = 2.4*ticks->length;
    ticks->have_labels = 1;

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

struct $axes* cplot_axes_alloc() {
    struct $axes *axes = calloc(1, sizeof(struct $axes));
    axes->borders = ($f4si){0, 0, 1, 1};
    axes->background = -1;

    axes->axis = calloc((axes->mem_axis = 4) + 1, sizeof(void*));
    axes->axis[0] = cplot_axis_alloc(axes);
    axes->axis[0]->x_or_y = 'x'; // bottom x-axis
    axes->axis[0]->pos = 1;
    axes->axis[0]->ticks[0] = cplot_ticks_alloc(axes->axis[0]);
    axes->axis[0]->nticks++;

    axes->axis[1] = cplot_axis_alloc(axes);
    axes->axis[1]->x_or_y = 'y'; // left y-axis
    axes->axis[1]->pos = 0;
    axes->axis[1]->ticks[0] = cplot_ticks_alloc(axes->axis[1]);
    axes->axis[1]->nticks++;

    axes->ttra = calloc(1, sizeof(struct ttra));
    ttra_init(axes->ttra);

    axes->legend.rowheight = 1.0 / 40;
    axes->legend.symbolspace_per_rowheight = 1.25;
    axes->legend.posx = 0;
    axes->legend.posy = 1;
    axes->legend.hvalign[1] = -1;

    return axes;
}

void cplot_ticks_draw(struct $ticks *ticks, unsigned *canvas, int axeswidth, int axesheight, int ystride) {
    char tick[32];
    struct ttra *ttra = NULL;

    if (ticks->have_labels) {
	ttra = ticks->axis->axes->ttra;
	ttra->canvas = canvas;
	ttra->realw = axeswidth;
	ttra->realh = axesheight;
	ttra->w = ticks->ro_labelarea[2] - ticks->ro_labelarea[0];
	ttra->h = ticks->ro_labelarea[3] - ticks->ro_labelarea[1];
	ttra->fg_default = ticks->color;
	ttra->bg_default = -1;
	ttra_print(ttra, "\033[0m");
	ttra_set_fontheight(ttra, ticks->rowheight*axesheight);
    }

    int isx = ticks->axis->x_or_y == 'x';
    int line_px[4];
    line_px[isx+0] = ticks->ro_lines[0];
    line_px[isx+2] = ticks->ro_lines[1];
    int nticks = ticks->ticker.init(&ticks->ticker, ticks->axis->min, ticks->axis->max); // turhaan init aina uudestaan
    float thickness = ticks->thickness < 0 ? ticks->axis->thickness * -ticks->thickness : ticks->thickness;
    thickness *= axesheight;
    if (thickness < 1) thickness = 1;
    int gridline[4];
    int *xywh = ticks->axis->axes->ro_inner_xywh;
    gridline[isx] = xywh[isx];
    gridline[isx+2] = xywh[isx] + xywh[isx+2];
    int inner_area[] = xywh_to_area(xywh);

    for (int itick=0; itick<nticks; itick++) {
	double pos_data = ticks->ticker.get_tick(&ticks->ticker, itick, tick, 32);
	double pos_rel = (pos_data - ticks->axis->min) / (ticks->axis->max - ticks->axis->min);
	if (!isx)
	    pos_rel = 1 - pos_rel;
	line_px[!isx] = line_px[!isx+2] = xywh[!isx] + iroundpos(pos_rel * xywh[!isx+2]);
	draw_thick_line_bresenham(canvas, ystride, line_px, ticks->color, thickness, ticks->ro_tot_area);
	int area_text[4] = {0};
	if (ttra && tick[0])
	    put_text(ttra, tick, line_px[0], line_px[3], ticks->hvalign_text[!isx], ticks->hvalign_text[isx], 0, area_text, 0);
	if (ticks->grid_on) {
	    gridline[!isx] = gridline[!isx+2] = line_px[!isx];
	    int thickness = iroundpos(ticks->grid_pen.thickness * axesheight);
	    if (thickness < 1) thickness = 1;
	    draw_thick_line_bresenham(canvas, axeswidth, gridline, ticks->grid_pen.color, thickness, inner_area);
	}
    }
}

void cplot_get_axislabel_xy(struct $axistext *axistext, int xy[2]) {
    int coord = axistext->axis->x_or_y == 'y';
    int *axis_tot_area = axistext->axis->ro_linetick_area;
    int axislength = axis_tot_area[coord+2] - axis_tot_area[coord];
    xy[coord] = iroundpos(axis_tot_area[coord] + axislength * axistext->pos);
    xy[!coord] = axis_tot_area[!coord + 2*(axistext->axis->pos >= 0.5)];
}

void cplot_axistext_draw(struct $axistext *axistext, unsigned *canvas, int axeswidth, int axesheight, int ystride) {
    struct ttra *ttra = axistext->axis->axes->ttra;
    ttra->canvas = canvas;
    ttra->realw = axeswidth;
    ttra->realh = axesheight;
    ttra->w = axistext->ro_area[2] - axistext->ro_area[0];
    ttra->h = axistext->ro_area[3] - axistext->ro_area[1];
    ttra->fg_default = axistext->axis->color;
    ttra->bg_default = -1;
    ttra_print(ttra, "\033[0m");
    ttra_set_fontheight(ttra, axistext->rowheight*axesheight);
    int xy[2];
    int coord = axistext->axis->x_or_y == 'y';
    int *axis_tot_area = axistext->axis->ro_linetick_area;
    int axislength = axis_tot_area[coord+2] - axis_tot_area[coord];
    xy[coord] = iroundpos(axis_tot_area[coord] + axislength * axistext->pos);
    xy[!coord] = axis_tot_area[!coord + 2*(axistext->axis->pos >= 0.5)];
    put_text(ttra, axistext->text, xy[0], xy[1], axistext->hvalign[coord], axistext->hvalign[!coord], axistext->rotation100, axistext->ro_area, 0);
}

void cplot_axis_render(struct $axis *axis, unsigned *canvas, int axeswidth, int axesheight, int ystride) {
    float thickness = axis->thickness * axesheight;
    if (thickness < 1) thickness = 1;
    int area[4] = xywh_to_area(axis->axes->ro_inner_xywh);

    int isx = axis->x_or_y == 'x';
    area[isx+2] += (thickness+1)/2;
    int WH[] = {axeswidth, axesheight};
    if (area[isx+2] > WH[isx]) area[isx+2] = WH[isx];

    draw_thick_line_bresenham(canvas, ystride, axis->ro_line, axis->color, thickness, area);
    for (int i=0; i<axis->nticks; i++)
	cplot_ticks_draw(axis->ticks[i], canvas, axeswidth, axesheight, ystride);
    for (int i=0; i<axis->ntext; i++)
	cplot_axistext_draw(axis->text[i], canvas, axeswidth, axesheight, ystride);
}

void cplot_axes_render(struct $axes *axes, unsigned *canvas, int axeswidth, int axesheight, int ystride) {
    unsigned bg = axes->background;
    for (int j=0; j<axesheight; j++) {
	unsigned ind0 = j * ystride;
	for (int i=0; i<axeswidth; i++)
	    canvas[ind0+i] = bg;
    }
    for (int i=0; i<axes->naxis; i++)
	cplot_axis_render(axes->axis[i], canvas, axeswidth, axesheight, ystride);
    for (int i=0; i<axes->ndata; i++)
	cplot_data_render(axes->data[i], canvas, axeswidth, axesheight, ystride);
}

static void init_datastyle(struct $data *data) {
    if (data->markersize == 0)
	data->markersize = 1.0 / 150;
    if (!data->marker)
	data->marker = "o";
    if (!data->color)
	data->color = RGB(0, 70, 185);
}

void cplot_get_legend_dims(struct $axes *axes, int *lines, int *cols) {
    *lines = *cols = 0;
    for (int i=0; i<axes->ndata; i++)
	if (axes->data[i]->label) {
	    int w, h;
	    ttra_get_textdims_chars(axes->data[i]->label, &w, &h);
	    *lines += h;
	    *cols = max(*cols, w);
	}
}

void cplot_get_legend_dims_px(struct $axes *axes, int *y, int *x, int axesheight) {
    cplot_get_legend_dims(axes, y, x);
    ttra_set_fontheight(axes->ttra, iroundpos(axes->legend.rowheight * axesheight));
    *y *= axes->ttra->fontheight;
    *x *= axes->ttra->fontwidth;
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

void cplot_add_axistext(struct $axis *axis, struct $axistext *text) {
    if (axis->ntext >= axis->mem_text)
	axis->text = realloc(axis->text, (axis->mem_text = axis->ntext + 2) * sizeof(void*));
    axis->text[axis->ntext++] = text;
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
	.type = cplot_axistext_label,
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
    for (int i=0; i<axis->nticks; i++)
	free(axis->ticks[i]);
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

struct $axes* $plot_args(struct cplot_args *args) {
    struct $axes *axes = args->axes ? args->axes : cplot_axes_alloc();
    args->axes = axes;
    if (args->ydata)
	add_data(args);
    return axes;
}

void $axes_draw(struct $axes *axes, unsigned *canvas, int axeswidth, int axesheight, int ystride) {
    cplot_axes_commit(axes, axeswidth, axesheight);
    cplot_axes_render(axes, canvas, axeswidth, axesheight, ystride);
    /* Tässä on jotain vikaa. Vasta toisella piirrolla menee oikein.
       Piirrettäköön siis tilapäisratkaisuna kahdesti, kunnes löydän vian ja korjaan sen.
       Menee oikein, vaikka ensimmäisen kommitin poistaisi. */
    cplot_axes_commit(axes, axeswidth, axesheight);
    cplot_axes_render(axes, canvas, axeswidth, axesheight, ystride);
    cplot_legend(axes, (struct cplot_drawarea){canvas, axeswidth, axesheight, ystride});
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
