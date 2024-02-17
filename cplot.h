#ifndef __cplot_h__
#define __cplot_h__

#define $f2si	cplot_f2si
#define $f4si	cplot_f4si
#define $axis	cplot_axis
#define $ticks	cplot_ticks
#define $axes	cplot_axes
#define $axes_draw	cplot_axes_draw
#define $axislabel	cplot_axislabel
#define $show	cplot_show
#define $free	cplot_free
#define $free_axis	cplot_free_axis
#define $plot	cplot_plot
#define $plot_inl	cplot_plot_inl
#define $plot_args	cplot_plot_args

typedef float $f4si __attribute__((vector_size (16)));
typedef float $f2si __attribute__((vector_size (8)));

struct $axis;

struct cplot_pen {
    unsigned color;
    float thickness;
    /* linestyle? */
};

/* crossaxis: Where ticks are parallel to the axis?
 *	0: Ticks start at the axis i.e. are right or below.
 *	1: Ticks end at the axis i.e. are left or above.
 */
struct $ticks {
    struct $axis *axis;
    int (*get_nticks)(float min, float max);
    float (*get_tick)(int ind, float min, float max, char *out, int sizeout);
    unsigned color;
    float crossaxis, length, thickness;

    int grid_on;
    struct cplot_pen grid_pen;

    struct ttra *ttra;
    float hvalign_text[2];
    int ascending; // nimiöt tulevat suurempaan päähän
    float rowheight;

    int ro_lines[2], ro_labelarea[4], ro_tot_area[4];
};

struct $axis {
    struct $axes *axes;
    int x_or_y;
    float pos, min, max;
    unsigned color;
    float thickness;
    struct $axistext **text;
    int mem_text, ntext;
    struct $ticks *ticks[3];
    int nticks;
    int ro_line[4], ro_tick_area[4], ro_tot_area[4];
};

struct $axistext {
    struct $axis *axis;
    char *text;
    int owner;
    float pos, rowheight, rotation100;
    float hvalign[2];
    int ro_area[4];
};

struct $axes {
    $f4si borders;
    struct $axis **axis;
    int naxis, mem_axis;
    unsigned background;
    struct ttra *ttra;
    int ro_inner_xywh[4];
};

struct $args {
    void *ydata, *xdata;
    int ytype, xtype;
    struct $axes *axes;
};

struct $axes* $plot_args(struct $args *args);
static inline struct $axes* $plot_inl(struct $args args) {
    return $plot_args(&args);
}
#define cplot_plot(...) $plot_inl((struct $args){__VA_ARGS__})

void $axes_draw(struct $axes *axes, unsigned *canvas, int axeswidth, int axesheight, int ystride);
void $axislabel(struct $axis *axis, char *label);
void $show(struct $axes *axes);
void $free(struct $axes *axes);
void $free_axis(struct $axis *axis);

#ifndef using_cplot
#undef $f2si
#undef $f4si
#undef $axis
#undef $ticks
#undef $axes
#undef $axes_draw
#undef $axislabel
#undef $show
#undef $free
#undef $free_axis
#undef $plot
#undef $plot_inl
#undef $plot_args
#endif

#endif
