static void init_plus(unsigned char *to, int tow, int toh) {
	float rside = min(tow, toh) / 15.0;
	float x0 = tow*0.5 - rside,
		  x1 = tow*0.5 + rside;
	float y0 = toh*0.5 - rside,
		  y1 = toh*0.5 + rside;
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
}

static void init_triangle(unsigned char *to, int tow, int toh) {
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
}

static void init_circle(unsigned char *to, int tow, int toh) {
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
}

/* Only lines commented as 'different' differ from the init_circle function above */
static void init_4star(unsigned char *to, int tow, int toh) {
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
}
