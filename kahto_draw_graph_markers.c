static void draw_datum_count(struct draw_data_args *ar) {
	unsigned (*count)[ar->axis_xywh_outer[2]] = (void*)ar->canvascount;
	int x = ar->yxz[1], y = ar->yxz[0];
	float alfa = ar->alpha;
	if (!ar->bmap) {
		if (0 <= x && x < ar->axis_xywh_outer[2] && 0 <= y && y < ar->axis_xywh_outer[3])
			count[y][x] += alfa;
		return;
	}
	int j0 = max(0, y-ar->maph/2);
	int j1 = min(ar->axis_xywh_outer[3]-1, y+(ar->maph-ar->maph/2));
	int i0 = max(0, x-ar->mapw/2);
	int i1 = min(ar->axis_xywh_outer[2]-1, x+(ar->mapw-ar->mapw/2));
	int bj0 = j0 - (y-ar->maph/2);
	int bi0 = i0 - (x-ar->mapw/2);
	const unsigned char (*bmap)[ar->mapw] = (void*)ar->bmap;
	alfa /= 255;
	for (int j=j0; j<j1; j++, bj0++)
		for (int i=i0, bi=bi0; i<i1; i++, bi++)
			count[j][i] += bmap[bj0][bi] * alfa;
}

static void draw_datum(struct draw_data_args *ar) {
	if (ar->canvascount)
		draw_datum_count(ar);
	int *yxz = ar->yxz;
	if (!ar->bmap) {
		if (0 <= yxz[1] && yxz[1] < ar->axis_xywh_outer[2] && 0 <= yxz[0] && yxz[0] < ar->axis_xywh_outer[3])
			ar->canvas[(ar->axis_xywh_outer[1] + yxz[0]) * ar->ystride + ar->axis_xywh_outer[0] + yxz[1]] = ar->color;
		/* alfaa ei ole toteutettu tähän */
		return;
	}
	int x0_ = ar->axis_xywh_outer[0],       y0_ = ar->axis_xywh_outer[1];
	int x1_ = x0_ + ar->axis_xywh_outer[2], y1_ = y0_ + ar->axis_xywh_outer[3];
	int x0  = x0_ + ar->yxz[1] - ar->mapw/2,      y0 = y0_ + yxz[0] - ar->maph/2;
	int j0  = max(0, y0_ - y0);
	int j1  = min(ar->maph, y1_ - y0);
	int i0  = max(0, x0_ - x0);
	int i1  = min(ar->mapw, x1_ - x0);
	uint32_t (*canvas)[ar->ystride] = (void*)ar->canvas;
	const unsigned char (*bmap)[ar->mapw] = (void*)ar->bmap;
	int alfa = ar->alpha;
	for (int j=j0, y=y0+j0; j<j1; j++, y++)
		for (int i=i0; i<i1; i++)
			tocanvas(canvas[y]+x0+i, bmap[j][i]*alfa/255, ar->color);
}

static void draw_data_xya(struct draw_data_args *restrict ar) {
	ar->alpha = ar->yxz[2];
	draw_datum(ar);
}

static void draw_data_xyc_rev(struct draw_data_args *restrict ar) {
	ar->color = from_cmap(ar->cmap + (255-ar->yxz[2])*3);
	draw_datum(ar);
}

static void draw_data_xyc(struct draw_data_args *restrict ar) {
	ar->color = from_cmap(ar->cmap + ar->yxz[2]*3);
	draw_datum(ar);
}

static void draw_data_xyc_list(struct draw_data_args *restrict ar) {
	ar->color = ar->colors[ar->ipoint % ar->ncolors];
	draw_datum(ar);
}

static unsigned char* kahto_data_marker_bmap
(struct kahto_graph *graph, unsigned char *bmap, int *has_marker, int *width, int *height) {
	if (!graph->markerstyle.marker) {
		*has_marker = 0;
		return NULL;
	}
	typeof(init_literal) *initfun = init_literal;
	*has_marker = 1;
	if (!graph->markerstyle.literal)
		switch (*graph->markerstyle.marker) {
			case ' ':
			case  0 : *has_marker = 0;
			case '.': return NULL;
			case '+': initfun = (void*)init_plus; break;
			case '^': initfun = (void*)init_triangle; break;
			case '*':
			case '4': initfun = (void*)init_4star; break;
			case 'o': initfun = (void*)init_circle; break;
			default: break;
		}

	bmap = initfun(bmap, width, height, graph);

	if (graph->markerstyle.nofill) {
		int W = *width, H = *height;
		int w = W * graph->markerstyle.nofill,
			h = *height * graph->markerstyle.nofill;
		int x0 = (W - w) / 2,
			y0 = (H - h) / 2;
		unsigned char *bmap2 = malloc(w * h);
		unsigned char *bmap1 = initfun(bmap2, &w, &h, graph);
		for (int j=0; j<h; j++)
			for (int i=0; i<w; i++)
				bmap[(j+y0)*W + i+x0] -= bmap1[j*w+i];
		if (bmap2 != bmap1)
			free(bmap2);
		free(bmap1);
	}
	return bmap;
}

void kahto_draw_graph_markers
(struct kahto_graph *graph, struct kahto_figure *fig, struct draw_data_args *args) {
	double yxmin[] = {
		graph->yxaxis[0]->min,
		graph->yxaxis[1]->min,
	};
	double yxdiff[] = {
		graph->yxaxis[0]->max - yxmin[0],
		graph->yxaxis[1]->max - yxmin[1],
	};
	const int *margin = fig->ro_inner_margin;
	int yxlen[] = {fig->ro_inner_xywh[3]-margin[1]-margin[3], fig->ro_inner_xywh[2]-margin[0]-margin[2]};
	struct kahto_axis *caxis = graph->yxaxis[2];

	int width, height, marker;
	width = height = topixels(graph->markerstyle.size, fig);
	unsigned char bmap_buff[width*height];
	args->bmap = kahto_data_marker_bmap(graph, bmap_buff, &marker, &width, &height);
	args->mapw = width;
	args->maph = height;
	/* args->bmap points to bmap_buff or is NULL or a malloced pointer */

	struct kahto_data
		*xdata = graph->data.list.xdata,
		*ydata = graph->data.list.ydata,
		*zdata = graph->data.list.zdata;

	if (graph->markerstyle.count)
		args->canvascount = calloc(fig->ro_inner_xywh[2] * fig->ro_inner_xywh[3], sizeof(unsigned));

	/* help for xdata */
	double xstep = xdata->length > 1 ? (xdata->minmax[1] - xdata->minmax[0]) / (xdata->length-1) : 0;
	double xpix_per_unit = yxlen[1] / yxdiff[1] * xstep;
	double x0data_axis = xdata->minmax[0] - yxmin[1];
	/* help for zdata */
	double caxislim[3] = {0/0.0, 0/0.0, 0/0.0};
	typeof(get_datalevel[0]) get_datalevel_fun = NULL;
	if (caxis && zdata) {
		caxislim[0] = caxis->min;
		caxislim[1] = caxis->center;
		caxislim[2] = caxis->max;
		get_datalevel_fun = my_isnan(caxislim[1]) ?
			get_datalevel[zdata->type] : get_datalevel_with_center[zdata->type];
	}

	void (*draw_data_fun)(struct draw_data_args*) = draw_datum;
	if (caxis) {
		if (caxis->feature == kahto_color_e)
			if (caxis->reverse_cmap)
				draw_data_fun = draw_data_xyc_rev;
			else
				draw_data_fun = draw_data_xyc;
		else if (caxis->feature == kahto_alpha_e)
			draw_data_fun = draw_data_xya;
	}
	else if (graph->colors)
		draw_data_fun = draw_data_xyc_list;

	long end = graph->data.list.ydata->length;
	for (int ipoint=0; ipoint<end; ipoint++) {
		int yxz[3];
		if (xdata->data)
			yxz[1] = get_datapx[xdata->type](xdata->data, ipoint*xdata->stride, yxmin[1], yxdiff[1], yxlen[1]);
		else
			yxz[1] = iroundpos((x0data_axis + ipoint*xstep) *  xpix_per_unit);
		yxz[1] += margin[0];
		if (get_datalevel_fun)
			yxz[2] = get_datalevel_fun(zdata->data, ipoint*zdata->stride, caxislim, 255);
		yxz[0] = get_datapx_inv[ydata->type](ydata->data, ipoint*ydata->stride, yxmin[0], yxdiff[0], yxlen[0])
			+ margin[1];

		args->yxz = yxz;
		args->ipoint = ipoint;
		draw_data_fun(args);
	}

	if (args->bmap != bmap_buff)
		free(args->bmap);

	if (!args->canvascount)
		return;

#if 0
	/* ainakin tästä eteenpäin on kesken */
	unsigned (*count)[xywh0[2]] = (void*)data_args.canvascount;
	float min, max;
	struct kahto_axis *countaxis = graph->countaxis;
	if (!countaxis || countaxis->range_isset != kahto_range_isset) {
		min = max = count[0][0];
		for (int j=0; j<xywh0[3]; j++)
			for (int i=0; i<xywh0[2]; i++)
				if (count[j][i] > max)
					max = count[j][i];
				else if (count[j][i] < min)
					min = count[j][i];
		min /= 255;
		max /= 255;
		if (countaxis) {
			countaxis->min = min;
			countaxis->max = max;
		}
		fprintf(stderr, "Warning: countaxis range must be given by user\n");
		fprintf(stderr, "current range is [%f, %f]\n", min, max);
	}
	else
		min = countaxis->min,
			max = countaxis->max;
	float range = max - min;

	const unsigned char *cmap = countaxis ? countaxis->cmap : cmh_colormaps[default_colormap].map;
	uint32_t (*canvasview)[ystride] = (void*)(canvas + xywh0[1]*ystride + xywh0[0]);
	for (int j=0; j<xywh0[3]; j++)
		for (int i=0; i<xywh0[2]; i++) {
			if (!count[j][i])
				continue;
			float level = (count[j][i]/255 - min) / range;
			if (level < 0)
				level = 0;
			else if (level > 1)
				level = 1;
			int ilevel = iroundpos(level * 255);
			if (count[j][i] < 255)
				tocanvas(&canvasview[j][i], count[j][i], from_cmap(cmap+ilevel*3));
			else
				canvasview[j][i] = from_cmap(cmap+ilevel*3);
		}
	free(data_args.canvascount);
#endif
}
