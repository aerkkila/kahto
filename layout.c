#ifndef CPLOT_NO_VERSION_CHECK
#define CPLOT_NO_VERSION_CHECK
#endif
#include "cplot.h"
#include <ttra.h>

static void get_ticklabel_parallel_area(struct ttra *ttra, struct cplot_ticks *tk, int ipar, int *edges_figpx) {
	int nlabels = tk->tickerdata.common.nticks, area[4];
	char labelbuff[128];
	char *label = labelbuff;
	double min = tk->axis->min;
	double range = tk->axis->max - min;
	int textloc[2] = {0};
	ttra_set_fontheight(ttra, topixels(tk->rowheight, tk->axis->figure));

	int xywh[4], *inner_margin = tk->axis->figure->ro_inner_margin;
	memcpy(xywh, tk->axis->figure->ro_inner_xywh, sizeof(xywh));
	xywh[ipar] += inner_margin[ipar];
	xywh[ipar+2] -= inner_margin[ipar] + inner_margin[ipar+2];

	if (!tk->have_labels) {
		double dataval = tk->get_tick(tk, 0, &label, 128);
		edges_figpx[0] = (dataval - min) / range * xywh[ipar+2] + xywh[ipar];
		dataval = tk->get_tick(tk, nlabels-1, &label, 128);
		edges_figpx[1] = (dataval - min) / range * xywh[ipar+2] + xywh[ipar];
		return;
	}

	for (int i=0; i<nlabels; i++) {
		double dataval = tk->get_tick(tk, i, &label, 128);
		double relval = (dataval - min) / range;
		if (ipar)
			relval = 1 - relval;
		textloc[ipar] = iround(relval*xywh[ipar+2]) + xywh[ipar];
		put_text(ttra, label, textloc[0], textloc[1], tk->xyalign_text[0], tk->xyalign_text[1], tk->rotation_grad, area, 1);
		update_min(edges_figpx[0], area[ipar]);
		update_max(edges_figpx[1], area[ipar+2]);
	}
}

static void axis_set_parallel_sizes(struct cplot_axis *axis, int firsttime) {
	const int *xywh = axis->figure->ro_inner_xywh;
	int ipar = axis->direction == 1;
	axis->ro_line[ipar] = axis->ro_area[ipar] = xywh[ipar];
	axis->ro_line[ipar+2] = axis->ro_area[ipar+2] = xywh[ipar] + xywh[ipar+2];
	/* text->ro_area[parallel] already contains the limits around the zero point
	   or further has been moved to correct place */
	if (firsttime)
		for (int i=0; i<axis->ntexts; i++) {
			int move = axis->ro_area[ipar] + iround(xywh[ipar+2] * axis->text[i]->pos);
			axis->text[i]->ro_area[ipar+0] += move;
			axis->text[i]->ro_area[ipar+2] += move;
		}

	struct cplot_figure *fig = axis->figure;
	struct cplot_ticks *tk = axis->ticks;
	if (!tk || !tk->visible)
		return;
	int edges_figpx[] = {xywh[2+ipar], -xywh[2+ipar]};
	get_ticklabel_parallel_area(fig->ttra, tk, ipar, edges_figpx);
	tk->ro_labelarea[ipar+0] = edges_figpx[0];
	tk->ro_labelarea[ipar+2] = edges_figpx[1];
}

static void get_parallel_limits(struct cplot_axis *axis, int *limits) {
	int iort = axis->direction == 0;
	int ipar = !iort;
	struct cplot_figure *fig = axis->figure;
	int side = axis->pos >= 0.5;
	if (!axis->ticks || !axis->ticks->visible) {
		limits[0] = limits[1] = 0;
		return;
	}
	int *area = axis->ticks->ro_labelarea;

	if (ipar == 1)
		update_max(limits[0], fig->title.ro_area[3]);

	for (int iaxis=0; iaxis<fig->naxis; iaxis++) {
		struct cplot_axis *ax1 = fig->axis[iaxis];
		if (ax1->direction == axis->direction)
			continue;
		struct cplot_ticks *tk1 = ax1->ticks;
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

void limits_to_conflicts(struct cplot_axis *axis, int *limits) {
	/* Limits was the pixels in which the room ends (exclusive?).
	   This converts it to conflict in pixels units. */
	if (!axis->ticks) {
		limits[0] = limits[1] = 0;
		return;
	}
	int a, ipar = axis->direction==1;
	a = limits[0] - axis->ticks->ro_labelarea[ipar];
	limits[0] = a < 0 ? 0 : a;
	a = axis->ticks->ro_labelarea[ipar+2] - limits[1];
	limits[1] = a < 0 ? 0 : a;
}

struct layout_ort_args {
	int *imargin_xyxy, iort, iouter, iinner, iside;
	struct cplot_figure *fig;
};

#define unpack_args(a)\
	int __attribute__((unused)) *imargin_xyxy = a->imargin_xyxy, \
	iort = a->iort, \
	iouter = a->iouter, \
	iinner = a->iinner, \
	iside = a->iside

static void _axis_line_orthogonal(struct cplot_axis *axis, struct layout_ort_args *args) {
	float fw;
	if (axis->linestyle.style == cplot_line_none_e) {
		if (!axis->po[1])
			return;
		fw = axis->po[1]; // coloraxis
	}
	else
		fw = axis->linestyle.thickness; // regular axis

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
	axis->ro_line[iort] = axis->ro_line[iort+2] = axis->ro_area[iinner];
}

static void _axis_tick_lines_orthogonal(struct cplot_axis *axis, struct layout_ort_args *args) {
	struct cplot_ticks *tk = axis->ticks;
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

static void _axis_ticklabels_orthogonal(struct cplot_axis *axis, struct layout_ort_args *args, struct ttra *ttra) {
	struct cplot_ticks *tk = axis->ticks;
	if (!tk || !tk->visible || !tk->have_labels)
		return;
	unpack_args(args);
	int nlabels = tk->tickerdata.common.nticks,
		area[4], max01[2] = {0};
	char labelbuff[128];
	char *label = labelbuff;
	ttra_set_fontheight(ttra, topixels(tk->rowheight, args->fig));
	for (int i=0; i<nlabels; i++) {
		tk->get_tick(tk, i, &label, 128);
		put_text(ttra, label, 0, 0, tk->xyalign_text[0], tk->xyalign_text[1], tk->rotation_grad, area, 1);
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

static void _axis_texts_orthogonal(struct cplot_axis *axis, struct layout_ort_args *args, struct ttra *ttra) {
	int imaxtext = 0;
	unpack_args(args);
	int sizes[axis->ntexts];
	for (int itext=0; itext<axis->ntexts; itext++) {
		if (!axis->text[itext])
			continue;
		struct cplot_axistext *axistext = axis->text[itext];
		ttra_set_fontheight(ttra, topixels(axistext->rowheight, args->fig));
		int area[4];
		put_text(ttra, axistext->text, 0, 0, axistext->hvalign[!iort], axistext->hvalign[iort], axistext->rotation_grad, area, 1);
		sizes[itext] = iside ? area[iort+2] : -area[iort];
		update_max(imaxtext, sizes[itext]);
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

void cplot_axis_get_orthogonal(struct cplot_axis *axis, int *imargin_xyxy) {
	if (axis->direction < 0)
		return;
	int isx = axis->direction == 0;
	int iort = isx;
	struct ttra *ttra = axis->figure->ttra;
	ttra_set_fontheight(ttra, 30);

	struct layout_ort_args args = {
		.imargin_xyxy = imargin_xyxy,
		.iort = iort,
		.iouter = iort + 2*(axis->pos >= 0.5),
		.iinner = iort + 2*(axis->pos < 0.5),
		.iside = axis->pos >= 0.5,
		.fig = axis->figure,
	};

	/* From outside in. These change imargin_xyxy and the object.*/
	_axis_texts_orthogonal(axis, &args, ttra);
	_axis_ticklabels_orthogonal(axis, &args, ttra);
	_axis_tick_lines_orthogonal(axis, &args);
	_axis_line_orthogonal(axis, &args);
}

/* add room for markers whose value is in the axis area but which are clipped partially */
void cplot_make_inner_margin(struct cplot_figure *fig) {
	for (int i=0; i<fig->ndata; i++) {
		struct cplot_data *data = fig->data[i];
		float size_marker = data->markerstyle.size * !!cplot_visible_marker(data->markerstyle.marker);
		float size_line = data->linestyle.thickness * (data->linestyle.style != cplot_line_none_e);
		float size = max(size_marker, size_line);
		if (size <= 0)
			continue;
		for (int iaxis=0; iaxis<2; iaxis++) {
			struct cplot_axis *axis = data->yxaxis[iaxis];
			if (!axis)
				continue;
			double axisrange = axis->max - axis->min;
			int axislen = fig->ro_inner_xywh[2+axis->direction];
			/* This was derived using pen and paper. Reading this code might be challenging. */
			float s0 = (max(axis->min, data->minmax[iaxis][0]) - axis->min) / axisrange;
			float s1 = (min(axis->max, data->minmax[iaxis][1]) - axis->min) / axisrange;
			float size05_axis = topixels(size, fig)*0.5 / axislen;
			float innerfraction = (1 - 2 * size05_axis) / (s1 - s0);
			float m0_axis = size05_axis - innerfraction * s0;
			float m1_axis = 1 - (m0_axis + innerfraction);
			if (m0_axis > 0) {
				int m0 = iroundpos(m0_axis * axislen);
				update_max(fig->ro_inner_margin[axis->direction], m0);
			}
			if (m1_axis > 0) {
				int m1 = iroundpos(m1_axis * axislen);
				update_max(fig->ro_inner_margin[axis->direction+2], m1);
			}
		}
	}
}

static void adjust_moveaxis(int *area, int iort, int side, int *testarea, const int *limits, int *moveaxis) {
	if (side) {
		if (testarea[iort+side*2] <= area[iort]) // not in line with the axis
			return;
	}
	else if (testarea[iort] >= area[iort+2]) // not in line with the axis
		return;
	int ipar = !iort;
	if (testarea[ipar] < area[ipar])
		update_max(moveaxis[ipar], testarea[ipar+2]-area[ipar]);
	else
		update_max(moveaxis[ipar+2], area[ipar+2]-testarea[ipar]);
}

static void set_moveaxis_based_on_texts(struct cplot_axis *axis, const int *limits, int *moveaxis) {
	struct cplot_figure *fig = axis->figure;
	struct cplot_ticks *tk;
	if (!axis || !(tk = axis->ticks) || !tk || !tk->visible || !tk->have_labels)
		return;
	int *area = tk->ro_labelarea,
		iort = axis->direction == 0,
		ipar = axis->direction == 1,
		side = axis->pos >= 0.5;

	if (ipar == 1 && fig->title.text && fig->title.text[0])
		adjust_moveaxis(area, iort, side, fig->title.ro_area, limits, moveaxis);

	for (int iaxis=0; iaxis<fig->naxis; iaxis++) {
		struct cplot_axis *ax1 = fig->axis[iaxis];
		if (ax1->direction == axis->direction)
			continue;
		for (int i=0; i<ax1->ntexts; i++)
			adjust_moveaxis(area, iort, side, ax1->text[i]->ro_area, limits, moveaxis);
	}
}

static void fit_to_figure(struct cplot_axis **axis_xyxy, int limits[4][2], int *moveaxis) {
	for (int iaxis=0; iaxis<4; iaxis++) {
		if (!axis_xyxy[iaxis])
			continue;
		struct cplot_ticks *tk = axis_xyxy[iaxis]->ticks;
		if (!tk || !tk->visible)
			continue;
		int try = limits[iaxis][0] - tk->ro_labelarea[iaxis%2];
		update_max(moveaxis[iaxis%2 + 0*2], try);
		try = tk->ro_labelarea[iaxis%2 + 2] - limits[iaxis][1];
		update_max(moveaxis[iaxis%2 + 1*2], try);
	}
}

/* Close your eyes. I am going to do something scary. */
#define return return fig->ro_cannot_draw =

int cplot_figure_layout(struct cplot_figure *fig) {
	int imargin_xyxy[4];
	for (int i=0; i<4; i++)
		imargin_xyxy[i] = topixels(fig->margin[i], fig);
	memset(fig->ro_inner_margin, 0, sizeof(fig->ro_inner_margin));
	if (!fig->ttra->text_initialized)
		ttra_init(fig->ttra);
	if (fig->title.text) {
		ttra_set_fontheight(fig->ttra, topixels(fig->title.rowheight, fig));
		put_text(fig->ttra, fig->title.text, fig->wh[0]*0.5, 0, -0.5, 0.1, fig->title.rotation_grad, fig->title.ro_area, 1);
		imargin_xyxy[1] += fig->title.ro_area[3];
	}

	ttra_set_fontheight(fig->ttra, 30);
	cplot_make_range(fig);

	/* tick initialization */
	for (int iaxis=0; iaxis<fig->naxis; iaxis++) {
		struct cplot_axis *axis = fig->axis[iaxis];
		if (axis->ticks && axis->ticks->init)
			axis->ticks->init(axis->ticks, axis->min, axis->max);
	}

	/* orthogonal axis size */
	for (int outside=1; outside>=0; outside--)
		for (int iaxis=0; iaxis<fig->naxis; iaxis++) {
			struct cplot_axis *axis = fig->axis[iaxis];
			if (axis->pos == (int)axis->pos && axis->outside == outside)
				cplot_axis_get_orthogonal(axis, imargin_xyxy);
		}

	if (fig->wh[0] <= imargin_xyxy[0]+imargin_xyxy[2] ||
		fig->wh[1] <= imargin_xyxy[1]+imargin_xyxy[3])
		return 1;

	if (imargin_xyxy[0] < 0 || imargin_xyxy[1] < 0)
		return 1;

	fig->ro_inner_xywh[0] = imargin_xyxy[0];
	fig->ro_inner_xywh[1] = imargin_xyxy[1];
	fig->ro_inner_xywh[2] = fig->wh[0] - imargin_xyxy[2] - fig->ro_inner_xywh[0];
	fig->ro_inner_xywh[3] = fig->wh[1] - imargin_xyxy[3] - fig->ro_inner_xywh[1];
	const int *axis_xywh = fig->ro_inner_xywh;
	if (axis_xywh[2] <= 0 || axis_xywh[3] <= 0 || axis_xywh[0] >= fig->wh[0] || axis_xywh[1] >= fig->wh[1])
		return 1;

	struct cplot_axis *axis_xyxy[4] = {0};
	for (int i=0; i<fig->naxis; i++) {
		int ipos = fig->axis[i]->pos != 0;
		if (!fig->axis[i]->outside && ipos == fig->axis[i]->pos)
			axis_xyxy[!!fig->axis[i]->direction + ipos*2] = fig->axis[i];
	}

	int imargin0[] = {
		topixels(fig->margin[0], fig),
		topixels(fig->margin[1], fig),
		topixels(fig->margin[2], fig),
		topixels(fig->margin[3], fig),
	};

	cplot_make_inner_margin(fig);

	/* while (1) but avoid halting in certain situations */
	for (int iloop=0; iloop<30; iloop++) {
		/* parallel size */
		for (int i=0; i<fig->naxis; i++)
			axis_set_parallel_sizes(fig->axis[i], iloop);

		/*      ⁰⁰              ⁰¹
		 *      ¹⁰    0 (x0)    ³⁰
		 *        |¯¯¯¯¯¯¯¯¯¯¯¯|
		 * 1 (y0) |            | 3 (y1)
		 *        |____________|
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
		},
			moveaxis[4] = {0};
		/* Make sure everything fits into the figure. */
		fit_to_figure(axis_xyxy, limits, moveaxis);

		for (int i=0; i<4; i++)
			if (axis_xyxy[i])
				set_moveaxis_based_on_texts(axis_xyxy[i], limits[i], moveaxis);

		/* These make sure that two axis do not conflict. */
		for (int i=0; i<4; i++)
			if (axis_xyxy[i])
				get_parallel_limits(axis_xyxy[i], limits[i]);
			else
				memset(limits[i], 0, 2*sizeof(int));
		for (int i=0; i<4; i++)
			if (axis_xyxy[i])
				limits_to_conflicts(axis_xyxy[i], limits[i]);
		/* top left */
		if (limits[0][0] < limits[1][0])
			update_max(moveaxis[0], limits[0][0]);
		else update_max(moveaxis[1], limits[1][0]);
		/* top right */
		if (limits[0][1] < limits[3][0])
			update_max(moveaxis[0], limits[0][1]);
		else update_max(moveaxis[3], limits[3][0]);
		/* bottom left */
		if (limits[2][0] < limits[1][1])
			update_max(moveaxis[2], limits[2][0]);
		else update_max(moveaxis[1], limits[1][1]);
		/* bottom right */
		if (limits[2][1] < limits[3][1])
			update_max(moveaxis[2], limits[2][1]);
		else update_max(moveaxis[3], limits[3][1]);

		for (int i=0; i<4; i++)
			if (moveaxis[i]) {
				for (; i<4; i++)
					fig->ro_inner_margin[i] += moveaxis[i];
				goto next;
			}
		goto loop_done;
next:
		if (fig->ro_inner_margin[0] + fig->ro_inner_margin[2] >= axis_xywh[2] ||
			fig->ro_inner_margin[1] + fig->ro_inner_margin[3] >= axis_xywh[3])
			return 1;
	}
	fprintf(stderr, "Loop in %s reached maximum iterations.\n", __func__);
loop_done:
	legend_placement(fig);
	texts_placement(fig);
	return 0;
}

#undef return
