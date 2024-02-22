#include <stdio.h>

#define Sign(a) ((a) < 0 ? -1 : ((a) > 0) * 1)

double cplot_get_tick_linear(struct $ticker *this, int ind, char *out, int sizeout) {
    double step = this->tickerdata.lin.step;
    double min = this->tickerdata.lin.min;
    double val = ind * step + min;

    double toint = val - iround(val);
    if (toint < 0) toint = -toint;
    if (toint < 1e-9)
	snprintf(out, sizeout, "%.0f", val);
    else
	snprintf(out, sizeout, "%.1f", val);

    return val;
}

int cplot_init_ticker_caveman(struct $ticker *this, double min, double max) {
    this->species = ticker_linear;
    this->get_tick = cplot_get_tick_linear;
    this->tickerdata.lin = (struct cplot_tickerdata_linear) {
	.nticks = 7,
	.step = (max - min) / 6,
	.min = min,
    };
    return this->tickerdata.lin.nticks;
}

int cplot_init_ticker_default(struct $ticker *this, double min, double max) {
    double step_opts[] = {1, 1.5, 2, 2.5, 3, 4, 5};

    int maxsign = Sign(max);
    max *= maxsign; // absolute value
    int maxpower = 0;
    double base = 1;
    if (max >= 1) {
	while (base < max) {
	    base *= 10;
	    maxpower++;
	}
	/* pitäisi huomioida likipitäen yhtäsuuruus */
	if (base > max) {
	    base *= 0.1;
	    maxpower--;
	}
    }
    else
	while (base > max) {
	    base *= 0.1;
	    maxpower--;
	}
    max *= maxsign;

    int nstep_opt = arrlen(step_opts);
    double best_step, best_diff = DBL_MAX;
    double target_n = 7;
    double diff = max - min;
    if (diff / (base*step_opts[0]) < target_n)
	base *= 0.1;
    for (int iopt=0; iopt<nstep_opt; iopt++) {
	double step = base * step_opts[iopt];
	double n = diff / step;
	double targetdiff = n - target_n;
	if (targetdiff < 0) targetdiff = -targetdiff;
	if (targetdiff < best_diff) {
	    best_diff = targetdiff;
	    best_step = step;
	}
    }

    this->species = ticker_linear;
    this->get_tick = cplot_get_tick_linear;
    this->tickerdata.lin = (struct cplot_tickerdata_linear) {
	.nticks = diff / best_step,
	.step = best_step,
	.min = best_step * (int)(min/best_step),
    };

    return this->tickerdata.lin.nticks;
}
