#include "kahto_init_markers.c"
#include "kahto_draw_data_markers.c"
#include "kahto_draw_data_lines.c"
#include "kahto_colormesh.c"

void kahto_graph_render(struct kahto_graph *graph, uint32_t *canvas, int ystride, struct kahto_figure *fig, long start) {
	if (is_colormesh(graph))
		return kahto_colormesh_render(graph, canvas, ystride, fig, start);

	const int *margin = fig->ro_inner_margin;
	const int *xywh0 = fig->ro_inner_xywh;
	int yxlen[] = {xywh0[3]-margin[1]-margin[3], xywh0[2]-margin[0]-margin[2]};
	struct kahto_axis *caxis = graph->yxaxis[2];
	int area[] = xywh_to_area(xywh0);

	struct draw_data_args args = {
		.canvas = canvas,
		.ystride = ystride,
		.axis_xywh_outer = xywh0,
		.axis_area_outer = area,
		.cmap = caxis ? caxis->cmap : NULL,
		.reverse_cmap = caxis ? caxis->reverse_cmap : 0,
		.color = graph->color,
		.alpha = graph->alpha,
		.colors = graph->colors,
		.ncolors = graph->ncolors,
	};

	kahto_graph_draw_markers(graph, fig, &args);
	kahto_graph_draw_lines(graph, fig, &args);
}
