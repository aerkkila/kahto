#include <kahto.h>

int main() {
	float boxes[][5] = {
		{0.0, 1.0, 2.0, 3.0, 4.0},
		{0.1, 0.8, 2.5, 3.0, 3.5},
		{-0.1, 0.5, 1.5, 5., 5.2},
	};

	float x[] = {-0.1, 0.9, 1.9};
	struct kahto_figure *fig = kahto_yx(boxes[0], x, 3, .yxzowner[1]=-1/*copy x*/,
		.ysublength=5, // 5 members in each box
		.draw_marker_fun=kahto_draw_boxmarker_5);

	for (int i=0; i<3; i++)
		x[i] += 0.2;

	/* edit style */
	struct kahto_draw_boxmarker_args args = {.boxwidth=0.04, .mlinewidth=0.5};
	kahto_yx(boxes[0], x, 3, .figure=fig,
		.ysublength=5, .draw_marker_fun=kahto_draw_boxmarker_5, .draw_marker_fun_args=&args);

	kahto_show(fig);
}
