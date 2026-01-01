struct draw_data_args {
	uint32_t *canvas;
	unsigned *canvascount;
	int ystride;
	const unsigned char *bmap;
	int mapw, maph;
	unsigned char alpha;
	const int *axis_xywh_outer;
	uint32_t color;
	uint32_t *colors;
	int ncolors, ipoint;

	short *yxz;
	const unsigned char *cmap;
	char reverse_cmap;
};

#include "kahto_init_markers.c"
#include "kahto_draw_data_markers.c"
#include "kahto_draw_data_lines.c"
#include "kahto_colormesh.c"

void kahto_graph_render(struct kahto_graph *graph, uint32_t *canvas, int ystride, struct kahto_figure *fig, long start) {
	if (is_colormesh(graph))
		return kahto_colormesh_render(graph, canvas, ystride, fig, start);
	kahto_graph_draw_markers(graph, canvas, ystride, fig, start);
	kahto_graph_draw_lines(graph, canvas, ystride, fig, start);
}
