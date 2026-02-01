#include <kahto.h>

int main() {
	int a[] = {0, 3, -1, 2, 4};
	struct kahto_figure *fig = kahto_figure_new();

	kahto_y(a, 5, .figure=fig, .label="first data",
		.yxzowner[0]=-1); // copy data because the array will be changed

	for (int i=0; i<5; i++)
		a[i] += 1;

	kahto_y(a, 5, .figure=fig, .label="second data");
	kahto_show(fig);
}
