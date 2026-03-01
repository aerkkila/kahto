static inline char set_bit(char num, int ibit, int val) {
	return ((num) & ~(1 << (ibit))) // reset bit
		| (val) << (ibit); /* set bit */
}

void kahto_draw_graph_lines
(struct kahto_graph *graph, struct kahto_figure *fig, struct kahto_draw_data_args *args) {
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

	int yxlogscale[] = {graph->yxaxis[0]->logscale, graph->yxaxis[1]->logscale};
	double yxmultiplier[] = {1, 1};
	typeof(get_datapx[0]) get_yxpx[] = {
		get_datapx_inv[ydata->type],
		get_datapx[xdata->type],
	};

	for (int iyx=0; iyx<2; iyx++)
		if (yxlogscale[iyx]) {
			// convert from log_e to log_base
			yxmultiplier[iyx] = 1 / log(graph->yxaxis[iyx]->ticks->tickerdata.log.base);
			if (iyx == 0)
				get_yxpx[0] = get_datapx_log_inv[ydata->type];
			else
				get_yxpx[1] = get_datapx_inv[xdata->type];
			yxmin[iyx] = log(yxmin[iyx]) * yxmultiplier[iyx];
			yxdiff[iyx] = log(graph->yxaxis[iyx]->max) * yxmultiplier[iyx] - yxmin[iyx];
		}

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

	char notapixel = 0;
	int xoffset = (xdata->data ? 0 :
		get_datapx[kahto_f8](&graph->xoffset, 0, yxmin[1], yxdiff[1], yxlen[1], 1));
	long end = graph->data.list.ydata->length, ipoint=0;
	int xy[2][2], z[2];
	int iyxz = 0;
	xy[iyxz][1] = get_yxpx[0](ydata->data, ipoint*ydata->stride, yxmin[0], yxdiff[0], yxlen[0], yxmultiplier[0]);
	notapixel = set_bit(notapixel, iyxz*2+1, xy[iyxz][1]==NOT_A_PIXEL);
	xy[iyxz][1] += margin[1];
	if (xdata->data) {
		xy[iyxz][0] = get_yxpx[1](xdata->data, ipoint*xdata->stride, yxmin[1], yxdiff[1], yxlen[1], yxmultiplier[1]);
		notapixel = set_bit(notapixel, iyxz*2+0, xy[iyxz][0]==NOT_A_PIXEL);
	}
	else
		xy[iyxz][0] = xoffset + iroundpos((x0data_axis + ipoint*xstep) *  xpix_per_unit);
	xy[iyxz][0] += args->xywh_limits[0] + margin[0];
	xy[iyxz][1] += args->xywh_limits[1];
	if (get_datalevel_fun)
		z[iyxz] = get_datalevel_fun(zdata->data, ipoint*zdata->stride, caxislim, 255);

	int32_t carry = 0;
	for (ipoint=1; ipoint<end; ipoint++) {
		iyxz = !iyxz;
		xy[iyxz][1] = get_yxpx[0](ydata->data, ipoint*ydata->stride, yxmin[0], yxdiff[0], yxlen[0], yxmultiplier[0]);
		notapixel = set_bit(notapixel, iyxz*2+1, xy[iyxz][1]==NOT_A_PIXEL);
		xy[iyxz][1] += margin[1];
		if (xdata->data) {
			xy[iyxz][0] = get_yxpx[1](xdata->data, ipoint*xdata->stride, yxmin[1], yxdiff[1], yxlen[1], yxmultiplier[1]);
			notapixel = set_bit(notapixel, iyxz*2+0, xy[iyxz][0]==NOT_A_PIXEL);
		}
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
		xy[iyxz][0] += args->xywh_limits[0];
		xy[iyxz][1] += args->xywh_limits[1];
		if (!notapixel)
			carry = draw_line(args->canvas, args->ystride, xy[0], area, &graph->linestyle, fig, carry);
	}
}
