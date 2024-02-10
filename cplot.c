#include <stdlib.h>
#include <unistd.h>
#include <ttra.h>

#define using_cplot
#include "cplot.h"

#include "rendering.c"
#include "ticks.c"
#include "wayland_helper/waylandhelper.h"

#define min(a,b) ((a) < (b) ? a : (b))

static unsigned fg = 0xff<<24;

static inline int iroundpos(float f) {
    int a = f;
    return a + (f-a >= 0.5);
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

static $f2si get_ticklabel_limits(struct $ticks *tk) {
    $f2si lim2 = ($f2si){tk->axis->pos, tk->axis->pos + tk->length} + tk->crossaxis * tk->length;
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
	    if (tk->ascending)	lim2[1] += maxrows * tk->rowheight; // alignment?
	    else 		lim2[0] -= maxrows * tk->rowheight; // alignment?
	    break;
	case 'y':
	    float ratio = (float)tk->ttra->fontwidth / tk->ttra->fontheight;
	    if (tk->ascending)	lim2[1] += maxcols * tk->rowheight * ratio;
	    else		lim2[0] -= maxcols * tk->rowheight * ratio;
	    break;
	default:
	    __builtin_unreachable();
    }
    return lim2;
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
    int nticks = ticks->get_nticks(min, max);
    float thickness = ticks->thickness < 0 ? ticks->axis->thickness * -ticks->thickness : ticks->thickness;
    thickness *= min(axeswidth, axesheight);
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
	$f4si line;
	switch (ticks->axis->x_or_y) {
	    case 'x': line = ($f4si){pos, ort[0], pos, ort[1]}; break;
	    case 'y': line = ($f4si){ort[0], pos, ort[1], pos}; break;
	    default: __builtin_unreachable();
	}
	$i4si line_px = relative_line_topixels(line, limits, axeswidth, axesheight);
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

    $f4si limits = {0, 0, 1, 1};
    int ntickers = ptrlen(axes->ticks);
    for (int itick=0; itick<ntickers; itick++) {
	if (axes->ticks[itick]->ttra)
	    ttra_set_fontheight(axes->ticks[itick]->ttra, axes->ticks[itick]->rowheight * axesheight);
	$f2si par = get_ticklabel_limits(axes->ticks[itick]);
	int coord = axes->ticks[itick]->axis->x_or_y == 'x'; // ticks are orthogonal to this
	if (par[0] < limits[0+coord]) limits[0+coord] = par[0];
	if (par[1] > limits[2+coord]) limits[2+coord] = par[1];
    }

    for (int i=0; axes->axis[i]; i++)
	$axis_draw(axes->axis[i], canvas, axeswidth, axesheight, ystride, limits);

    for (int i=0; i<ntickers; i++)
	$ticks_draw(axes->ticks[i], canvas, axeswidth, axesheight, ystride, limits);
	
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
