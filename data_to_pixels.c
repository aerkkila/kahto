static void get_datapx_i1(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const char *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround(pos * axislen);
    }
}

static void get_datapx_inv_i1(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const char *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround((1 - pos) * axislen);
    }
}

static void get_datapx_u1(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const unsigned char *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround(pos * axislen);
    }
}

static void get_datapx_inv_u1(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const unsigned char *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround((1 - pos) * axislen);
    }
}

static void get_datapx_i2(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const short *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround(pos * axislen);
    }
}

static void get_datapx_inv_i2(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const short *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround((1 - pos) * axislen);
    }
}

static void get_datapx_u2(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const unsigned short *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround(pos * axislen);
    }
}

static void get_datapx_inv_u2(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const unsigned short *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround((1 - pos) * axislen);
    }
}

static void get_datapx_i4(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const int *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround(pos * axislen);
    }
}

static void get_datapx_inv_i4(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const int *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround((1 - pos) * axislen);
    }
}

static void get_datapx_u4(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const unsigned *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround(pos * axislen);
    }
}

static void get_datapx_inv_u4(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const unsigned *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround((1 - pos) * axislen);
    }
}

static void get_datapx_i8(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const long *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround(pos * axislen);
    }
}

static void get_datapx_inv_i8(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const long *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround((1 - pos) * axislen);
    }
}

static void get_datapx_u8(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const long unsigned *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround(pos * axislen);
    }
}

static void get_datapx_inv_u8(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const long unsigned *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround((1 - pos) * axislen);
    }
}

static void get_datapx_f4(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const float *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround(pos * axislen);
    }
}

static void get_datapx_inv_f4(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const float *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround((1 - pos) * axislen);
    }
}

static void get_datapx_f8(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const double *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround(pos * axislen);
    }
}

static void get_datapx_inv_f8(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const double *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround((1 - pos) * axislen);
    }
}

static void get_datapx_f10(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const long double *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround(pos * axislen);
    }
}

static void get_datapx_inv_f10(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) {
    const long double *data = vdata;
    axislen--;
    data += istart;
    int len = iend - istart;
    for (int i=0; i<len; i++) {
	float pos = (data[i] - axismin) / axisdiff;
	out[i*2] = iround((1 - pos) * axislen);
    }
}

static void (*get_datapx[])(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) = {
    [cplot_i1] = get_datapx_i1,
    [cplot_u1] = get_datapx_u1,
    [cplot_i2] = get_datapx_i2,
    [cplot_u2] = get_datapx_u2,
    [cplot_i4] = get_datapx_i4,
    [cplot_u4] = get_datapx_u4,
    [cplot_i8] = get_datapx_i8,
    [cplot_u8] = get_datapx_u8,
    [cplot_f4] = get_datapx_f4,
    [cplot_f8] = get_datapx_f8,
    [cplot_f10] = get_datapx_f10,
};

static void (*get_datapx_inv[])(long istart, long iend, const void *vdata, short *out, int axismin, int axisdiff, int axislen) = {
    [cplot_i1] = get_datapx_inv_i1,
    [cplot_u1] = get_datapx_inv_u1,
    [cplot_i2] = get_datapx_inv_i2,
    [cplot_u2] = get_datapx_inv_u2,
    [cplot_i4] = get_datapx_inv_i4,
    [cplot_u4] = get_datapx_inv_u4,
    [cplot_i8] = get_datapx_inv_i8,
    [cplot_u8] = get_datapx_inv_u8,
    [cplot_f4] = get_datapx_inv_f4,
    [cplot_f8] = get_datapx_inv_f8,
    [cplot_f10] = get_datapx_inv_f10,
};
