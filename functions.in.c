#include <float.h>

static void get_datapx_@dtype(long istart, long iend, const void *vdata, short *out, int axismin, double axisdiff, int axislen) {
    const $dtype *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround(pos * axislen - 0.5);
    }
}

static void get_datapx_inv_@dtype(long istart, long iend, const void *vdata, short *out, int axismin, double axisdiff, int axislen) {
    const $dtype *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround((1 - pos) * axislen - 0.5);
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

static double get_max_@dtype(const void *vdata, long length) {
    const $dtype *data = vdata;
    double max = -DBL_MAX;
    for (long i=0; i<length; i++) {
	if (data[i] > max)
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
