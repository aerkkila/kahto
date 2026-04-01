#include <float.h>
#include "definitions.h"

static inline int my_isnan_float(float f) {
	const uint32_t exponent = ((1u<<31)-1) - ((1u<<(31-8))-1);
	uint32_t bits;
	memcpy(&bits, &f, 4);
	return (bits & exponent) == exponent;
}

static inline int my_isnan_double(double f) {
	const uint64_t exponent = ((1lu<<63)-1) - ((1lu<<(63-11))-1);
	uint64_t bits;
	memcpy(&bits, &f, 8);
	return (bits & exponent) == exponent;
}

#define my_isnan(a) ((typeof(a))1.5 == 1 ? 0 : sizeof(a) == 4 ? my_isnan_float(a) : my_isnan_double(a))

@startperl
static short get_datapx_@dtype(const void *vdata, long ind, double axismin, double axisdiff, int axislen, double _) {
	const $dtype *data = vdata;
	if (my_isnan(data[ind]))
		return NOT_A_PIXEL;
	float pos = (data[ind] - axismin) / axisdiff;
	return iround(pos * (axislen-1) - 0.5);
}

static short get_datapx_inv_@dtype(const void *vdata, long ind, double axismin, double axisdiff, int axislen, double _) {
	const $dtype *data = vdata;
	if (my_isnan(data[ind]))
		return NOT_A_PIXEL;
	float pos = (data[ind] - axismin) / axisdiff;
	return iround((1-pos) * (axislen-1) - 0.5);
}

static short get_datapx_log_@dtype(const void *vdata, long ind, double axismin, double axisdiff, int axislen, double multiplier) {
	const $dtype *data = vdata;
	if (my_isnan(data[ind]))
		return NOT_A_PIXEL;
	float pos = (log(data[ind])*multiplier - axismin) / axisdiff;
	return iround(pos * (axislen-1) - 0.5);
}

static short get_datapx_log_inv_@dtype(const void *vdata, long ind, double axismin, double axisdiff, int axislen, double multiplier) {
	const $dtype *data = vdata;
	if (my_isnan(data[ind]))
		return NOT_A_PIXEL;
	float pos = (log(data[ind])*multiplier - axismin) / axisdiff;
	return iround((1-pos) * (axislen-1) - 0.5);
}

static short get_datalevel_@dtype(const void *vdata, long ind, double *axislim, int axislen) {
	const $dtype *data = vdata;
	if (my_isnan(data[ind]))
		return -1;
	float pos = (data[ind]-axislim[0]) / (axislim[2]-axislim[0]);
	if (pos < 0) pos = 0;
	if (pos > 1) pos = 1;
	return iroundpos(pos * axislen);
}

static short get_datalevel_with_center_@dtype(const void *vdata, long ind, double *axislim, int axislen) {
	const $dtype *data = vdata;
	if (my_isnan(data[ind]))
		return -1;
	int side = data[ind] >= axislim[1];
	float pos = (data[ind]-axislim[side]) / (axislim[side+1]-axislim[side]) + 0.5*side;
	if (pos < 0) pos = 0;
	if (pos > 1) pos = 1;
	return iroundpos(pos * axislen);
}

static double get_floating_@dtype(const void *vdata, long ind) {
	return ((const $dtype*)vdata)[ind];
}

static double get_min_@dtype(const void *vdata, long length, int stride) {
	const $dtype *data = vdata;
	double min = DBL_MAX;
	long i=0;
	for (; i<length; i++)
		if (!my_isnan(data[i*stride]))
			goto a;
	return NAN;
a:
	for (; i<length; i++) {
		$dtype val = data[i*stride];
		if (!my_isnan(val) && val < min)
			min = val;
	}
	return min;
}

static double get_max_@dtype(const void *vdata, long length, int stride) {
	const $dtype *data = vdata;
	double max = -DBL_MAX;
	long i=0;
	for (; i<length; i++)
		if (!my_isnan(data[i*stride]))
			goto a;
	return NAN;
a:
	for (; i<length; i++) {
		$dtype val = data[i*stride];
		if (!my_isnan(val) && val > max)
			max = val;
	}
	return max;
}

static void get_minmax_@dtype(const void *vdata, long length, double *minmax, int stride) {
	const $dtype *data = vdata;
	minmax[0] = DBL_MAX;
	minmax[1] = -DBL_MAX;
	long i=0;
	for (; i<length; i++)
		if (!my_isnan(data[i*stride]))
			goto a;
	minmax[0] = minmax[1] = NAN;
	return;
a:
	for (; i<length; i++) {
		long ind = i*stride;
		if (my_isnan(data[ind]))
			continue;
		if (data[ind] < minmax[0])
			minmax[0] = data[ind];
		if (data[ind] > minmax[1])
			minmax[1] = data[ind];
	}
}
