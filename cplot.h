#ifndef __cplot_h__
#define __cplot_h__

#define cplot_notype 0
#define cplot_i1 (10 + sizeof(char))
#define cplot_i2 (10 + sizeof(short))
#define cplot_i4 (10 + sizeof(int))
#define cplot_i8 (10 + sizeof(long))
#define cplot_u1 sizeof(unsigned char)
#define cplot_u2 sizeof(unsigned short)
#define cplot_u4 sizeof(unsigned int)
#define cplot_u8 sizeof(unsigned long)
#define cplot_f4 (20 + sizeof(float))
#define cplot_f8 (20 + sizeof(double))
#define cplot_f10 (20 + sizeof(long double))

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

typedef float cplot_f4si __attribute__((vector_size (16)));
typedef float cplot_f2si __attribute__((vector_size (8)));

extern unsigned cplot_colorscheme[];
extern int cplot_ncolors;
extern int cplot_default_width, cplot_default_height;

struct cplot_axis;
struct cplot_data;

struct cplot_pen {
    unsigned color;
    float thickness;
    /* linestyle? */
};

struct cplot_tickerdata_linear {
    int nticks, baseten;
    double step, min, base;
};

union cplot_tickerdata {
    struct cplot_tickerdata_linear lin;
    /* for future use */
};

enum cplot_tickere {ticker_linear};

struct cplot_ticker {
    enum cplot_tickere species;
    int (*init)(struct cplot_ticker *this, double min, double max); // returns the number of ticks
    double (*get_tick)(struct cplot_ticker *this, int ind, char *out, int sizeout);
    union cplot_tickerdata tickerdata;
};

/* crossaxis: Where ticks are parallel to the axis?
 *	0: Ticks start at the axis i.e. are right or below.
 *	1: Ticks end at the axis i.e. are left or above.
 */
struct cplot_ticks {
    struct cplot_axis *axis;
    struct cplot_ticker ticker;
    unsigned color;
    float crossaxis, length, thickness;

    int grid_on;
    struct cplot_pen grid_pen;

    int have_labels;
    float hvalign_text[2];
    int ascending; // nimiöt tulevat suurempaan päähän
    float rowheight;

    int ro_lines[2], ro_labelarea[4], ro_tot_area[4];
};

struct cplot_axis {
    struct cplot_axes *axes;
    int x_or_y;
    float pos;
    double min, max;
    int range_isset;
    unsigned color;
    float thickness;
    struct cplot_axistext **text;
    int mem_text, ntext;
    struct cplot_ticks *ticks;
    int ro_line[4], ro_tick_area[4], ro_linetick_area[4];
};

enum axistext_type {cplot_axistext_other, cplot_axistext_label, cplot_axistext_tickmul};

struct cplot_axistext {
    struct cplot_axis *axis;
    char *text;
    enum axistext_type type;
    int owner;
    float pos, rowheight, rotation100;
    float hvalign[2];
    int ro_area[4];
};

/* If changed, cplot_args must be changed similarily. */
struct cplot_data {
    void *yxzdata[3];
    int yxztype[3];
    long length;
    struct cplot_axis *yxaxis[2];
    double minmax[3][2];
    char have_minmax[3]; // bits: minbit, maxbit
    int owner[3];
    char *label;
    /* style */
    const char* marker;
    int literal_marker;
    float markersize;
    unsigned color;
    const char *linestyle;
    float line_thickness;
};

enum cplot_whatisthis {cplot_axes_e, cplot_layout_e};

struct cplot_axes {
    /* Shared between axes and layout. Order matters here. */
    enum cplot_whatisthis whatisthis;
    int wh[2];
    unsigned background;
    /* end shared */
    int startcanvas;
    struct cplot_axis **axis;
    int naxis, mem_axis;
    struct ttra *ttra;
    int ro_inner_xywh[4];
    struct cplot_data **data;
    int ndata, mem_data, icolor;

    struct legend {
	float rowheight, symbolspace_per_rowheight;
	int ro_xywh[4], ro_text_left, automatic_placement;
	float posx, posy, hvalign[2];
    } legend;
};

struct cplot_layout {
    /* Shared between axes and layout. Order matters here. */
    enum cplot_whatisthis whatisthis;
    int wh[2];
    unsigned background;
    /* end shared */
    struct cplot_axes **axes;
    float (*xywh)[4];
    int naxes;
};

struct cplot_args {
    struct cplot_axes *axes;

    /* struct cplot_data inlined. ydata must stay first */
    void *ydata, *xdata, *zdata;
    int ytype, xtype, ztype;
    long len;
    struct cplot_axis *yaxis, *xaxis;
    double minmax[3][2];
    char have_minmax[3]; // bits: minbit, maxbit
    int yxzowner[3];
    char *label;

    const char* marker;
    int literal_marker;
    float markersize;
    unsigned color;
    const char *linestyle;
    float line_thickness;

    /* end struct cplot_data */
    int copy[3]; // not used yet
};

struct cplot_drawarea {
    unsigned *canvas;
    int axeswidth, axesheight, ystride;
};

#define cplot_y(y, ...) cplot_plot_inl((struct cplot_args){.ydata=(y), .ytype=cplot_type(*(y)), __VA_ARGS__})
#define cplot_yx(y, x, ...) cplot_plot_inl((struct cplot_args){	\
    .ydata=(y),							\
    .xdata=(x),							\
    .ytype=cplot_type(*(y)),					\
    .xtype=cplot_type(*(x)),					\
    __VA_ARGS__							\
    })

struct cplot_ticks* cplot_ticks_new(struct cplot_axis *axis);
struct cplot_axis* cplot_axis_new(struct cplot_axes *axes, int x_or_y);
struct cplot_layout* cplot_layout_new(int nrows, int ncols);
struct cplot_axes* cplot_plot_args(struct cplot_args *args);
static inline struct cplot_axes* cplot_plot_inl(struct cplot_args args) {
    return cplot_plot_args(&args);
}
#define cplot_plot(...) cplot_plot_inl((struct cplot_args){__VA_ARGS__})

void cplot_axislabel(struct cplot_axis *axis, char *label);
void cplot_show(void *axes_or_layout);
void cplot_destroy(void *axes_or_layout);
void cplot_destroy_axis(struct cplot_axis *axis);
void cplot_fini();
void cplot_add_axistext(struct cplot_axis *axis, struct cplot_axistext *text);
void cplot_write_png(void *axes_or_layout, const char *name);

#define cplot_ix0axis 0
#define cplot_iy0axis 1

static inline struct cplot_axis* cplot_xaxis0(struct cplot_axes *axes) { return axes->axis[cplot_ix0axis]; }
static inline struct cplot_axis* cplot_yaxis0(struct cplot_axes *axes) { return axes->axis[cplot_iy0axis]; }

/* Käyttäjä tuskin tarvitsee näitä. */
void cplot_axes_render(struct cplot_axes *axes, unsigned *canvas, int ystride);
void cplot_axes_commit(struct cplot_axes *axes);
void cplot_clear_slot(struct cplot_layout *layout, int islot, unsigned *canvas, int ystride);

/* Käyttäjä ei tarvitse näitä. */
void cplot_layout_to_axes(struct cplot_layout *layout);
void cplot_axes_draw(struct cplot_axes *axes, unsigned *canvas, int ystride);

#endif
