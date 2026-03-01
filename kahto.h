#ifndef __kahto_h__
#define __kahto_h__
#include <stdlib.h> // malloc, pthread_t
#include <stdint.h> // uint32_t

#define kahto_notype 0
#define kahto_i1  (1 + 0)
#define kahto_i2  (1 + 1)
#define kahto_i4  (1 + 2)
#define kahto_i8  (1 + 4)

#define kahto_u1  (6 + 0)
#define kahto_u2  (6 + 1)
#define kahto_u4  (6 + 2)
#define kahto_u8  (6 + 4)

#define kahto_f4  (9 + 2)
#define kahto_f8  (9 + 4)
#define kahto_f10 (9 + 8)

/* returns some of the enumerations above according to the data type */
#define kahto_type(a) ((int)(                                                  \
		sizeof(a)/2 + (           /* enumeration is datasize/2 + identifier */ \
			(typeof(a))1.5 == 1 ? /* integer or floating point number */       \
			(typeof(a))(-1) < 0 ? /* signed or unsigned integer */             \
			1 : 6 : 9)))          /* identifiers for signed, unsigned and floating point respectively */

extern const unsigned char kahto_sizes[];

/* To zoom axis to some piece of data, set axis->range_isset = kahto_range_isset */
#define kahto_minbit 1
#define kahto_maxbit 2
#define kahto_range_isset (kahto_minbit|kahto_maxbit)
#define kahto_automatic -9010 // assumed to be negative

#define kahto_rgb(r, g, b) (0xff<<24 | (r)<<16 | (g)<<8 | (b)<<0)

#define __kahto_version_in_program 43
extern const int __kahto_version_in_library;
#ifndef KAHTO_NO_VERSION_CHECK
static void __attribute__((constructor)) kahto_check_version() {
	if (__kahto_version_in_library != __kahto_version_in_program)
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

typedef float kahto_f4si __attribute__((vector_size (16)));
typedef float kahto_f2si __attribute__((vector_size (8)));

extern unsigned *kahto_colorschemes[];
extern int kahto_ncolors[];
extern int kahto_default_width, kahto_default_height;

enum kahto_linestyle_e {
	kahto_line_none_e, kahto_line_normal_e, kahto_line_dashed_e,
	kahto_line_bresenham_xiaolin_e, kahto_line_circle_e,
	kahto_line_future_e, // This will be the default in future, but doesn't work well enough yet.
};

/* fixed order means changing the order of the fields will be avoided in future updates.
   Therefore, one can quite safely fill the struct by referring only to the first field by name.
   E.g. struct kahto_markerstyle mstyle = {.marker="x", 0.01,};
   */

struct kahto_marker {
	unsigned char *bmap;
	short w, h, pixelsize;
};

struct kahto_linestyle {
	enum kahto_linestyle_e style; // 1. fixed order
	float thickness;              // 2. fixed order
	unsigned color;
	float *pattern;   // 1. fixed order
	int patternlen;   // 2. fixed order
	unsigned *colors; // 3. fixed order. If given, must be the same length as pattern (default 2)
	int align; // (y = 0, thickness = 3) => y:={-1,0,1} (align = 0), y:={-2,-1,0} (align = -1), y:={0,1,2} (align = 1)
};

struct kahto_markerstyle {
	const char* marker;	// 1. fixed order
	float size;		// 2. fixed order
	float nofill;	// 3. fixed order
	int literal:1, count:1;
	unsigned color;
};

struct kahto_tickerdata_linear {
	int nticks, // must be first
		baseten;
	double step, min, base, target_nticks;
	int omit_coef, out_omitted_coef, nsubticks;
};

struct kahto_tickerdata_datetime {
	int nticks, // must be first
		step;
	double target_nticks;
	long min;
	const char *form;
};

struct kahto_tickerdata_arbitrary {
	int nticks; // must be first
	double *ticks;
	union {
		const char **c;
		char **m;
	} labels;
	double min, max; // only used with relcoord
};

struct kahto_tickerdata_common { // only used to access shared members
	int nticks;
};

union kahto_tickerdata { // kahto_tickerdata_common defines how each struct must begin
	struct kahto_tickerdata_common common;
	struct kahto_tickerdata_linear lin;
	struct kahto_tickerdata_datetime datetime;
	struct kahto_tickerdata_arbitrary arb;
};

enum kahto_tickere {kahto_ticker_linear, kahto_ticker_datetime, kahto_ticker_arbitrary};

struct kahto_ticks {
	struct kahto_axis *axis;

	enum kahto_tickere species;
	void (*init)(struct kahto_ticks *this, double min, double max);
	double (*get_tick)(struct kahto_ticks *this, int ind, char **label, int sizelabel);
	int (*get_maxn_subticks)(struct kahto_ticks *this);
	int (*get_subticks)(struct kahto_ticks *this, float *out);
	union kahto_tickerdata tickerdata;
	int integers_only;

	/* variable1: variable for minor ticks */
	unsigned color, color1;
	float length, length1;

	struct kahto_linestyle linestyle, gridstyle, linestyle1, gridstyle1;

	int visible, have_labels;
	float xyalign_text[2];
	float rowheight, rotation_grad;

	int ro_lines[2], ro_lines1[2], ro_labelarea[4];
};

enum kahto_feature {kahto_position_e, kahto_color_e, kahto_alpha_e};

struct kahto_axis {
	struct kahto_figure *figure;
	char direction, outside, visible; // direction: x=0, y=1
	float pos;
	double min, max,
		   center; // works only for coloraxis
	int range_isset, ro_line[4];
	enum kahto_feature feature;
	struct kahto_ticks *ticks;
	struct kahto_axistext **text;
	int ntexts, memtext;

	struct kahto_linestyle linestyle;

	float po[2]; // parallel and orthogonal lengths if this is a coloraxis
	unsigned char *cmap;
	int reverse_cmap;
	int ro_area[4]; // area of the finitely thick line
};

enum axistext_type {kahto_axistext_other, kahto_axistext_label, kahto_axistext_tickmul};

struct kahto_axistext {
	struct kahto_axis *axis;
	char *text;
	enum axistext_type type;
	int owner;
	float pos, rowheight, rotation_grad;
	float hvalign[2];
	int ro_area[4];
};

enum kahto_coords_reference {kahto_dataarea_e, kahto_figurearea_e, kahto_dataarea_inner_e};

struct kahto_text {
	const char *text; // const will be discarded on destroy, if owner
	float xy[2], rowheight, // fixed order until this
		  rotation_grad, hvalign[2];
	char owner;
	enum kahto_coords_reference reference;
	int ro_area[4];
};

struct kahto_data {
	void *data;
	int type;
	long length, sublength;
	short stride;
	char have_minmax, owner;
	double minmax[2];
	int n_users;
	struct kahto_data *prev, *next;
};

struct kahto_draw_data_args {
	uint32_t *canvas;
	unsigned *canvascount;
	int ystride;
	unsigned char alpha;
	const int *xywh_limits;
	uint32_t color;
	uint32_t *colors;
	int ncolors, ipoint;

	// if sublength, then xz(y[sublength]) instead of yxz,
	// there should be a user-given drawing function in that case
	int *yxz, sublength;
	const unsigned char *cmap;
	char reverse_cmap;

	unsigned char *bmap;
	int mapw, maph;

	struct kahto_graph *graph;
	struct kahto_figure *fig;
};

/* if draw_marker_fun = kahto_draw_boxmarker_,
   then this is draw_marker_fun_args */
struct kahto_draw_boxmarker_args {
	float boxwidth, linewidth, mlinewidth;
};

struct kahto_graph {
	union {
		struct {
			struct kahto_data *ydata, *xdata, *zdata, *e0data, *e1data;
		} list;
		struct kahto_data *arr[5];
	} data;
	/* the rest must match with kahto_args */
	struct kahto_axis *yxaxis[3], *countaxis;
	char cmap_owner;
	const char *label; // 1. fixed order. The const will be discarded, if labelowner is true.
	int labelowner;    // 2. fixed order
	/* style */
	void (*draw_marker_fun)(struct kahto_draw_data_args*);
	void *draw_marker_fun_args;
	struct kahto_markerstyle markerstyle;	// fixed order
	struct kahto_linestyle linestyle, errstyle;	// fixed order
	unsigned color; // overridden by style.color
	unsigned *colors, ncolors; // data repeat these colors, overrides other color settings
	unsigned char *cmap, alpha;
	int cmh_enum, icolor;
	double xoffset; // if xdata is not given, xₙ = xoffset + n
	unsigned equal_xy : 1, // only works with colormesh
			 exact : 1, // only needed with colormesh
			 legend_coloronly : 1;
};

enum kahto_fill {kahto_no_fill_e, kahto_fill_bg_e, kahto_fill_color_e};
enum kahto_placement {kahto_placement_none, kahto_placement_first, kahto_placement_singlemaxdist};
enum kahto_topixels_reference {kahto_total_height, kahto_total_width, kahto_this_height, kahto_this_width};

/* fixed order */
struct kahto_colorscheme {
	unsigned *colors;
	int ncolors, cmh_enum;
	char without_ends, owner;
};

struct kahto_figure {
	int draw_counter, wh[2];
	unsigned background;
	struct waylandhelper *wlh;
	char *name; /* window title (kahto_show) or filename (kahto_save_png) */
	/* For animated plot. */
	/* return 1 if something changed on the screen, -1 if animation ended, 0 if nothing changed */
	int (*update)(struct kahto_figure*, uint32_t *canvas, int ystride, long count, double elapsed);
	void *userdata;
	/* This allows user to draw arbitrary things to the figure.
	   This is called as the last thing in the drawing function. */
	void (*after_drawing)(struct kahto_figure*, uint32_t *canvas, int ystride);
	struct kahto_figure *super;
	char ro_cannot_draw : 1, ro_colors_set : 1;

	struct kahto_figure **subfigures;
	float (*subfigures_xywh)[4];
	int nsubfigures, memsubfigures;

	struct kahto_axis **axis;
	int naxis, mem_axis;
	/* these figures share the same x-coordinates
	   after computing the layout, each size is changed according to the smallest x-axis */
	struct kahto_figure **connected_x; int nconnected_x;
	struct ttra *ttra;
	char ttra_owner;
	int ro_inner_xywh[4], ro_inner_margin[4], ro_corner[2];
	float margin[4];
	struct kahto_graph **graph;
	int ngraph, mem_graph, icolor;
	struct kahto_data data;
	struct kahto_colorscheme colorscheme;
	struct kahto_text title;
	struct kahto_text *texts; // These do not affect the layout. Use kahto_[add_]text to modify.
	int ntexts, memtext;
	enum kahto_topixels_reference topixels_reference;
	void (*fix_too_little_space)(struct kahto_figure*);
	void (*revert_fixes)(struct kahto_figure*);
	float fontheightmul; // multiply all fontheights (rowheight) with this number
	float fracsizemul; // multiply all fractional sizes with this number

	struct legend {
		/* if legend would cover some data, its size is multiplied with scale in range [minscale,1] */
		float rowheight, symbolspace_per_rowheight, scale, minscale;
		/* if visible = -1, legend is drawn only if it doesn't cover any data */
		int ro_xywh[4], ro_text_left, ro_place_err, *ro_datay, ro_datay_len, visible;
		float posx, posy, hvalign[2];
		struct kahto_linestyle borderstyle;
		enum kahto_fill fill;
		unsigned fillcolor;
		enum kahto_placement placement;
		unsigned char coloronly:1;
	} legend;

	struct kahto_async *async;
};

struct kahto_args {
	struct kahto_figure *figure;
	struct kahto_figure **figureptr;

	void *ydata, *xdata, *zdata, *edata0, *edata1;
	int ytype, xtype, ztype, e0type, e1type; // unspecified eXtype is assumed equal to ytype
	long kahto_len, kahto_ylen, kahto_xlen; // xlen and ylen are for colormesh
	short ystride, xstride, zstride, e0stride, e1stride;
	int ysublength; // if given, also draw_marker_fun must be given
	double minmax[5][2];
	char have_minmax[5], // bits: kahto_minbit, kahto_maxbit
		 yxzowner[5];
	enum kahto_feature zfeature; // = color
	/* below must match with kahto_graph */
	struct kahto_axis *yaxis, *xaxis, *caxis, *countaxis; // yaxis must stay first
	char cmap_owner;
	const char *label; // 1. fixed order. The const will be discarded, if labelowner is true.
	int labelowner;    // 2. fixed order; Copied, if owner = -1.

	void (*draw_marker_fun)(struct kahto_draw_data_args*);
	void *draw_marker_fun_args; // will be in kahto_draw_data_args
	struct kahto_markerstyle markerstyle;
	struct kahto_linestyle
		linestyle, errstyle;
	unsigned color;
	unsigned *colors, ncolors; // data repeat these colors, overrides other color settings
	unsigned char *cmap, alpha;
	int cmh_enum, icolor;
	double xoffset;
	unsigned equal_xy : 1, // only works with colormesh
			 exact : 1, // only needed with colormesh
			 legend_coloronly : 1;
	/* above must match with kahto_graph */

	double caxis_center; // datavalue that evaluates to center color of cmap

	/* Excecuted in kahto_plot_args before anything else is done.
	   Intended for changing the default arguments. */
	void (*argsfun)(struct kahto_args*);
	void *userdata; // if argsfun needs some data

	/* if ystride > 1, plot the skipped data with the same stride The number of graphs equals ystride. */
	char plot_interlazed_y;
	const char **labels; // a different label for each data for interlazed plots
};

/* To change the defaultargs, use the following macro before #include <kahto.h>, e.g.
#define kahto_update_defaultargs ,.linestyle.thickness = 1./400 */
#ifndef kahto_update_defaultargs
#define kahto_update_defaultargs
#endif

#define __kahto_defaultargs					\
	.markerstyle.marker = "o",				\
	.markerstyle.size = 1./70,				\
	.ystride = 1,							\
	.xstride = 1,							\
	.zstride = 1,							\
	.e0stride = 1,							\
	.e1stride = 1,							\
	.linestyle.thickness = 1./600,			\
	.errstyle.style = kahto_line_normal_e,	\
	.icolor = kahto_automatic,				\
	.errstyle.thickness = 1./600,           \
	.caxis_center = 0./0.,                  \
	.zfeature = kahto_color_e,				\
	.alpha = 255							\
	kahto_update_defaultargs

#define kahto_lineargs						\
	.linestyle.style = kahto_line_normal_e,	\
	.markerstyle.marker = NULL

#define kahto_y(y, ...) kahto_plot_inl((struct kahto_args){	\
	__kahto_defaultargs,		\
	.ydata=(y),					\
	.ytype=kahto_type(*(y)),	\
	.kahto_len=__VA_ARGS__		\
	})
#define kahto_yx(y, x, ...) kahto_plot_inl((struct kahto_args){	\
	__kahto_defaultargs,		\
	.ydata=(y),					\
	.xdata=(x),					\
	.ytype=kahto_type(*(y)),	\
	.xtype=kahto_type(*(x)),	\
	.kahto_len=__VA_ARGS__		\
	})
#define kahto_yz(y, z, ...) kahto_plot_inl((struct kahto_args){	\
	__kahto_defaultargs,		\
	.ydata=(y),					\
	.zdata=(z),					\
	.ytype=kahto_type(*(y)),	\
	.ztype=kahto_type(*(z)),	\
	.kahto_len=__VA_ARGS__		\
	})
#define kahto_yxz(y, x, z, ...) kahto_plot_inl((struct kahto_args){	\
	__kahto_defaultargs,		\
	.ydata=(y),					\
	.xdata=(x),					\
	.zdata=(z),					\
	.ytype=kahto_type(*(y)),	\
	.xtype=kahto_type(*(x)),	\
	.ztype=kahto_type(*(z)),	\
	.kahto_len=__VA_ARGS__		\
	})
#define kahto_colormesh(z, ...) kahto_plot_inl((struct kahto_args){ \
	__kahto_defaultargs,        \
	.zdata=(z),                 \
	.ztype=kahto_type(*(z)),    \
	.equal_xy=1,                \
	.exact=1,                   \
	.kahto_ylen=__VA_ARGS__	    \
	})

/* Manual call is needed, if parameters in ttra such as ttra->fonttype are modified before calling
   the drawing or layout function which calls this automatically if necessary.
   Manual call is also needed, if the ttra instance should not be shared with fig->super. */
struct ttra* kahto_figure_ttra_new(struct kahto_figure *fig);

struct kahto_ticks* kahto_ticks_new(struct kahto_axis *axis);
struct kahto_axis* kahto_axis_new(struct kahto_figure *figure, int x_or_y, float position);
struct kahto_axis* kahto_coloraxis_new(struct kahto_figure *figure, int x_or_y);
struct kahto_figure* kahto_figure_new();
struct kahto_figure* kahto_subfigures_new(int nrows, int ncols);
/* xsizes[ncols] defines the proportional width of each column
 * the leftover space is divided equally between all columns with 0 value:
 * i.e. {0, 0.2, 0.3, 0} -> {0.25, 0.2, 0.3, 0.25}
 * ysizes[nrows] is similar to xsizes
 * xyowner: (xowner<<0) | (yowner<<1)
 */
struct kahto_figure* kahto_subfigures_grid_new(int nrows, int ncols, float *ysizes, float *xsizes, unsigned xyowner);

/* Calling:
 *     kahto_subfigures_xyarr_new(4, (0.1, 0.1), 3, (0., 0.5, 0.2))
 * has the same effect as:
 *     float xarr[4] = {0.1, 0.1};
 *     float yarr[3] = {0, 0.5, 0.2};
 *     kahto_subfigures_grid_new(3, 4, yarr, xarr, 0)
 * parentheses are mandatory around xarr and yarr
 * numbers to xarr and yarr must be explicitely expressed as floating point: not '0' but '0.'
 */
#define kahto_expand(...) __VA_ARGS__ // used to remove parentheses: kahto_expand (a,b) -> a,b
#define kahto_subfigures_xyarr_new(xlen, xarr, ylen, yarr) \
	kahto_subfigures_grid_new(ylen, xlen,                  \
		kahto_f4arr(ylen, -1., kahto_expand yarr, -1.),    \
		kahto_f4arr(xlen, -1., kahto_expand xarr, -1.), 3)

/* Same as kahto_subfigures_xyarr_new but does not allocate memory.
   Works only with constant expressions, i.e. value known by compiler.
   Needs c23 standard to work. */
#define kahto_subfigures_sxyarr_new(xlen, xarr, ylen, yarr) \
	kahto_subfigures_grid_new(ylen, xlen,                   \
		(static float[]){kahto_expand yarr, [ylen]=0},      \
		(static float[]){kahto_expand xarr, [xlen]=0}, 0)

/*
 * This creates a subplot grid, where each figure has the same position with the corresponding number in txt.
 * For example:
 *  "1100000\n"
 *  "11--222\n"
 * To add space, use '-' or any letter below 0x30. Spaces and empty lines are ignored.
 * This allows expressing the same with a raw string literal, even when it is indented:
 * {
 *     R"(
 *     1100000
 *     11--222
 *     )";
 * }
 */
struct kahto_figure* kahto_subfigures_text_new(const char *txt);

struct kahto_figure* kahto_add_subfigures(struct kahto_figure *fig, int n);

struct kahto_figure* kahto_plot_args(struct kahto_args *args);
static inline struct kahto_figure* kahto_plot_inl(struct kahto_args args) {
	return kahto_plot_args(&args);
}

struct kahto_figure* kahto_add_text(struct kahto_figure *figure, struct kahto_text *text); // kahto_text will be copied
#define kahto_text(figure, ...) kahto_add_text_inl(figure, (struct kahto_text){__VA_ARGS__})
static inline struct kahto_figure* kahto_add_text_inl(struct kahto_figure *figure, struct kahto_text text) {
	return kahto_add_text(figure, &text);
}

#define kahto_line(y0, x0, y1, x1, ...) kahto_line_inl(y0, x0, y1, x1, (struct kahto_args){	\
	__kahto_defaultargs,	\
	kahto_lineargs,			\
	__VA_ARGS__				\
	})

static inline struct kahto_figure* kahto_line_inl(float y0, float x0, float y1, float x1, struct kahto_args args) {
	double *buff = malloc(4*sizeof(double));
	double *ydata = buff, *xdata = buff+2;
	xdata[0] = x0;
	xdata[1] = x1;
	ydata[0] = y0;
	ydata[1] = y1;
	args.xdata = xdata;
	args.ydata = ydata;
	args.xtype = kahto_type(xdata[0]);
	args.ytype = kahto_type(ydata[0]);
	args.kahto_len = 2;
	args.yxzowner[0] = 1;
	return kahto_plot_args(&args);
}

struct kahto_axistext* kahto_axislabel(struct kahto_axis *axis, char *label);
void kahto_ticklabels(struct kahto_axis *axis, char **labels, int howmany);
struct kahto_axis* kahto_remove_ticks(struct kahto_axis *axis);
void kahto_destroy(struct kahto_figure *figure);
void kahto_destroy_axis(struct kahto_axis *axis);
void kahto_destroy_graph(struct kahto_graph*);
struct kahto_axistext* kahto_add_axistext(struct kahto_axis *axis, struct kahto_axistext *text);

/* Available only if compiled with waylandhelper */
struct kahto_figure* kahto_show_preserve_(struct kahto_figure *figure, char *name); // returns the input
void kahto_show_(struct kahto_figure *figure, char *name); // destroys the input
struct kahto_async* kahto_async_show(struct kahto_figure*);

#define kahto_show(...) _kahto_show(__VA_ARGS__, NULL) // kahto_show(figure) OR kahto_show(figure, "name")
#define _kahto_show(a, b, ...) kahto_show_(a, b)

#define kahto_show_preserve(...) _kahto_show_preserve(__VA_ARGS__, NULL)
#define _kahto_show_preserve(a, b, ...) kahto_show_preserve_(a, b)

struct kahto_figure* kahto_write_png_preserve(struct kahto_figure *figure, const char *name); // returns the input figure
void kahto_write_png(struct kahto_figure *figure, const char *name); // destroys the input
#define _kahto_save_png(a, b, ...) kahto_write_png(a, b)
#define kahto_save_png(...) _kahto_save_png(__VA_ARGS__, NULL) // kahto_write_png but name is an optional argument
#define _kahto_save_png_preserve(a, b, ...) kahto_write_png_preserve(a, b)
#define kahto_save_png_preserve(...) _kahto_save_png_preserve(__VA_ARGS__, NULL)

/* Available only if compiled with video support. Function figure->update has to be defined. */
struct kahto_figure* kahto_write_mp4_preserve(struct kahto_figure *figure, const char *name, float fps); // returns the input
void kahto_write_mp4(struct kahto_figure *figure, const char *name, float fps); // destroys the input
struct kahto_async* kahto_async_write_mp4(struct kahto_figure *fig, const char *name, float fps);

/* cmap is in form [rgb]*256 */
#define kahto_colorscheme_to_cmap colorscheme_to_cmap__renamed__make_cmap_from_colorscheme
#define kahto_cmap_to_colorscheme cmap_to_colorscheme__renamed__make_colorscheme_from_cmap
unsigned char __attribute__((malloc))* kahto_make_cmap_from_colorscheme(const unsigned *scheme, int len);
unsigned __attribute__((malloc))* kahto_make_colorscheme_from_cmap(unsigned *dest, const unsigned char *cmap, int n, int without_ends);

static inline struct kahto_axis* kahto_set_range(struct kahto_axis *axis, double min, double max) {
	axis->min = min;
	axis->max = max;
	axis->range_isset = kahto_range_isset;
	return axis;
}
static inline struct kahto_axis* kahto_set_min(struct kahto_axis *axis, double min) {
	axis->min = min;
	axis->range_isset |= kahto_minbit;
	return axis;
}
static inline struct kahto_axis* kahto_set_max(struct kahto_axis *axis, double max) {
	axis->max = max;
	axis->range_isset |= kahto_maxbit;
	return axis;
}

/* get latest graph */
static inline struct kahto_graph* kahto_glg(struct kahto_figure *fig) {
	if (fig->ngraph <= 0)
		return NULL;
	return fig->graph[fig->ngraph-1];
}

/* get latest xaxis */
static inline struct kahto_axis* kahto_glx(struct kahto_figure *fig) {
	if (fig->ngraph <= 0)
		return NULL;
	return fig->graph[fig->ngraph-1]->yxaxis[1];
}

/* get latest yaxis */
static inline struct kahto_axis* kahto_gly(struct kahto_figure *fig) {
	if (fig->ngraph <= 0)
		return NULL;
	return fig->graph[fig->ngraph-1]->yxaxis[0];
}

void kahto_init_ticker_default(struct kahto_ticks *this, double min, double max);
void kahto_init_ticker_simple(struct kahto_ticks *this, double min, double max);
void kahto_init_ticker_datetime(struct kahto_ticks *this, double min, double max);
void kahto_init_ticker_arbitrary_datacoord(struct kahto_ticks *this, double min, double max);
void kahto_init_ticker_arbitrary_datacoord_enum(struct kahto_ticks *this, double min, double max);
void kahto_init_ticker_arbitrary_relcoord(struct kahto_ticks *this, double min, double max);

/* can be given by user to graph->draw_marker_fun */
void kahto_draw_boxmarker_5(struct kahto_draw_data_args *args);

struct kahto_async {
	struct kahto_figure *figure;
	uint32_t *canvas;
	int ystride;
	pthread_t _thread;
	signed char _lock, _exit;
	float _fps;
};
int kahto_async_lock(struct kahto_async *async);
void kahto_async_unlock(struct kahto_async *async);
void kahto_async_unlock_step(struct kahto_async *async);
void kahto_async_destroy(struct kahto_async *async);
int kahto_async_running(struct kahto_async *async);
void kahto_async_stop(struct kahto_async *async); // destroy without stop is enough
/* waits for all n threads to terminate and destroys them */
void kahto_async_join(struct kahto_async **async, int n);

void kahto_draw_figure(struct kahto_figure *figure, uint32_t *canvas, int ystride);
int  kahto_figure_layout(struct kahto_figure *figure);
void kahto_make_range(struct kahto_figure *);
struct kahto_args* kahto_defaultargs(struct kahto_args *args); // returns the input
struct kahto_args* kahto_default_lineargs(struct kahto_args *args); // returns the input
/* Figure is re-initialized as in kahto_figure_new(), except that ttra is preserved.
   This increases performance because initializing ttra is slow. */
struct kahto_figure* kahto_clean(struct kahto_figure*);
void kahto_xywh_to_subfigures(struct kahto_figure*);

/* These are used automatically when necessary
   but user might need these in user-defined drawing functions, e.g. figure->update. */
int  kahto_topixels(float size, struct kahto_figure *figure) __attribute__((pure));
void kahto_xywh_to_figure(struct kahto_figure*);
void kahto_draw_figures(struct kahto_figure *figure, uint32_t *canvas, int ystride);
void kahto_layout(struct kahto_figure *figure);
void kahto_set_colors(struct kahto_figure*);
void kahto_legend_draw(struct kahto_figure*, uint32_t *canvas, int ystride);
void kahto_draw_graph(struct kahto_graph *graph, uint32_t *canvas, int ystride, struct kahto_figure *fig, long start);
void kahto_clear_data(struct kahto_figure *figure, uint32_t *canvas, int ystride);
void kahto_draw_grid(struct kahto_figure *figure, uint32_t *canvas, int ystride);
void kahto_draw(struct kahto_figure *fig, uint32_t *canvas, int ystride);

void kahto__sprint_supernum(char *out, int sizeout, int num);
float __attribute__((malloc))* kahto_f4arr(int n, double terminator, ...);

#endif
