#include <kahto.h>
#include <math.h>

#define len 100
#define π 3.14159265358979

int main() {
	float y[len], x[len];
	const float nwaves = 1.5;
	for (int i=0; i<len; i++) {
		x[i] = i / (len-1.0) * nwaves * (2*π);
		y[i] = sinf(x[i]);
	}

	struct kahto_figure *fig = kahto_yx(y, x, len, kahto_lineargs);

	struct kahto_ticks *ticks = kahto_glg(fig)->yxaxis[1]->ticks;
	/* ticks->init points to the function which determines the ticks.
	   All functions are listed in kahto.h (kahto_init_ticker_*).
	   In this case we select the function which reads arbitrary data given by user
	   and interprets the ticks locations as data coordinates; the values on x-array. */
	ticks->init = kahto_init_ticker_arbitrary_datacoord;
	/* The data is given to the struct kahto_tickerdata_arbitrary,
	   which is found from union tickerdata in field arb. */
	char *labels[] = {"0", "\e$π^-1$", "π", "2π", "2.5π", "3π", "4ln(π)"};
	double coords[] = {0,      1/π,     π,   2*π,  2.5*π,  3*π, 4*log(π)}; // don't have to be sorted
	ticks->tickerdata.arb = (struct kahto_tickerdata_arbitrary) {
		.labels.m = labels,
		.ticks = coords,
		.nticks = sizeof(coords)/sizeof(coords[0]),
		/* fields min and max need not to be set manually */
	};

	/***********************************************************/

	/* Another example */
	float y2[] = {1, 1.5, -0.5, 3};
	float y2low[] = {0.8, 1.1, -0.6, 2.9}; // lower error bars
	float y2high[] = {1.1, 1.75, -0.3, 3.1}; // higher error bars
	struct kahto_figure *fig2 = kahto_y(y2, sizeof(y2)/sizeof(y2[0]), .edata0=y2low, .edata1=y2high);

	ticks = kahto_glg(fig2)->yxaxis[1]->ticks;
	char *labels2[] = {
		"first location",
		"",
		"\e[4;91mthird location\e[0m", // styling text with ansi escape code
		"last"
	};
	/* There is a special method for placing the ticks to coordinates 0, 1, 2, ... */
	ticks->init = kahto_init_ticker_arbitrary_datacoord_enum;
	ticks->tickerdata.arb.labels.m = labels2;
	ticks->tickerdata.arb.nticks = sizeof(labels2)/sizeof(labels2[0]);
	/* locations (tickerdata.arb.ticks) need not to be set in this case */

	ticks->rotation_grad = -35;
	/* We want the right edge of texts to be on the tick location.
	   Therefore we move texts to left by the amount of their widths, i.e. -1 width to right */
	ticks->xyalign_text[0] = -1;

	/* put both subfigures to a figure */
	struct kahto_figure *super = kahto_subfigures_new(2, 1);
	super->wh[0] /= 2;
	super->subfigures[0] = fig;
	super->subfigures[1] = fig2;
	kahto_show(super);
}
