#ifndef CPLOT_NO_VERSION_CHECK
#define CPLOT_NO_VERSION_CHECK
#endif
#include "cplot.h"
#include <ttra.h>

void commit_legend(struct cplot_axes *axes, int axeswidth, int axesheight) {
    if (!axes->legend.visible)
	return;
    int height, width;
    cplot_get_legend_dims_px(axes, &height, &width, axesheight);
    axes->legend.ro_xywh[2] = width;
    axes->legend.ro_xywh[3] = height;
    axes->legend.ro_xywh[0] =
	axes->ro_inner_xywh[0] + axes->legend.posx * axes->ro_inner_xywh[2] +
	axes->legend.ro_xywh[2] * axes->legend.hvalign[0];
    axes->legend.ro_xywh[1] =
	axes->ro_inner_xywh[1] + axes->legend.posy * axes->ro_inner_xywh[3] +
	axes->legend.ro_xywh[3] * axes->legend.hvalign[1];
    do {
	if (!axes->legend.automatic_placement)
	    break;
	int iplace, jplace;
	cplot_find_empty_rectangle(axes, width, height, &iplace, &jplace);
	if (iplace < 0) {
	    axes->legend.ro_place_found = 0;
	    break;
	}
	axes->legend.ro_place_found = 1;
	axes->legend.ro_xywh[0] = iplace;
	axes->legend.ro_xywh[1] = jplace;
    } while (0);
}

static void cplot_ticks_set_parameters(struct cplot_ticks *ticks) {
    if (ticks->axis->pos >= 0.5) {
	if (ticks->hvalign_text[1] == cplot_automatic)
	    ticks->hvalign_text[1] = 0;
	if (ticks->crossaxis == cplot_automatic)
	    ticks->crossaxis = 0;
	if (ticks->ascending == cplot_automatic)
	    ticks->ascending = 1;
	if (ticks->crossaxis1 == cplot_automatic)
	    ticks->crossaxis1 = 0;
    }
    else {
	if (ticks->hvalign_text[1] == cplot_automatic)
	    ticks->hvalign_text[1] = -1;
	if (ticks->crossaxis == cplot_automatic)
	    ticks->crossaxis = -1;
	if (ticks->ascending == cplot_automatic)
	    ticks->ascending = 0;
	if (ticks->crossaxis1 == cplot_automatic)
	    ticks->crossaxis1 = -1;
    }
}

void cplot_ticks_commit(struct cplot_ticks *ticks, int axesheight, const int axis_xywh[4]) {
    struct ttra *ttra = NULL;
    if (ticks->have_labels) {
	ttra = ticks->axis->axes->ttra;
	ttra_set_fontheight(ttra, ticks->rowheight*axesheight);
    }

    int isx = ticks->axis->direction == 0;
    int line_px[4];
    line_px[isx] = ticks->axis->ro_line[isx] + ticks->crossaxis * ticks->length * axesheight; // thickness?
    line_px[isx+2] = ticks->axis->ro_line[isx] + (ticks->crossaxis+1) * ticks->length * axesheight;

    ticks->ro_lines1[0] = ticks->axis->ro_line[isx] + ticks->crossaxis1 * ticks->length1 * axesheight;
    ticks->ro_lines1[1] = ticks->axis->ro_line[isx] + (ticks->crossaxis1+1) * ticks->length1 * axesheight;

    ticks->ro_lines[0] = line_px[isx];
    ticks->ro_lines[1] = line_px[isx+2];
    int minmaxpos[2];
    memcpy(ticks->ro_labelarea, ticks->axis->ro_line, sizeof(ticks->ro_labelarea));
    int nticks = ticks->ticker.tickerdata.common.nticks;
    char tickbuff[500];
    char *tick = tickbuff;
    int side = ticks->axis->pos >= 0.5;

    /* Silmukan käyminen takaperin muuttaisi minmaxpos-muuttujaa. */
    for (int itick=0; itick<nticks; itick++) {
	double pos_data = ticks->ticker.get_tick(&ticks->ticker, itick, &tick, sizeof(tick));
	double pos_rel = (pos_data - ticks->axis->min) / (ticks->axis->max - ticks->axis->min);
	if (!isx)
	    pos_rel = 1 - pos_rel;
	line_px[!isx] = line_px[!isx+2] = minmaxpos[itick!=0] = axis_xywh[!isx] + iroundpos(pos_rel * axis_xywh[!isx+2]);
	int area_text[4] = {0};
	if (ttra && tick[0])
	    if (put_text(ttra, tick, line_px[side*2], line_px[1+side*2], ticks->hvalign_text[!isx],
		    ticks->hvalign_text[isx], ticks->rotation100, area_text, 1) >= 0) { // successful geometry
		update_min(ticks->ro_labelarea[0], area_text[0]);
		update_min(ticks->ro_labelarea[1], area_text[1]);
		update_max(ticks->ro_labelarea[2], area_text[2]);
		update_max(ticks->ro_labelarea[3], area_text[3]);
	    }
    }

    ticks->ro_tot_area[isx+0] = min(ticks->ro_labelarea[isx+0], ticks->ro_lines[0]);
    ticks->ro_tot_area[isx+2] = max(ticks->ro_labelarea[isx+2], ticks->ro_lines[1]);
    ticks->ro_tot_area[!isx+0] = min(ticks->ro_labelarea[!isx+0], minmaxpos[0]);
    ticks->ro_tot_area[!isx+2] = max(ticks->ro_labelarea[!isx+2], minmaxpos[1]);

    update_min(ticks->ro_tot_area[isx+0], ticks->ro_lines1[0]);
    update_max(ticks->ro_tot_area[isx+2], ticks->ro_lines1[1]);

    struct cplot_axis *a = ticks->axis;
    update_min(a->ro_tick_area[0], ticks->ro_tot_area[0]);
    update_min(a->ro_tick_area[1], ticks->ro_tot_area[1]);
    update_max(a->ro_tick_area[2], ticks->ro_tot_area[2]);
    update_max(a->ro_tick_area[3], ticks->ro_tot_area[3]);
}

/* axis_xywh on nyt tiedossa ja tikkien lopulliset alueet voidaan määrittää */
void get_ticklabel_limits_round3(struct cplot_axis *axis, const float overgoing[2], int axesheight, const int axis_xywh[4]) {
    cplot_f4si line = axis_get_line(axis);
    int side = axis->pos >= 0.5;
    int isx = axis->direction == 0;
    if (!axis->outside) {
	int tmp[] = {
	    axis_xywh[0] + line[0] * (axis_xywh[2]-1),
	    axis_xywh[1] + line[1] * (axis_xywh[3]-1),
	    axis_xywh[0] + line[2] * (axis_xywh[2]-1),
	    axis_xywh[1] + line[3] * (axis_xywh[3]-1),
	};
	memcpy(axis->ro_line, tmp, sizeof(tmp));
    }
    else {
	axis->ro_area[!isx] = axis->ro_line[!isx] = axis_xywh[!isx];
	axis->ro_area[!isx+2] = axis->ro_line[!isx] = axis_xywh[!isx] + axis_xywh[!isx+2];
    }

    axis->ro_tick_area[0] = axis->ro_tick_area[2] = axis->ro_line[0];
    axis->ro_tick_area[1] = axis->ro_tick_area[3] = axis->ro_line[1];
    int halfthickness = iroundpos(axis->linestyle.thickness/2 * axesheight);
    if (side) {
	axis->ro_tick_area[isx] += halfthickness;
	axis->ro_tick_area[isx+2] += halfthickness;
    }
    else {
	axis->ro_tick_area[isx] -= halfthickness;
	axis->ro_tick_area[isx+2] -= halfthickness;
    }

    if (axis->ticks)
	cplot_ticks_commit(axis->ticks, axesheight, axis_xywh);

    axis->ro_linetick_area[0] = min(axis->ro_line[0], axis->ro_tick_area[0]);
    axis->ro_linetick_area[1] = min(axis->ro_line[1], axis->ro_tick_area[1]);
    axis->ro_linetick_area[2] = max(axis->ro_line[2], axis->ro_tick_area[2]);
    axis->ro_linetick_area[3] = max(axis->ro_line[3], axis->ro_tick_area[3]);

    if (!axis->outside) {
	memcpy(axis->ro_area, axis->ro_line, sizeof(axis->ro_line));
	axis->ro_area[isx] -= iroundpos(axis->linestyle.thickness * axesheight);
	axis->ro_area[isx+2] += iroundpos(axis->linestyle.thickness * axesheight);
    }
}

/* This function may need to be called multiple times until nothing changes anymore. */
int cplot_axis_commit_parallel(struct cplot_axis *axis, float *out[2], int xywh[4]) {
    struct cplot_ticks *tk = axis->ticks;
    struct ttra *ttra = axis->axes->ttra;
    int isx = axis->direction == 0, change = 0;

    if (tk && tk->have_labels) {
	ttra_set_fontheight(ttra, iroundpos(tk->rowheight * axis->axes->wh[1]));
	int nlabels = tk->ticker.tickerdata.common.nticks,
	    area[4], max01[2] = {0}, pos_xy[2] = {0}, size=xywh[!isx+2];
	char labbuff[128];
	char *lab = labbuff;
	for (int ilab=0; ilab<nlabels; ilab++) {
	    double pos_data = tk->ticker.get_tick(&tk->ticker, ilab, &lab, sizeof(lab));
	    double pos_rel = (pos_data - axis->min) / (axis->max - axis->min);
	    if (!isx)
		pos_rel = 1 - pos_rel;
	    pos_xy[!isx] = iroundpos(pos_rel * xywh[!isx+2]);
	    put_text(ttra, lab, pos_xy[0], pos_xy[1], tk->hvalign_text[!isx], tk->hvalign_text[isx], tk->rotation100, area, 1);
	    if (-area[!isx] > max01[0])
		max01[0] = -area[isx];
	    if (area[!isx+2] - size > max01[1])
		max01[1] = area[!isx+2] - size;
	}
	float fmax01[] = {
	    (float)max01[0] / ttra->fontheight * tk->rowheight,
	    (float)max01[1] / ttra->fontheight * tk->rowheight,
	};
	if (*out[0] < fmax01[0]) change = 1, *out[0] = fmax01[0];
	if (*out[1] < fmax01[1]) change = 1, *out[1] = fmax01[1];
    }

    /* axistext? */

    return change;
}

/* out is in height units */
void cplot_axis_commit_orthogonal(struct cplot_axis *axis, float out_[2], int margin[4]) {
    if (axis->direction < 0)
	return;
    int isx = axis->direction == 0;
    int side = axis->pos >= 0.5;

    float *out = out_;
    float other_out[2];
    if (axis->outside)
	out = other_out;

    if (axis->linestyle.style != cplot_line_none_e) {
	out[0] = axis->linestyle.thickness / 2;
	out[1] = axis->linestyle.thickness / 2;
    }
    else
	out[0] = out[1] = 0;

    if (axis->outside) {
	int wh[2];
	wh[!isx] = iroundpos(axis->po[0] * axis->axes->wh[1]) - margin[!isx] - margin[!isx+2];
	wh[isx]  = iroundpos(axis->po[1] * axis->axes->wh[1]);

	int space = axis->axes->wh[!isx] - margin[!isx] - margin[!isx+2];
	axis->ro_area[!isx] = margin[!isx] + (space - wh[!isx]) / 2; // centralize in the parallel direction
	axis->ro_area[!isx+2] = axis->ro_area[!isx] + wh[!isx];

	axis->ro_area[isx] = axis->pos < 0.5 ? margin[isx] : axis->axes->wh[isx] - margin[isx+2] - wh[isx];
	axis->ro_area[isx+2] = axis->ro_area[isx] + wh[isx];

	margin[isx + (axis->pos >= 0.5) * 2] += wh[isx]; // if this is x-direction, it affects y-margin
    }

    struct cplot_ticks *tk = axis->ticks;
    if (tk && tk->ticker.init) {
	out[0] += get_ticks_underaxislength(tk);
	out[1] += get_ticks_overaxislength(tk);
    }

    struct ttra *ttra = axis->axes->ttra;
    ttra_set_fontheight(ttra, 50);

    if (tk->have_labels) {
	int nlabels = tk->ticker.tickerdata.common.nticks,
	    area[4], max01[2] = {0};
	char labbuff[128];
	char *lab = labbuff;
	for (int i=0; i<nlabels; i++) {
	    tk->ticker.get_tick(&tk->ticker, i, &lab, 128);
	    put_text(ttra, lab, 0, 0, tk->hvalign_text[!isx], tk->hvalign_text[isx], tk->rotation100, area, 1);
	    if (-area[isx] > max01[0])
		max01[0] = -area[isx];
	    if (area[isx+2] > max01[1])
		max01[1] = area[isx+2];
	}
	float fmax01[] = {
	    (float)max01[0] / ttra->fontheight * tk->rowheight,
	    (float)max01[1] / ttra->fontheight * tk->rowheight,
	};
	int asc = !!tk->ascending;
	out[asc] += fmax01[asc];
	float other = fmax01[!asc] - out[!asc];
	out[!asc] += other * (other > 0);
    }

    float max_negpos[2] = {0};
    for (int itext=0; itext<axis->ntext; itext++) {
	if (!axis->text[itext])
	    continue;
	struct cplot_axistext *axistext = axis->text[itext];
	int area[4];
	put_text(ttra, axistext->text, 0, 0, axistext->hvalign[!isx], axistext->hvalign[isx], axistext->rotation100, area, 1);
	float frac = (float)area[isx+2] / ttra->fontheight * axistext->rowheight;
	update_max(max_negpos[1], frac);
	frac = -(float)area[isx] / ttra->fontheight * axistext->rowheight;
	update_max(max_negpos[0], frac);
    }
    out[side] += max_negpos[side];
    float other = max_negpos[!side] - out[side];
    if (other > out[!side])
	out[!side] = other;

    if (axis->outside) {
	int addroom = iroundpos(out[axis->pos >= 0.5] * axis->axes->wh[1]);
	margin[isx + (axis->pos >= 0.5) * 2] += addroom;
	axis->ro_area[isx+0] -= addroom;
	axis->ro_area[isx+2] -= addroom;
	axis->ro_line[0] = axis->ro_area[!isx*2];
	axis->ro_line[1] = axis->ro_area[1 + isx*2];
	axis->ro_line[2] = axis->ro_area[2];
	axis->ro_line[3] = axis->ro_area[3];
    }
}

static void update_xywh(struct cplot_axes *axes, float overgoing[2][2][2], int margin[4]) {
    float ratio = (float)axes->wh[0] / axes->wh[1];
    int *xywh = axes->ro_inner_xywh;
    xywh[0] = iroundpos((overgoing[0][0][0] + overgoing[0][0][1]) * axes->wh[1]) + margin[0];
    xywh[1] = iroundpos((overgoing[1][0][0] + overgoing[1][0][1]) * axes->wh[1]) + margin[1];
    xywh[2] = iroundpos((ratio - overgoing[0][1][0] - overgoing[0][1][1]) * axes->wh[1]) - xywh[0] - margin[2];
    xywh[3] = iroundpos((1 - overgoing[1][1][0] - overgoing[1][1][1]) * axes->wh[1]) - xywh[1] - margin[3];
}

int cplot_axes_commit(struct cplot_axes *axes) {
    int margin[4] = {
	iroundpos(axes->wh[1] * axes->margin[0]),
	iroundpos(axes->wh[1] * axes->margin[1]),
	iroundpos(axes->wh[1] * axes->margin[2]),
	iroundpos(axes->wh[1] * axes->margin[3]),
    };
    if (!axes->ttra->text_initialized)
	ttra_init(axes->ttra);

    float overgoing[2/*xy*/][2/*side:-+*/][2/*direction:-+*/] = {0};

    for (int i=0; i<axes->naxis; i++) {
	struct cplot_axis *axis = axes->axis[i];
	if (axis->ticks)
	    cplot_ticks_set_parameters(axis->ticks);
	if (axis->range_isset != (cplot_minbit | cplot_maxbit))
	    axis_init_range(axis);
	if (axis->pos != (int)axis->pos)
	    continue;
	if (axis->ticks->ticker.init)
	    axis->ticks->ticker.init(&axis->ticks->ticker, axis->min, axis->max);
	cplot_axis_commit_orthogonal(axis, overgoing[axis->direction==0][axis->pos==1], margin);
    }
    update_xywh(axes, overgoing, margin);

    /* Adjust exteremes to prevent markers from cutting on the edges. */
    for (int i=0; i<axes->ndata; i++) {
	struct cplot_data *data = axes->data[i];
	for (int iaxis=0; iaxis<2; iaxis++) {
	    struct cplot_axis *axis = data->yxaxis[iaxis];
	    if (!axis)
		continue;
	    double sizedata = data->markersize * axes->wh[1] * (axis->max - axis->min) / axes->ro_inner_xywh[2];
	    if (!(axis->range_isset & cplot_minbit_const))
		update_min(axis->min, data->minmax[iaxis][0] - sizedata*0.5);
	    if (!(axis->range_isset & cplot_maxbit_const))
		update_max(axis->max, data->minmax[iaxis][1] + sizedata*0.5);
	}
    }

    for (int iloop=0; iloop<axes->naxis*20; iloop++) { // while (1) but prevents theoretical infinite loop
	for (int i=0; i<axes->naxis; i++) {
	    struct cplot_axis *axis = axes->axis[i];
	    int isx = axis->direction == 0;
	    float *ends[] = {&overgoing[!isx][0][0], &overgoing[!isx][1][1]};
	    if (cplot_axis_commit_parallel(axis, ends, axes->ro_inner_xywh))
		goto parallel_again;
	}
	break;
parallel_again:
	update_xywh(axes, overgoing, margin);
    }

    int *axis_xywh = axes->ro_inner_xywh;
    if (axis_xywh[2] <= 0 || axis_xywh[3] <= 0 || axis_xywh[0] >= axes->wh[0] || axis_xywh[1] >= axes->wh[1])
	return 1;

    for (int iaxis=0; iaxis<axes->naxis; iaxis++) {
	struct cplot_axis *axis = axes->axis[iaxis];
	get_ticklabel_limits_round3(axis, overgoing[axis->direction==0][axis->pos==1], axes->wh[1], axis_xywh);
    }

    commit_legend(axes, axes->wh[0], axes->wh[1]);
    return 0;
}
