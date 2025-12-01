struct kahto_axis* kahto_axis_void_new(struct kahto_figure *figure) {
	struct kahto_axis *axis = calloc(1, sizeof(struct kahto_axis));
	axis->figure = figure;
	axis->center = 0.0 / 0.0;
	if (figure->mem_axis < figure->naxis+1) {
		figure->axis = realloc(figure->axis, (figure->mem_axis+=2) * sizeof(void*));
		memset(figure->axis + figure->naxis+1, 0, (figure->mem_axis - (figure->naxis+1)) * sizeof(void*));
	}
	figure->axis[figure->naxis++] = axis;
	return axis;
}

struct kahto_axis* kahto_axis_init(struct kahto_axis *axis, int x_or_y, float pos) {
	*axis = (struct kahto_axis) {
		.figure = axis->figure,
		.visible = 1,
		.linestyle.color = 0xff<<24,
		.linestyle.thickness = 1.0 / 400,
		.linestyle.style = 1,
		.linestyle.align = pos == 0 ? -1 : pos == 1 ? 1 : 0,
		.pos = pos,
		.min = 0,
		.max = 1,
		.direction = x_or_y != 'x',
		.center = 0.0 / 0.0,
	};
	axis->ticks = kahto_ticks_new(axis);
	return axis;
}

struct kahto_axis* kahto_axis_new(struct kahto_figure *figure, int x_or_y, float pos) {
	return kahto_axis_init(kahto_axis_void_new(figure), x_or_y, pos);
}

struct kahto_axis* kahto_coloraxis_init(struct kahto_axis *axis, int x_or_y) {
	*axis = (struct kahto_axis) {
		.figure = axis->figure,
		.visible = 1,
		.max = 1,
		.cmap = cmh_colormaps[default_colormap].map,
		.feature = kahto_color_e,
		.direction = x_or_y != 'x',
		.outside = 1,
		.pos = 1,
		.po[0] = 1,
		.po[1] = 1.0/30,
		.center = 0.0 / 0.0,
	};
	axis->ticks = kahto_ticks_new(axis);
	return axis;
}

struct kahto_axis* kahto_coloraxis_new(struct kahto_figure *figure, int x_or_y) {
	return kahto_coloraxis_init(kahto_axis_void_new(figure), x_or_y);
}

struct kahto_axis* kahto_featureaxis_new(struct kahto_figure *figure, int x_or_y, enum kahto_feature feature) {
	switch (feature) {
		case kahto_position_e: return kahto_axis_new(figure, x_or_y, 1);
		case kahto_color_e:    return kahto_coloraxis_new(figure, x_or_y);
		default: break;
	}
	struct kahto_axis *axis = kahto_axis_void_new(figure);
	axis->min = 0;
	axis->max = 1;
	axis->direction = x_or_y != 'x';
	axis->feature = feature;
	return axis;
}

struct kahto_ticks* kahto_ticks_new(struct kahto_axis *axis) {
	struct kahto_ticks *ticks = calloc(1, sizeof(struct kahto_ticks));
	int ipar = axis->direction == 1;
	ticks->axis = axis;
	ticks->color = 0xff<<24;
	ticks->init = kahto_init_ticker_default;
	ticks->length = 1.0 / 80;
	ticks->xyalign_text[ipar] = -0.5;
	ticks->xyalign_text[!ipar] = -1 * (axis->pos < 0.5);

	ticks->linestyle.thickness = 1.0 / 1200;
	ticks->linestyle.color = RGB(0, 0, 0);
	ticks->linestyle.style = kahto_line_normal_e;

	ticks->gridstyle.thickness = 1.0 / 1200;
	ticks->gridstyle.color = 0xffcccccc;

	ticks->rowheight = 1./35;
	ticks->visible = 1;
	ticks->have_labels = 1;

	ticks->linestyle1.style = kahto_line_normal_e;
	ticks->linestyle1.thickness = 1.0 / 1200;
	ticks->linestyle1.color = 0xff666666;
	ticks->length1 = 1.0 / 180;

	return ticks;
}

struct ttra* kahto_figure_ttra_new(struct kahto_figure *figure) {
	figure->ttra = calloc(1, sizeof(struct ttra));
	figure->ttra->fonttype = ttra_sans_e;
	figure->ttra->chop_lines = 1;
	figure->ttra_owner = 1;
	return figure->ttra;
}

struct kahto_figure* kahto_figure_init(struct kahto_figure *figure) {
	memset(figure, 0, sizeof(*figure));
	figure->background = -1;

	figure->wh[0] = kahto_default_width;
	figure->wh[1] = kahto_default_height;

	figure->topixels_reference = kahto_this_height;
	figure->colorscheme.colors = kahto_colorschemes[0];
	figure->title.rowheight = 0.05;
	figure->fontheightmul = 1;
	figure->fracsizemul = 1;

	figure->legend.rowheight = 1.0 / 35;
	figure->legend.symbolspace_per_rowheight = 1.25;
	figure->legend.posx = 0;
	figure->legend.posy = 1;
	figure->legend.hvalign[1] = -1;
	figure->legend.placement = kahto_placement_singlemaxdist;
	figure->legend.visible = 1;
	figure->legend.borderstyle.thickness = 1.0/1200;
	figure->legend.borderstyle.style = kahto_line_normal_e;
	figure->legend.borderstyle.color = 0xff<<24;
	figure->legend.fill = kahto_fill_bg_e;
	figure->legend.minscale = 0.6;
	figure->legend.scale = 1;

	return figure;
}

struct kahto_figure* kahto_figure_new() {
	return kahto_figure_init(malloc(sizeof(struct kahto_figure)));
}
