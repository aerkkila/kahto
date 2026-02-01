void kahto_draw_graph_lines
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
	int area[] = xywh_to_area(fig->ro_inner_xywh);

	struct kahto_axis *caxis = graph->yxaxis[2];

	struct kahto_data
		*xdata = graph->data.list.xdata,
		*ydata = graph->data.list.ydata,
		*zdata = graph->data.list.zdata;

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

	long end = graph->data.list.ydata->length, ipoint=0;
	int xy[2][2], z[2];
	int iyxz = 0;
	xy[iyxz][1] = get_datapx_inv[ydata->type](ydata->data, ipoint*ydata->stride, yxmin[0], yxdiff[0], yxlen[0])
		+ margin[1];
	if (xdata->data)
		xy[iyxz][0] = get_datapx[xdata->type](xdata->data, ipoint*xdata->stride, yxmin[1], yxdiff[1], yxlen[1]);
	else
		xy[iyxz][0] = iroundpos((x0data_axis + ipoint*xstep) *  xpix_per_unit);
	xy[iyxz][0] += args->axis_xywh_outer[0] + margin[0];
	xy[iyxz][1] += args->axis_xywh_outer[1];
	if (get_datalevel_fun)
		z[iyxz] = get_datalevel_fun(zdata->data, ipoint*zdata->stride, caxislim, 255);

	int32_t carry = 0;
	for (ipoint=1; ipoint<end; ipoint++) {
		iyxz = !iyxz;
		xy[iyxz][1] = get_datapx_inv[ydata->type](ydata->data, ipoint*ydata->stride, yxmin[0], yxdiff[0], yxlen[0])
			+ margin[1];
		if (xdata->data)
			xy[iyxz][0] = get_datapx[xdata->type](xdata->data, ipoint*xdata->stride, yxmin[1], yxdiff[1], yxlen[1]);
		else
			xy[iyxz][0] = iroundpos((x0data_axis + ipoint*xstep) *  xpix_per_unit);
		xy[iyxz][0] += margin[0];
		if (get_datalevel_fun) {
			z[iyxz] = get_datalevel_fun(zdata->data, ipoint*zdata->stride, caxislim, 255);
			short level = (z[0] + z[1]) * 0.5;
			if (caxis->reverse_cmap)
				level = 255 - level;
			graph->linestyle.color = from_cmap(caxis->cmap+3*level);
		}
		xy[iyxz][0] += args->axis_xywh_outer[0];
		xy[iyxz][1] += args->axis_xywh_outer[1];
		carry = draw_line(args->canvas, args->ystride, xy[0], area, &graph->linestyle, fig, carry);
	}
}

static void legend_draw_marker(struct kahto_figure *fig, struct kahto_graph *graph,
	uint32_t *canvas, int ystride, int x0, int y0, int text_left)
{
	int width, height, marker;
	width = height = topixels(graph->markerstyle.size, fig);
	unsigned char bmap_buff[width*height];
	unsigned char *bmap = kahto_data_marker_bmap(graph, bmap_buff, &marker, &width, &height);
	int *xywh = graph->yxaxis[0]->figure->ro_inner_xywh;
	x0 -= xywh[0];
	y0 -= xywh[1];
	struct kahto_axis *caxis = graph->yxaxis[2];

	int yx[] = {y0, x0};
	struct draw_data_args args = {
		.yxz = yx,
		.canvas = canvas,
		.ystride = ystride,
		.axis_xywh_outer = xywh,
		.bmap = bmap,
		.mapw = width,
		.maph = height,
		.color = graph->color,
		.cmap = caxis ? caxis->cmap : NULL,
		.reverse_cmap = caxis ? caxis->reverse_cmap : 0,
		.alpha = graph->alpha,
	};

	if (marker) {
		if (graph->data.list.zdata)
			draw_data_xyc(&args);
		else
			draw_datum(&args);
	}

	if (graph->linestyle.style) {
		int xypixels[] = {x0 - (text_left+1)/3, y0, x0 + (text_left+1)/3, y0};
		int area[] = xywh_to_area(fig->ro_inner_xywh);
		draw_line(canvas, ystride, xypixels, area, &graph->linestyle, fig, 0);
	}

	if (bmap != bmap_buff)
		free(bmap);
}
