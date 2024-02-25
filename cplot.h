#ifndef __cplot_h__
#define __cplot_h__

#define $f2si	cplot_f2si
#define $f4si	cplot_f4si
#define $axis	cplot_axis
#define $ticks	cplot_ticks
#define $ticker	cplot_ticker
#define $tickerdata cplot_tickerdata
#define $axes	cplot_axes
#define $data	cplot_data
#define $axes_draw	cplot_axes_draw
#define $axislabel	cplot_axislabel
#define $show	cplot_show
#define $free	cplot_free
#define $free_axis	cplot_free_axis
#define $plot	cplot_plot
#define $plot_inl	cplot_plot_inl
#define $plot_args	cplot_plot_args

#define cplot_notype 0
#define cplot_i1 11	// signed integer is enumerated as size + 10
#define cplot_i2 12
#define cplot_i4 14
#define cplot_i8 18
#define cplot_u1 1	// unsigned integer is enumerated as size
#define cplot_u2 2
#define cplot_u4 4
#define cplot_u8 8
#define cplot_f4 24	// floating point number is enumerated as size + 20
#define cplot_f8 28
#define cplot_f10 36 // f10 is likely to take 16 bytes due to padding bytes
_Static_assert(sizeof(long double) == 16);

/* returns some of the enumerations above according to the data type */
#define cplot_type(a) ((int)( \
	    (typeof(a))1.5 == 1 ?	/* integer or floating point number */ \
	    /* integer */ \
	    sizeof(a) + (		/* size of integer */ \
	    (typeof(a))(-1) < 0 ?	/* signed or unsigned */ \
	     10 : 0 )			/* signed integer is enumerated as size + 10 */ \
	    /* floating point */ \
	    : sizeof(a) + 20 ))		/* floating point number is enumerated as size + 20 */

#define minbit 1
#define maxbit 2

#define cplot_rgb(r, g, b) (0xff<<24 | (r)<<16 | (g)<<8 | (b)<<0)

typedef float $f4si __attribute__((vector_size (16)));
typedef float $f2si __attribute__((vector_size (8)));

struct $axis;
struct $data;

struct cplot_pen {
    unsigned color;
    float thickness;
    /* linestyle? */
};

struct cplot_tickerdata_linear {
    int nticks, baseten;
    double step, min, base;
};

union $tickerdata {
    struct cplot_tickerdata_linear lin;
    /* for future use */
};

enum cplot_tickere {ticker_linear};

struct $ticker {
    enum cplot_tickere species;
    int (*init)(struct $ticker *this, double min, double max); // returns the number of ticks
    double (*get_tick)(struct $ticker *this, int ind, char *out, int sizeout);
    union $tickerdata tickerdata;
};

/* crossaxis: Where ticks are parallel to the axis?
 *	0: Ticks start at the axis i.e. are right or below.
 *	1: Ticks end at the axis i.e. are left or above.
 */
struct $ticks {
    struct $axis *axis;
    struct $ticker ticker;
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
    float pos;
    double min, max;
    int range_isset;
    unsigned color;
    float thickness;
    struct $axistext **text;
    int mem_text, ntext;
    struct $ticks *ticks[3];
    int nticks;
    int ro_line[4], ro_tick_area[4], ro_linetick_area[4];
};

enum axistext_type {cplot_axistext_other, cplot_axistext_label, cplot_axistext_tickmul};

struct $axistext {
    struct $axis *axis;
    char *text;
    enum axistext_type type;
    int owner;
    float pos, rowheight, rotation100;
    float hvalign[2];
    int ro_area[4];
};

/* If changed, cplot_args must be changed similarily. */
struct $data {
    void *yxzdata[3];
    int yxztype[3];
    long length;
    struct $axis *yxaxis[2];
    double minmax[3][2];
    char have_minmax[3]; // bits: minbit, maxbit
    int owner[3];
    /* style */
    const char* marker;
    int literal_marker;
    float markersize;
    unsigned color;
    const char *linestyle;
    float line_thickness;
};

struct $axes {
    $f4si borders; // not used yet
    struct $axis **axis;
    int naxis, mem_axis;
    unsigned background;
    struct ttra *ttra;
    int ro_inner_xywh[4];
    struct $data **data;
    int ndata, mem_data;
};

struct cplot_args {
    struct $axes *axes;

    /* struct $data inlined. ydata must stay first */
    void *ydata, *xdata, *zdata;
    int ytype, xtype, ztype;
    long len;
    struct $axis *yaxis, *xaxis;
    double minmax[3][2];
    char have_minmax[3]; // bits: minbit, maxbit
    int yxzowner[3];

    const char* marker;
    int literal_marker;
    float markersize;
    unsigned color;
    const char *linestyle;
    float line_thickness;

    /* end struct $data */
    int copy[3]; // not used yet
};

#define cplot_y(y, ...) $plot_inl((struct cplot_args){.ydata=(y), .ytype=cplot_type(*(y)), __VA_ARGS__})
#define cplot_yx(y, x, ...) $plot_inl((struct cplot_args){	\
    .ydata=(y),							\
    .xdata=(x),							\
    .ytype=cplot_type(*(y)),					\
    .xtype=cplot_type(*(x)),					\
    __VA_ARGS__							\
    })

struct $axes* $plot_args(struct cplot_args *args);
static inline struct $axes* $plot_inl(struct cplot_args args) {
    return $plot_args(&args);
}
#define cplot_plot(...) $plot_inl((struct cplot_args){__VA_ARGS__})

void $axes_draw(struct $axes *axes, unsigned *canvas, int axeswidth, int axesheight, int ystride);
void $axislabel(struct $axis *axis, char *label);
void $show(struct $axes *axes);
void $free(struct $axes *axes);
void $free_axis(struct $axis *axis);

void cplot_axes_render(struct $axes *axes, unsigned *canvas, int axeswidth, int axesheight, int ystride);
void cplot_axes_commit(struct $axes *axes, int axeswidth, int axesheight);
void cplot_add_axistext(struct $axis *axis, struct $axistext *text);

static inline struct $axis* cplot_xaxis0(struct $axes *axes) {
    return axes->axis[0];
}

static inline struct $axis* cplot_yaxis0(struct $axes *axes) {
    return axes->axis[1];
}

#ifndef using_cplot
#undef $f2si
#undef $f4si
#undef $axis
#undef $ticks
#undef $ticker
#undef $tickerdata
#undef $axes
#undef $data
#undef $axes_draw
#undef $axislabel
#undef $show
#undef $free
#undef $free_axis
#undef $plot
#undef $plot_inl
#undef $plot_args

#else
#define $xaxis0 cplot_xaxis0
#define $yaxis0 cplot_yaxis0
#define $type	cplot_type
#endif

#endif
