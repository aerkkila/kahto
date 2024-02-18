static void get_datapx_@dtype(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const $dtype *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround(pos * axislen);
    }
}

static void get_datapx_inv_@dtype(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const $dtype *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround((1 - pos) * axislen);
    }
}
