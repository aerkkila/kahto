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
static void get_datapx_@dtype(long istart, long iend, const void *vdata, short *out, double axismin, double axisdiff, int axislen, int stridein, int strideout, int addthis) {
    const $dtype *data = vdata;
    axislen--;
    data += istart * stridein;
    long len = iend - istart;
    for (long i=0; i<len; i++) {
	long iout = i * strideout;
	long iin = i * stridein;
#if cplot_@dtype >= cplot_f4 // any floating point
	if (
#if cplot_@dtype == cplot_f4
	    my_isnan_float(data[iin])
#else
	    my_isnan_double(data[iin])
#endif
	) {
	    out[iout] = -9999;
	    continue;
	}
#endif
	float pos = (data[iin] - axismin) / axisdiff;
	out[iout] = iround(pos * axislen - 0.5) + addthis;
    }
}

static void get_datapx_inv_@dtype(long istart, long iend, const void *vdata, short *out, double axismin, double axisdiff, int axislen, int stridein, int strideout, int addthis) {
    const $dtype *data = vdata;
    axislen--;
    data += istart * stridein;
    long len = iend - istart;
    for (long i=0; i<len; i++) {
	long iout = i * strideout;
	long iin = i * stridein;
#if cplot_@dtype >= cplot_f4 // any floating point
	if (
#if cplot_@dtype == cplot_f4
	    my_isnan_float(data[iin])
#else
	    my_isnan_double(data[iin])
#endif
	) {
	    out[iout] = -9999;
	    continue;
	}
#endif
	float pos = (data[iin] - axismin) / axisdiff;
	out[iout] = iround((1 - pos) * axislen - 0.5) + addthis;
    }
}

static void get_datalevels_@dtype(long istart, long iend, const void *vdata, unsigned char *out, double axismin, double axisdiff, float scale, int stridein) {
    const $dtype *data = vdata;
    data += istart * stridein;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i*stridein] - axismin) / axisdiff;
	out[i] = iround(pos*scale);
    }
}

static double get_min_@dtype(const void *vdata, long length, int stride) {
    const $dtype *data = vdata;
    double min = DBL_MAX;
    for (long i=0; i<length; i++) {
	if (data[i*stride] < min)
	    min = data[i*stride];
    }
    return min;
}

static double get_min_with_float_@dtype(const void *vdata, long length, float *other, int stride, int stride_oth) {
    const $dtype *data = vdata;
    double min = DBL_MAX;
    for (long i=0; i<length; i++) {
	if (data[i*stride] < min && !my_isnan_float(other[i*stride_oth]))
	    min = data[i*stride];
    }
    return min;
}

static double get_min_with_double_@dtype(const void *vdata, long length, double *other, int stride, int stride_oth) {
    const $dtype *data = vdata;
    double min = DBL_MAX;
    for (long i=0; i<length; i++) {
	if (data[i*stride] < min && !my_isnan_double(other[i*stride_oth]))
	    min = data[i*stride];
    }
    return min;
}

static double get_max_@dtype(const void *vdata, long length, int stride) {
    const $dtype *data = vdata;
    double max = -DBL_MAX;
    for (long i=0; i<length; i++) {
	if (data[i*stride] > max)
	    max = data[i*stride];
    }
    return max;
}

static double get_max_with_float_@dtype(const void *vdata, long length, float *other, int stride, int stride_oth) {
    const $dtype *data = vdata;
    double max = -DBL_MAX;
    for (long i=0; i<length; i++) {
	if (data[i*stride] > max && !my_isnan_float(other[i*stride_oth]))
	    max = data[i*stride];
    }
    return max;
}

static double get_max_with_double_@dtype(const void *vdata, long length, double *other, int stride, int stride_oth) {
    const $dtype *data = vdata;
    double max = -DBL_MAX;
    for (long i=0; i<length; i++) {
	if (data[i*stride] > max && !my_isnan_double(other[i*stride_oth]))
	    max = data[i*stride];
    }
    return max;
}

static void get_minmax_@dtype(const void *vdata, long length, double *minmax, int stride) {
    const $dtype *data = vdata;
    minmax[0] = DBL_MAX;
    minmax[1] = -DBL_MAX;
    for (long i=0; i<length; i++) {
	long ind = i*stride;
	if (data[ind] < minmax[0])
	    minmax[0] = data[ind];
	if (data[ind] > minmax[1])
	    minmax[1] = data[ind];
    }
}

static void get_minmax_with_double_@dtype(const void *vdata, long length, double *minmax, double *other, int stride, int stride_oth) {
    const $dtype *data = vdata;
    minmax[0] = DBL_MAX;
    minmax[1] = -DBL_MAX;
    for (long i=0; i<length; i++) {
	long ind = i*stride,
	     ind_oth = i*stride_oth;
	if (data[ind] < minmax[0] && !my_isnan_double(other[ind_oth]))
	    minmax[0] = data[ind];
	if (data[ind] > minmax[1] && !my_isnan_double(other[ind_oth]))
	    minmax[1] = data[ind];
    }
}

static void get_minmax_with_float_@dtype(const void *vdata, long length, double *minmax, float *other, int stride, int stride_oth) {
    const $dtype *data = vdata;
    minmax[0] = DBL_MAX;
    minmax[1] = -DBL_MAX;
    for (long i=0; i<length; i++) {
	long ind = i*stride,
	     ind_oth = i*stride_oth;
	if (data[ind] < minmax[0] && !my_isnan_float(other[ind_oth]))
	    minmax[0] = data[ind];
	if (data[ind] > minmax[1] && !my_isnan_float(other[ind_oth]))
	    minmax[1] = data[ind];
    }
}
