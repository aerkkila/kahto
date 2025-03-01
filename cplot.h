#ifndef __cplot_h__
#define __cplot_h__
#include <stdlib.h> // cplot_line_inl uses malloc
#include <stdint.h> // uint32_t

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

/* For example: assert(axes->axis[cplot_ix]->direction == cplot_ix); // x-axis */
#define cplot_ix 0
#define cplot_iy 1

/* returns some of the enumerations above according to the data type */
#define cplot_type(a) ((int)( \
		(typeof(a))1.5 == 1 ?	/* integer or floating point number */ \
		/* integer */ \
		sizeof(a) + (		/* size of integer */ \
			(typeof(a))(-1) < 0 ?	/* signed or unsigned */ \
			10 : 0 )			/* signed integer is enumerated as size + 10 */ \
			/* floating point */ \
			: sizeof(a) + 20 ))		/* floating point number is enumerated as size + 20 */

extern const unsigned char cplot_sizes[];

/* To zoom axis to some piece of data, set axis->range_isset = cplot_range_isset */
#define cplot_minbit 1
#define cplot_maxbit 2
#define cplot_range_isset (cplot_minbit|cplot_maxbit)
#define cplot_automatic -9010 // assumed to be negative

#define cplot_rgb(r, g, b) (0xff<<24 | (r)<<16 | (g)<<8 | (b)<<0)

#define __cplot_version_in_program 21
extern const int __cplot_version_in_library;
#ifndef CPLOT_NO_VERSION_CHECK
static void __attribute__((constructor)) cplot_check_version() {
	if (__cplot_version_in_library != __cplot_version_in_program)
		goto fail;
	return;
fail: __attribute__((cold));
	  /* Using assembly here to avoid including stdio or unistd in this header. */
	  const char errmsg[] = "The program has to be recompiled (" __FILE__ ").\n";
	  asm (
		  "movq $2, %%rdi\n" // stderr
		  "movq %0, %%rsi\n"
		  "movq %1, %%rdx\n"
		  "movq $1, %%rax\n" // write
		  "syscall\n"
		  :
		  : "rm"(errmsg), "rm"(sizeof(errmsg)-1)
		  : "rdi", "rsi", "rdx", "rax"
	  );
	  asm (
		  "movq $45, %rdi\n" // exit status
		  "movq $60, %rax\n" // exit
		  "syscall\n"
	  );
}
#endif

typedef float cplot_f4si __attribute__((vector_size (16)));
typedef float cplot_f2si __attribute__((vector_size (8)));

extern unsigned *cplot_colorschemes[];
extern int cplot_ncolors[];
extern int cplot_default_width, cplot_default_height;

struct cplot_axis;
struct cplot_data;

enum cplot_linestyle_e {cplot_line_none_e, cplot_line_normal_e, cplot_line_dashed_e};

/* Fixed order means that future updates won't change the order of the fields.
   Therefore, one can safely fill the struct by referring only to the first field by name.
   E.g. struct cplot_markerstyle mstyle = {.marker="x", 0.01,};
   Otherwise, the structs can change in the future.
   */

struct cplot_linestyle {
	enum cplot_linestyle_e style;
	unsigned color;
	float thickness;
	float *pattern;   // 1. fixed order
	int patternlen;   // 2. fixed order
	unsigned *colors; // 3. fixed order. If given, must be the same length as pattern (default 2)
	int align; // (y = 0, thickness = 3) => y:={-1,0,1} (align = 0), y:={-2,-1,0} (align = -1), y:={0,1,2} (align = 1)
};

struct cplot_markerstyle {
	const char* marker;	// 1. fixed order
	float size;		// 2. fixed order
	float nofill;	// 3. fixed order
	int literal;
	unsigned color;
};

struct cplot_tickerdata_linear {
	int nticks, // must be first
		baseten;
	double step, min, base, target_nticks;
	int omit_coef, out_omitted_coef, nsubticks;
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

struct cplot_ticks {
	struct cplot_axis *axis;

	enum cplot_tickere species;
	void (*init)(struct cplot_ticks *this, double min, double max);
	double (*get_tick)(struct cplot_ticks *this, int ind, char **label, int sizelabel);
	int (*get_maxn_subticks)(struct cplot_ticks *this);
	int (*get_subticks)(struct cplot_ticks *this, float *out);
	union cplot_tickerdata tickerdata;
	int integers_only;

	/* variable1: variable for minor ticks */
	unsigned color, color1;
	float length, length1;

	struct cplot_linestyle linestyle, gridstyle, linestyle1, gridstyle1;

	int visible, have_labels;
	float xyalign_text[2];
	float rowheight, rotation_grad;

	int ro_lines[2], ro_lines1[2], ro_labelarea[4];
};

struct cplot_axis {
	struct cplot_axes *axes;
	int direction, outside; // direction: x=0, y=1
	float pos;
	double min, max;
	int range_isset, ro_line[4];
	struct cplot_ticks *ticks;
	struct cplot_axistext **text;
	int mem_text, ntext;

	struct cplot_linestyle linestyle;

	float po[2]; // parallel and orthogonal lengths if this is a coloraxis
	unsigned char *cmap;
	int reverse_cmap;
	int ro_area[4]; // area of the finitely thick line
};

enum axistext_type {cplot_axistext_other, cplot_axistext_label, cplot_axistext_tickmul};

struct cplot_axistext {
	struct cplot_axis *axis;
	char *text;
	enum axistext_type type;
	int owner;
	float pos, rowheight, rotation_grad;
	float hvalign[2];
	int ro_area[4];
};

struct cplot_text {
	char *text;
	float pos, rowheight, rotation_grad;
	float hvalign[2];
	int ro_area[4];
	char textowner;
};

union cplot_errorbars {
	struct {
		void *y0, *y1, *x0, *x1;
	} list;
	void *yx[4];
};

/* If changed, cplot_args must be changed similarily. */
struct cplot_data {
	/* fixed order below */
	void *yxzdata[3];
	int yxztype[3];
	long length;
	/* fixed order above */
	short yxzstride[3];
	struct cplot_axis *yxaxis[2], *caxis;
	double minmax[3][2];
	char have_minmax[3]; // bits: cplot_minbit, cplot_maxbit
	char owner[3], cmap_owner;
	double yxz0[3], yxzstep[3];
	char *label;	// 1. fixed order
	int labelowner;	// 2. fixed order
	union cplot_errorbars err;
	char err_owner[4];
	/* style */
	struct cplot_markerstyle markerstyle;	// fixed order
	struct cplot_linestyle linestyle, errstyle;	// fixed order
	unsigned color; // overridden by style.color
	unsigned *colors, ncolors; // data repeat these colors, overrides other color settings
	unsigned char *cmap;
	int cmh_enum, icolor;
};

/* standalone can mean any class listed below */
enum cplot_standalone_type {cplot_axes_e, cplot_subplots_e};

enum cplot_fill {cplot_no_fill_e, cplot_fill_bg_e, cplot_fill_color_e};
enum cplot_placement {cplot_placement_none, cplot_placement_first, cplot_placement_singlemaxdist};
enum cplot_topixels_reference {cplot_super_height, cplot_super_width, cplot_this_height, cplot_this_width};

#define inherit_cplot_standalone_common \
	enum cplot_standalone_type type;    \
	int wh[2];                          \
	unsigned background;                \
	struct waylandhelper *wlh;          \
	char *name; /* window title (cplot_show) or filename (cplot_save_png) */ \
	/* For animated plot. */            \
	/* return 1 if something changed on the screen, -1 if animation ended, 0 if nothing changed */ \
	int (*update)(struct cplot_axes*, uint32_t *canvas, int ystride, long count, double elapsed);  \
	void *userdata

/* What is common between all standalone drawables, listed in cplot_standalone_type */
struct cplot_standalone_common {
	inherit_cplot_standalone_common;
};

struct cplot_axes {
	inherit_cplot_standalone_common;
	int startcanvas;
	struct cplot_axis **axis, *last_caxis;
	int naxis, mem_axis;
	struct ttra *ttra;
	int ro_inner_xywh[4], ro_inner_margin[4];
	float margin[4];
	struct cplot_data **data;
	int ndata, mem_data;
	unsigned *colorscheme;
	int ncolors, icolor;
	struct cplot_text title;
	enum cplot_topixels_reference topixels_reference;
	struct cplot_standalone_common *super;

	struct legend {
		float rowheight, symbolspace_per_rowheight;
		int ro_xywh[4], ro_text_left, visible, ro_place_err;
		float posx, posy, hvalign[2];
		struct cplot_linestyle borderstyle;
		enum cplot_fill fill;
		unsigned fillcolor;
		enum cplot_placement placement;
	} legend;
};

struct cplot_subplots {
	inherit_cplot_standalone_common;
	struct cplot_axes **axes;
	float (*xywh)[4];
	int naxes;
};

struct cplot_args {
	struct cplot_axes *axes;
	struct cplot_axes **axesptr;

	/* struct cplot_data inlined. ydata must stay first */
	void *ydata, *xdata, *zdata;
	int ytype, xtype, ztype;
	long len;
	short ystride, xstride, zstride;
	struct cplot_axis *yaxis, *xaxis, *caxis;
	double minmax[3][2];
	char have_minmax[3]; // bits: cplot_minbit, cplot_maxbit
	char yxzowner[3], cmap_owner;
	double y0, x0, z0, ystep, xstep, zstep;
	char *label;	// 1. fixed order
	int labelowner;	// 2. fixed order; Copied, if owner = -1.
	union cplot_errorbars err;
	char err_owner[4]; // to copy, owner = -1

	struct cplot_markerstyle markerstyle;
	struct cplot_linestyle
		linestyle, errstyle;
	unsigned color;
	unsigned *colors, ncolors; // data repeat these colors, overrides other color settings
	unsigned char *cmap;
	int cmh_enum, icolor;
	/* end struct cplot_data */

	char copy[3]; // Deprecated. Use 'owner = -1' instead.
	/* Excecuted in cplot_plot_args before anything else is done.
	   Intended for changing the default arguments. */
	void (*argsfun)(struct cplot_args*);
	void *userdata; // if argsfun needs some data
};

/* To change the defaultargs, use the following macro before #include <cplot.h>, e.g.
#define cplot_update_defaultargs ,.linestyle.thickness = 1./400 */
#ifndef cplot_update_defaultargs
#define cplot_update_defaultargs
#endif

#define __cplot_defaultargs					\
	.markerstyle.marker = "o",				\
	.markerstyle.size = 1./70,				\
	.ystride = 1,							\
	.xstride = 1,							\
	.zstride = 1,							\
	.ystep = 1,								\
	.xstep = 1,								\
	.zstep = 1,								\
	.linestyle.thickness = 1./600,			\
	.errstyle.style = cplot_line_normal_e,	\
	.icolor = cplot_automatic,				\
	.errstyle.thickness = 1./600 cplot_update_defaultargs

#define cplot_lineargs						\
	.linestyle.style = cplot_line_normal_e,	\
	.markerstyle.marker = NULL

#define cplot_y(y, ...) cplot_plot_inl((struct cplot_args){	\
	__cplot_defaultargs,		\
	.ydata=(y),					\
	.ytype=cplot_type(*(y)),	\
	.ztype=0,					\
	__VA_ARGS__					\
	})
#define cplot_yx(y, x, ...) cplot_plot_inl((struct cplot_args){	\
	__cplot_defaultargs,		\
	.ydata=(y),					\
	.xdata=(x),					\
	.ytype=cplot_type(*(y)),	\
	.xtype=cplot_type(*(x)),	\
	.ztype=0,					\
	__VA_ARGS__					\
	})
#define cplot_yz(y, z, ...) cplot_plot_inl((struct cplot_args){	\
	__cplot_defaultargs,		\
	.ydata=(y),					\
	.zdata=(z),					\
	.ytype=cplot_type(*(y)),	\
	.ztype=cplot_type(*(z)),	\
	__VA_ARGS__					\
	})
#define cplot_yxz(y, x, z, ...) cplot_plot_inl((struct cplot_args){	\
	__cplot_defaultargs,		\
	.ydata=(y),					\
	.xdata=(x),					\
	.zdata=(z),					\
	.ytype=cplot_type(*(y)),	\
	.xtype=cplot_type(*(x)),	\
	.ztype=cplot_type(*(z)),	\
	__VA_ARGS__					\
	})

struct cplot_ticks* cplot_ticks_new(struct cplot_axis *axis);
struct cplot_axis* cplot_axis_new(struct cplot_axes *axes, int x_or_y, float position);
struct cplot_axis* cplot_coloraxis_new(struct cplot_axes *axes, int x_or_y);
struct cplot_axes* cplot_axes_new();
struct cplot_subplots* cplot_subplots_new(int nrows, int ncols);
/* xsizes[ncols] defines the proportional width of each column
 * the leftover space is divided equally between all columns with 0 value:
 * i.e. {0, 0.2, 0.3, 0} -> {0.25, 0.2, 0.3, 0.25}
 * ysizes[nrows] is similar to xsizes
 * xyowner: (xowner<<0) | (yowner<<1)
 */
struct cplot_subplots* cplot_subplots_grid_new(int nrows, int ncols, float *ysizes, float *xsizes, unsigned xyowner);

/* Calling:
 *     cplot_subplots_xyarr_new(4, (0.1, 0.1), 3, (0., 0.5, 0.2))
 * has the same effect as:
 *     float xarr[4] = {0.1, 0.1};
 *     float yarr[3] = {0, 0.5, 0.2};
 *     cplot_subplots_grid_new(3, 4, yarr, xarr, 0)
 * parentheses are mandatory around xarr and yarr
 * numbers to xarr and yarr must be passed as floating: not '0' but '0.'
 */
#define cplot_expand(...) __VA_ARGS__ // used to remove parentheses: cplot_expand (a,b) -> a,b
#define cplot_subplots_xyarr_new(xlen, xarr, ylen, yarr) \
	cplot_subplots_grid_new(ylen, xlen,                  \
		cplot_f4arr(ylen, -1., cplot_expand yarr, -1.),  \
		cplot_f4arr(xlen, -1., cplot_expand xarr, -1.), 3)

/* Same as cplot_subplots_xyarr_new but does not allocate memory.
   However, works only with constant expressions. Needs c23 standard to work.*/
#define cplot_subplots_sxyarr_new(xlen, xarr, ylen, yarr) \
	cplot_subplots_grid_new(ylen, xlen,                   \
		(static float[]){cplot_expand yarr, [ylen]=0},    \
		(static float[]){cplot_expand xarr, [xlen]=0}, 0)

/*
   This creates a subplot grid, where each axes has the same position position with the corresponding number in txt.
   For example:
   *  "1100000\n"
   *  "11--222\n"
   To add space, use '-' or any letter below 0x30. Spaces and empty lines are ignored.
   This allows expressing the same with a raw string literal, even when it is indented:
   R"(
   1100000
   11--222
   )";
   */
struct cplot_subplots* cplot_subplots_text_new(const char *txt);

struct cplot_axes* cplot_plot_args(struct cplot_args *args);
static inline struct cplot_axes* cplot_plot_inl(struct cplot_args args) {
	return cplot_plot_args(&args);
}

#define cplot_line(y0, x0, y1, x1, ...) cplot_line_inl(y0, x0, y1, x1, (struct cplot_args){	\
	__cplot_defaultargs,	\
	cplot_lineargs,			\
	__VA_ARGS__				\
	})

static inline struct cplot_axes* cplot_line_inl(float y0, float x0, float y1, float x1, struct cplot_args args) {
	double *buff = malloc(4*sizeof(double));
	double *ydata = buff, *xdata = buff+2;
	xdata[0] = x0;
	xdata[1] = x1;
	ydata[0] = y0;
	ydata[1] = y1;
	args.xdata = xdata;
	args.ydata = ydata;
	args.xtype = cplot_type(xdata[0]);
	args.ytype = cplot_type(ydata[0]);
	args.len = 2;
	args.yxzowner[0] = 1;
	return cplot_plot_args(&args);
}

struct cplot_axistext* cplot_axislabel(struct cplot_axis *axis, char *label);
void cplot_ticklabels(struct cplot_axis *axis, char **labels, int howmany);
struct cplot_axis* cplot_remove_ticks(struct cplot_axis *axis);
void cplot_destroy(void *standalone);
void cplot_destroy_axis(struct cplot_axis *axis);
void cplot_destroy_data(struct cplot_data *data);
struct cplot_axistext* cplot_add_axistext(struct cplot_axis *axis, struct cplot_axistext *text);

/* Available only if compiled with waylandhelper */
void* cplot_show_preserve(void *standalone); // returns the input
void* cplot_show(void *standalone); // destroys the input and returns NULL

void* cplot_write_png_preserve(void *standalone, const char *name); // returns the input standalone
void* cplot_write_png(void *standalone, const char *name); // destroys the input and returns NULL
#define _cplot_save_png(a, b, ...) cplot_write_png(a, b)
#define cplot_save_png(...) _cplot_save_png(__VA_ARGS__, NULL) // cplot_write_png but name is an optional argument
#define _cplot_save_png_preserve(a, b, ...) cplot_write_png_preserve(a, b)
#define cplot_save_png_preserve(...) _cplot_save_png_preserve(__VA_ARGS__, NULL)

/* Available only if compiled with video support. Function axes->update has to be defined. */
void* cplot_write_mp4_preserve(void *standalone, const char *name, float fps); // returns the input
void* cplot_write_mp4(void *standalone, const char *name, float fps); // destroys the input and returns NULL

unsigned char __attribute__((malloc))* cplot_colorscheme_cmap(unsigned *scheme, int len);
static inline struct cplot_axis* cplot_set_range(struct cplot_axis *axis, double min, double max) {
	axis->min = min;
	axis->max = max;
	axis->range_isset = cplot_range_isset;
	return axis;
}
static inline struct cplot_axis* cplot_set_min(struct cplot_axis *axis, double min) {
	axis->min = min;
	axis->range_isset |= cplot_minbit;
	return axis;
}
static inline struct cplot_axis* cplot_set_max(struct cplot_axis *axis, double max) {
	axis->max = max;
	axis->range_isset |= cplot_maxbit;
	return axis;
}

void cplot_init_ticker_default(struct cplot_ticks *this, double min, double max);
void cplot_init_ticker_simple(struct cplot_ticks *this, double min, double max);
void cplot_init_ticker_datetime(struct cplot_ticks *this, double min, double max);
void cplot_init_ticker_arbitrary_datacoord(struct cplot_ticks *this, double min, double max);
void cplot_init_ticker_arbitrary_datacoord_enum(struct cplot_ticks *this, double min, double max);
void cplot_init_ticker_arbitrary_relcoord(struct cplot_ticks *this, double min, double max);

#define cplot_ix0axis 0
#define cplot_iy0axis 1

static inline struct cplot_axis* cplot_xaxis0(struct cplot_axes *axes) { return axes->axis[cplot_ix0axis]; }
static inline struct cplot_axis* cplot_yaxis0(struct cplot_axes *axes) { return axes->axis[cplot_iy0axis]; }

void cplot_axes_render(struct cplot_axes *axes, uint32_t *canvas, int ystride);
int  cplot_axes_layout(struct cplot_axes *axes);
void cplot_clear_slot(struct cplot_subplots *subplots, int islot, uint32_t *canvas, int ystride);
void cplot_axis_datarange(struct cplot_axis*);
struct cplot_args* cplot_defaultargs(struct cplot_args *args); // returns the input
struct cplot_args* cplot_default_lineargs(struct cplot_args *args); // returns the input

/* For animated plot to be used in the cplot_axes.update(). */
void cplot_legend_draw(struct cplot_axes*, uint32_t *canvas, int ystride);
void cplot_data_render(struct cplot_data *data, uint32_t *canvas, int ystride, struct cplot_axes *axes, long start);
void cplot_clear_data(struct cplot_axes *axes, uint32_t *canvas, int ystride);
void cplot_draw_grid(struct cplot_axes *axes, uint32_t *canvas, int ystride);
void cplot_draw(void *vplot, uint32_t *canvas, int ystride);

/* Käyttäjä ei tarvitse näitä. */
void cplot_subplots_to_axes(struct cplot_subplots *subplots);
void cplot_axes_draw(struct cplot_axes *axes, uint32_t *canvas, int ystride);

void cplot__sprint_supernum(char *out, int sizeout, int num);
float __attribute__((malloc))* cplot_f4arr(int n, double terminator, ...);

#undef inherit_cplot_standalone_common

#endif
