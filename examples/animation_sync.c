#include <kahto.h>
#include <stdlib.h>
#include <unistd.h>
#define arrlen(a) (sizeof(a) / sizeof((a)[0]))

struct args {
	int ifig;
	float *ydata;
	int ydatalen;
};

int update(struct kahto_figure *fig, uint32_t *canvas, int ystride, long iupdate, double sec_from_start) {
	struct args *args = fig->userdata;

	if (args->ifig >= 40)
		return -1; // finish

	double when_to_update = args->ifig / 20.0; // 20 fps
	if (sec_from_start < when_to_update)
		return 0; // no update

	args->ydata[args->ifig++ % args->ydatalen] = (double)rand() / (RAND_MAX+1L);
	kahto_clear_data(fig, canvas, ystride);
	kahto_draw_figures(fig, canvas, ystride);
	return 1; // update
}

int main(int argc, char **argv) {
	float ydata[50];
	for (int i=0; i<arrlen(ydata); i++)
		ydata[i] = (double)rand() / (RAND_MAX+1L);
	struct kahto_figure *fig = kahto_y(ydata, arrlen(ydata));
	kahto_set_range(kahto_gly(fig), 0, 1);
	fig->update = update;
	struct args args = {
		.ydata = ydata,
		.ydatalen = arrlen(ydata),
	};
	fig->userdata = &args;
	if (argc > 1)
		kahto_write_mp4(fig, "sync.mp4", 20);
	else
		kahto_show(fig);
}
