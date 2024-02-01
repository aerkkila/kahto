#include <stdlib.h>
#include <unistd.h>

#define using_cplot
#include "cplot.h"

#include "rendering.c"
#include "ticks.c"
#include "wayland_helper/waylandhelper.h"

#define min(a,b) ((a) < (b) ? a : (b))

static unsigned fg = 0xff<<24;

struct $axis* axis_alloc() {
    struct $axis *axis = calloc(1, sizeof(struct $axis));
    axis->color = fg;
    axis->thickness = 1.0 / 400;
    axis->min = 0;
    axis->max = 1;
    return axis;
}

struct $ticks* ticks_alloc() {
    struct $ticks *ticks = calloc(1, sizeof(struct $ticks));
    ticks->color = fg;
    ticks->get_ticks = get_ticks_basic;
    ticks->length = 1.0 / 100;
    ticks->thickness = -1; // same as axis
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
    axes->axis[1] = axis_alloc();
    axes->axis[1]->pos = 0;
    axes->axis[1]->x_or_y = 'y'; // left y-axis

    axes->ticks = calloc((axes->mem_ticks = 8) + 1, sizeof(void*));
    axes->ticks[0] = ticks_alloc();
    axes->ticks[0]->axis = axes->axis[0];
    axes->ticks[0]->crossaxis = 0; // ulkopuolella
    axes->ticks[1] = ticks_alloc();
    axes->ticks[1]->axis = axes->axis[1];
    axes->ticks[1]->crossaxis = -1; // ulkopuolella
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
    return __builtin_convertvector(line * __builtin_convertvector(($i4si){axeswidth, axesheight, axeswidth, axesheight}, $f4si), $i4si);
}

static inline $i4si relative_line_topixels($f4si fline, $f4si limits, int axeswidth, int axesheight) {
    $i4si line = normalized_line_topixels(normalize_relative_line(fline, limits), axeswidth, axesheight);
    if (line[0] < 0) line[0] = 0;
    if (line[1] < 0) line[1] = 0;
    if (line[2] > axeswidth) line[2] = axeswidth;
    if (line[3] > axesheight) line[3] = axesheight;
    return line;
}

static inline $f4si axis_get_line(struct $axis *axis) {
    switch (axis->x_or_y) {
	case 'x':
	    return ($f4si){0, axis->pos, 1, axis->pos};
	case 'y':
	    return ($f4si){axis->pos, 0, axis->pos, 1};
	default:
	    __builtin_unreachable();
    }
}

void $axis_draw(struct $axis *axis, unsigned *canvas, int axeswidth, int axesheight, int ystride, $f4si limits) {
    float thickness = axis->thickness * min(axeswidth, axesheight);
    if (thickness > 1e-9 && thickness < 1)
	thickness = 1;
    $f4si line = axis_get_line(axis);
    $i4si line_px = relative_line_topixels(line, limits, axeswidth, axesheight);
    draw_thick_line_bresenham(canvas, ystride, line_px, axis->color, thickness, axesheight);
}

static inline $f2si get_ticks_orthogonal(struct $ticks *tk) {
    return ($f2si){tk->axis->pos, tk->axis->pos + tk->length} + tk->crossaxis * tk->length;
}

int ptrlen(void* vptr) {
    void **ptr = vptr;
    int len=0;
    while(*ptr++) len++;
    return len;
}

void $ticks_draw(struct $ticks *ticks, unsigned *canvas, int axeswidth, int axesheight, int ystride, $f4si limits) {
    $f2si ort = get_ticks_orthogonal(ticks);
    float min = ticks->axis->min;
    float max = ticks->axis->max;
    int nticks = ticks->get_ticks(-1, min, max);
    float thickness = ticks->thickness < 0 ? ticks->axis->thickness * -ticks->thickness : ticks->thickness;
    thickness *= min(axeswidth, axesheight);
    if (thickness > 1e-9 && thickness < 1)
	thickness = 1;

    for (int itick=0; itick<nticks; itick++) {
	float pos = ticks->get_ticks(itick, min, max);
	$f4si line;
	switch (ticks->axis->x_or_y) {
	    case 'x': line = ($f4si){pos, ort[0], pos, ort[1]}; break;
	    case 'y': line = ($f4si){ort[0], pos, ort[1], pos}; break;
	    default: __builtin_unreachable();
	}
	$i4si line_px = relative_line_topixels(line, limits, axeswidth, axesheight);
	draw_thick_line_bresenham(canvas, ystride, line_px, ticks->color, thickness, axesheight);
    }
}

$f4si get_tick_line(int ind, struct $ticks *ticks, $f4si limits, $f4si laatikko) {
    float pos = ticks->get_ticks(ind, ticks->axis->min, ticks->axis->max);
    float ero = ticks->axis->max = ticks->axis->min;
    float rel = (pos - ticks->axis->min) / ero;
    // kesken
    return ($f4si){0};
}

void $axes_draw(struct $axes *axes, unsigned *canvas, int axeswidth, int axesheight, int ystride) {
    unsigned bg = axes->background;
    for (int j=0; j<axesheight; j++) {
	unsigned ind0 = j * ystride;
	for (int i=0; i<axeswidth; i++)
	    canvas[ind0+i] = bg;
    }

    $f4si limits = {0, 0, 1, 1};
    int ntickers = ptrlen(axes->ticks);
    for (int itick=0; itick<ntickers; itick++) {
	$f2si par = get_ticks_orthogonal(axes->ticks[itick]);
	int coord = axes->ticks[itick]->axis->x_or_y == 'x'; // ticks are orthogonal to this
	if (par[0] < limits[0+coord]) limits[0+coord] = par[0];
	if (par[1] > limits[2+coord]) limits[2+coord] = par[1];
    }

    for (int i=0; axes->axis[i]; i++)
	$axis_draw(axes->axis[i], canvas, axeswidth, axesheight, ystride, limits);

    for (int i=0; i<ntickers; i++)
	$ticks_draw(axes->ticks[i], canvas, axeswidth, axesheight, ystride, limits);
	
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
