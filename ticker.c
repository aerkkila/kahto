#include <stdio.h>

const char *cplot_supernum[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};

double cplot_get_tick_linear(struct cplot_ticker *this, int ind, char *out, int sizeout) {
    double step = this->tickerdata.lin.step,
	   base = this->tickerdata.lin.base,
	   min = this->tickerdata.lin.min;
    double val = ind * step + min;

    if (base < 0.05 || base > 5000) {
	const char *form = (step == (long)step ? "%.0f\n10" : "%.1f\n10");
	snprintf(out, sizeout, form, val/base, base);
	int baseten = this->tickerdata.lin.baseten;
	char buff[20];
	int len;
	sprintf(buff, "%i%n", baseten, &len);
	int slen = strlen(out);
	for (int i=0; i<len; i++) {
	    const char *str = (buff[i] == '-' ? "⁻" : cplot_supernum[buff[i] - '0']);
	    int slen1 = strlen(str);
	    if (slen+slen1+1 >= sizeout)
		break;
	    strcpy(out + slen, str);
	    slen += slen1;
	}
    }
    else if (base < 1) {
	const char *form = (step == (long)step ? "%.1f" : "%.2f");
	snprintf(out, sizeout, form, val);
    }
    else {
	const char *form = (step == (long)step ? "%.0f" : "%.1f");
	snprintf(out, sizeout, form, val);
    }
    return val;
}

int cplot_init_ticker_caveman(struct cplot_ticker *this, double min, double max) {
    this->species = ticker_linear;
    this->get_tick = cplot_get_tick_linear;
    this->tickerdata.lin = (struct cplot_tickerdata_linear) {
	.nticks = 7,
	.step = (max - min) / 6,
	.min = min,
	.base = 1,
    };
    return this->tickerdata.lin.nticks;
}

int cplot_init_ticker_default(struct cplot_ticker *this, double min, double max) {
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
    double maxtick = best_step * (int)(max/best_step);
    double mintick = best_step * (int)(min/best_step);
    this->tickerdata.lin = (struct cplot_tickerdata_linear) {
	.nticks = iroundpos((maxtick - mintick) / best_step) + 1,
	.step = best_step,
	.min = mintick,
	.base = base,
	.baseten = maxpower,
    };

    return this->tickerdata.lin.nticks;
}
