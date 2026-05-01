static inline void tocanvas(unsigned *ptr, int value, unsigned color);

static void rotate100
(unsigned *to, int tostride, int tw, int th, const unsigned *color, const unsigned char *alpha, int fw, int fh) {
	int tcpw = min(tw, fh),
		tcph = min(th, fw);
	for (int j=0; j<tcph; j++)
		for (int i=0; i<tcpw; i++) {
			int ifrom = (tcpw-i-1)*fw + j;
			tocanvas(to+j*tostride+i, alpha[ifrom], color[ifrom]);
		}
}

static void rotate300
(unsigned *to, int tostride, int tw, int th, const unsigned *color, const unsigned char *alpha, int fw, int fh) {
	int tcpw = min(tw, fh),
		tcph = min(th, fw);
	for (int j=0; j<tcph; j++)
		for (int i=0; i<tcpw; i++) {
			int ifrom = i*fw + tcph-j-1;
			tocanvas(to+j*tostride+i, alpha[ifrom], color[ifrom]);
		}
}

static void get_rotated_area(float w, float h, float *restrict area, float rot_grad) {
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

static void update_alpha(unsigned char *to, unsigned char value) {
	float opacity0 = 1 - to[0]/255.0;
	float opacity = opacity0 * (1 - value/255.0);
	to[0] = iroundpos((1 - opacity) * 255);
}

static void update_color_and_alpha
(unsigned *tocolor, unsigned color, unsigned char *toalpha, unsigned char alpha, int ind) {
	if (toalpha[ind]) {
		float relative_alpha = (float)alpha / toalpha[ind];
		float mul = 1. / (1. + relative_alpha);
		unsigned newcolor = 0;
		for (int i=0; i<4; i++) {
			unsigned fg = color >> i*8 & 0xff;
			unsigned bg = *tocolor >> i*8 & 0xff;
			unsigned c = iroundpos((fg * relative_alpha + bg) * mul);
			newcolor += c << i*8;
		}
		update_alpha(toalpha+ind, alpha);
	}
	else {
		toalpha[ind] = alpha;
		tocolor[ind] = color;
	}
}

static void put_tmp_rot
(unsigned *tocolor, unsigned color, unsigned char *toalpha, unsigned char alpha,
 int length, int tw, float y, float x) {
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
		unsigned char a = iroundpos(alpha * yxlength0[0] * yxlength0[1]);
		update_color_and_alpha(tocolor, color, toalpha, a, ind);
	}

	ind = yxind0[0]*tw + yxind1[1];
	if (ind >= 0 && ind < length) {
		unsigned char a = iroundpos(alpha * yxlength0[0] * yxlength1[1]);
		update_color_and_alpha(tocolor, color, toalpha, a, ind);
	}

	ind = yxind1[0]*tw + yxind0[1];
	if (ind >= 0 && ind < length) {
		unsigned char a = iroundpos(alpha * yxlength1[0] * yxlength0[1]);
		update_color_and_alpha(tocolor, color, toalpha, a, ind);
	}

	ind = yxind1[0]*tw + yxind1[1];
	if (ind >= 0 && ind < length) {
		unsigned char a = iroundpos(alpha * yxlength1[0] * yxlength1[1]);
		update_color_and_alpha(tocolor, color, toalpha, a, ind);
	}
}

static void rotate(
	unsigned *to, int ystride, int tox, int toy, int tw, int th,
	const unsigned *color, const unsigned char *alpha, int fw, int fh,
	float rot_grad)
{
	to += toy*ystride + tox; // move the corner
	int irot = rot_grad;
	if (irot == rot_grad) {
		if (irot < 0)
			irot += 400 * (-irot/400 + !!(irot%400));
		else if (irot >= 400)
			irot -= irot/400 * 400;
		if (irot == 100) return rotate100(to, ystride, tw, th, color, alpha, fw, fh);
		if (irot == 300) return rotate300(to, ystride, tw, th, color, alpha, fw, fh);
	}

	float si = sinf(rot_grad * 3.14159265358979 / 200);
	float co = cosf(rot_grad * 3.14159265358979 / 200);

	float area[4];
	get_rotated_area(fw, fh, area, rot_grad);
	int new_w = area[2] - area[0] + 2;
	int new_h = area[3] - area[1] + 2;
	int newlen = new_w * new_h;
	int xshift = round(-area[0]);
	int yshift = round(-area[1]);

	unsigned char *alpharot = malloc(newlen);
	memset(alpharot, 0, newlen);
	unsigned *colorrot = malloc(newlen * sizeof(colorrot[0]));
	for (int y0=0; y0<fh; y0++)
		for (int x0=0; x0<fw; x0++)
			if (alpha[y0*fw + x0]) {
				float x1 = x0*co - y0*si;
				float y1 = x0*si + y0*co;
				put_tmp_rot(
					colorrot, color[y0*fw + x0], alpharot, alpha[y0*fw + x0],
					newlen, new_w, y1+yshift, x1+xshift);
			}

	int cpw = min(tw, new_w),
		cph = min(th, new_h);
	for (int j=0; j<cph; j++)
		for (int i=0; i<cpw; i++)
			if (alpharot[j*new_w+i]) {
				unsigned char a = alpharot[j*new_w+i];
				a = a > 255*2/3 ? 255 : a*3/2;
				tocanvas(to+j*ystride+i, a, colorrot[j*new_w+i]);
			}

	free(alpharot);
	free(colorrot);
}
