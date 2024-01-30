#ifndef __cplot_h__
#define __cplot_h__

#define $f4si	cplot_f4si
#define $i4si	cplot_i4si
#define $axis	cplot_axis
#define $ticks	cplot_ticks
#define $axes	cplot_axes
#define $show	cplot_show
#define $plot	cplot_plot
#define $plot_inl	cplot_plot_inl
#define $plot_args	cplot_plot_args

typedef float $f4si __attribute__((vector_size (16)));
typedef int $i4si __attribute__((vector_size (16)));

struct $axis {
    $f4si line;
    unsigned color;
    float thickness;
    float min, max;
};

/* crossaxis: Where ticks are parallel to the axis?
 *	0: Ticks start at the axis i.e. are right or below.
 *	1: Ticks end at the axis i.e. are left or above.
 */
struct $ticks {
    struct $axis *axis;
    int (*get_ticks)(float **out, float min, float max);
    unsigned color;
    float crossaxis, length, thickness;
    struct grid {
	unsigned color;
	float length, thickness;
    } grid;
    char *labels;
};

struct $text {
    struct $axis *axis;
    char *text;
    unsigned color;
    float fontsize, rotation100, auto_axisrot100;
    float halign, valign;
};

struct $axes {
    $f4si borders;
    struct $axis **axis;
    struct $ticks **ticks;
    struct $text **text;
    int mem_axis, mem_ticks, mem_text;
    unsigned background;
};

struct $args {
    struct $axes *axes;
    void *ydata, *xdata;
    int ytype, xtype;
};

struct $axes* $plot_args(struct $args *args);
static inline struct $axes* $plot_inl(struct $args args) {
    return $plot_args(&args);
}
#define cplot_plot(...) $plot_inl((struct $args){__VA_ARGS__})

void $show(struct $axes *axes);

#ifndef using_cplot
#undef $f4si
#undef $i4si
#undef $axis
#undef $ticks
#undef $axes
#undef $show
#undef $plot
#undef $plot_inl
#undef $plot_args
#endif

#endif
