void kahto_colormesh_render(
		struct kahto_graph *graph, uint32_t *canvas, int ystride, struct kahto_figure *fig, long start) {
	int xdatalen = graph->data.list.xdata->length;
	int ydatalen = graph->data.list.ydata->length;
	int xywh[4];
	inner_without_margin(xywh, graph->yxaxis[0]->figure);
	struct kahto_axis *caxis = graph->yxaxis[2];
	const unsigned char *cmap = caxis->cmap;
	const int reverse_cmap = caxis->reverse_cmap;

	float data_per_pixel[] = {
		(float)xdatalen / xywh[2],
		(float)ydatalen / xywh[3],
	};
	/* akseli piirretään väärin, koska kokoa muokataan vain täällä */
	if (graph->equal_xy) {
		data_per_pixel[0] = max(data_per_pixel[0], data_per_pixel[1]);
		data_per_pixel[1] = data_per_pixel[0];
	}
	if (graph->exact)
		for (int i=0; i<2; i++)
			data_per_pixel[i] = data_per_pixel[i] >= 1 ? ceil(data_per_pixel[i]) :
				1.0 / floor(1.0/data_per_pixel[i]);

	uint32_t (*canvas2d)[ystride] = (void*)canvas;
	canvas2d = (void*)(canvas2d[xywh[1]] + xywh[0]); // move corner to starting location

	int xpixlen = iceil(xdatalen / data_per_pixel[0]);
	double pixel_per_data[] = {
		1 / data_per_pixel[0],
		1 / data_per_pixel[1],
	};
	const double epsilon = 1e-8;

	for (int iypix=0; ; iypix++) {
		double ydata0 = iypix * data_per_pixel[1];
		double ydata1 = (iypix+1) * data_per_pixel[1];
		if (iypix > 0) {
			double ydata2 = (iypix-1) * data_per_pixel[1];
			if ((int)(ydata2+epsilon) == (int)(ydata0+epsilon)
					&& (int)(ydata1+epsilon) == (int)(ydata0+epsilon)) {
				memcpy(canvas2d[iypix], canvas2d[iypix-1], sizeof(uint32_t)*xpixlen);
				continue;
			}
		}
		if (ydata0 >= ydatalen)
			break;
		if (ydata1 > ydatalen)
			ydata1 = ydatalen;

		double rowvalues_[xpixlen+2];
		double rowweight_[xpixlen+2];
		double *rowvalues = rowvalues_+1;
		double *rowweight = rowweight_+1;
		memset(rowvalues_, 0, sizeof(rowvalues_));
		memset(rowweight_, 0, sizeof(rowweight_));

		while (ydata0 < ydata1) {
			int iydata = ydata0 + epsilon;
			int idata = iydata*xdatalen;
			double yweight = iydata+1 - ydata0;
			if (ydata1 < iydata+1)
				yweight -= iydata+1 - ydata1;
			for (int ix=0; ix<xdatalen; ix++) {
				double xpix0 = ix * pixel_per_data[0];
				double xpix1 = (ix+1) * pixel_per_data[0];
				int ixpix0 = iceil(xpix0 - epsilon);
				int ixpix1 = xpix1 + epsilon;
				double weight = (ixpix0 - xpix0) * yweight;
				double val = get_floating[graph->data.list.zdata->type](graph->data.list.zdata->data, idata+ix);
				rowvalues[ixpix0-1] += weight * val;
				rowweight[ixpix0-1] += weight;
				for (int i=ixpix0; i<ixpix1; i++) {
					rowvalues[i] += val;
					rowweight[i] += 1;
				}
				weight = xpix1 - ixpix1;
				rowvalues[ixpix1] += val * weight;
				rowweight[ixpix1] += weight;
			}
			ydata0 = (int)(ydata0+1+epsilon);
		}

		for (int i=0; i<xpixlen; i++)
			rowvalues[i] /= rowweight[i];

		unsigned char zlevels[xpixlen];
		struct kahto_axis *caxis = graph->yxaxis[2];
		if (my_isnan(caxis->center))
			get_datalevels_f8(
				0, xpixlen, rowvalues, zlevels, caxis->min, caxis->max, 255, 1);
		else
			get_datalevels_with_center_f8(
				0, xpixlen, rowvalues, zlevels, caxis->min, caxis->center, caxis->max, 255, 1);

		if (reverse_cmap)
			for (int i=0; i<xpixlen; i++)
				canvas2d[iypix][i] = from_cmap(cmap + (255-zlevels[i])*3);
		else
			for (int i=0; i<xpixlen; i++)
				canvas2d[iypix][i] = from_cmap(cmap + zlevels[i]*3);
	}
}
