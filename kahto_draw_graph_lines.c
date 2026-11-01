static inline char set_bit(char num, int ibit, int val) {
	return (num & ~(1 << ibit))/*set bit to 0*/ | val << ibit;
}

static void draw_arrowtip(uint32_t *canvas, int ystride, const int *xy, struct kahto_graph *graph, const int *area) {
	const struct kahto_arrowstyle *style = &graph->arrowstyle;
	if (style->style == kahto_arrowstyle_none)
		return;
	float corners[6] = { xy[2], xy[3] };
	float mainvector[] = {xy[0] - xy[2], xy[1] - xy[3]};
	float halfangle_rad = style->angle_grad / 400 * PI;
	float co = cosf(halfangle_rad);
	float si = sinf(halfangle_rad);
	float mainlen = sqrt(mainvector[0]*mainvector[0] + mainvector[1]*mainvector[1]);
	for (int iside=0; iside<2; iside++) {
		float vector[] = {
			mainvector[0]*co - mainvector[1]*si,
			mainvector[0]*si + mainvector[1]*co,
		};
		float length;
		if (style->as_fraction_of_line && style->length < 1)
			length = style->length * mainlen;
		else
			length = tofpixels(style->length, graph->figure);
		vector[0] *= length/mainlen;
		vector[1] *= length/mainlen;
		corners[(iside+1)*2+0] = corners[0]+vector[0];
		corners[(iside+1)*2+1] = corners[1]+vector[1];
		si = -si; // angle = -angle
	}

	switch(style->style) {
		case kahto_arrowstyle_triangle:
			kahto_fill_triangle(canvas, ystride, corners, style->color, area);
			break;
		case kahto_arrowstyle_none:
			__builtin_unreachable();
	}
}

void kahto_draw_graph_lines
(struct kahto_graph *graph, struct kahto_figure *fig, struct kahto_draw_data_args *args) {
	if (!graph->data.list.ydata->length)
		return;
	struct kahto_data
		*xdata = graph->data.list.xdata,
		*ydata = graph->data.list.ydata,
		*zdata = graph->data.list.zdata;

	struct kahto_get_datapx getpx;
	kahto_init_get_datapx(&getpx, graph);
	int area[] = xywh_to_area(fig->ro_inner_xywh);

	int startpos_yx[2];
	for (int i=0; i<2; i++)
		startpos_yx[i] = graph->yxaxis[i]->ro_minmaxpos[0];
	struct kahto_axis *caxis = graph->yxaxis[2];

	/* help for xdata */
	double xstep = xdata->length > 1 ? (xdata->minmax[1] - xdata->minmax[0]) / (xdata->length-1) : 0;
	double xpix_per_step = getpx.yxlen[1] / getpx.yxdiff[1] * xstep;
	double x0data_axis = xdata->minmax[0] - getpx.yxmin[1];
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
		get_datapx[kahto_f8](&graph->xoffset, 0, getpx.yxmin[1], getpx.yxdiff[1], getpx.yxlen[1], 1));
	long end = graph->data.list.ydata->length, ipoint=0;
	int xy[2][2], z[2];
	int iyxz = 0;
	xy[iyxz][1] = kahto_get_datapx_y(getpx, ydata->data, ipoint*ydata->stride);
	notapixel = set_bit(notapixel, iyxz*2+1, xy[iyxz][1]==NOT_A_PIXEL);
	xy[iyxz][1] += startpos_yx[0];
	if (xdata->data) {
		xy[iyxz][0] = kahto_get_datapx_x(getpx, xdata->data, ipoint*xdata->stride);
		notapixel = set_bit(notapixel, iyxz*2+0, xy[iyxz][0]==NOT_A_PIXEL);
	}
	else
		xy[iyxz][0] = xoffset + iroundpos((x0data_axis + ipoint*xstep) *  xpix_per_step);
	xy[iyxz][0] += startpos_yx[1];
	if (get_datalevel_fun)
		z[iyxz] = get_datalevel_fun(zdata->data, ipoint*zdata->stride, caxislim, 255);

	int32_t carry = 0;
	for (ipoint=1; ipoint<end; ipoint++) {
		iyxz = !iyxz;
		xy[iyxz][1] = kahto_get_datapx_y(getpx, ydata->data, ipoint*ydata->stride);
		notapixel = set_bit(notapixel, iyxz*2+1, xy[iyxz][1]==NOT_A_PIXEL);
		xy[iyxz][1] += startpos_yx[0];
		if (xdata->data) {
			xy[iyxz][0] = kahto_get_datapx_x(getpx, xdata->data, ipoint*xdata->stride);
			notapixel = set_bit(notapixel, iyxz*2+0, xy[iyxz][0]==NOT_A_PIXEL);
		}
		else
			xy[iyxz][0] = iroundpos((x0data_axis + ipoint*xstep) *  xpix_per_step);
		xy[iyxz][0] += startpos_yx[1];
		if (get_datalevel_fun) {
			z[iyxz] = get_datalevel_fun(zdata->data, ipoint*zdata->stride, caxislim, 255);
			short level = (z[0] + z[1]) * 0.5;
			if (caxis->reverse_cmap)
				level = 255 - level;
			graph->linestyle.color = from_cmap(caxis->cmap+3*level);
		}
		if (!notapixel) {
			carry = draw_line(args->canvas, args->ystride, xy[0], area, &graph->linestyle, fig, carry);
			draw_arrowtip(args->canvas, args->ystride, xy[0], graph, area);
		}
	}
}
