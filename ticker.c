#include <stdio.h>

#define Sign(a) ((a) < 0 ? -1 : ((a) > 0) * 1)

int cplot_get_lintick_multiplication(struct $ticker *this, char *out, int sizeout) {
    double base = this->tickerdata.lin.base;
    if (base < 0.05) {
	snprintf(out, sizeout, "%.1e", base);
	return 1;
    }
    if (sizeout)
	out[0] = 0;
    return 0;
}

double cplot_get_tick_linear(struct $ticker *this, int ind, char *out, int sizeout) {
    double step = this->tickerdata.lin.step,
	   base = this->tickerdata.lin.base,
	   min = this->tickerdata.lin.min;
    double val = ind * step + min;

    const char *form = (step == (long)step ? "%.0f" : "%.1f");
    snprintf(out, sizeout, form, base < 0.05 ? val/base : val);
    return val;
}

int cplot_init_ticker_caveman(struct $ticker *this, double min, double max) {
    this->species = ticker_linear;
    this->get_tick = cplot_get_tick_linear;
    this->get_multiplication = cplot_get_lintick_multiplication;
    this->tickerdata.lin = (struct cplot_tickerdata_linear) {
	.nticks = 7,
	.step = (max - min) / 6,
	.min = min,
	.base = 1,
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
    this->get_multiplication = cplot_get_lintick_multiplication;
    this->tickerdata.lin = (struct cplot_tickerdata_linear) {
	.nticks = diff / best_step,
	.step = best_step,
	.min = best_step * (int)(min/best_step),
	.base = base,
    };

    return this->tickerdata.lin.nticks;
}
