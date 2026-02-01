#include <kahto.h>
#define arrlen(a) (sizeof(a) / sizeof((a)[0]))

void shift(float *a, int n) {
	for (int i=0; i<n; i++) a[i] += 0.3;
}

/* Any string can be used in markerstyle.marker.
   Some single character strings have a special meaning,
   for example "x", "o", and "^", mean cross, sphere, and triangle, respectively.
   If the string doesn't have a special meaning it will be used as the marker as is.
   To override the special meaning, use .markerstyle.literal=1.
   */

int main() {
	float data[] = {1, 2, 0.8};
	int len = arrlen(data);
	struct kahto_figure *fig = kahto_figure_new();

	kahto_y(data, len, .yxzowner[0]=-1, .figure=fig, .label="first",
		.markerstyle.marker="x");

	shift(data, len);
	kahto_y(data, len, .yxzowner[0]=-1, .figure=fig, .label="first but literal",
		.markerstyle.marker="x", .markerstyle.literal=1);

	shift(data, len);
	kahto_y(data, len, .yxzowner[0]=-1, .figure=fig, .label="second",
		.markerstyle.marker="o", .linestyle.style=kahto_line_normal_e);

	shift(data, len);
	kahto_y(data, len, .yxzowner[0]=-1, .figure=fig, .label="third",
		.markerstyle.marker="Hello world!");

	shift(data, len);
	kahto_y(data, len, .yxzowner[0]=-1, .figure=fig, .label="fourth",
		.markerstyle=(struct kahto_markerstyle){.marker="o", .size=0.02, .nofill=0.5, .color=0xff00aaaa},
		.linestyle=(struct kahto_linestyle){
			.style=kahto_line_dashed_e,
			.thickness=0.004,
			.pattern=(static float[]){0.01, 0.01, 0.04, 0.08},
			.patternlen=4,
		}
	);

	fig->legend.borderstyle.color=0xffaa33ff;
	fig->legend.borderstyle.thickness=0.01;
	fig->legend.fillcolor = 0xff888888;
	fig->legend.symbolspace_per_rowheight = 6;
	kahto_show(fig);
}
