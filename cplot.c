#include <stdlib.h>
#include <unistd.h>

#define using_cplot
#include "cplot.h"

#include "rendering.c"
#include "ticks.c"
#include "wayland_helper/waylandhelper.h"

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
    return ticks;
}

struct $axes* axes_alloc() {
    struct $axes *axes = calloc(1, sizeof(struct $axes));
    axes->borders = ($f4si){0, 0, 1, 1};
    axes->background = -1;
    axes->axis  = calloc((axes->mem_axis = 4) + 1, sizeof(void*));
    axes->axis[0] = axis_alloc();
    axes->axis[0]->line = ($f4si){0, 1, 1, 1};
    axes->axis[1] = axis_alloc();
    axes->axis[1]->line = ($f4si){0, 0, 0, 1};
    axes->ticks = calloc((axes->mem_ticks = 8) + 1, sizeof(void*));
    axes->ticks[0] = ticks_alloc();
    axes->ticks[0]->axis = axes->axis[0];
    axes->ticks[0]->crossaxis = -1; // ulkopuolella
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

static inline $i4si relative_line_topixels($f4si line, $f4si limits, int axeswidth, int axesheight) {
    return normalized_line_topixels(normalize_relative_line(line, limits), axeswidth, axesheight);
}

void $axis_draw(struct $axis *axis, unsigned *canvas, int axeswidth, int axesheight, int ystride, $f4si limits) {
    /*$f4si fborders = axis->line;
    fborders = ($f4si){
	(fborders[0] - limits[0]) / (limits[2] - limits[0]),
	(fborders[1] - limits[1]) / (limits[3] - limits[1]),
	(fborders[2] - limits[0]) / (limits[2] - limits[0]),
	(fborders[3] - limits[1]) / (limits[3] - limits[1]),
    };
    fborders *= __builtin_convertvector(($i4si){axeswidth, axesheight, axeswidth, axesheight}, $f4si);
    $i4si borders = __builtin_convertvector(fborders, $i4si);*/
    $i4si borders = relative_line_topixels(axis->line, limits, axeswidth, axesheight);

    float avgsize = (axeswidth + axesheight) * 0.5;
    float thickness = axis->thickness * avgsize;
    if (thickness > 1e-9 && thickness < 1)
	thickness = 1;
    draw_thick_line_bresenham(canvas, ystride, borders, axis->color, thickness, axesheight);
}

$f4si get_tick_area(struct $ticks *tk) {
    $f4si area = tk->axis->line;
    float dx = area[2] - area[0];
    float dy = area[3] - area[1];
    float dxy[] = {dx, dy};
    float axislen = sqrt(dx*dx + dy*dy);

    dx = dx / axislen * tk->length;
    dy = dy / axislen * tk->length;
    $f4si parallel0 = {area[0], area[1], area[0] + dy, area[1] - dx};
    float w = parallel0[2] - parallel0[0];
    float h = parallel0[3] - parallel0[1];
    parallel0 += tk->crossaxis * ($f4si){w, h, w, h};

    for (int is_y = 0; is_y <= 1; is_y++) {
	int smaller = parallel0[2 + is_y] < parallel0[0 + is_y];
	float min = parallel0[2*smaller + is_y];
	float max = parallel0[2*!smaller + is_y];
	if (dxy[is_y] < 0)	min += dxy[is_y];
	else 		max += dxy[is_y];
	area[0 + is_y] = min;
	area[2 + is_y] = max;
    }
    return area;
}

void $axes_draw(struct $axes *axes, unsigned *canvas, int axeswidth, int axesheight, int ystride) {
    unsigned bg = axes->background;
    for (int j=0; j<axesheight; j++) {
	unsigned ind0 = j * ystride;
	for (int i=0; i<axeswidth; i++)
	    canvas[ind0+i] = bg;
    }

    $f4si limits = {0, 0, 1, 1};
    struct $ticks **tk = axes->ticks;
    while (*tk) {
	$f4si tklaatikko = get_tick_area(*tk);
	if (tklaatikko[0] < limits[0]) limits[0] = tklaatikko[0];
	if (tklaatikko[1] < limits[1]) limits[1] = tklaatikko[1];
	if (tklaatikko[2] > limits[2]) limits[2] = tklaatikko[2];
	if (tklaatikko[3] > limits[3]) limits[3] = tklaatikko[3];
	tk++;
    }

    for (int i=0; axes->axis[i]; i++)
	$axis_draw(axes->axis[i], canvas, axeswidth, axesheight, ystride, limits);
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
