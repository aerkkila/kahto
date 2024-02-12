#include <stdlib.h>
#include <unistd.h>
#include <ttra.h>

#define using_cplot
#include "cplot.h"

#include "rendering.c"
#include "ticks.c"
#include "wayland_helper/waylandhelper.h"

#define min(a,b) ((a) < (b) ? a : (b))
#define max(a,b) ((a) > (b) ? a : (b))

static unsigned fg = 0xff<<24;

static inline int iroundpos(float f) {
    int a = f;
    return a + (f-a >= 0.5);
}

static inline int iceil(float f) {
    int a = f;
    return a + (a != f);
}

struct $axis* axis_alloc() {
    struct $axis *axis = calloc(1, sizeof(struct $axis));
    axis->color = fg;
    axis->thickness = 1.0 / 400;
    axis->min = 0;
    axis->max = 1;
    return axis;
}

struct $ticks* ticks_alloc(int have_labels) {
    struct $ticks *ticks = calloc(1, sizeof(struct $ticks));
    ticks->color = fg;
    ticks->get_tick = get_tick_basic;
    ticks->get_nticks = get_nticks_basic;
    ticks->length = 1.0 / 80;
    ticks->thickness = -1; // same as axis
    if (have_labels) {
	ticks->ttra = calloc(1, sizeof(struct ttra));
	ttra_init(ticks->ttra);
	ticks->rowheight = 2.4*ticks->length;
    }
    return ticks;
}

struct $axes* axes_alloc() {
    struct $axes *axes = calloc(1, sizeof(struct $axes));
    axes->borders = ($f4si){0, 0, 1, 1};
    axes->background = -1;

    axes->axis  = calloc((axes->mem_axis = 4) + 1, sizeof(void*));
    axes->axis[0] = axis_alloc();
    axes->axis[0]->pos = 1;
    axes->axis[0]->x_or_y = 'x'; // bottom x-axis

    axes->ticks = calloc((axes->mem_ticks = 8) + 1, sizeof(void*));
    axes->ticks[0] = ticks_alloc(1);
    axes->ticks[0]->axis = axes->axis[0];
    axes->ticks[0]->crossaxis = 0; // ulkopuolella
    axes->ticks[0]->alignment = north_e;
    axes->ticks[0]->ascending = 1;

    axes->axis[1] = axis_alloc();
    axes->axis[1]->pos = 0;
    axes->axis[1]->x_or_y = 'y'; // left y-axis

    axes->ticks[1] = ticks_alloc(1);
    axes->ticks[1]->axis = axes->axis[1];
    axes->ticks[1]->crossaxis = -1; // ulkopuolella
    axes->ticks[1]->alignment = east_e; // muuttakaani myöhemmin
    return axes;
}

struct $axes* $plot_args(struct $args *args) {
    struct $axes *axes = args->axes ? args->axes : axes_alloc();
    return axes;
}

static inline $f4si normalize_relative_line($f4si line, $f4si limits) {
    return ($f4si){
	(line[0] - limits[0]) / (limits[2] - limits[0]),
	(line[1] - limits[1]) / (limits[3] - limits[1]),
	(line[2] - limits[0]) / (limits[2] - limits[0]),
	(line[3] - limits[1]) / (limits[3] - limits[1]),
    };
}

static inline $i4si normalized_line_topixels($f4si line, int axeswidth, int axesheight) {
    return ($i4si) {
	iroundpos(line[0] * (axeswidth-1)),
	    iroundpos(line[1] * (axesheight-1)),
	    iroundpos(line[2] * (axeswidth-1)),
	    iroundpos(line[3] * (axesheight-1))
    };
}

static inline $i4si relative_line_topixels($f4si fline, $f4si limits, int axeswidth, int axesheight) {
    $i4si line = normalized_line_topixels(normalize_relative_line(fline, limits), axeswidth, axesheight);
    if (line[0] < 0) {line[0] = 0; goto virhe;}
    if (line[1] < 0) {line[1] = 0; goto virhe;}
    if (line[2] >= axeswidth) {line[2] = axeswidth-1; goto virhe;}
    if (line[3] >= axesheight) {line[3] = axesheight-1; goto virhe;}
    return line;
virhe: __attribute__((cold));
    fprintf(stderr, "normalisointi epäonnistui\n");
    return line;
}

static inline $f4si axis_get_line(struct $axis *axis) {
    switch (axis->x_or_y) {
	case 'x': return ($f4si){0, axis->pos, 1, axis->pos};
	case 'y': return ($f4si){axis->pos, 0, axis->pos, 1};
	default: __builtin_unreachable();
    }
}

void $axis_draw(struct $axis *axis, unsigned *canvas, int axeswidth, int axesheight, int ystride, int axis_xywh[4]) {
    float thickness = axis->thickness * axesheight;
    if (thickness > 1e-9 && thickness < 1)
	thickness = 1;
    $f4si line = axis_get_line(axis);

    $i4si line_px = {
	axis_xywh[0] + line[0] * (axis_xywh[2]-1),
	axis_xywh[1] + line[1] * (axis_xywh[3]-1),
	axis_xywh[0] + line[2] * (axis_xywh[2]-1),
	axis_xywh[1] + line[3] * (axis_xywh[3]-1),
    };
    //$i4si line_px = relative_line_topixels(line, limits, axeswidth, axesheight);
    draw_thick_line_bresenham(canvas, ystride, line_px, axis->color, thickness, axesheight);
}

static inline float get_ticks_overlength(struct $ticks *tk) {
    float f = tk->length * (1 + tk->crossaxis) + tk->axis->pos - 1;
    return f * (f>0);
}

static inline float get_ticks_underlength(struct $ticks *tk) {
    float f = -(tk->crossaxis*tk->length + tk->axis->pos);
    return f * (f>0);
}

static $f2si get_ticklabel_limits(struct $ticks *tk, int axeswidth, int axesheight) {
    $f2si lim2 = {get_ticks_underlength(tk), get_ticks_overlength(tk)};
    if (!tk->get_nticks)
	return lim2;
    float min = tk->axis->min,
	  max = tk->axis->max;

    int nlabels = tk->get_nticks(min, max);
    char out[128];
    int maxcols=0, maxrows=0;
    for (int i=0; i<nlabels; i++) {
	int width, height;
	tk->get_tick(i, min, max, out, 128);
	ttra_get_textdims_chars(out, &width, &height);
	if (height > maxrows)
	    maxrows = height;
	if (width > maxcols)
	    maxcols = width;
    }

    switch (tk->axis->x_or_y) {
	case 'x':
	    lim2[tk->ascending] += (float)maxrows * tk->ttra->fontheight / axesheight; // alignment?
	    break;
	case 'y':
	    lim2[tk->ascending] += (float)maxcols * tk->ttra->fontwidth / axesheight;
	    break;
    }
    return lim2;
}

int ptrlen(void* vptr) {
    void **ptr = vptr;
    int len=0;
    while(*ptr++) len++;
    return len;
}

void $ticks_draw(struct $ticks *ticks, unsigned *canvas, int axeswidth, int axesheight, int ystride, int axis_xywh[4]) {
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

    for (int itick=0; itick<nticks; itick++) {
	float pos = ticks->get_tick(itick, min, max, tick, 32);
	int ortc = ticks->axis->x_or_y == 'x';
	if (!ortc)
	    pos = 1 - pos;
	$i4si line_px;
	line_px[ortc] = axis_xywh[ortc] +
	    iroundpos(ticks->axis->pos * axis_xywh[ortc+2] + ticks->crossaxis * ticks->length * axesheight);
	line_px[ortc+2] = axis_xywh[ortc] +
	    iroundpos(ticks->axis->pos * axis_xywh[ortc+2] + (ticks->crossaxis+1) * ticks->length * axesheight);
	line_px[!ortc] = axis_xywh[!ortc] + iroundpos(pos * axis_xywh[!ortc+2]);
	line_px[!ortc+2] = axis_xywh[!ortc] + iroundpos(pos * axis_xywh[!ortc+2]);
	//$i4si line_px = relative_line_topixels(line, limits, axeswidth, axesheight);
	draw_thick_line_bresenham(canvas, ystride, line_px, ticks->color, thickness, axesheight);
	if (ticks->ttra && tick[0])
	    put_text(ticks->ttra, tick, line_px[0], line_px[3], ticks->alignment, 0); // xy ei ole välttämättä oikein
    }
}

void $axes_draw(struct $axes *axes, unsigned *canvas, int axeswidth, int axesheight, int ystride) {
    unsigned bg = axes->background;
    for (int j=0; j<axesheight; j++) {
	unsigned ind0 = j * ystride;
	for (int i=0; i<axeswidth; i++)
	    canvas[ind0+i] = bg;
    }

    $f4si overgoing = {0, 0, 0, 0};
    int ntickers = ptrlen(axes->ticks);
    for (int itick=0; itick<ntickers; itick++) {
	if (axes->ticks[itick]->ttra)
	    ttra_set_fontheight(axes->ticks[itick]->ttra, axes->ticks[itick]->rowheight * axesheight);
	$f2si over = get_ticklabel_limits(axes->ticks[itick], axeswidth, axesheight);
	int coord = axes->ticks[itick]->axis->x_or_y == 'x'; // ticks are orthogonal to this
	overgoing[coord+0] = max(overgoing[coord+0], over[0]);
	overgoing[coord+2] = max(overgoing[coord+2], over[1]);
    }

    overgoing *= (float)axesheight; // to pixels
    int axis_xywh[4] = {iceil(overgoing[0]), iceil(overgoing[1])};
    axis_xywh[2] = axeswidth - axis_xywh[0] - iceil(overgoing[2]);
    axis_xywh[3] = axesheight - axis_xywh[1] - iceil(overgoing[3]);

    for (int i=0; axes->axis[i]; i++)
	$axis_draw(axes->axis[i], canvas, axeswidth, axesheight, ystride, axis_xywh);

    for (int i=0; i<ntickers; i++)
	$ticks_draw(axes->ticks[i], canvas, axeswidth, axesheight, ystride, axis_xywh);
	
}

void $free(struct $axes *axes) {
    struct $ticks **ticks = axes->ticks;
    while (*ticks) {
	if (ticks[0]->ttra) {
	    ttra_destroy(ticks[0]->ttra);
	    free(ticks[0]->ttra);
	}
	free(*ticks++);
    }
    free(axes->ticks);
    struct $axis **axis = axes->axis;
    while (*axis) free(*axis++);
    free(axes->axis);
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
