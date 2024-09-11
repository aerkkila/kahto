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

void get_rotated_area(float w, float h, float *restrict area, float rot100) {
    if ((int)(rot100*1000) % 100000 == 0) {
	area[0] = 0;
	area[1] = 0;
	area[2] = w;
	area[3] = h;
	return;
    }
    float si = sinf(rot100 * 3.14159265358979 / 50);
    float co = cosf(rot100 * 3.14159265358979 / 50);
    float xy[3][2] = {{0,h}, {w,0}, {w,h}};
    memset(area, 0, 4*sizeof(area[0]));
    for (int i=0; i<3; i++) {
	float x = xy[i][0] * co - xy[i][1] * si;
	float y = xy[i][0] * si + xy[i][1] * co;
	if (x < area[0])
	    area[0] = x;
	else if (x > area[2])
	    area[2] = x;
	if (y < area[1])
	    area[1] = y;
	else if (y > area[3])
	    area[3] = y;
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

    float area[4];
    get_rotated_area(fw, fh, area, rot100);
    int new_w = area[2] - area[0] + 2;
    int new_h = area[3] - area[1] + 2;
    unsigned *apu = malloc(new_w * new_h * sizeof(unsigned));
    memset(apu, -1, new_w * new_h * sizeof(unsigned));
    int shift = round(-area[0]) + round(-area[1])*new_w; // tärkeä pyöristää erikseen
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
	for (int i=0; i<cpw; i++)
	    if (apu[j*new_w+i] != -1)
		to[j*tw+i] = apu[j*new_w+i];
	//memcpy(to + j*tw, apu+j*new_w, cpw*sizeof(to[0]));

    free(apu);
}
