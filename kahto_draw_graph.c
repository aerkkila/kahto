#include "kahto_init_markers.c"
#include "kahto_draw_graph_markers.c"
#include "kahto_draw_graph_lines.c"
#include "kahto_colormesh.c"

/* can be given by user to graph->draw_marker_fun */
void kahto_draw_boxmarker_5(struct kahto_draw_data_args *args) {
	struct kahto_draw_boxmarker_args *bargs = args->graph->draw_marker_fun_args;
	struct kahto_draw_boxmarker_args bargs_ = {0};
	if (!bargs)
		bargs = &bargs_;
	int boxw = topixels(bargs->boxwidth ? bargs->boxwidth : 0.02, args->fig);

	if (args->yxyx_oversize_out) {
		args->yxyx_oversize_out[1] = boxw/2;
		args->yxyx_oversize_out[3] = boxw - boxw/2;
		args->yxyx_oversize_out[0] = args->yxyx_oversize_out[2] = 0;
		return;
	}

	int linew = topixels(bargs->linewidth ? bargs->linewidth : 0.003, args->fig);
	int mlinew = topixels(bargs->linewidth ? bargs->mlinewidth : 0.003, args->fig);
	int mlinealpha = bargs->mlinealpha ? bargs->mlinealpha : 255;
	int area[] = {0, 0, args->fig->wh[0], args->fig->wh[1]};
	int *xzy = args->yxz;
	int ydirection = xzy[2+1] < xzy[2+3];

	{
		int xyxy[] = {
			xzy[0]-boxw/2,
			xzy[2 + (ydirection ? 1 : 3)],
			xzy[0]+boxw-boxw/2,
			xzy[2 + (ydirection ? 3 : 1)],
		};
		kahto_fill_box(args->canvas, args->ystride, xyxy, args->color);

		xyxy[1] = xyxy[3] = xzy[2+2]; // midline
		kahto_draw_straight_line(args->canvas, args->ystride, xyxy, args->fig->background,
			mlinew, area, mlinealpha);
	}

	int xyxy[] = {
		xzy[0],
		xzy[2+0],
		xzy[0],
		xzy[2+1],
	};
	kahto_draw_straight_line(args->canvas, args->ystride, xyxy, args->color, linew, area, 255);
	xyxy[1] = xzy[2+3];
	xyxy[3] = xzy[2+4];
	kahto_draw_straight_line(args->canvas, args->ystride, xyxy, args->color, linew, area, 255);
}

/* can be given by user to graph->draw_marker_fun */
void kahto_draw_violin(struct kahto_draw_data_args *args) {
	struct kahto_draw_violin_args *bargs = args->graph->draw_marker_fun_args;
	struct kahto_draw_violin_args bargs_ = {0};
	if (!bargs)
		bargs = &bargs_;
	const int *restrict lim = args->xywh_limits;

	int mem = args->fig->topixels_reference;
	if (bargs->topixels_reference)
		args->fig->topixels_reference = bargs->topixels_reference;
	int violinw = topixels(bargs->width ? bargs->width : 0.08, args->fig);
	args->fig->topixels_reference = mem;

	if (args->yxyx_oversize_out) {
		args->yxyx_oversize_out[1] = violinw - violinw/2;
		args->yxyx_oversize_out[3] = violinw/2;
		args->yxyx_oversize_out[0] = args->yxyx_oversize_out[2] = 0;
		return;
	}

	int area[] = xywh_to_area(lim);
	int *xzy = args->yxz;
	int *ydata = xzy+2;

	int *w = calloc(lim[3] * sizeof(w[1]), 1);
	for (int i=0; i<args->sublength; i++) {
		if (area[1] <= ydata[i] && ydata[i] < area[3])
			++w[ydata[i]-area[1]];
	}
	int maxw = 0;
	for (int i=0; i<lim[3]; i++)
		if (w[i] > maxw)
			maxw = w[i];
	double scale = (double)violinw / maxw;

	for (int i=0; i<lim[3]; i++) {
		if (!w[i])
			continue;
		int xyxy[] = {
			xzy[0]+violinw/2,
			i+area[1],
			xzy[0]+violinw/2-w[i]*scale,
			i+area[1],
		};
		if (xyxy[0] != xyxy[2])
			kahto_draw_straight_line(args->canvas, args->ystride, xyxy, args->color, 1, area, 255);
	}

	free(w);
}

/* start is unused */
void kahto_draw_graph(struct kahto_graph *graph, uint32_t *canvas, int ystride, struct kahto_figure *fig, long start) {
	if (!graph->data.list.ydata->length)
		return;
	if (is_colormesh(graph))
		return kahto_colormesh_render(graph, canvas, ystride, fig, start);

	const int *xywh0 = fig->ro_inner_xywh;
	//const int *margin = fig->ro_inner_margin;
	//int yxlen[] = {xywh0[3]-margin[1]-margin[3], xywh0[2]-margin[0]-margin[2]}; // area check is missing
	struct kahto_axis *caxis = graph->yxaxis[2];

	struct kahto_draw_data_args args = {
		.canvas = canvas,
		.ystride = ystride,
		.xywh_limits = xywh0,
		.cmap = caxis ? caxis->cmap : NULL,
		.reverse_cmap = caxis ? caxis->reverse_cmap : 0,
		.color = graph->markerstyle.color,
		.alpha = graph->alpha,
		.colors = graph->colors,
		.ncolors = graph->ncolors,
		.graph = graph,
		.fig = fig,
	};
	if (graph->colormodify)
		args.color = graph->colormodify(args.color);
	kahto_draw_graph_markers(graph, fig, &args);

	args.color = graph->linestyle.color;
	if (graph->colormodify)
		args.color = graph->colormodify(args.color);
	kahto_draw_graph_lines(graph, fig, &args);
}
