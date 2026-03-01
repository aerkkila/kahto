#include <kahto.h>
#include <math.h>
#include <stdlib.h>

int cmp(const void *va, const void *vb) {
	float a = ((float*)va)[0];
	float b = ((float*)vb)[0];
	return a <= b ? a < b ? -1 : 0 : 1;
}

int main() {
	float ydata[200];
	float xdata[200];
	for (int i=0; i<200; i++) {
		ydata[i] = exp((double)rand() / (RAND_MAX+1L) * 10);
		xdata[i] = exp((double)rand() / (RAND_MAX+1L) * 10);
	}
	qsort(ydata, 200, sizeof(float), cmp);

	struct kahto_figure *figs = kahto_subfigures_new(2, 2);

	figs->subfigures[0] = kahto_y(ydata, 200);
	figs->subfigures[1] = kahto_y(ydata, 200);
	figs->subfigures[2] = kahto_yx(ydata, xdata, 200);
	figs->subfigures[3] = kahto_yx(ydata, xdata, 200);

	kahto_gly(figs->subfigures[1])->logscale = 1; // gly: get latest yaxis
	kahto_gly(figs->subfigures[1])->ticks->tickerdata.log.base = 10; // this is the default

	kahto_gly(figs->subfigures[3])->logscale = 1;
	kahto_glx(figs->subfigures[3])->logscale = 1;

	kahto_show(figs);
}
