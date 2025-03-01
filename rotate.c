void rotate100(unsigned *to, int tostride, int tw, int th, const unsigned *from, int fw, int fh) {
	int tcpw = min(tw, fh),
		tcph = min(th, fw);
	for (int j=0; j<tcph; j++)
		for (int i=0; i<tcpw; i++)
			to[j*tostride+i] = from[(tcpw-i-1)*fw + j];
}

void rotate300(unsigned *to, int tostride, int tw, int th, const unsigned *from, int fw, int fh) {
	int tcpw = min(tw, fh),
		tcph = min(th, fw);
	for (int j=0; j<tcph; j++)
		for (int i=0; i<tcpw; i++)
			to[j*tostride+i] = from[i*fw + tcph-j-1];
}

static inline void tocanvas(unsigned *ptr, int value, unsigned color);

void get_rotated_area(float w, float h, float *restrict area, float rot_grad) {
	if (iround(rot_grad*1000) % 400000 == 0) {
		area[0] = 0;
		area[1] = 0;
		area[2] = w;
		area[3] = h;
		return;
	}
	float si = sinf(rot_grad * 3.14159265358979 / 200.);
	float co = cosf(rot_grad * 3.14159265358979 / 200.);
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

static void laita(unsigned *to, int length, int tw, float y, float x, unsigned color) {
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
	if (ind >= 0 && ind < length) {
		int value = round(255 * yxlength0[0] * yxlength0[1]);
		value = value < 128 ? value*2 : 255;
		tocanvas(to+ind, value, color);
	}

	ind = yxind0[0]*tw + yxind1[1];
	if (ind >= 0 && ind < length) {
		int value = round(255 * yxlength0[0] * yxlength1[1]);
		value = value < 128 ? value*2 : 255;
		tocanvas(to+ind, value, color);
	}

	ind = yxind1[0]*tw + yxind0[1];
	if (ind >= 0 && ind < length) {
		int value = round(255 * yxlength1[0] * yxlength0[1]);
		value = value < 128 ? value*2 : 255;
		tocanvas(to+ind, value, color);
	}

	ind = yxind1[0]*tw + yxind1[1];
	if (ind >= 0 && ind < length) {
		int value = round(255 * yxlength1[0] * yxlength1[1]);
		tocanvas(to+ind, value, color);
	}
}

void rotate(
	unsigned *to, int tox, int toy, int tw, int th,
	const unsigned *from, int fw, int fh,
	float rot_grad)
{
	to += toy*tw + tox;
	int irot = rot_grad;
	if (irot == rot_grad) {
		if (irot < 0)
			irot += 400 * (-irot/400 + !!(irot%400));
		else if (irot >= 400)
			irot -= irot/400 * 400;
		if (irot == 100) return rotate100(to, tw, tw-tox, th-toy, from, fw, fh);
		if (irot == 300) return rotate300(to, tw, tw-tox, th-toy, from, fw, fh);
	}

	float si = sinf(rot_grad * 3.14159265358979 / 200);
	float co = cosf(rot_grad * 3.14159265358979 / 200);

	float area[4];
	get_rotated_area(fw, fh, area, rot_grad);
	int new_w = area[2] - area[0] + 2;
	int new_h = area[3] - area[1] + 2;
	int apulen = new_w * new_h;
	unsigned *apu = malloc(apulen * sizeof(unsigned));
	memset(apu, -1, apulen * sizeof(unsigned));
	int xshift = round(-area[0]);
	int yshift = round(-area[1]);
	/*int shift = round(-area[0]) + round(-area[1])*new_w; // tärkeä pyöristää erikseen
	  apu += shift;*/
	for (int y0=0; y0<fh; y0++) {
		for (int x0=0; x0<fw; x0++) {
			float x1 = (x0+0.5)*co - (y0+0.5)*si;
			float y1 = (x0+0.5)*si + (y0+0.5)*co;
			laita(apu, apulen, new_w, y1+yshift, x1+xshift, from[y0*fw + x0]);
		}
	}
	//apu -= shift;

	int cpw = min(tw, new_w),
		cph = min(th, new_h);
	for (int j=0; j<cph; j++)
		for (int i=0; i<cpw; i++)
			if (apu[j*new_w+i] != -1)
				to[j*tw+i] = apu[j*new_w+i];
	//memcpy(to + j*tw, apu+j*new_w, cpw*sizeof(to[0]));

	free(apu);
}
