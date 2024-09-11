void rotate25(unsigned *to, int tw, const unsigned *from, int fw, int fh) {
    for (int j=0; j<fh; j++)
	for (int i=0; i<fw; i++)
	    to[i*tw + fh-1-j] = from[j*fw + i];
}

void rotate75(unsigned *to, int tw, const unsigned *from, int fw, int fh) {
    for (int j=0; j<fh; j++)
	for (int i=0; i<fw; i++)
	    to[(fw-1-i)*tw + j] = from[j*fw + i];
}

static inline void tocanvas(unsigned *ptr, int value, unsigned color);

void get_rotated_wh(int w, int h, int *W, int *H, int *Shift, float rot100) {
    float si = sinf(rot100 * 3.14159265358979 / 50);
    float co = cosf(rot100 * 3.14159265358979 / 50);
    int xy[3][2] = {{0,h}, {w,0}, {w,h}};
    int xmin = 0, xmax = 0, ymin = 0, ymax = 0;
    for (int i=0; i<3; i++) {
	int x = xy[i][0] * co - xy[i][1] * si;
	int y = xy[i][0] * si + xy[i][1] * co;
	if (x < xmin)
	    xmin = x;
	else if (x > xmax)
	    xmax = x;
	if (y < ymin)
	    ymin = y;
	else if (y > ymax)
	    ymax = y;
    }
    *W = xmax - xmin + 2;
    *H = ymax - ymin + 2;
    if (Shift) {
	int xshift = (xmin < 0 ? -xmin : xmin) + 1;
	int yshift = (ymin < 0 ? -ymin : ymin) + 1;
	*Shift = yshift * *W + xshift;
    }
}

static void laita(unsigned *to, int tw, float y, float x, unsigned color) {
    float yx[] = {y, x};
    int yxind0[2], yxind1[2];
    float yxlength0[2], yxlength1[2];
    for (int i=0; i<2; i++) {
	yxind0[i] = round(yx[i]);
	float frac = yx[i] - yxind0[i];
	if (frac < 0.5) {
	    yxlength0[i] = 0.5 + frac;
	    yxind1[i] = yxind0[i] - 1;
	}
	else {
	    yxlength0[i] = 1 - (frac-0.5);
	    yxind1[i] = yxind0[i] + 1;
	}
	yxlength1[i] = 1 - yxlength0[i];
    }

    int ind = yxind0[0]*tw + yxind0[1];
    int value = round(255 * yxlength0[0] * yxlength0[1]);
    value = value < 128 ? value*2 : 255;
    tocanvas(to+ind, value, color);

    ind = yxind0[0]*tw + yxind1[1];
    value = round(255 * yxlength0[0] * yxlength1[1]);
    value = value < 128 ? value*2 : 255;
    tocanvas(to+ind, value, color);

    ind = yxind1[0]*tw + yxind0[1];
    value = round(255 * yxlength1[0] * yxlength0[1]);
    value = value < 128 ? value*2 : 255;
    tocanvas(to+ind, value, color);

    ind = yxind1[0]*tw + yxind1[1];
    value = round(255 * yxlength1[0] * yxlength1[1]);
    tocanvas(to+ind, value, color);
}

void rotate(
    unsigned *to, int tw, int th,
    const unsigned *from, int fw, int fh,
    float rot100)
{
    if (rot100 == 25)
	return rotate25(to, tw, from, fw, fh);
    if (rot100 == 75)
	return rotate75(to, tw, from, fw, fh);

    float si = sinf(rot100 * 3.14159265358979 / 50);
    float co = cosf(rot100 * 3.14159265358979 / 50);

    int new_w, new_h, shift;
    get_rotated_wh(fw, fh, &new_w, &new_h, &shift, rot100);
    unsigned *apu = malloc(new_w * new_h * sizeof(unsigned));
    memset(apu, -1, new_w * new_h * sizeof(unsigned));
    apu += shift;
    for (int y0=0; y0<fh; y0++) {
	for (int x0=0; x0<fw; x0++) {
	    float x1 = (x0+0.5)*co - (y0+0.5)*si;
	    float y1 = (x0+0.5)*si + (y0+0.5)*co;
	    laita(apu, new_w, y1, x1, from[y0*fw + x0]);
	}
    }
    apu -= shift;

    int cpw = min(tw, new_w),
	cph = min(th, new_h);
    for (int j=0; j<cph; j++)
	memcpy(to + j*tw, apu+j*new_w, cpw*sizeof(to[0]));

    free(apu);
}
