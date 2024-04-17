#include <stdio.h>
#include <time.h>

const char *cplot_supernum[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};

double cplot_get_tick_linear(struct cplot_ticker *this, int ind, char **label, int sizelabel) {
    double step = this->tickerdata.lin.step,
	   base = this->tickerdata.lin.base,
	   min = this->tickerdata.lin.min;
    double val = ind * step + min;

    if (base < 0.05 || base > 5000) {
	const char *form = (step == (long)step ? "%.0f\n10" : "%.1f\n10");
	snprintf(*label, sizelabel, form, val/base, base);
	int baseten = this->tickerdata.lin.baseten;
	char buff[20];
	int len;
	sprintf(buff, "%i%n", baseten, &len);
	int slen = strlen(*label);
	for (int i=0; i<len; i++) {
	    const char *str = (buff[i] == '-' ? "⁻" : cplot_supernum[buff[i] - '0']);
	    int slen1 = strlen(str);
	    if (slen+slen1+1 >= sizelabel)
		break;
	    strcpy(*label + slen, str);
	    slen += slen1;
	}
    }
    else if (base < 1) {
	const char *form = (step == (long)step ? "%.1f" : "%.2f");
	snprintf(*label, sizelabel, form, val);
    }
    else {
	const char *form = (step == (long)step ? "%.0f" : "%.1f");
	snprintf(*label, sizelabel, form, val);
    }
    return val;
}

double cplot_get_tick_datetime_annual(struct cplot_ticker *this, int ind, char **label, int sizelabel) {
    int step = this->tickerdata.datetime.step;
    time_t min = this->tickerdata.datetime.min;
    struct tm tm = *gmtime(&min);
    tm.tm_year += ind * step;
    double val = timegm(&tm);
    if (tm.tm_yday == 0)
	snprintf(*label, sizelabel, "%04i", tm.tm_year+1900);
    else
	snprintf(*label, sizelabel, "%4i-%02i", tm.tm_year+1900, tm.tm_mon+1);
    return val;
}

double cplot_get_tick_arbitrary_datacoord(struct cplot_ticker *this, int ind, char **label, int _) {
    *label = this->tickerdata.arb.labels[ind];
    return this->tickerdata.arb.ticks[ind];
}

double cplot_get_tick_arbitrary_relcoord(struct cplot_ticker *this, int ind, char **label, int _) {
    *label = this->tickerdata.arb.labels[ind];
    double min = this->tickerdata.arb.min,
	   max = this->tickerdata.arb.max;
    return min + this->tickerdata.arb.ticks[ind] * (max - min);
}

#define default_linear_target_nticks 7
#define default_datetime_nticksmin 4
#define default_datetime_nticksmax 10

static double get_linticker_base(double max, int *maxpower_out) {
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
    //max *= maxsign;
    if (maxpower_out)
	*maxpower_out = maxpower;
    return base;
}

void cplot_init_ticker_simple(struct cplot_ticker *this, double min, double max) {
    this->species = cplot_ticker_linear;
    this->get_tick = cplot_get_tick_linear;
    double target_nticks0 = this->tickerdata.lin.target_nticks;
    double target_nticks = target_nticks0 ? target_nticks0 : default_linear_target_nticks;
    int maxpower;
    double base = get_linticker_base(max, &maxpower);
    this->tickerdata.lin = (struct cplot_tickerdata_linear) {
	.nticks = target_nticks,
	.step = (max - min) / (target_nticks-1),
	.min = min,
	.base = base,
	.baseten = maxpower,
    };
}

void cplot_init_ticker_arbitrary_datacoord(struct cplot_ticker *this, double min, double max) {
    this->species = cplot_ticker_arbitrary;
    this->get_tick = cplot_get_tick_arbitrary_datacoord;
    this->tickerdata.arb.min = min;
    this->tickerdata.arb.max = max;
}

void cplot_init_ticker_arbitrary_relcoord(struct cplot_ticker *this, double min, double max) {
    this->species = cplot_ticker_arbitrary;
    this->get_tick = cplot_get_tick_arbitrary_relcoord;
    this->tickerdata.arb.min = min;
    this->tickerdata.arb.max = max;
}

void cplot_init_ticker_default(struct cplot_ticker *this, double min, double max) {
    double step_opts[] = {1, 1.5, 2, 2.5, 5};

    int maxpower;
    double base = get_linticker_base(max, &maxpower);

    int nstep_opt = arrlen(step_opts);
    double best_step, best_diff = DBL_MAX;
    double target_n_orig = this->tickerdata.lin.target_nticks;
    double target_n = target_n_orig ? target_n_orig : default_linear_target_nticks;
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

    this->species = cplot_ticker_linear;
    this->get_tick = cplot_get_tick_linear;
    double maxtick = best_step * floor(max/best_step);
    double mintick = best_step * ceil(min/best_step);
    this->tickerdata.lin = (struct cplot_tickerdata_linear) {
	.nticks = iroundpos((maxtick - mintick) / best_step) + 1,
	.step = best_step,
	.min = mintick,
	.base = base,
	.baseten = maxpower,
	.target_nticks = target_n_orig,
    };
}

void cplot_init_ticker_datetime(struct cplot_ticker *this, double min, double max) {
    time_t time = min;
    struct tm tm0 = *gmtime(&time);
    time = max;
    struct tm tm1 = *gmtime(&time);
    double nticksmin0 = this->tickerdata.datetime.nticksmin;
    double nticksmax0 = this->tickerdata.datetime.nticksmax;
    int nticksmin = nticksmin0 ? nticksmin0 : default_datetime_nticksmin;
    int nticksmax = nticksmax0 ? nticksmax0 : default_datetime_nticksmax;

    this->species = cplot_ticker_datetime;
    if (tm1.tm_year - tm0.tm_year + (tm1.tm_mon >= tm0.tm_mon) >= nticksmin) {
	int nticks = tm1.tm_year - tm0.tm_year;
	if (nticks < nticksmin)
	    ;
	struct tm tm_min = {.tm_year = tm0.tm_year+1, .tm_mday=1};
	int mul5 = nticks / nticksmin;
	int n5 = mul5 / 5;
	mul5 = n5 * 5;
	int step = 1;
	if (mul5) {
	    tm_min.tm_year = tm_min.tm_year / mul5 * mul5 + mul5; // täsmälleen alkuhetkeä ei nyt merkitä
	    nticks = (tm1.tm_year / mul5 * mul5 - tm_min.tm_year) / mul5 + 1;
	    step = mul5;
	}
	int divisor = nticks / nticksmax + 1;
	nticks /= divisor;
	step *= divisor;
	time_t time_min = timegm(&tm_min);
	this->get_tick = cplot_get_tick_datetime_annual;
	this->tickerdata.datetime = (struct cplot_tickerdata_datetime) {
	    .step = step,
	    .min = time_min,
	    .nticksmin = nticksmin0,
	    .nticksmax = nticksmax0,
	    .nticks = nticks,
	};
    }
    else if (tm1.tm_mon - tm0.tm_mon + (tm1.tm_year-tm0.tm_year)*12 >= nticksmin)
	fprintf(stderr, "datetime-tikkeri on kesken\n");
    else
	fprintf(stderr, "datetime-tikkeri on kesken\n");
}
