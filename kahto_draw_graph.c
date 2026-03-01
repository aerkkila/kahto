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
	int linew = topixels(bargs->linewidth ? bargs->linewidth : 0.003, args->fig);
	int mlinew = topixels(bargs->linewidth ? bargs->mlinewidth : 0.003, args->fig);
	int area[] = {0, 0, args->fig->wh[1], args->fig->wh[0]};
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

		xyxy[1] = xyxy[3] = xzy[2+2];
		kahto_draw_straight_line(args->canvas, args->ystride, xyxy, args->fig->background, mlinew, area);
	}

	int xyxy[] = {
		xzy[0],
		xzy[2+0],
		xzy[0],
		xzy[2+1],
	};
	kahto_draw_straight_line(args->canvas, args->ystride, xyxy, args->color, linew, area);
	xyxy[1] = xzy[2+3];
	xyxy[3] = xzy[2+4];
	kahto_draw_straight_line(args->canvas, args->ystride, xyxy, args->color, linew, area);
}

/* start is unused */
void kahto_draw_graph(struct kahto_graph *graph, uint32_t *canvas, int ystride, struct kahto_figure *fig, long start) {
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
	kahto_draw_graph_markers(graph, fig, &args);

	args.color = graph->linestyle.color;
	kahto_draw_graph_lines(graph, fig, &args);
}
