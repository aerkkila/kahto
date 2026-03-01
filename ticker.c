#include <stdio.h>
#include <time.h>

/* get_tick should return the data value on the axis */

const char *kahto_supernum[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};

static inline long ifloor_ticker(double a) {
	a += 1e-7;
	long r = a;
	return r - (r > a);
}

static inline long iceil_ticker(double a) {
	a -= 1e-7;
	long r = a;
	return r + (r < a);
}

void kahto__sprint_supernum(char *out, int sizeout, int num) {
	char buff[20];
	int len;
	sprintf(buff, "%i%n", num, &len);
	int slen = 0;
	for (int i=0; i<len; i++) {
		const char *str = (buff[i] == '-' ? "⁻" : kahto_supernum[buff[i] - '0']);
		int slen1 = strlen(str);
		if (slen+slen1+1 >= sizeout)
			break;
		strcpy(out + slen, str);
		slen += slen1;
	}
}

double kahto_get_tick_linear(struct kahto_ticks *this, int ind, char **label, int sizelabel) {
	double step = this->tickerdata.lin.step,
		   base = this->tickerdata.lin.base,
		   min = this->tickerdata.lin.min;
	double val = ind * step + min;
	if (!sizelabel)
		return val;
	int baseten = this->tickerdata.lin.baseten;
	this->tickerdata.lin.out_omitted_coef = 0;

	if (baseten <= -4  || baseten >= 6) {
		const char *form = (step == (long)step ? "%.0f" : "%.1f");
		int nprinted = snprintf(*label, sizelabel, form, val/base, base);
		if (!this->tickerdata.lin.omit_coef) {
			nprinted += snprintf(*label+nprinted, sizelabel-nprinted, "\n10");
			kahto__sprint_supernum(*label+nprinted, sizelabel-nprinted, this->tickerdata.lin.baseten);
		}
		else
			this->tickerdata.lin.out_omitted_coef = 1;
	}
	else if (base < 1) {
		char buff[20];
		sprintf(buff, "%f", step);
		int ilast = 0, i = -1, idecimal = 0;
		while (buff[++i])
			if (buff[i] == '.' || buff[i] == ',') {
				idecimal = i;
				break;
			}
		while (buff[++i])
			if (buff[i] != '0')
				ilast = i;
		sprintf(buff, "%%.%if", ilast-idecimal);
		snprintf(*label, sizelabel, buff, val);
	}
	else {
		const char *form = (step == (long)step ? "%.0f" : "%.1f");
		snprintf(*label, sizelabel, form, val);
	}
	return val;
}

int kahto_get_maxn_subticks_linear(struct kahto_ticks *this) {
	return this->tickerdata.lin.nsubticks * (this->tickerdata.lin.nticks + 1);
}

int kahto_get_subticks_linear(struct kahto_ticks *this, float *pos) {
	double min = this->tickerdata.lin.min,
		   step = this->tickerdata.lin.step;
	int nsub = this->tickerdata.lin.nsubticks,
		nmaj = this->tickerdata.lin.nticks;
	int itick = 0;
	double substep = step / nsub;

	double room = min - this->axis->min;
	int n_under = (int)(room / substep);
	double min_under = min - n_under * substep;
	for (int i=0; i<n_under; i++)
		pos[itick++] = min_under + i*substep;

	for (int j=0; j<nmaj; j++)
		for (int i=1; i<nsub; i++)
			pos[itick++] = min + j*step + i*substep;
	return itick;
}

double kahto_get_tick_log(struct kahto_ticks *this, int ind, char **label, int sizelabel) {
	struct kahto_tickerdata_log *d = &this->tickerdata.log;
	int ival = d->ro_ilogmin + ind;
	float base = d->base;
	int ibase = base;
	if (base == ibase)
		snprintf(*label, sizelabel, "\e$%i^%i$", ibase, ival);
	else
		snprintf(*label, sizelabel, "\e$%.2f^%i$", base, ival);

	double multi = 1 / log(d->base);
	double axisminlog = log(this->axis->min) * multi;
	double axismaxlog = log(this->axis->max) * multi;

	double frac = (ival - axisminlog) / (axismaxlog - axisminlog);
	return this->axis->min + frac * (this->axis->max - this->axis->min);
}

int kahto_get_maxn_subticks_log(struct kahto_ticks *this) {
	int ibase = iceil(this->tickerdata.log.base);
	return this->tickerdata.log.nticks * (ibase-1);
}

int kahto_get_subticks_log(struct kahto_ticks *this, float *pos) {
	double ilogmin = this->tickerdata.log.ro_ilogmin,
		   min = this->axis->min,
		   max = this->axis->max,
		   base = this->tickerdata.log.base;
	int nmaj = this->tickerdata.common.nticks,
		itick = 0;

	double multi = 1 / log(base);
	double minlog = log(min) * multi;
	double maxlog = log(max) * multi;

	double lsubfrac[iceil(base)];
	for (int i=2; i<base; i++)
		lsubfrac[i] = log(i) * multi;

	for (int imaj=0; imaj<nmaj; imaj++) {
		int maj = ilogmin + imaj;
		double majfrac0 = (maj - minlog) / (maxlog - minlog);
		double majfrac1 = (maj+1 - minlog) / (maxlog - minlog);
		for (int isub=2; isub<base; isub++)
			pos[itick++] = min + majfrac0 * (max-min) + lsubfrac[isub] * (majfrac1-majfrac0) * (max-min);
	}

	return itick;
}

double kahto_get_tick_datetime_annual(struct kahto_ticks *this, int ind, char **label, int sizelabel) {
	int step = this->tickerdata.datetime.step;
	time_t min = this->tickerdata.datetime.min;
	struct tm tm = *gmtime(&min);
	tm.tm_year += ind * step;
	double val = timegm(&tm);
	if (this->tickerdata.datetime.form)
		strftime(*label, sizelabel, this->tickerdata.datetime.form, &tm);
	else if (tm.tm_yday == 0)
		snprintf(*label, sizelabel, "%04i", tm.tm_year+1900);
	else
		snprintf(*label, sizelabel, "%04i-%02i", tm.tm_year+1900, tm.tm_mon+1);
	return val;
}

double kahto_get_tick_datetime_monthly(struct kahto_ticks *this, int ind, char **label, int sizelabel) {
	int step = this->tickerdata.datetime.step;
	time_t min = this->tickerdata.datetime.min;
	struct tm tm = *gmtime(&min);
	tm.tm_mon += ind * step;
	double val = timegm(&tm);
	if (this->tickerdata.datetime.form)
		strftime(*label, sizelabel, this->tickerdata.datetime.form, &tm);
	else if (tm.tm_mday == 1)
		snprintf(*label, sizelabel, "%04i-%02i", tm.tm_year+1900, tm.tm_mon+1);
	else
		snprintf(*label, sizelabel, "%04i-%02i-%02i", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday);
	return val;
}

double kahto_get_tick_datetime_daily(struct kahto_ticks *this, int ind, char **label, int sizelabel) {
	int step = this->tickerdata.datetime.step;
	time_t min = this->tickerdata.datetime.min;
	struct tm tm = *gmtime(&min);
	tm.tm_mday += ind * step;
	double val = timegm(&tm);
	if (this->tickerdata.datetime.form)
		strftime(*label, sizelabel, this->tickerdata.datetime.form, &tm);
	else
		snprintf(*label, sizelabel, "%04i-%02i-%02i", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday);
	return val;
}

double kahto_get_tick_arbitrary_datacoord(struct kahto_ticks *this, int ind, char **label, int _) {
	*label = this->tickerdata.arb.labels.m[ind];
	return this->tickerdata.arb.ticks[ind];
}

double kahto_get_tick_arbitrary_datacoord_enum(struct kahto_ticks *this, int ind, char **label, int _) {
	*label = this->tickerdata.arb.labels.m[ind];
	return ind;
}

double kahto_get_tick_arbitrary_relcoord(struct kahto_ticks *this, int ind, char **label, int _) {
	*label = this->tickerdata.arb.labels.m[ind];
	double min = this->tickerdata.arb.min,
		   max = this->tickerdata.arb.max;
	return min + this->tickerdata.arb.ticks[ind] * (max - min);
}

int kahto_get_subticks_arbitrary_relcoord(struct kahto_ticks *this, float *pos) {
	struct kahto_tickerdata_arbitrary *d = &this->tickerdata.arb;
	double *s = d->subticks;
	double min = this->tickerdata.arb.min,
		   max = this->tickerdata.arb.max;
	for (int i=d->nsubticks-1; i>=0; i--)
		pos[i] = min + s[i] * (max - min);
	return d->nsubticks;
}

int kahto_get_subticks_arbitrary_datacoord(struct kahto_ticks *this, float *pos) {
	struct kahto_tickerdata_arbitrary *d = &this->tickerdata.arb;
	double *s = d->subticks;
	if (d->nsubticks == kahto_automatic) { // if automatic, subticks are 0.5, 1.5, etc.
		for (int i=d->nticks-1; i>=0; i--)
			pos[i] = i + 0.5;
		return d->nticks-1;
	}
	for (int i=d->nsubticks-1; i>=0; i--)
		pos[i] = s[i];
	return d->nsubticks;
}

int kahto_get_maxn_subticks_arbitrary(struct kahto_ticks *this) {
	int n = this->tickerdata.arb.nsubticks;
	if (n == kahto_automatic && this->get_subticks != kahto_get_subticks_arbitrary_datacoord)
		n = 0; // not supported
	return n == kahto_automatic ? this->tickerdata.arb.nticks-1 : n;
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
	if (maxpower_out)
		*maxpower_out = maxpower;
	return base;
}

void kahto_init_ticker_simple(struct kahto_ticks *this, double min, double max) {
	this->species = kahto_ticker_linear;
	this->get_tick = kahto_get_tick_linear;
	double target_nticks0 = this->tickerdata.lin.target_nticks;
	double target_nticks = target_nticks0 ? target_nticks0 : default_linear_target_nticks;
	int maxpower;
	double base = get_linticker_base(max-min, &maxpower);
	this->tickerdata.lin.nticks = target_nticks;
	this->tickerdata.lin.step = (max - min) / (target_nticks-1);
	this->tickerdata.lin.min = min;
	this->tickerdata.lin.base = base;
	this->tickerdata.lin.baseten = maxpower;
}

static void _init_ticker_arbitrary(struct kahto_ticks *this, double min, double max) {
	this->species = kahto_ticker_arbitrary;
	this->get_maxn_subticks = kahto_get_maxn_subticks_arbitrary;
	this->tickerdata.arb.min = min;
	this->tickerdata.arb.max = max;
}

void kahto_init_ticker_arbitrary_datacoord(struct kahto_ticks *this, double min, double max) {
	_init_ticker_arbitrary(this, min, max);
	this->get_tick = kahto_get_tick_arbitrary_datacoord;
	this->get_subticks = kahto_get_subticks_arbitrary_datacoord;
}

void kahto_init_ticker_arbitrary_datacoord_enum(struct kahto_ticks *this, double min, double max) {
	_init_ticker_arbitrary(this, min, max);
	this->get_tick = kahto_get_tick_arbitrary_datacoord_enum;
	this->get_subticks = kahto_get_subticks_arbitrary_datacoord;
}

void kahto_init_ticker_arbitrary_relcoord(struct kahto_ticks *this, double min, double max) {
	_init_ticker_arbitrary(this, min, max);
	this->get_tick = kahto_get_tick_arbitrary_relcoord;
	this->get_subticks = kahto_get_subticks_arbitrary_relcoord;
}

void kahto_init_ticker_log(struct kahto_ticks *this, double min, double max) {
	this->species = kahto_ticker_log;
	this->get_tick = kahto_get_tick_log;
	this->get_subticks = kahto_get_subticks_log;
	this->get_maxn_subticks = kahto_get_maxn_subticks_log;
	struct kahto_tickerdata_log *data = &this->tickerdata.log;
	if (!data->base)
		data->base = 10;
	double logmin = log(min) / log(data->base);
	double logmax = log(max) / log(data->base);
	int ilogmin = data->ro_ilogmin = floor(logmin);
	int ilogmax = data->ro_ilogmax = ceil(logmax);
	data->nticks = ilogmax - ilogmin;
	if (logmax - logmin == data->nticks+1) {
		++data->ro_ilogmax;
		++data->nticks;
	}
}

void kahto_init_ticker_default(struct kahto_ticks *this, double min, double max) {
	if (this->axis->logscale) {
		this->init = kahto_init_ticker_log;
		return kahto_init_ticker_log(this, min, max);
	}

	double step_opts0[] = {1, 1.5, 2, 2.5, 5};
	int subticks_mul0[] = {4, 3, 4, 5, 5};
	int nstep_opt0 = arrlen(step_opts0);
	double step_opts1[] = {1, 2, 5};
	int subticks_mul1[] = {4, 4, 5};
	int nstep_opt1 = arrlen(step_opts1);
	double *step_opts;
	int *subticks_mul;
	int nstep_opt;
	if (this->integers_only) {
		step_opts = step_opts1;
		nstep_opt = nstep_opt1;
		subticks_mul = subticks_mul1;
	}
	else {
		step_opts = step_opts0;
		nstep_opt = nstep_opt0;
		subticks_mul = subticks_mul0;
	}

	double target_n_orig = this->tickerdata.lin.target_nticks;
	double target_n = target_n_orig ? target_n_orig : default_linear_target_nticks;

	int tenpower;
	double base0, base1, diff = max - min;
	base0 = get_linticker_base(diff, &tenpower);
	long imin0 = iceil_ticker(min/base0) * 10;
	long imax0 = ifloor_ticker(max/base0) * 10;
	base1 = base0*0.1;
	long imin1 = iceil_ticker(min/base1) * 10;
	long imax1 = ifloor_ticker(max/base1) * 10;

	struct best {
		double min, max;
		int iopt, which, n, diff;
	} best = {.diff = 9999999};

	for (int iopt=0; iopt<nstep_opt; iopt++) {
		int istep = iroundpos(step_opts[iopt]*10);
		long iminnow0 = imin0 / istep * istep;
		iminnow0 += istep * (iminnow0 < imin0);
		long imaxnow0 = imax0 / istep * istep;
		imaxnow0 -= istep * (imaxnow0 > imax0);
		long iminnow1 = imin1 / istep * istep;
		iminnow1 += istep * (iminnow1 < imin1);
		long imaxnow1 = imax1 / istep * istep;
		imaxnow1 -= istep * (imaxnow1 > imax1);
		int n0 = (imaxnow0 - iminnow0) / istep + 1,
			n1 = (imaxnow1 - iminnow1) / istep + 1;
		int diff0 = target_n - n0,
			diff1 = target_n - n1;
		if (diff0 < 0)
			diff0 = -diff0 * 2; // this prefers (target - n) to (target + n)
		if (diff1 < 0)
			diff1 = -diff1 * 2;
		if (diff0 < best.diff)
			best = (struct best) {
				.min = iminnow0 / 10.0 * base0,
				.max = imaxnow0 / 10.0 * base0,
				.iopt = iopt,
				.which = 0,
				.n = n0,
				.diff = diff0,
			};
		if (diff1 < best.diff)
			best = (struct best) {
				.min = iminnow1 / 10.0 * base1,
				.max = imaxnow1 / 10.0 * base1,
				.iopt = iopt,
				.which = 1,
				.n = n1,
				.diff = diff1,
			};
	}

	this->species = kahto_ticker_linear;
	this->get_tick = kahto_get_tick_linear;
	this->get_maxn_subticks = kahto_get_maxn_subticks_linear;
	this->get_subticks = kahto_get_subticks_linear;
	this->tickerdata.lin.nticks = best.n;
	this->tickerdata.lin.step = step_opts[best.iopt] * (best.which ? base1 : base0);
	this->tickerdata.lin.min = best.min;
	this->tickerdata.lin.base = best.which ? base1 : base0;
	this->tickerdata.lin.baseten = tenpower - best.which;
	this->tickerdata.lin.target_nticks = target_n_orig;
	this->tickerdata.lin.nsubticks = subticks_mul[best.iopt];
}

void kahto_init_ticker_datetime(struct kahto_ticks *this, double min, double max) {
	time_t time = min;
	struct tm tm0 = *gmtime(&time);
	time = max;
	struct tm tm1 = *gmtime(&time);
	long best_diff = 10000000;
	int datetype = 0, step;
	double target_n_orig = this->tickerdata.datetime.target_nticks;
	double target_n = target_n_orig ? target_n_orig : default_linear_target_nticks;

	void check_bestness(long n, int type, const int *opts, int iopt) {
		long diff = Abs(n - target_n);
		if (diff < 0) diff = -diff;
		if (diff < best_diff) {
			best_diff = diff;
			step = opts[iopt];
			datetype = type;
		}
	}

	/* days */ {
		int opts[] = {1, 2, 5, 10};
		for (int iopt=0; iopt<arrlen(opts); iopt++) {
			long n = ((long)max - (long)min) / (86400 * opts[iopt]);
			check_bestness(n, 0, opts, iopt);
		}
	}
	/* months */ {
		int opts[] = {1, 2, 3, 4, 6};
		for (int iopt=0; iopt<arrlen(opts); iopt++) {
			int n = tm1.tm_year*12 + tm1.tm_mon - (tm0.tm_year*12 + tm0.tm_mon);
			n /= opts[iopt];
			check_bestness(n, 1, opts, iopt);
		}
	}
	/* years */ {
		int opts[] = {1, 2, 5, 15, 25};
		for (int iopt=0; iopt<arrlen(opts); iopt++) {
			int n = tm1.tm_year - tm0.tm_year;
			n /= opts[iopt];
			check_bestness(n, 2, opts, iopt);
		}
	}

	double firsttick;
	int nticks, add, mod;
	switch (datetype) {
		case 0:
			this->get_tick = kahto_get_tick_datetime_daily;
			add = !!(tm0.tm_sec + tm0.tm_min + tm0.tm_hour);
			tm0.tm_sec = tm0.tm_min = tm0.tm_hour = 0;
			tm0.tm_mday += add;
			mod = (tm0.tm_mday-1) % step;
			tm0.tm_mday += !!(mod) * (step - mod);
			firsttick = timegm(&tm0);
			nticks = (max - firsttick) / 86400;
			break;
		case 1:
			this->get_tick = kahto_get_tick_datetime_monthly;
			add = !!(tm0.tm_sec + tm0.tm_min + tm0.tm_hour + tm0.tm_mday-1);
			tm0.tm_sec = tm0.tm_min = tm0.tm_hour = 0;
			tm0.tm_mday = 1;
			tm0.tm_mon += add;
			mod = tm0.tm_mon % step;
			tm0.tm_mon += !!(mod) * (step - mod);
			firsttick = timegm(&tm0);
			int nmonths = tm1.tm_year * 12 + tm1.tm_mon - (tm0.tm_year * 12 + tm0.tm_mon);
			nticks = nmonths / step + 1;
			break;
		case 2:
			this->get_tick = kahto_get_tick_datetime_annual;
			struct tm year0tm = {.tm_year = tm0.tm_year, .tm_mday = 1};
			time_t year0_time = timegm(&year0tm);
			int year0 = tm0.tm_year + (year0_time != min);
			mod = year0 % step;
			year0 += !!(mod) * (step - mod);
			tm0 = (struct tm) { .tm_year = year0, .tm_mday = 1, };
			firsttick = timegm(&tm0);
			nticks = (tm1.tm_year - year0) / step + 1;
			printf("%i, %i\n", tm1.tm_year+1900, tm1.tm_mon+1);
			break;
		default: __builtin_unreachable();
	}

	struct kahto_tickerdata_datetime *dt = &this->tickerdata.datetime;
	dt->nticks = nticks;
	dt->step = step;
	dt->min = firsttick;
}
