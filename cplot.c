#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ttra.h>

#define using_cplot
#include "cplot.h"

#include "rotate.c"
#include "rendering.c"
#include "ticks.c"
#include "wayland_helper/waylandhelper.h"

#define min(a,b) ((a) < (b) ? a : (b))
#define max(a,b) ((a) > (b) ? a : (b))
#define update_min(a, b) ((a) = min(a,b))
#define update_max(a, b) ((a) = max(a,b))
#define arrlen(a) (sizeof(a) / sizeof(*(a)))

static unsigned fg = 0xff<<24;

static inline int iroundpos(float f) {
    int a = f;
    return a + (f-a >= 0.5);
}

static inline int iceil(float f) {
    int a = f;
    return a + (a != f);
}

struct $axis* axis_alloc(struct $axes *axes) {
    struct $axis *axis = calloc(1, sizeof(struct $axis));
    axis->axes = axes;
    axes->naxis++;
    axis->color = fg;
    axis->thickness = 1.0 / 400;
    axis->min = 0;
    axis->max = 1;
    return axis;
}

struct $ticks* ticks_alloc(struct $axis *axis, int have_labels) {
    struct $ticks *ticks = calloc(1, sizeof(struct $ticks));
    ticks->axis = axis;
    ticks->color = fg;
    ticks->get_tick = get_tick_basic;
    ticks->get_nticks = get_nticks_basic;
    ticks->length = 1.0 / 80;
    ticks->thickness = -1; // same as axis
    ticks->hvalign_text[0] = -0.5;

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

void $add_axistext(struct $axis *axis, struct $axistext *text) {
    if (axis->ntext >= axis->mem_text)
	axis->text = realloc(axis->text, (axis->mem_text = axis->ntext + 2) * sizeof(void*));
    axis->text[axis->ntext++] = text;
}

struct $axes* axes_alloc() {
    struct $axes *axes = calloc(1, sizeof(struct $axes));
    axes->borders = ($f4si){0, 0, 1, 1};
    axes->background = -1;

    axes->axis = calloc((axes->mem_axis = 4) + 1, sizeof(void*));
    axes->axis[0] = axis_alloc(axes);
    axes->axis[0]->x_or_y = 'x'; // bottom x-axis
    axes->axis[0]->pos = 1;
    axes->axis[0]->ticks[0] = ticks_alloc(axes->axis[0], 1);
    axes->axis[0]->nticks++;

    axes->axis[1] = axis_alloc(axes);
    axes->axis[1]->x_or_y = 'y'; // left y-axis
    axes->axis[1]->pos = 0;
    axes->axis[1]->ticks[0] = ticks_alloc(axes->axis[1], 1);
    axes->axis[1]->nticks++;

    axes->ttra = calloc(1, sizeof(struct ttra));
    ttra_init(axes->ttra);

    return axes;
}

struct $axes* $plot_args(struct $args *args) {
    struct $axes *axes = args->axes ? args->axes : axes_alloc();
    return axes;
}

$i2si $ticks_draw(struct $ticks *ticks, unsigned *canvas, int axeswidth, int axesheight, int ystride, const int axis_xywh[4]) {
    float min = ticks->axis->min;
    float max = ticks->axis->max;
    int nticks = ticks->get_nticks(min, max);
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

    /* Silmukan kääntäminen muuttaisi minmaxpos-muuttujaa. */
    for (int itick=0; itick<nticks; itick++) {
	float pos = ticks->get_tick(itick, min, max, tick, 32);
	if (!isx)
	    pos = 1 - pos;
	line_px[!isx] = line_px[!isx+2] = minmaxpos[itick!=0] = axis_xywh[!isx] + iroundpos(pos * axis_xywh[!isx+2]);
	draw_thick_line_bresenham(canvas, ystride, line_px, ticks->color, thickness, axesheight);
	int area_text[4] = {0};
	if (ticks->ttra && tick[0])
	    if (put_text(ticks->ttra, tick, line_px[0], line_px[3], ticks->hvalign_text[!isx], ticks->hvalign_text[isx], 0, area_text) >= 0) { // successful geometry
		update_min(ticks->ro_labelarea[0], area_text[0]);
		update_min(ticks->ro_labelarea[1], area_text[1]);
		update_max(ticks->ro_labelarea[2], area_text[2]);
		update_max(ticks->ro_labelarea[3], area_text[3]);
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

    return ($i2si) {line_px[isx], line_px[isx+2] + ticks->ttra->fontheight};
}

void $axistext_draw(struct $axistext *axistext, unsigned *canvas, int axeswidth, int axesheight, int ystride) {
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
    int *axis_area = axistext->axis->ro_line;
    int *axis_tot_area = axistext->axis->ro_tot_area;
    int axislength = axis_area[coord+2] - axis_area[coord];
    xy[coord] = iroundpos(axis_area[coord] + axislength * axistext->pos);
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

void $axis_draw(struct $axis *axis, unsigned *canvas, int axeswidth, int axesheight, int ystride, const int axis_xywh[4]) {
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
    draw_thick_line_bresenham(canvas, ystride, axis->ro_line, axis->color, thickness, axesheight);

    axis->ro_tick_area[0] = axis->ro_tick_area[2] = axis->ro_line[0];
    axis->ro_tick_area[1] = axis->ro_tick_area[3] = axis->ro_line[1];
    for (int i=0; i<axis->nticks; i++)
	$ticks_draw(axis->ticks[i], canvas, axeswidth, axesheight, ystride, axis_xywh);

    axis->ro_tot_area[0] = min(axis->ro_line[0], axis->ro_tick_area[0]);
    axis->ro_tot_area[1] = min(axis->ro_line[1], axis->ro_tick_area[1]);
    axis->ro_tot_area[2] = max(axis->ro_line[2], axis->ro_tick_area[2]);
    axis->ro_tot_area[3] = max(axis->ro_line[3], axis->ro_tick_area[3]);

    for (int i=0; i<axis->ntext; i++)
	$axistext_draw(axis->text[i], canvas, axeswidth, axesheight, ystride);
}

static inline float get_ticks_overlength(struct $ticks *tk) {
    float f = tk->length * (1 + tk->crossaxis) + tk->axis->pos - 1;
    return f * (f>0);
}

static inline float get_ticks_underlength(struct $ticks *tk) {
    float f = -(tk->crossaxis*tk->length + tk->axis->pos);
    return f * (f>0);
}

static $f2si get_axislabel_limits(struct $axis *axis, int axeswidth, int axesheight, $f2si lim2) {
    struct ttra *ttra = axis->axes->ttra;
    int coord = axis->x_or_y == 'x';
    $f2si new = lim2;
    for (int itext=0; itext<axis->ntext; itext++) {
	struct $axistext *text = axis->text[itext];
	int wh[2];
	ttra_set_fontheight(ttra, text->rowheight * axesheight);
	ttra_get_textdims_pixels(ttra, text->text, wh+0, wh+1);
	float frac = wh[coord + ((int)text->rotation100 % 50 == 25)] / (float)axesheight;
	int side = axis->pos >= 0.5;
	new[side] = max(lim2[side]+frac, new[side]);
    }
    return new;
}

/* Akselia kohtisuoraan */
static $f2si get_ticklabel_limits(struct $axis *axis, int axeswidth, int axesheight) {
    $f2si lim2out = {0};
    for (int i=0; i<axis->nticks; i++) {
	struct $ticks *tk = axis->ticks[i];
	$f2si lim2 = {get_ticks_underlength(tk), get_ticks_overlength(tk)};
	if (!tk->get_nticks)
	    goto endloop;
	float min = tk->axis->min,
	      max = tk->axis->max;

	ttra_set_fontheight(tk->ttra, tk->rowheight * axesheight);
	int nlabels = tk->get_nticks(min, max);
	char out[128];
	int maxcols=0, maxrows=0;
	int width, height;
	for (int i=0; i<nlabels; i++) {
	    tk->get_tick(i, min, max, out, 128);
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
    lim2out = get_axislabel_limits(axis, axeswidth, axesheight, lim2out);
    return lim2out;
}

/* Akselin suuntaan */
static void get_ticklabel_limits_round2(struct $axis *axis, int axeswidth, int axesheight, int axis_xywh[4]) {
    for (int iticks=0; iticks<axis->nticks; iticks++) {
	struct $ticks *tk = axis->ticks[iticks];
	if (!tk->get_nticks)
	    continue;
	float min = axis->min,
	      max = axis->max;

	int nlabels = tk->get_nticks(min, max);
	char out[128];
	int coord = axis->x_or_y == 'y';
	for (int i=0; i<nlabels; i++) {
	    int wh[2];
	    float pos = tk->get_tick(i, min, max, out, 128);
	    int pos_px = axis_xywh[coord] + iroundpos(pos * (axis_xywh[coord+2]-1));
	    ttra_get_textdims_pixels(tk->ttra, out, wh+0, wh+1);
	    /* lower side */
	    int edge = pos_px + wh[coord] * tk->hvalign_text[0];
	    if (edge < 0) {
		axis_xywh[coord] += -edge;
		axis_xywh[coord+2] -= -edge;
		pos_px = axis_xywh[coord] + iroundpos(pos * (axis_xywh[coord+2]-1));
	    }
	    /* higher side */
	    edge = pos_px + wh[coord] * (1 + tk->hvalign_text[0]);
	    int WH[] = {axeswidth, axesheight};
	    if (edge >= WH[coord])
		axis_xywh[coord+2] = (WH[coord] - axis_xywh[coord] - wh[coord] * (1 + tk->hvalign_text[0])) / pos;
	}
    }
}

int ptrlen(void* vptr) {
    void **ptr = vptr;
    int len=0;
    while(*ptr++) len++;
    return len;
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
	$f2si over = get_ticklabel_limits(axes->axis[iaxis], axeswidth, axesheight);
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

    for (int i=0; axes->axis[i]; i++)
	$axis_draw(axes->axis[i], canvas, axeswidth, axesheight, ystride, axis_xywh);
}

void $axislabel(struct $axis *axis, char *label) {
    struct $axistext *text = malloc(sizeof(struct $axistext));
    *text = (struct $axistext) {
	.text = label,
	.pos = 0.5,
	.hvalign = {-0.5, 0},
	.rowheight = (axis->nticks ? axis->ticks[0]->rowheight : 2.4/80) * 1.3,
	.axis = axis,
	.rotation100 = 25 * (axis->x_or_y == 'y'),
    };
    $add_axistext(axis, text);
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
