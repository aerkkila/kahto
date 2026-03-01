#include <kahto.h>

int main() {
	int length = 30;
	float y0[length], y1[length], y2[length];
	for (int i=0; i<length; i++) {
		y0[i] = (i%4 + i%3) * i/5;
		y1[i] = (i%4 + i%3) * i/5 * i*i;
		y2[i] = (i%4 + i%3) * i/5 + i*i;
	}

	struct kahto_figure *fig = kahto_y(y0, length);

	struct kahto_axis *ax = kahto_axis_new(fig, 'y', 1);
	kahto_axislabel(ax, "second");
	kahto_y(y1, length, .yaxis=ax, .label="second");

	ax = kahto_axis_new(fig, 'y', 0);
	ax->outside = 1;
	kahto_axislabel(ax, "third");
	kahto_y(y2, length, .yaxis=ax, .label="third");

	kahto_show(fig);
}
