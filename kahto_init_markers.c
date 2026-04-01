static unsigned char* init_plus(unsigned char *to, int *_tow, int *_toh) {
	int tow = *_tow, toh = *_toh;
	float halfthickness = min(tow, toh) / 15.0;
	float x0 = tow*0.5 - halfthickness,
		  x1 = tow*0.5 + halfthickness;
	float y0 = toh*0.5 - halfthickness,
		  y1 = toh*0.5 + halfthickness;
	for (int j=0; j<toh; j++) {
		float a = y0 - j;
		float b = j+1 - y1;
		float yfrac = 1.0 - a * (a>0) - b * (b>0);
		for (int i=0; i<tow; i++) {
			float a = x0 - i;
			float b = i+1 - x1;
			float xfrac = 1.0 - a * (a>0) - b * (b>0);
			float frac = max(yfrac, xfrac);
			to[j*tow+i] = frac*255 * (frac >= 0);
		}
	}
	return to;
}

static unsigned char* init_xmarker(unsigned char *to, int *_tow, int *_toh) {
	int tow = *_tow, toh = *_toh;
	float halfthickness = min(tow, toh) / 12.0;
	float a0 = (toh-1.) / (tow-1.);
	float a1 = -a0;
	float b0 = 0;
	float b1 = toh-1;
	for (int j=0; j<toh; j++) {
		for (int i=0; i<tow; i++) {
			float s = 0;
			for (int jj=0; jj<8; jj++) {
				for (int ii=0; ii<8; ii++) {
					float x = i + ii/8.0;
					float y = j + jj/8.0;
					float d0 = (a0*x + b0 - y) / a0;
					float d1 = (a1*x + b1 - y) / a1;
					if (d0 < 0) d0 = -d0;
					if (d1 < 0) d1 = -d1;
					s += (d0 <= halfthickness) || (d1 <= halfthickness);
				}
			}
			to[j*tow+i] = 255 * (s/(8*8));
		}
	}
	return to;
}

static unsigned char* init_triangle(unsigned char *to, int *_tow, int *_toh) {
	int tow = *_tow, toh = *_toh;
	int odd_tow = tow - (tow % 2 == 0);
	int side = min(odd_tow, toh);
	int rectheight = sin(3.14159265/3) * side;
	int y0 = (toh - rectheight) * 0.5;
	float halfw0 = 0.5;
	float halfw1 = side * 0.5;
	/* halfw0 + (rectheight-1) * addw = halfw1 */
	float addw = (halfw1 - halfw0) / (rectheight - 1);
	memset(to, 0, tow*toh);

	int imid = tow/2;
	float ifloat = halfw0;
	for (int j=y0; j<y0+rectheight; j++) {
		int iint = ifloat - 0.5; // without the central pixel
		float lastfrac = ifloat - 0.5 - iint;
		unsigned char lastval = lastfrac * 255;
		if (lastval) {
			to[j*tow + imid - iint - 1] = lastval;
			to[j*tow + imid + iint + 1] = lastval;
		}
		memset(to + j*tow + imid - iint, 255, iint*2+1);
		ifloat += addw;
	}
	return to;
}

static unsigned char* init_triangle_down(unsigned char *to, int *_tow, int *_toh) {
	int tow = *_tow, toh = *_toh;
	unsigned char *tmp = malloc(tow*toh);
	init_triangle(tmp, _tow, _toh);
	for (int i=0; i<toh; i++)
		memcpy(to+(toh-1-i)*tow, tmp+i*tow, tow);
	free(tmp);
	return to;
}

static unsigned char* init_circle(unsigned char *to, int *_tow, int *_toh) {
	int tow = *_tow, toh = *_toh;
	const int size = min(tow, toh);
	const int size16 = size * 16;
	const int r = size16 / 2;
	unsigned char (*to16)[size16] = malloc(size16 * size16);
	/* https://en.wikipedia.org/wiki/Midpoint_circle_algorithm#Jesko's_Method */
	/* Unlike in the method, our circle is always even because of the multiplication with an even number */
	int t1 = r/16, // 16 belongs to the method and is unrelated to size16
		x = r,
		y = 0;
	while (x >= y) {
		int x0 = r-x,
			x1 = r+x,
			y0 = r-y,
			y1 = r+y;
		memset(to16[y0], 0, x0);
		memset(to16[y0]+x0, 1, x1-x0);
		memset(to16[y0]+x1, 0, size16-x1);

		memset(to16[y1-1], 0, x0);
		memset(to16[y1-1]+x0, 1, x1-x0);
		memset(to16[y1-1]+x1, 0, size16-x1);

		memset(to16[x0], 0, y0);
		memset(to16[x0]+y0, 1, y1-y0);
		memset(to16[x0]+y1, 0, size16-y1);

		memset(to16[x1-1], 0, y0);
		memset(to16[x1-1]+y0, 1, y1-y0);
		memset(to16[x1-1]+y1, 0, size16-y1);

		t1 += ++y;
		if (t1 >= x)
			t1 -= x--;
	}

	memset(to, 0, tow*toh);
	for (int j=0; j<size16; j++)
		for (int i=0; i<size16; i++)
			to[j/16*tow+i/16] += to16[j][i] && (j%16 || i%16);
	free(to16);
	return to;
}

/* Only lines commented as 'different' differ from the init_circle function above */
static unsigned char* init_4star(unsigned char *to, int *_tow, int *_toh) {
	int tow = *_tow, toh = *_toh;
	const int size = min(tow, toh);
	const int size16 = size * 16;
	const int r = size16 / 2;
	unsigned char (*to16)[size16] = malloc(size16 * size16);
	/* https://en.wikipedia.org/wiki/Midpoint_circle_algorithm#Jesko's_Method */
	/* Unlike in the method, our circle is always even because of the multiplication with an even number */
	int t1 = r/16, // 16 belongs to the method and is unrelated to size16
		x = r,
		y = 0;
	while (x >= y) {
		int x0 = x,				// different
			x1 = size16 - x,	// different
			y0 = y,				// different
			y1 = size16 - y;	// different
		memset(to16[y0], 0, x0);
		memset(to16[y0]+x0, 1, x1-x0);
		memset(to16[y0]+x1, 0, size16-x1);

		memset(to16[y1-1], 0, x0);
		memset(to16[y1-1]+x0, 1, x1-x0);
		memset(to16[y1-1]+x1, 0, size16-x1);

		memset(to16[x0], 0, y0);
		memset(to16[x0]+y0, 1, y1-y0);
		memset(to16[x0]+y1, 0, size16-y1);

		memset(to16[x1-1], 0, y0);
		memset(to16[x1-1]+y0, 1, y1-y0);
		memset(to16[x1-1]+y1, 0, size16-y1);

		t1 += ++y;
		if (t1 >= x)
			t1 -= x--;
	}

	memset(to, 0, tow*toh);
	for (int j=0; j<size16; j++)
		for (int i=0; i<size16; i++)
			to[j/16*tow+i/16] += to16[j][i] && (j%16 || i%16);
	free(to16);
	return to;
}

static unsigned char* init_literal(unsigned char *bmap, int *w, int *h, struct kahto_graph *graph) {
	struct ttra *ttra = graph->yxaxis[0]->figure->ttra;
	struct ttra memttra = *ttra;
	ttra_set_xy0(ttra, 0, 0);
	ttra_set_fontheight(ttra, *h);
	ttra_get_textdims_pixels(ttra, graph->markerstyle.marker, w, h);
	bmap = calloc(*w**h, 4);
	ttra->canvas = (void*)bmap;
	ttra->ystride = ttra->x1 = *w;
	ttra->y1 = *h;
	ttra->alphamode = 1;
	ttra_print(ttra, graph->markerstyle.marker);
	*ttra = memttra;
	return bmap;
}
