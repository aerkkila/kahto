static void axis_update_range(struct $axis *axis) {
    int isx = axis->x_or_y == 'x';
    axis->min = DBL_MAX;
    axis->max = -DBL_MIN;
    for (int idata=0; idata<axis->axes->ndata; idata++) {
	struct $data *data = axis->axes->data[idata];
	if (data->yxaxis[isx] != axis)
	    continue;

	if (data->yxztype[isx] == cplot_notype)
	    switch (data->have_minmax[isx]) {
		case 0: data->minmax[isx][0] = 0;
			data->minmax[isx][1] = data->length-1;
			break;
		case maxbit: data->minmax[isx][0] = 0; break;
		case minbit: data->minmax[isx][1] = data->length-1; break;
		default: break;
	    }
	else
	    switch (data->have_minmax[isx]) {
		case 0:
		    get_minmax[data->yxztype[isx]](data->yxzdata[isx], data->length, data->minmax[isx]);
		    break;
		case maxbit:
		    data->minmax[isx][0] = get_min[data->yxztype[isx]](data->yxzdata[isx], data->length);
		    break;
		case minbit:
		    data->minmax[isx][1] = get_max[data->yxztype[isx]](data->yxzdata[isx], data->length);
		    break;
		default: break;
	    }

	data->have_minmax[isx] = minbit | maxbit;
	if (!(axis->range_isset & minbit))
	    update_min(axis->min, data->minmax[isx][0]);
	if (!(axis->range_isset & maxbit))
	    update_max(axis->max, data->minmax[isx][1]);
    }
    axis->range_isset = minbit | maxbit;
}

static void get_axislabel_limits(struct $axis *axis, int axeswidth, int axesheight, float lim2inout[2]) {
    struct ttra *ttra = axis->axes->ttra;
    int coord = axis->x_or_y == 'x';
    float old[2];
    memcpy(old, lim2inout, sizeof(old));
    int side = axis->pos >= 0.5;
    for (int itext=0; itext<axis->ntext; itext++) {
	struct $axistext *axistext = axis->text[itext];
	int area[4];
	ttra_set_fontheight(ttra, axistext->rowheight * axesheight);
	put_text(ttra, axistext->text, 0, 0, axistext->hvalign[!coord], axistext->hvalign[coord], axistext->rotation100, area, 1);
	float frac = area[coord+side*2] / (float)axesheight * (-1 + side*2);
	update_max(lim2inout[side], old[side]+frac);
    }
}

void cplot_ticks_commit(struct $ticks *ticks, int axeswidth, int axesheight, const int axis_xywh[4]) {
    if (ticks->ttra)
	ttra_set_fontheight(ticks->ttra, ticks->rowheight*axesheight);

    int isx = ticks->axis->x_or_y == 'x';
    int line_px[4];
    line_px[isx] = axis_xywh[isx] +
	iroundpos(ticks->axis->pos * axis_xywh[isx+2] + ticks->crossaxis * ticks->length * axesheight);
    line_px[isx+2] = axis_xywh[isx] +
	iroundpos(ticks->axis->pos * axis_xywh[isx+2] + (ticks->crossaxis+1) * ticks->length * axesheight);

    ticks->ro_lines[0] = line_px[isx];
    ticks->ro_lines[1] = line_px[isx+2];
    int minmaxpos[2];
    memcpy(ticks->ro_labelarea, ticks->axis->ro_line, sizeof(ticks->ro_labelarea));
    int nticks = ticks->ticker.init(&ticks->ticker, ticks->axis->min, ticks->axis->max); // turhaan init aina uudestaan
    char tick[500];

    /* Silmukan käyminen takaperin muuttaisi minmaxpos-muuttujaa. */
    for (int itick=0; itick<nticks; itick++) {
	double pos_data = ticks->ticker.get_tick(&ticks->ticker, itick, tick, sizeof(tick));
	double pos_rel = (pos_data - ticks->axis->min) / (ticks->axis->max - ticks->axis->min);
	if (!isx)
	    pos_rel = 1 - pos_rel;
	line_px[!isx] = line_px[!isx+2] = minmaxpos[itick!=0] = axis_xywh[!isx] + iroundpos(pos_rel * axis_xywh[!isx+2]);
	int area_text[4] = {0};
	if (ticks->ttra && tick[0])
	    if (put_text(ticks->ttra, tick, line_px[0], line_px[3], ticks->hvalign_text[!isx],
		    ticks->hvalign_text[isx], 0, area_text, 1) >= 0) { // successful geometry
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

    struct $axis *a = ticks->axis;
    update_min(a->ro_tick_area[0], ticks->ro_tot_area[0]);
    update_min(a->ro_tick_area[1], ticks->ro_tot_area[1]);
    update_max(a->ro_tick_area[2], ticks->ro_tot_area[2]);
    update_max(a->ro_tick_area[3], ticks->ro_tot_area[3]);
}

/* Akselia kohtisuoraan */
static void get_ticklabel_limits_round1(struct $axis *axis, int axeswidth, int axesheight, float lim2out[2]) {
    memset(lim2out, 0, 2*sizeof(float));
    float min = axis->min,
	  max = axis->max;
    for (int i=0; i<axis->nticks; i++) {
	struct $ticks *tk = axis->ticks[i];
	float lim2[] = {get_ticks_underlength(tk), get_ticks_overlength(tk)};
	if (!tk->ticker.init)
	    goto endloop;

	int nlabels = tk->ticker.init(&tk->ticker, min, max);
	char out[128];
	int maxcols=0, maxrows=0;
	int width, height;
	for (int i=0; i<nlabels; i++) {
	    tk->ticker.get_tick(&tk->ticker, i, out, 128);
	    ttra_get_textdims_chars(out, &width, &height);
	    if (height > maxrows)
		maxrows = height;
	    if (width > maxcols)
		maxcols = width;
	}

	/* voinee siirtää silmukan ulkopuolelle */
	switch (axis->x_or_y) {
	    case 'x':
		lim2[tk->ascending] += (float)maxrows * tk->ttra->fontheight / axesheight; // alignment?
		break;
	    case 'y':
		lim2[tk->ascending] += (float)maxcols * tk->ttra->fontwidth / axesheight;
		break;
	}

endloop:
	lim2out[0] = max(lim2[0], lim2out[0]);
	lim2out[1] = max(lim2[1], lim2out[1]);
    }
    get_axislabel_limits(axis, axeswidth, axesheight, lim2out);
}

/* Akselin suuntaan */
static void get_ticklabel_limits_round2(struct $axis *axis, int axeswidth, int axesheight, int axis_xywh[4]) {
    double axisdiff = axis->max - axis->min;
    for (int iticks=0; iticks<axis->nticks; iticks++) {
	struct $ticks *tk = axis->ticks[iticks];
	if (!tk->ticker.init)
	    continue;
	float min = axis->min,
	      max = axis->max;

	ttra_set_fontheight(tk->ttra, tk->rowheight * axesheight);
	int nlabels = tk->ticker.init(&tk->ticker, min, max);
	char out[128];
	int coord = axis->x_or_y == 'y';
	for (int i=0; i<nlabels; i++) {
	    int wh[2];
	    double pos_data = tk->ticker.get_tick(&tk->ticker, i, out, 128);
	    double pos_rel = (pos_data - axis->min) / axisdiff;
	    int position_px = axis_xywh[coord] + iroundpos(pos_rel * (axis_xywh[coord+2]-1));
	    ttra_get_textdims_pixels(tk->ttra, out, wh+0, wh+1);
	    /* lower side */
	    /* pyöräytystä ei ole huomioitu */
	    int edge = position_px + wh[coord] * tk->hvalign_text[0];
	    if (edge < 0) {
		axis_xywh[coord] += -edge;
		axis_xywh[coord+2] -= -edge;
		position_px = axis_xywh[coord] + iroundpos(pos_rel * (axis_xywh[coord+2]-1));
	    }
	    /* higher side */
	    edge = position_px + wh[coord] * (1 + tk->hvalign_text[0]);
	    int WH[] = {axeswidth, axesheight};
	    if (edge >= WH[coord])
		axis_xywh[coord+2] = (WH[coord] - axis_xywh[coord] - wh[coord] * (1 + tk->hvalign_text[0])) / pos_rel;
	}
    }
}

/* axis_xywh on nyt tiedossa ja tikkien lopulliset alueet voidaan määrittää */
void get_ticklabel_limits_round3(struct $axis *axis, int axeswidth, int axesheight, const int axis_xywh[4]) {
    $f4si line = axis_get_line(axis);
    {
	int tmp[] = {
	    axis_xywh[0] + line[0] * (axis_xywh[2]-1),
	    axis_xywh[1] + line[1] * (axis_xywh[3]-1),
	    axis_xywh[0] + line[2] * (axis_xywh[2]-1),
	    axis_xywh[1] + line[3] * (axis_xywh[3]-1),
	};
	memcpy(axis->ro_line, tmp, sizeof(tmp));
    }

    axis->ro_tick_area[0] = axis->ro_tick_area[2] = axis->ro_line[0];
    axis->ro_tick_area[1] = axis->ro_tick_area[3] = axis->ro_line[1];

    for (int i=0; i<axis->nticks; i++)
	cplot_ticks_commit(axis->ticks[i], axeswidth, axesheight, axis_xywh);

    axis->ro_linetick_area[0] = min(axis->ro_line[0], axis->ro_tick_area[0]);
    axis->ro_linetick_area[1] = min(axis->ro_line[1], axis->ro_tick_area[1]);
    axis->ro_linetick_area[2] = max(axis->ro_line[2], axis->ro_tick_area[2]);
    axis->ro_linetick_area[3] = max(axis->ro_line[3], axis->ro_tick_area[3]);
}

void cplot_axes_commit(struct $axes *axes, int axeswidth, int axesheight) {
    $f4si overgoing = {0};
    for (int iaxis=0; iaxis<axes->naxis; iaxis++) {
	if (axes->axis[iaxis]->range_isset != (minbit | maxbit))
	    axis_update_range(axes->axis[iaxis]);

	float over[2];
	get_ticklabel_limits_round1(axes->axis[iaxis], axeswidth, axesheight, over);
	int coord = axes->axis[iaxis]->x_or_y == 'x'; // ticks are orthogonal to this
	overgoing[coord+0]  = max(overgoing[coord+0], over[0]);
	overgoing[coord+2]  = max(overgoing[coord+2], over[1]);
    }

    overgoing *= (float)axesheight; // to pixels
    int *axis_xywh = axes->ro_inner_xywh;
    axis_xywh[0] = iceil(overgoing[0]);
    axis_xywh[1] = iceil(overgoing[1]);
    axis_xywh[2] = axeswidth - axis_xywh[0] - iceil(overgoing[2]);
    axis_xywh[3] = axesheight - axis_xywh[1] - iceil(overgoing[3]);

    for (int iaxis=0; iaxis<axes->naxis; iaxis++)
	get_ticklabel_limits_round2(axes->axis[iaxis], axeswidth, axesheight, axis_xywh);

    for (int iaxis=0; iaxis<axes->naxis; iaxis++)
	get_ticklabel_limits_round3(axes->axis[iaxis], axeswidth, axesheight, axis_xywh);
}
