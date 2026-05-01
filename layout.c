#ifndef KAHTO_NO_VERSION_CHECK
#define KAHTO_NO_VERSION_CHECK
#endif
#include "kahto.h"
#include <ttra.h>

static void update_maxarea(int *a, int *b) {
	if (b[0] < a[0]) a[0] = b[0];
	if (b[1] < a[1]) a[1] = b[1];
	if (b[2] > a[2]) a[2] = b[2];
	if (b[3] > a[3]) a[3] = b[3];
}

static void get_axisarea(struct kahto_axis *ax, int area[4]) {
	memcpy(area, ax->ro_area, 4*sizeof(int));
	if (ax->ticks)
		update_maxarea(area, ax->ticks->ro_labelarea);
	for (int i=0; i<ax->ntexts; i++)
		update_maxarea(area, ax->text[i]->ro_area);
}

static int get_subfigures_area(struct kahto_figure *fig, int area[4]) {
	int first = 1;
	int help[4];
	for (int ifig=0; ifig<fig->nsubfigures; ifig++) {
		if (!fig->subfigures[ifig])
			continue;
		memcpy(help, fig->subfigures[ifig]->ro_corner, 2*sizeof(int));
		help[2] = help[0] + fig->subfigures[ifig]->wh[0];
		help[3] = help[1] + fig->subfigures[ifig]->wh[1];
		if (first) {
			memcpy(area, help, sizeof(help));
			first = 0;
		}
		else
			update_maxarea(area, help);
	}
	return first;
}

/* Usually returned area is the size of the whole figure.
   Smaller, if e.g. a standalone coloraxis or a standalone legend is drawn,
   because there is nothing which can expand to fill the whole figure.
   Returns whether the figure size should be changed to match returned area. */
static int get_used_area(struct kahto_figure *fig, int area[4]) {
	int cmp[4] = {0};
	int adjust_size = !!fig->naxis; // adjust size if there is an axis
	memcpy(area, fig->legend.ro_xywh, sizeof(fig->legend.ro_xywh));
	if (memcmp(cmp, area, sizeof(cmp))) {
		area[2] += area[0];
		area[3] += area[1];
		adjust_size = 1; // adjust size if there is a legend
	}
	update_maxarea(area, fig->title.ro_area); // don't adjust size for standalone titles

	int help[4];
	for (int iaxis=0; iaxis<fig->naxis; iaxis++)
		if (fig->axis[iaxis]) {
			get_axisarea(fig->axis[iaxis], help);
			update_maxarea(area, help);
		}

	if (fig->ro_internal->subfiguresize_ready) {
		int areasub[4];
		if (get_subfigures_area(fig, areasub))
			return adjust_size;
		adjust_size = 1; // adjust size according to the subfigures

		if (!(area[2] || area[3])) {
			memcpy(area, areasub, sizeof(areasub));
			return adjust_size;
		}

		if (areasub[2] || areasub[3])
			update_maxarea(area, areasub);
	}

	return adjust_size;
}

static void get_ticklabel_parallel_area(struct ttra *ttra, struct kahto_ticks *tk, int ipar, int *edges_figpx) {
	int nlabels = tk->tickerdata.common.nticks, area[4];
	char labelbuff[128];
	char *label = labelbuff;
	double min = tk->axis->min;
	double range = tk->axis->max - min;
	int textloc[2] = {0};
	set_fontheight(tk->axis->figure, tk->rowheight);

	int *axmargin = tk->axis->ro_margin_minmax;
	int startlen[] = {
		tk->axis->figure->ro_inner_xywh[ipar+0],
		tk->axis->figure->ro_inner_xywh[ipar+2],
	};
	startlen[0] += axmargin[0];
	startlen[1] -= axmargin[0] + axmargin[1];

	if (!tk->visible_labels) {
		double dataval = tk->get_tick(tk, 0, &label, 128);
		edges_figpx[0] = (dataval - min) / range * startlen[1] + startlen[0];
		dataval = tk->get_tick(tk, nlabels-1, &label, 128);
		edges_figpx[1] = (dataval - min) / range * startlen[1] + startlen[0];
		return;
	}

	for (int i=0; i<nlabels; i++) {
		double dataval = tk->get_tick(tk, i, &label, 128);
		double relval = (dataval - min) / range;
		if (ipar)
			relval = 1 - relval;
		textloc[ipar] = iround(relval*startlen[1]) + startlen[0];
		put_text(ttra, label, textloc[0], textloc[1], tk->xyalign_text[0], tk->xyalign_text[1], tk->rotation_grad, area, 1);
		update_min(edges_figpx[0], area[ipar]);
		update_max(edges_figpx[1], area[ipar+2]);
	}
}

static int axis_set_parallel_sizes(struct kahto_axis *axis, int firsttime) {
	if (my_isnan(axis->min) || my_isnan(axis->max))
		return 0;
	const int *xywh = axis->figure->ro_inner_xywh;
	int ipar = axis->direction == 'y';
	axis->ro_area[ipar] = xywh[ipar];
	axis->ro_area[ipar+2] = xywh[ipar] + xywh[ipar+2];
	axis->ro_minmaxpos[0] = axis->ro_area[ipar] + axis->ro_margin_minmax[0];
	axis->ro_minmaxpos[1] = axis->ro_area[ipar+2] - 1 - axis->ro_margin_minmax[1];
	if (axis->ro_minmaxpos[0] >= axis->ro_minmaxpos[1])
		return 1;

	axis->ro_pix_per_unit = (axis->ro_minmaxpos[1] - axis->ro_minmaxpos[0]) / (axis->max - axis->min);

	/* text->ro_area[parallel] already contains the limits around the zero point
	   or further has been moved to correct place */
	if (firsttime)
		for (int i=0; i<axis->ntexts; i++) {
			int move = axis->ro_area[ipar] + iround(xywh[ipar+2] * axis->text[i]->pos);
			axis->text[i]->ro_area[ipar+0] += move;
			axis->text[i]->ro_area[ipar+2] += move;
		}

	struct kahto_figure *fig = axis->figure;
	struct kahto_ticks *tk = axis->ticks;
	if (!axis->visible || !tk || !tk->visible)
		return 0;
	int edges_figpx[] = {xywh[2+ipar], -xywh[2+ipar]};
	get_ticklabel_parallel_area(fig->ttra, tk, ipar, edges_figpx);
	tk->ro_labelarea[ipar+0] = edges_figpx[0];
	tk->ro_labelarea[ipar+2] = edges_figpx[1];
	return 0;
}

static void get_parallel_limits(struct kahto_axis *axis, int *limits) {
	int iort = axis->direction == 'x';
	int ipar = !iort;
	struct kahto_figure *fig = axis->figure;
	int side = axis->pos >= 0.5;
	if (!axis->ticks || !axis->ticks->visible) {
		limits[0] = limits[1] = 0;
		return;
	}
	const int *area = axis->ticks->ro_labelarea;

	if (axis->direction == 'y')
		update_max(limits[0], fig->title.ro_area[3]);

	for (int iaxis=0; iaxis<fig->naxis; iaxis++) {
		struct kahto_axis *ax1 = fig->axis[iaxis];
		if (ax1->direction == axis->direction)
			continue;
		struct kahto_ticks *tk1 = ax1->ticks;
		if (!tk1)
			continue;
		if (side) {
			if (tk1->ro_labelarea[iort+side*2] <= area[iort]) // not seen by axis
				continue;
		}
		else if (tk1->ro_labelarea[iort] >= area[iort+2]) // not seen by axis
			continue;
		if (ax1->pos < 0.5) {
			int val = tk1->ro_labelarea[ipar+2];
			update_max(limits[0], val);
		}
		else {
			int val = tk1->ro_labelarea[ipar];
			update_min(limits[1], val);
		}
	}
}

void limits_to_conflicts(struct kahto_axis *axis, int *limits) {
	/* Limits was the pixels in which the room ends (exclusive?).
	   This converts it to conflict in pixels units. */
	if (!axis->ticks) {
		limits[0] = limits[1] = 0;
		return;
	}
	int a, ipar = axis->direction=='y';
	a = limits[0] - axis->ticks->ro_labelarea[ipar];
	limits[0] = a < 0 ? 0 : a;
	a = axis->ticks->ro_labelarea[ipar+2] - limits[1];
	limits[1] = a < 0 ? 0 : a;
}

struct layout_ort_args {
	int *imargin_xyxy, iort, iouter, iinner, iside;
	struct kahto_figure *fig;
};

#define unpack_args(a)\
	int __attribute__((unused)) *imargin_xyxy = a->imargin_xyxy, \
	iort = a->iort, \
	iouter = a->iouter, \
	iinner = a->iinner, \
	iside = a->iside

static void _axis_line_orthogonal(struct kahto_axis *axis, struct layout_ort_args *args) {
	if (!axis->visible)
		return;
	float fw;
	if (!axis->po[1])
		fw = axis->linestyle.thickness; // regular axis
	else
		fw = axis->po[1]; // coloraxis

	unpack_args(args);
	int iw = topixels(fw, args->fig);
	if (iside) {
		axis->ro_area[iouter] = axis->figure->wh[iort] - imargin_xyxy[iouter];
		axis->ro_area[iinner] = axis->ro_area[iouter] - iw;
	}
	else {
		axis->ro_area[iouter] = imargin_xyxy[iouter];
		axis->ro_area[iinner] = axis->ro_area[iouter] + iw;
	}
	imargin_xyxy[iouter] += iw;
}

static void _axis_tick_lines_orthogonal(struct kahto_axis *axis, struct layout_ort_args *args) {
	struct kahto_ticks *tk = axis->ticks;
	if (!tk || !tk->visible || !tk->init)
		return;
	unpack_args(args);
	if (tk->length1 > tk->length)
		tk->length1 = tk->length;
	int length = topixels(tk->length, args->fig);
	int length1 = topixels(tk->length1, args->fig);
	if (iside) {
		tk->ro_lines[iside] = axis->figure->wh[iort] - imargin_xyxy[iouter];
		tk->ro_lines1[!iside] = tk->ro_lines[!iside] = tk->ro_lines[iside] - length;
		tk->ro_lines1[iside] = tk->ro_lines1[!iside] + length1;
	}
	else {
		tk->ro_lines[iside] = imargin_xyxy[iouter];
		tk->ro_lines1[iside] = tk->ro_lines[iside] + length-length1;
		tk->ro_lines[!iside] = tk->ro_lines1[!iside] = tk->ro_lines[iside] + length;
	}
	imargin_xyxy[iouter] += length;
}

static void _axis_ticklabels_orthogonal(struct kahto_axis *axis, struct layout_ort_args *args) {
	struct kahto_ticks *tk = axis->ticks;
	if (!tk || !tk->visible || !tk->visible_labels)
		return;
	unpack_args(args);
	int nlabels = tk->tickerdata.common.nticks,
		area[4], max01[2] = {0};
	char labelbuff[128];
	char *label = labelbuff;
	set_fontheight(args->fig, tk->rowheight);
	for (int i=0; i<nlabels; i++) {
		tk->get_tick(tk, i, &label, 128);
		put_text(axis->figure->ttra, label, 0, 0, tk->xyalign_text[0], tk->xyalign_text[1], tk->rotation_grad, area, 1);
		if (-area[iort] > max01[0])
			max01[0] = -area[iort];
		if (area[iort+2] > max01[1])
			max01[1] = area[iort+2];
	}
	int reserved = max01[iside];
	int length = max01[1] + max01[0]; // positive is always away from the baseline
	if (iside) {
		tk->ro_labelarea[iouter] = axis->figure->wh[iort] - imargin_xyxy[iouter];
		tk->ro_labelarea[iinner] = tk->ro_labelarea[iouter] - length;
	}
	else {
		tk->ro_labelarea[iouter] = imargin_xyxy[iouter];
		tk->ro_labelarea[iinner] = tk->ro_labelarea[iouter] + length;
	}
	imargin_xyxy[iouter] += reserved;
}

static void _axis_texts_orthogonal(struct kahto_axis *axis, struct layout_ort_args *args) {
	int imaxtext = 0;
	unpack_args(args);
	int sizes[axis->ntexts];
	for (int itext=0; itext<axis->ntexts; itext++) {
		if (!axis->text[itext])
			continue;
		struct kahto_axistext *axistext = axis->text[itext];
		set_fontheight(args->fig, axistext->rowheight);
		int area[4];
		put_text(axis->figure->ttra, axistext->text.c, 0, 0, axistext->hvalign[!iort], axistext->hvalign[iort], axistext->rotation_grad, area, 1);
		sizes[itext] = iside ? area[iort+2] : -area[iort];
		update_max(imaxtext, sizes[itext]);
		/* save the parallel size to avoid calling put_text again */
		axistext->ro_area[!iort] = area[!iort];
		axistext->ro_area[!iort+2] = area[!iort+2];
	}
	if (iside) {
		int centerpos = axis->figure->wh[iort] - imargin_xyxy[iouter] - imaxtext;
		for (int itext=0; itext<axis->ntexts; itext++)
			if (axis->text[itext]) {
				axis->text[itext]->ro_area[iinner] = centerpos;
				axis->text[itext]->ro_area[iouter] = centerpos + sizes[itext];
			}
	}
	else {
		int centerpos = imargin_xyxy[iouter] + imaxtext;
		for (int itext=0; itext<axis->ntexts; itext++)
			if (axis->text[itext]) {
				axis->text[itext]->ro_area[iinner] = centerpos;
				axis->text[itext]->ro_area[iouter] = centerpos - sizes[itext];
			}
	}
	imargin_xyxy[iouter] += imaxtext;
}

#undef unpack_args

void kahto_axis_get_orthogonal(struct kahto_axis *axis, int *imargin_xyxy) {
	if (axis->direction < 0)
		return;
	int isx = axis->direction == 'x';
	int iort = isx;

	struct layout_ort_args args = {
		.imargin_xyxy = imargin_xyxy,
		.iort = iort,
		.iouter = iort + 2*(axis->pos >= 0.5),
		.iinner = iort + 2*(axis->pos < 0.5),
		.iside = axis->pos >= 0.5,
		.fig = axis->figure,
	};

	/* From outside in. These change imargin_xyxy and the object.*/
	_axis_texts_orthogonal(axis, &args);
	_axis_ticklabels_orthogonal(axis, &args);
	_axis_tick_lines_orthogonal(axis, &args);
	_axis_line_orthogonal(axis, &args);
}

/* add room for markers whose value is in the axis area but which are clipped partially */
static int room_for_markers_on_edge(struct kahto_figure *fig) {
	for (int i=0; i<fig->ngraph; i++) {
		struct kahto_graph *graph = fig->graph[i];
		int yxyx[4];
		if (graph->draw_marker_fun) {
			struct kahto_draw_data_args args = {.yxyx_oversize_out=yxyx, .fig=fig, .graph=graph};
			graph->draw_marker_fun(&args); // fills yxyx_oversize_out
		}
		else {
			int isize = 0;
			if (graph->linestyle.style != kahto_line_none_e)
				isize = topixels(graph->linestyle.thickness, fig);
			if (kahto_visible_marker(graph->markerstyle.marker)) {
				int a = topixels_marker(graph);
				update_max(isize, a);
			}
			if (isize <= 0)
				continue;
			yxyx[0] = yxyx[1] = yxyx[2] = yxyx[3] = isize/2;
		}
		for (int iaxis=0; iaxis<2; iaxis++) {
			struct kahto_axis *axis = graph->yxaxis[iaxis];
			if (!axis)
				continue;
			struct kahto_data *data = graph->data.arr[iaxis];
			double axisrange = axis->max - axis->min;
			if (my_isnan(axisrange))
				continue;
			int axislen = fig->ro_inner_xywh[2+(axis->direction=='y')];
			/* This was derived using pen and paper. Reading this code might be challenging. */
			float s0 = (max(axis->min, data->minmax[0]) - axis->min) / axisrange;
			float s1 = (min(axis->max, data->minmax[1]) - axis->min) / axisrange;
			float innerfraction[2];
			for (int iside=0; iside<2; iside++) {
				float size = (float)yxyx[iaxis+iside*2] / axislen;
				innerfraction[iside] = (1 - 2 * size) / (s1 - s0);
			}
			float m0_axis = (float)yxyx[iaxis] / axislen - innerfraction[0] * s0;
			float m1_axis = 1 - (m0_axis + innerfraction[1]);
			int backwards = axis->direction == 'y';
			char change = 0;
			if (m0_axis > 0) {
				int m0 = iroundpos(m0_axis * axislen);
				if (m0 > axis->ro_margin_minmax[backwards]) {
					axis->ro_margin_minmax[backwards] = m0;
					change = 1;
				}
			}
			if (m1_axis > 0) {
				int m1 = iroundpos(m1_axis * axislen);
				if (m1 > axis->ro_margin_minmax[!backwards]) {
					axis->ro_margin_minmax[!backwards] = m1;
					change = 1;
				}
			}
			if (change)
				if (axis_set_parallel_sizes(axis, 0))
					return 1;
		}
	}
	return 0;
}

static void adjust_addmargin(const int *area, int iort, int side, int *testarea, int *addmargin) {
	if (testarea[iort+side*2] <= area[iort+!side*2]) // not in line with the axis
		return;
	int ipar = !iort;
	if (testarea[ipar] < area[ipar])
		update_max(addmargin[0], testarea[ipar+2]-area[ipar]);
	else
		update_max(addmargin[1], area[ipar+2]-testarea[ipar]);
}

static int add_margin_based_on_texts(struct kahto_axis **axis_xyxy) {
	int ret = 0;
	for (int iaxis=0; iaxis<4; iaxis++) {
		int addmargin[2] = {0};
		struct kahto_axis *axis = axis_xyxy[iaxis];
		if (!axis)
			continue;
		struct kahto_figure *fig = axis->figure;
		struct kahto_ticks *tk = axis->ticks;
		if (!tk || !tk->visible || !tk->visible_labels)
			continue;

		const int *area = tk->ro_labelarea,
			  iort = axis->direction == 'x',
			  side = axis->pos >= 0.5;

		/* y-axis labels might overlap with the title */
		if (axis->direction == 'y' && fig->title.text && fig->title.text[0])
			adjust_addmargin(area, iort, side, fig->title.ro_area, addmargin);

		for (int iaxis=0; iaxis<fig->naxis; iaxis++) {
			struct kahto_axis *ax1 = fig->axis[iaxis];
			if (ax1->direction == axis->direction)
				continue;
			for (int i=0; i<ax1->ntexts; i++)
				adjust_addmargin(area, iort, side, ax1->text[i]->ro_area, addmargin);
		}

		if (addmargin[0] || addmargin[1]) {
			axis->ro_margin_minmax[0] += addmargin[0];
			axis->ro_margin_minmax[1] += addmargin[1];
			if (axis_set_parallel_sizes(axis, 0))
				return -1;
			ret = 1;
		}
	}
	return ret;
}

static int fit_to_figure(struct kahto_axis **axis_xyxy, int limits[4][2]) {
	int ret = 0;
	for (int iaxis=0; iaxis<4; iaxis++) {
		if (!axis_xyxy[iaxis])
			continue;
		struct kahto_ticks *tk = axis_xyxy[iaxis]->ticks;
		if (!tk || !tk->visible)
			continue;
		int add[] = {
			limits[iaxis][0] - tk->ro_labelarea[iaxis%2 + 0],
			tk->ro_labelarea[iaxis%2 + 2] - limits[iaxis][1],
		};
		char change = 0;
		for (int iside=0; iside<2; iside++)
			if (add[iside] > 0) {
				axis_xyxy[iaxis]->ro_margin_minmax[iside] += add[iside];
				change = 1;
			}
		if (change) {
			if (axis_set_parallel_sizes(axis_xyxy[iaxis], 0))
				return -1;
			ret = 1;
		}
	}
	return ret;
}

/* This will make people freak out. */
#define return return fig->ro_cannot_draw =

static int kahto_figure_layout(struct kahto_figure *fig, int imargin_xyxy[4]) {
	if (*(long*)fig->ro_wh0)
		memcpy(fig->wh, fig->ro_wh0, sizeof(fig->wh));
	else
		memcpy(fig->ro_wh0, fig->wh, sizeof(fig->wh));

everything_again_except_wh:
	for (int i=0; i<4; i++)
		imargin_xyxy[i] = topixels(fig->margin[i], fig);
	for (int i=0; i<fig->naxis; i++)
		memset(fig->axis[i]->ro_margin_minmax, 0, sizeof(fig->axis[i]->ro_margin_minmax));
	if (!fig->ttra) {
		struct kahto_figure *super = fig;
		while (super->super) {
			super = super->super;
			if (super->ttra) {
				fig->ttra = super->ttra;
				goto break0;
			}
		}
		kahto_figure_ttra_new(fig);
break0:
	}
	if (!fig->ttra->initialized)
		ttra_init(fig->ttra);
	if (fig->title.text) {
		set_fontheight(fig, fig->title.rowheight);
		put_text(fig->ttra, fig->title.text, fig->wh[0]*0.5, 0, -0.5, 0, fig->title.rotation_grad, fig->title.ro_area, 1);
		imargin_xyxy[1] += fig->title.ro_area[3];
	}

	kahto_make_range(fig);

	/* tick initialization */
	for (int iaxis=0; iaxis<fig->naxis; iaxis++) {
		struct kahto_axis *axis = fig->axis[iaxis];
		if (my_isnan(axis->min) || my_isnan(axis->max))
			continue;
		if (axis->ticks && axis->ticks->init)
			axis->ticks->init(axis->ticks, axis->min, axis->max);
	}

	/* orthogonal axis size */
	for (int outside=1; outside>=0; outside--)
		for (int iaxis=0; iaxis<fig->naxis; iaxis++) {
			struct kahto_axis *axis = fig->axis[iaxis];
			if (my_isnan(axis->min) || my_isnan(axis->max))
				continue;
			if (axis->pos == (int)axis->pos && axis->outside == outside)
				kahto_axis_get_orthogonal(axis, imargin_xyxy);
		}

	if (fig->wh[0] < imargin_xyxy[0]+imargin_xyxy[2] ||
		fig->wh[1] < imargin_xyxy[1]+imargin_xyxy[3])
		return 1;

	if (imargin_xyxy[0] < 0 || imargin_xyxy[1] < 0)
		return 1;

	fig->ro_inner_xywh[0] = imargin_xyxy[0];
	fig->ro_inner_xywh[1] = imargin_xyxy[1];
	fig->ro_inner_xywh[2] = fig->wh[0] - imargin_xyxy[2] - fig->ro_inner_xywh[0];
	fig->ro_inner_xywh[3] = fig->wh[1] - imargin_xyxy[3] - fig->ro_inner_xywh[1];
	const int *axis_xywh = fig->ro_inner_xywh;
	if (axis_xywh[2] < 0 || axis_xywh[3] < 0 || axis_xywh[0] > fig->wh[0] || axis_xywh[1] > fig->wh[1])
		return 1;

	struct kahto_axis *axis_xyxy[4] = {0};
	for (int i=0; i<fig->naxis; i++) {
		int ipos = fig->axis[i]->pos != 0;
		if (my_isnan(fig->axis[i]->min) || my_isnan(fig->axis[i]->max))
			continue;
		if (!fig->axis[i]->outside && ipos == fig->axis[i]->pos)
			axis_xyxy[(fig->axis[i]->direction=='y') + ipos*2] = fig->axis[i];
	}

	int imargin0[] = {
		topixels(fig->margin[0], fig),
		topixels(fig->margin[1], fig),
		topixels(fig->margin[2], fig),
		topixels(fig->margin[3], fig),
	};

	for (int i=0; i<fig->naxis; i++)
		if (axis_set_parallel_sizes(fig->axis[i], 1)) // may be needed in room_for_markers_on_edge
			return 1;

	if (room_for_markers_on_edge(fig))
		return 1;

	/* This loop adjusts fig->ro_inner_margin so that axes and ticklabels etc.
	   fit in the figure without overlapping. */
	for (int iloop=0; iloop<300; iloop++) { // while (1) but avoid halting when something goes wrong
		/*      ⁰⁰              ⁰¹
		 *      ¹⁰    0 (x0)    ³⁰
		 *        ┌────────────┐
		 * 1 (y0) │            │ 3 (y1)
		 *        └────────────┘
		 *      ²⁰    2 (x1)    ²¹
		 *      ¹¹              ³¹
		 *
		 * Limits[iaxis] (iaxis is the big number above)
		 * shows first, until which point there is room in the parallel direction.
		 * Then it is converted to show, how many pixels the labelarea conflicts with other stuff
		 * in the parallel directions.
		 */
		int limits[4][2] = {
			{imargin0[0], fig->wh[0]-imargin0[2]},
			{imargin0[1], fig->wh[1]-imargin0[3]},
			{imargin0[0], fig->wh[0]-imargin0[2]},
			{imargin0[1], fig->wh[1]-imargin0[3]},
		};

		/* Make sure everything fits to the figure. */
		switch (fit_to_figure(axis_xyxy, limits)) {
			case 0: break;
			case 1: continue;
			case -1: return 1;
		}

		/* Why not use limits here? Does this even have to be a separate function? */
		switch (add_margin_based_on_texts(axis_xyxy)) {
			case 0: break;
			case 1: continue;
			case -1: return 1;
		}

		for (int i=0; i<4; i++)
			if (axis_xyxy[i])
				get_parallel_limits(axis_xyxy[i], limits[i]);
			else
				memset(limits[i], 0, 2*sizeof(int));
		for (int i=0; i<4; i++)
			if (axis_xyxy[i])
				limits_to_conflicts(axis_xyxy[i], limits[i]);

		/* in conflicting corners increase only that margin which needs less adjustment */
		if (limits[0][0] > limits[1][0]) // top left
			limits[0][0] = 0;
		else limits[1][0] = 0;

		if (limits[0][1] > limits[3][0]) // top right
			limits[0][1] = 0;
		else limits[3][0] = 0;

		if (limits[2][0] > limits[1][1]) // bottom left
			limits[2][0] = 0;
		else limits[1][1] = 0;

		if (limits[2][1] > limits[3][1]) // bottom right
			limits[2][1] = 0;
		else limits[3][1] = 0;

		char done = 1;
		for (int i=0; i<4; i++)
			for (int ii=0; ii<2; ii++)
				if (limits[i][ii]) {
					done = 0;
					axis_xyxy[i]->ro_margin_minmax[ii] += limits[i][ii];
					if (axis_set_parallel_sizes(axis_xyxy[i], 0))
						return 1;
				}
		if (done)
			goto loop_done;

		/* if markertyle->size_in_[xy]axisunit, needed inner margin may have changed */
		if (room_for_markers_on_edge(fig))
			return 1;

	} // for iloop < maxloops
	fprintf(stderr, "Loop in %s reached maximum iterations.\n", __func__);
loop_done:

	for (int i=fig->ngraph-1; i>=0; i--)
		if (fig->graph[i]->equal_scale_xy) {
			struct kahto_axis **yxax = fig->graph[i]->yxaxis;
			int smaller = yxax[1]->ro_pix_per_unit < yxax[0]->ro_pix_per_unit;
			int newdiff = (yxax[!smaller]->max - yxax[!smaller]->min) * yxax[smaller]->ro_pix_per_unit;
			int olddiff = yxax[!smaller]->ro_minmaxpos[1] - yxax[!smaller]->ro_minmaxpos[0];
			if (newdiff < olddiff) {
				fig->wh[yxax[!smaller]->direction == 'y'] -= olddiff - newdiff;
				goto everything_again_except_wh; // would be better to adjust things here
			}
		}

	legend_placement(fig);
	texts_placement(fig);

	/* If the whole figure could not be used, it is made smaller. */
	// For normal figures (containing data), don't do the size check.
	// The main reason is that it is unnecessary.
	// I am also not sure that it would always work correctly.
	for (int i=fig->ngraph-1; i>=0; i--)
		if (fig->graph[i]->data.list.ydata->length)
			goto end;
	int area[4];
	if (!get_used_area(fig, area)) // returns false if size should not be changed
		goto end;
	int w = area[2] - area[0],
		h = area[3] - area[1];

	if (w < fig->wh[0])
		fig->wh[0] = w;
	if (h < fig->wh[1])
		fig->wh[1] = h;

end:
	return 0;
}

#undef return

static void align_axis_min(struct kahto_align *restrict a) {
	int pos = 0;
	for (int i=a->naxes-1; i>=0; i--)
		update_max(a->axes[i]->ro_minmaxpos[0], pos);
	for (int i=a->naxes-1; i>=0; i--)
		if (a->axes[i]->ro_minmaxpos[0] < pos) {
			a->axes[i]->ro_margin_minmax[0] += pos - a->axes[i]->ro_minmaxpos[0];
			axis_set_parallel_sizes(a->axes[i], 0);
		}
}

static void align_axis_max(struct kahto_align *restrict a) {
	int pos = 0;
	for (int i=a->naxes-1; i>=0; i--)
		update_min(a->axes[i]->ro_minmaxpos[1], pos);
	for (int i=a->naxes-1; i>=0; i--)
		if (a->axes[i]->ro_minmaxpos[1] > pos) {
			a->axes[i]->ro_margin_minmax[0] += a->axes[i]->ro_minmaxpos[0] - pos;
			axis_set_parallel_sizes(a->axes[i], 0);
		}
}

void kahto_layout(struct kahto_figure *fig) {
	int pxmargin_xyxy[4];
	/* first this figure because title and outside axes etc. affect subfigure sizes */
	fig->ro_internal->subfiguresize_ready = 0;
	if (kahto_figure_layout(fig, pxmargin_xyxy) && fig->fix_too_little_space) {
		fig->fix_too_little_space(fig);
		kahto_figure_layout(fig, pxmargin_xyxy);
	}
	kahto_xywh_to_subfigures(fig, pxmargin_xyxy);

	/* then subfigures */
	for (int i=0; i<fig->nsubfigures; i++)
		if ((fig->subfigures[i]))
			kahto_layout(fig->subfigures[i]);

	struct kahto_align *a = fig->ro_internal->align_min;
	while (a) {
		align_axis_min(a);
		a = a->next;
	}
	a = fig->ro_internal->align_max;
	while (a) {
		align_axis_max(a);
		a = a->next;
	}

	/* this figure again because subfigure sizes may change affecting this figure too */
	if (kahto_figure_layout(fig, pxmargin_xyxy) && fig->fix_too_little_space) {
		fig->fix_too_little_space(fig);
		kahto_figure_layout(fig, pxmargin_xyxy);
	}
}
