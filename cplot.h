#ifndef __cplot_h__
#define __cplot_h__
#define CMH_ENUM_ONLY
#include "cmh_colormaps.h"

#define cplot_notype 0
#define cplot_i1 (10 + 1)
#define cplot_i2 (10 + 2)
#define cplot_i4 (10 + 4)
#define cplot_i8 (10 + 8)
#define cplot_u1 1
#define cplot_u2 2
#define cplot_u4 4
#define cplot_u8 8
#define cplot_f4 (20 + 4)
#define cplot_f8 (20 + 8)
#define cplot_f10 (20 + 16)

/* returns some of the enumerations above according to the data type */
#define cplot_type(a) ((int)( \
	    (typeof(a))1.5 == 1 ?	/* integer or floating point number */ \
	    /* integer */ \
	    sizeof(a) + (		/* size of integer */ \
	    (typeof(a))(-1) < 0 ?	/* signed or unsigned */ \
	     10 : 0 )			/* signed integer is enumerated as size + 10 */ \
	    /* floating point */ \
	    : sizeof(a) + 20 ))		/* floating point number is enumerated as size + 20 */

/* If axis has range_isset & cplot_minbit, range will be edited so that markers don't cut on the edges.
   With additional cplot_minbit_const, range will not be edited.
   To zoom axis to some piece of data, use cplot_minbit|cplot_minbit_const and cplot_maxbit|cplot_maxbit_const. */
#define cplot_minbit 1
#define cplot_maxbit 2
#define cplot_minbit_const 4
#define cplot_maxbit_const 8
#define cplot_automatic -9010

#define cplot_rgb(r, g, b) (0xff<<24 | (r)<<16 | (g)<<8 | (b)<<0)

typedef float cplot_f4si __attribute__((vector_size (16)));
typedef float cplot_f2si __attribute__((vector_size (8)));

extern unsigned cplot_colorscheme[];
extern int cplot_ncolors;
extern int cplot_default_width, cplot_default_height;

struct cplot_axis;
struct cplot_data;

enum cplot_linestyle_e {cplot_line_none_e, cplot_line_normal_e, cplot_line_dashed_e};

struct cplot_linestyle {
    enum cplot_linestyle_e style;
    unsigned color;
    float thickness;
    float *pattern;
    unsigned *colors; // for future use
    int patternlen;
};

struct cplot_tickerdata_linear {
    int nticks, // must be first
	baseten;
    double step, min, base, target_nticks;
    int omit_coef, out_omitted_coef;
};

struct cplot_tickerdata_datetime {
    int nticks, // must be first
	step;
    double target_nticks;
    long min;
    const char *form;
};

struct cplot_tickerdata_arbitrary {
    int nticks; // must be first
    double *ticks;
    char **labels;
    double min, max; // only used with relcoord
};

struct cplot_tickerdata_common { // only used to access shared members
    int nticks;
};

union cplot_tickerdata { // cplot_tickerdata_common defines how each struct must begin
    struct cplot_tickerdata_common common;
    struct cplot_tickerdata_linear lin;
    struct cplot_tickerdata_datetime datetime;
    struct cplot_tickerdata_arbitrary arb;
};

enum cplot_tickere {cplot_ticker_linear, cplot_ticker_datetime, cplot_ticker_arbitrary};

struct cplot_ticker {
    enum cplot_tickere species;
    void (*init)(struct cplot_ticker *this, double min, double max);
    double (*get_tick)(struct cplot_ticker *this, int ind, char **label, int sizelabel);
    union cplot_tickerdata tickerdata;
    int integers_only;
};

/* crossaxis: Where ticks are parallel to the axis?
 *	0: Ticks start at the axis i.e. are right or below.
 *	1: Ticks end at the axis i.e. are left or above.
 */
struct cplot_ticks {
    struct cplot_axis *axis;
    struct cplot_ticker ticker;
    unsigned color;
    float crossaxis, length;

    struct cplot_linestyle linestyle, gridstyle;

    int have_labels;
    float hvalign_text[2];
    int ascending; // whether labels are in the end and not start
    float rowheight, rotation100;

    int ro_lines[2], ro_labelarea[4], ro_tot_area[4];
};

struct cplot_axis {
    struct cplot_axes *axes;
    int direction, outside; // x=0, y=1
    float pos;
    double min, max;
    int range_isset, ro_line[4], ro_tick_area[4];
    struct cplot_ticks *ticks;
    struct cplot_axistext **text;
    int mem_text, ntext;

    struct cplot_linestyle linestyle;
    int ro_linetick_area[4];

    float po[4]; // parallel and orthogonal lengths
    const unsigned char *cmap;
    int reverse_cmap;
    int ro_area[4], ro_tot_area[4];
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

struct cplot_text {
    char *text;
    float pos, rowheight, rotation100;
    float hvalign[2];
    int ro_area[4];
};

/* If changed, cplot_args must be changed similarily. */
struct cplot_data {
    void *yxzdata[3];
    int yxztype[3];
    long length;
    struct cplot_axis *yxaxis[2], *caxis;
    double minmax[3][2];
    char have_minmax[3]; // bits: cplot_minbit, cplot_maxbit
    char owner[3];
    char *label;
    /* style */
    const char* marker;
    int literal_marker;
    float markersize;
    unsigned color;
    struct cplot_linestyle linestyle;
    const unsigned char *cmap;
    int cmh_enum;
};

enum cplot_whatisthis {cplot_axes_e, cplot_layout_e};
enum cplot_fill {cplot_no_fill_e, cplot_fill_bg_e, cplot_fill_color_e};

struct cplot_axes {
    /* Shared between axes and layout. Order matters here. */
    enum cplot_whatisthis whatisthis;
    int wh[2];
    unsigned background;
    /* end shared */
    int startcanvas;
    struct cplot_axis **axis, *last_caxis;
    int naxis, mem_axis;
    struct ttra *ttra;
    int ro_inner_xywh[4];
    float margin[4];
    struct cplot_data **data;
    int ndata, mem_data, icolor;

    struct legend {
	float rowheight, symbolspace_per_rowheight;
	int ro_xywh[4], ro_text_left, automatic_placement;
	float posx, posy, hvalign[2];
	struct cplot_linestyle borderstyle;
	enum cplot_fill fill;
	unsigned fillcolor;
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
    struct cplot_axis *yaxis, *xaxis, *caxis;
    double minmax[3][2];
    char have_minmax[3]; // bits: cplot_minbit, cplot_maxbit
    char yxzowner[3];
    char *label;

    const char* marker;
    int literal_marker;
    float markersize;
    unsigned color;
    struct cplot_linestyle linestyle;
    const char *cmap;
    int cmh_enum;

    /* end struct cplot_data */
    int copy[3]; // not used yet
};

struct cplot_drawarea {
    unsigned *canvas;
    int axeswidth, axesheight, ystride;
};

#define cplot_y(y, ...) cplot_plot_inl((struct cplot_args){	\
    .ydata=(y),							\
    .ytype=cplot_type(*(y)),					\
    __VA_ARGS__							\
    })
#define cplot_yx(y, x, ...) cplot_plot_inl((struct cplot_args){	\
    .ydata=(y),							\
    .xdata=(x),							\
    .ytype=cplot_type(*(y)),					\
    .xtype=cplot_type(*(x)),					\
    __VA_ARGS__							\
    })
#define cplot_yz(y, z, ...) cplot_plot_inl((struct cplot_args){	\
    .ydata=(y),							\
    .zdata=(z),							\
    .ytype=cplot_type(*(y)),					\
    .ztype=cplot_type(*(z)),					\
    __VA_ARGS__							\
    })
#define cplot_yxz(y, x, z, ...) cplot_plot_inl((struct cplot_args){	\
    .ydata=(y),							\
    .xdata=(x),							\
    .zdata=(z),							\
    .ytype=cplot_type(*(y)),					\
    .xtype=cplot_type(*(x)),					\
    .ztype=cplot_type(*(z)),					\
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

struct cplot_axistext* cplot_axislabel(struct cplot_axis *axis, char *label);
void cplot_show(void *axes_or_layout);
void cplot_destroy(void *axes_or_layout);
void cplot_destroy_axis(struct cplot_axis *axis);
struct cplot_axistext* cplot_add_axistext(struct cplot_axis *axis, struct cplot_axistext *text);
void cplot_write_png(void *axes_or_layout, const char *name);

void cplot_init_ticker_default(struct cplot_ticker *this, double min, double max);
void cplot_init_ticker_simple(struct cplot_ticker *this, double min, double max);
void cplot_init_ticker_datetime(struct cplot_ticker *this, double min, double max);
void cplot_init_ticker_arbitrary_datacoord(struct cplot_ticker *this, double min, double max);
void cplot_init_ticker_arbitrary_relcoord(struct cplot_ticker *this, double min, double max);

#define cplot_ix0axis 0
#define cplot_iy0axis 1

static inline struct cplot_axis* cplot_xaxis0(struct cplot_axes *axes) { return axes->axis[cplot_ix0axis]; }
static inline struct cplot_axis* cplot_yaxis0(struct cplot_axes *axes) { return axes->axis[cplot_iy0axis]; }

/* Käyttäjä tuskin tarvitsee näitä. */
void cplot_axes_render(struct cplot_axes *axes, unsigned *canvas, int ystride);
int  cplot_axes_commit(struct cplot_axes *axes);
void cplot_clear_slot(struct cplot_layout *layout, int islot, unsigned *canvas, int ystride);

/* Käyttäjä ei tarvitse näitä. */
void cplot_layout_to_axes(struct cplot_layout *layout);
void cplot_axes_draw(struct cplot_axes *axes, unsigned *canvas, int ystride);

void cplot__sprint_supernum(char *out, int sizeout, int num);

#endif
