static void draw_datum_count(struct kahto_draw_data_args *ar) {
	unsigned (*count)[ar->xywh_limits[2]] = (void*)ar->canvascount;
	int x = ar->yxz[1], y = ar->yxz[0];
	float alfa = ar->alpha;
	if (!ar->bmap) {
		if (0 <= x && x < ar->xywh_limits[2] && 0 <= y && y < ar->xywh_limits[3])
			count[y][x] += alfa;
		return;
	}
	int j0 = max(0, y-ar->maph/2);
	int j1 = min(ar->xywh_limits[3]-1, y+(ar->maph-ar->maph/2));
	int i0 = max(0, x-ar->mapw/2);
	int i1 = min(ar->xywh_limits[2]-1, x+(ar->mapw-ar->mapw/2));
	int bj0 = j0 - (y-ar->maph/2);
	int bi0 = i0 - (x-ar->mapw/2);
	const unsigned char (*bmap)[ar->mapw] = (void*)ar->bmap;
	alfa /= 255;
	for (int j=j0; j<j1; j++, bj0++)
		for (int i=i0, bi=bi0; i<i1; i++, bi++)
			count[j][i] += bmap[bj0][bi] * alfa;
}

static void draw_datum(struct kahto_draw_data_args *ar) {
	if (ar->canvascount)
		draw_datum_count(ar);
	int *yxz = ar->yxz;
	if (!ar->bmap) {
		if (0 <= yxz[1] && yxz[1] < ar->xywh_limits[2] && 0 <= yxz[0] && yxz[0] < ar->xywh_limits[3])
			ar->canvas[(ar->xywh_limits[1] + yxz[0]) * ar->ystride + ar->xywh_limits[0] + yxz[1]] = ar->color;
		/* alfaa ei ole toteutettu tähän */
		return;
	}
	int x0_ = ar->xywh_limits[0],       y0_ = ar->xywh_limits[1];
	int x1_ = x0_ + ar->xywh_limits[2], y1_ = y0_ + ar->xywh_limits[3];
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

static void draw_data_xya(struct kahto_draw_data_args *restrict ar) {
	ar->alpha = ar->yxz[2];
	draw_datum(ar);
}

static void draw_data_xyc_rev(struct kahto_draw_data_args *restrict ar) {
	ar->color = from_cmap(ar->cmap + (255-ar->yxz[2])*3);
	draw_datum(ar);
}

static void draw_data_xyc(struct kahto_draw_data_args *restrict ar) {
	ar->color = from_cmap(ar->cmap + ar->yxz[2]*3);
	draw_datum(ar);
}

static void draw_data_xyc_list(struct kahto_draw_data_args *restrict ar) {
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
			case 'o': initfun = (void*)init_circle; break;
			case 'x': initfun = (void*)init_xmarker; break;
			case '+': initfun = (void*)init_plus; break;
			case '^': initfun = (void*)init_triangle; break;
			case 'v': initfun = (void*)init_triangle_down; break;
			case '*':
			case '4': initfun = (void*)init_4star; break;
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
(struct kahto_graph *graph, struct kahto_figure *fig, struct kahto_draw_data_args *args) {
	struct kahto_data
		*xdata = graph->data.list.xdata,
		*ydata = graph->data.list.ydata,
		*zdata = graph->data.list.zdata;

	int yxlogscale[] = {graph->yxaxis[0]->logscale, graph->yxaxis[1]->logscale};
	double yxmultiplier[] = {1, 1};
	typeof(get_datapx[0]) get_yxpx[] = {
		get_datapx_inv[ydata->type],
		get_datapx[xdata->type],
	};

	double yxmin[] = {
		graph->yxaxis[0]->min,
		graph->yxaxis[1]->min,
	};
	double yxdiff[] = {
		graph->yxaxis[0]->max - yxmin[0],
		graph->yxaxis[1]->max - yxmin[1],
	};

	for (int iyx=0; iyx<2; iyx++)
		if (yxlogscale[iyx]) {
			// convert from log_e to log_base
			yxmultiplier[iyx] = 1 / log(graph->yxaxis[iyx]->ticks->tickerdata.log.base);
			if (iyx == 0)
				get_yxpx[0] = get_datapx_log_inv[ydata->type];
			else
				get_yxpx[1] = get_datapx_log[xdata->type];
			yxmin[iyx] = log(yxmin[iyx]) * yxmultiplier[iyx];
			yxdiff[iyx] = log(graph->yxaxis[iyx]->max) * yxmultiplier[iyx] - yxmin[iyx];
		}

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

	void (*draw_data_fun)(struct kahto_draw_data_args*) = draw_datum;
	if (graph->draw_marker_fun)
		draw_data_fun = graph->draw_marker_fun;
	else if (caxis) {
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

	int xoffset = xdata->data ? 0 :
		get_datapx[kahto_f8](&graph->xoffset, 0, yxmin[1], yxdiff[1], yxlen[1], 1);

	int area[] = xywh_to_area(fig->ro_inner_xywh);
	struct kahto_data *edatalist[] = {
		graph->data.list.e0data,
		graph->data.list.e1data,
	};
	int yxz[3];
	args->yxz = yxz;
	long end = graph->data.list.ydata->length;
	int ysublen = ydata->sublength;
	int ystride = ydata->stride * (ysublen ? ysublen : 1);
	int *xzy_ = NULL;
	if (ysublen) {
		xzy_ = malloc((ydata->sublength + 2) * sizeof(xzy_[0]));
		args->yxz = xzy_;
		args->sublength = ysublen;
	}

	typeof(get_datapx[0]) get_epx[arrlen(edatalist)];
	for (int iedata=0; iedata<arrlen(edatalist); iedata++) {
		struct kahto_data *edata = edatalist[iedata];
		if (edata)
			get_epx[iedata] =
				yxlogscale[0] ? get_datapx_log_inv[edata->type] :
				get_datapx_inv[edata->type];
	}

	for (int ipoint=0; ipoint<end; ipoint++) {
		if (xdata->data) {
			yxz[1] = get_yxpx[1](xdata->data, ipoint*xdata->stride, yxmin[1], yxdiff[1], yxlen[1], yxmultiplier[1]);
			if (yxz[1] == NOT_A_PIXEL)
				continue;
		}
		else
			yxz[1] = xoffset + iroundpos((x0data_axis + ipoint*xstep) *  xpix_per_unit);
		yxz[1] += margin[0];
		if (get_datalevel_fun)
			yxz[2] = get_datalevel_fun(zdata->data, ipoint*zdata->stride, caxislim, 255);
		if (!ysublen) {
			yxz[0] = get_yxpx[0](ydata->data, ipoint*ystride, yxmin[0], yxdiff[0], yxlen[0], yxmultiplier[0]);
			if (yxz[0] == NOT_A_PIXEL)
				continue;
			yxz[0] += margin[1];
		}
		else {
			/* args->xywh_limits are embedded to coordinates in userfunctions */
			xzy_[0] = yxz[1] + args->xywh_limits[0];
			xzy_[1] = yxz[2];
			for (int iy=0; iy<ysublen; iy++)
				xzy_[2+iy] = get_yxpx[0](ydata->data, ipoint*ystride+iy, yxmin[0], yxdiff[0], yxlen[0], yxmultiplier[0])
					+ margin[1] + args->xywh_limits[1];
		}

		args->ipoint = ipoint;
		draw_data_fun(args);

		for (int iedata=0; iedata<arrlen(edatalist); iedata++) {
			struct kahto_data *edata = edatalist[iedata];
			if (!edata)
				continue;
			int y = get_epx[iedata](edata->data, ipoint*edata->stride, yxmin[0], yxdiff[0], yxlen[0], yxmultiplier[0]);
			if (y == NOT_A_PIXEL)
				continue;
			y += margin[1] + args->xywh_limits[1];
			int x = yxz[1] + args->xywh_limits[0];
			int xyxy[] = {x, yxz[0]+args->xywh_limits[1], x, y};
			draw_line(args->canvas, args->ystride, xyxy, area, &graph->errstyle, fig, 0);
		}
	}
	free(xzy_);

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
