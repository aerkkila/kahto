#include <float.h>

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

@startperl
static void get_datapx_@dtype(long istart, long iend, const void *vdata, short *out, double axismin, double axisdiff, int axislen, int strideout) {
    const $dtype *data = vdata;
    axislen--;
    data += istart;
    long len = iend - istart;
    for (int i=0; i<len; i++) {
#if cplot_@dtype >= cplot_f4 // any floating point
	if (
#if cplot_@dtype == cplot_f4
	    my_isnan_float(data[i])
#else
	    my_isnan_double(data[i])
#endif
	)
	    continue;
#endif
	float pos = (data[i] - axismin) / axisdiff;
	out[i*strideout] = iround(pos * axislen - 0.5);
    }
}

static void get_datapx_inv_@dtype(long istart, long iend, const void *vdata, short *out, double axismin, double axisdiff, int axislen, int strideout) {
    const $dtype *data = vdata;
    axislen--;
    data += istart;
    long len = iend - istart;
    for (long i=0; i<len; i++) {
#if cplot_@dtype >= cplot_f4 // any floating point
	if (
#if cplot_@dtype == cplot_f4
	    my_isnan_float(data[i])
#else
	    my_isnan_double(data[i])
#endif
	)
	    continue;
#endif
	float pos = (data[i] - axismin) / axisdiff;
	out[i*strideout] = iround((1 - pos) * axislen - 0.5);
    }
}

static void get_datalevels_@dtype(long istart, long iend, const void *vdata, unsigned char *out, double axismin, double axisdiff, float scale) {
    const $dtype *data = vdata;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i] = iround(pos*scale);
    }
}

static double get_min_@dtype(const void *vdata, long length) {
    const $dtype *data = vdata;
    double min = DBL_MAX;
    for (long i=0; i<length; i++) {
	if (data[i] < min)
	    min = data[i];
    }
    return min;
}

static double get_min_with_float_@dtype(const void *vdata, long length, float *other) {
    const $dtype *data = vdata;
    double min = DBL_MAX;
    for (long i=0; i<length; i++) {
	if (data[i] < min && !my_isnan_float(other[i]))
	    min = data[i];
    }
    return min;
}

static double get_min_with_double_@dtype(const void *vdata, long length, double *other) {
    const $dtype *data = vdata;
    double min = DBL_MAX;
    for (long i=0; i<length; i++) {
	if (data[i] < min && !my_isnan_double(other[i]))
	    min = data[i];
    }
    return min;
}

static double get_max_@dtype(const void *vdata, long length) {
    const $dtype *data = vdata;
    double max = -DBL_MAX;
    for (long i=0; i<length; i++) {
	if (data[i] > max)
	    max = data[i];
    }
    return max;
}

static double get_max_with_float_@dtype(const void *vdata, long length, float *other) {
    const $dtype *data = vdata;
    double max = -DBL_MAX;
    for (long i=0; i<length; i++) {
	if (data[i] > max && !my_isnan_float(other[i]))
	    max = data[i];
    }
    return max;
}

static double get_max_with_double_@dtype(const void *vdata, long length, double *other) {
    const $dtype *data = vdata;
    double max = -DBL_MAX;
    for (long i=0; i<length; i++) {
	if (data[i] > max && !my_isnan_double(other[i]))
	    max = data[i];
    }
    return max;
}

static void get_minmax_@dtype(const void *vdata, long length, double *minmax) {
    const $dtype *data = vdata;
    minmax[0] = DBL_MAX;
    minmax[1] = -DBL_MAX;
    for (long i=0; i<length; i++) {
	if (data[i] < minmax[0])
	    minmax[0] = data[i];
	if (data[i] > minmax[1])
	    minmax[1] = data[i];
    }
}

static void get_minmax_with_double_@dtype(const void *vdata, long length, double *minmax, double *other) {
    const $dtype *data = vdata;
    minmax[0] = DBL_MAX;
    minmax[1] = -DBL_MAX;
    for (long i=0; i<length; i++) {
	if (data[i] < minmax[0] && !my_isnan_double(other[i]))
	    minmax[0] = data[i];
	if (data[i] > minmax[1] && !my_isnan_double(other[i]))
	    minmax[1] = data[i];
    }
}

static void get_minmax_with_float_@dtype(const void *vdata, long length, double *minmax, float *other) {
    const $dtype *data = vdata;
    minmax[0] = DBL_MAX;
    minmax[1] = -DBL_MAX;
    for (long i=0; i<length; i++) {
	if (data[i] < minmax[0] && !my_isnan_float(other[i]))
	    minmax[0] = data[i];
	if (data[i] > minmax[1] && !my_isnan_float(other[i]))
	    minmax[1] = data[i];
    }
}
