#include "kahto_init_markers.c"
#include "kahto_draw_graph_markers.c"
#include "kahto_draw_graph_lines.c"
#include "kahto_colormesh.c"

/* start is unused */
void kahto_draw_graph(struct kahto_graph *graph, uint32_t *canvas, int ystride, struct kahto_figure *fig, long start) {
	if (is_colormesh(graph))
		return kahto_colormesh_render(graph, canvas, ystride, fig, start);

	const int *xywh0 = fig->ro_inner_xywh;
	//const int *margin = fig->ro_inner_margin;
	//int yxlen[] = {xywh0[3]-margin[1]-margin[3], xywh0[2]-margin[0]-margin[2]}; // area check is missing
	struct kahto_axis *caxis = graph->yxaxis[2];

	struct draw_data_args args = {
		.canvas = canvas,
		.ystride = ystride,
		.xywh_limits = xywh0,
		.cmap = caxis ? caxis->cmap : NULL,
		.reverse_cmap = caxis ? caxis->reverse_cmap : 0,
		.color = graph->color,
		.alpha = graph->alpha,
		.colors = graph->colors,
		.ncolors = graph->ncolors,
	};

	kahto_draw_graph_markers(graph, fig, &args);
	kahto_draw_graph_lines(graph, fig, &args);
}
