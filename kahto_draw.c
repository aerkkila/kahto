/* These are called after calling the correct layout functions, which decide where to draw the objects. */

void kahto_draw_ticks(struct kahto_ticks *ticks, unsigned *canvas, int figurewidth, int figureheight, int ystride) {
	char tickbuff[128];
	char *tick = tickbuff;
	struct ttra *ttra = NULL;
	struct kahto_figure *figure = ticks->axis->figure;

	if (ticks->have_labels) {
		ttra = figure->ttra;
		ttra->canvas = canvas;
		ttra->ystride = ystride;
		ttra->x1 = figurewidth;
		ttra->y1 = figureheight;
		ttra->fg_default = ticks->color;
		ttra->bg_default = -1;
		ttra_print(ttra, "\033[0m");
		set_fontheight(figure, ticks->rowheight);
		ttra->fg_default = 0xff<<24;
	}

	int iort = ticks->axis->direction == 0;
	int isx = iort;
	int line_px[4];
	line_px[iort+0] = ticks->ro_lines[0];
	line_px[iort+2] = ticks->ro_lines[1];
	int nticks = ticks->tickerdata.common.nticks;
	int gridline[4], xywh[4], *margin=ticks->axis->figure->ro_inner_margin, *xywh_out=ticks->axis->figure->ro_inner_xywh;
	memcpy(xywh, xywh_out, sizeof(xywh));
	xywh[0] += margin[0];
	xywh[1] += margin[1];
	xywh[2] -= margin[0] + margin[2];
	xywh[3] -= margin[1] + margin[3];
	gridline[iort] = xywh_out[iort];
	gridline[iort+2] = xywh_out[iort] + xywh_out[iort+2];
	int inner_area[] = xywh_to_area(xywh_out);
	int side = ticks->axis->pos >= 0.5;

	const double axisdatamin = ticks->axis->min;
	const double axisdatalen = ticks->axis->max - axisdatamin;
	int tot_area[] = {0, 0, ticks->axis->figure->wh[0], ticks->axis->figure->wh[1]};
	tot_area[!iort] = ticks->axis->ro_line[!iort];
	tot_area[!iort+2] = ticks->axis->ro_line[!iort+2];
	for (int itick=0; itick<nticks; itick++) {
		double pos_data = ticks->get_tick(ticks, itick, &tick, 128);
		double pos_rel = (pos_data - axisdatamin) / axisdatalen;
		if (!isx)
			pos_rel = 1 - pos_rel;
		line_px[!iort] = line_px[!iort+2] = xywh[!iort] + iround(pos_rel * xywh[!iort+2]);
		draw_line(canvas, ystride, line_px, tot_area, &ticks->linestyle, figure, NULL, 0);
		int area_text[4] = {0};
		if (ttra && tick[0])
			put_text(ttra, tick, line_px[side*2], line_px[1+side*2], ticks->xyalign_text[0], ticks->xyalign_text[1], ticks->rotation_grad, area_text, 0);
		if (ticks->gridstyle.style) {
			gridline[!iort] = gridline[!iort+2] = line_px[!iort];
			draw_line(canvas, ystride, gridline, inner_area, &ticks->gridstyle, figure, NULL, 0);
		}
	}

	if (ticks->get_maxn_subticks) {
		nticks = ticks->get_maxn_subticks(ticks);
		float posdata[nticks];
		nticks = ticks->get_subticks(ticks, posdata);
		line_px[iort+0] = ticks->ro_lines1[0];
		line_px[iort+2] = ticks->ro_lines1[1];
		for (int i=0; i<nticks; i++) {
			double pos_rel = (posdata[i] - axisdatamin) / axisdatalen;
			if (!isx)
				pos_rel = 1 - pos_rel;
			line_px[!iort] = line_px[!iort+2] = xywh[!iort] + iroundpos(pos_rel * xywh[!iort+2]);
			draw_line(canvas, ystride, line_px, tot_area, &ticks->linestyle1, figure, NULL, 0);
			if (ticks->gridstyle1.style) {
				gridline[!iort] = gridline[!iort+2] = line_px[!iort];
				draw_line(canvas, ystride, gridline, inner_area, &ticks->gridstyle1, figure, NULL, 0);
			}
		}
	}
}

void kahto_draw_axistext(struct kahto_axistext *axistext, unsigned *canvas, int figw, int figh, int ystride) {
	struct ttra *ttra = axistext->axis->figure->ttra;
	ttra->canvas = canvas;
	ttra->ystride = ystride;
	ttra->x1 = figw;
	ttra->y1 = figh;
	ttra->fg_default = axistext->axis->linestyle.color;
	ttra->bg_default = -1;
	ttra_print(ttra, "\033[0m");
	set_fontheight(axistext->axis->figure, axistext->rowheight);
	int *area = axistext->ro_area;
	put_text(ttra, axistext->text, area[0], area[1], 0, 0, axistext->rotation_grad, area, 0);
	ttra->fg_default = 0xff<<24;
}

void kahto_draw_axis(struct kahto_axis *axis, unsigned *canvas, int figurewidth, int figureheight, int ystride) {
	if (!axis || axis->direction < 0 || !axis->visible)
		return;
	int isx = axis->direction == 0;

	if (axis->cmap) {
		const unsigned char *cmap = axis->cmap;
		int len = axis->ro_area[2+!isx] - axis->ro_area[0+!isx];
		int len1 = axis->ro_area[2+isx] - axis->ro_area[0+isx];
		unsigned char levels[len];
		{
			unsigned short values[len];
			if (!isx) // y-axis goes from bottom to top
				for (int i=0; i<len; i++)
					values[i] = len-1-i;
			else
				for (int i=0; i<len; i++)
					values[i] = i;
			if (my_isnan(axis->center))
				get_datalevels_u2(0, len, values, levels, 0, len-1, 255, 1);
			else {
				double center_rel = axis->center / (axis->max - axis->min);
				double center = len * center_rel;
				get_datalevels_with_center_u2(0, len, values, levels, 0, center, len-1, 255, 1);
			}
		}
		if (axis->reverse_cmap)
			for (int i=0; i<len; i++)
				levels[i] = 255 - levels[i];
		uint32_t (*canvas1)[ystride] = (void*)(canvas + axis->ro_area[1]*ystride + axis->ro_area[0]);
		if (!isx)
			for (int j=0; j<len; j++) {
				unsigned color = from_cmap(cmap + levels[j] * 3);
				for (int i=0; i<len1; i++)
					canvas1[j][i] = color;
			}
		else {
			for (int i=0; i<len; i++)
				canvas1[0][i] = from_cmap(cmap + levels[i] * 3);
			for (int j=1; j<len1; j++)
				memcpy(canvas1[j], canvas1[0], sizeof(canvas1[0]));
		}
	}

	if (axis->linestyle.style != kahto_line_none_e) {
		typeof(axis->ro_area[0]) area[4];
		memcpy(area, axis->ro_area, sizeof(area));
		if (area[isx] < 0) area[isx] = 0;
		if (area[isx+2] > axis->figure->wh[isx])
			area[isx+2] = axis->figure->wh[isx];

		int WH[] = {figurewidth, figureheight};
		if (area[isx+2] > WH[isx]) area[isx+2] = WH[isx];

		if (axis->linestyle.style)
			kahto_fill_box(canvas, ystride, area, axis->linestyle.color);
	}

	if (axis->ticks)
		kahto_draw_ticks(axis->ticks, canvas, figurewidth, figureheight, ystride);

	for (int i=0; i<axis->ntexts; i++)
		kahto_draw_axistext(axis->text[i], canvas, figurewidth, figureheight, ystride);
}

void kahto_draw_figure(struct kahto_figure *figure, uint32_t *canvas, int ystride) {
	if (figure->background)
		kahto_fill_u4(canvas, figure->background, figure->wh[0], figure->wh[1], ystride);
	if (figure->ro_cannot_draw)
		return;
	if (!figure->ro_colors_set)
		kahto_set_colors(figure);

	for (int i=0; i<figure->naxis; i++)
		kahto_draw_axis(figure->axis[i], canvas, figure->wh[0], figure->wh[1], ystride);
	for (int i=0; i<figure->ngraph; i++)
		kahto_graph_render(figure->graph[i], canvas, ystride, figure, 0);
	kahto_legend_draw(figure, canvas, ystride);

	struct ttra *ttra = figure->ttra;
	ttra->canvas = canvas;
	ttra->ystride = ystride;
	ttra->x1 = figure->wh[0];
	ttra->y1 = figure->wh[1];
	ttra->fg_default = 0xff<<24;
	ttra->bg_default = -1;
	ttra_printf(ttra, "\e[0m");
	if (figure->title.text) {
		struct kahto_text *text = &figure->title;
		set_fontheight(figure, text->rowheight);
		put_text(ttra, text->text, text->ro_area[0], text->ro_area[1], 0, 0, text->rotation_grad, text->ro_area, 0);
	}
	for (int i=0; i<figure->ntexts; i++) {
		struct kahto_text *text = figure->texts+i;
		set_fontheight(figure, text->rowheight);
		put_text(ttra, text->text, text->ro_area[0], text->ro_area[1], 0, 0, text->rotation_grad, text->ro_area, 0);
	}

	if (figure->after_drawing)
		figure->after_drawing(figure, canvas, ystride);
	++figure->draw_counter;
}

void kahto_draw_figures(struct kahto_figure *fig, uint32_t *canvas, int ystride) {
	kahto_draw_figure(fig, canvas, ystride); // before subfigures to not cover them with background color
	struct kahto_figure *f;
	for (int i=0; i<fig->nsubfigures; i++)
		if ((f = fig->subfigures[i]))
			kahto_draw_figures(f, canvas + f->ro_corner[1]*ystride + f->ro_corner[0], ystride);
	if (fig->revert_fixes)
		fig->revert_fixes(fig);
}

void kahto_draw(struct kahto_figure *fig, uint32_t *canvas, int ystride) {
	kahto_layout(fig);
	kahto_draw_figures(fig, canvas, ystride);
}

/* This is embeded to draw_ticks, but animation may need a standalone function. */
void kahto_draw_grid(struct kahto_figure *figure, uint32_t *canvas, int ystride) {
	int naxis = figure->naxis;
	int *xywh = figure->ro_inner_xywh;
	int inner_area[] = xywh_to_area(xywh);
	for (int iaxis=0; iaxis<naxis; iaxis++) {
		struct kahto_axis *axis = figure->axis[iaxis];
		struct kahto_ticks *ticks = axis->ticks;
		if (!ticks || !ticks->gridstyle.style)
			continue;
		int nticks = ticks->tickerdata.common.nticks;
		int isx = axis->direction == 0;
		const double axisdatamin = axis->min;
		const double axisdatalen = axis->max - axisdatamin;
		int gridline[4];
		gridline[isx] = xywh[isx];
		gridline[isx+2] = xywh[isx] + xywh[isx+2];
		for (int itick=0; itick<nticks; itick++) {
			double pos_data = ticks->get_tick(ticks, itick, NULL, 0);
			double pos_rel = (pos_data - axisdatamin) / axisdatalen;
			if (!isx)
				pos_rel = 1 - pos_rel;
			gridline[!isx] = gridline[!isx+2] = xywh[!isx] + iroundpos(pos_rel * xywh[!isx+2]);
			draw_line(canvas, ystride, gridline, inner_area, &ticks->gridstyle, figure, NULL, 0);
		}
	}
}
