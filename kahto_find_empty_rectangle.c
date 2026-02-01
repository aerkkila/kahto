/* Tätä voisi nopeuttaa käymällä löydettyä kohtaa läpi eteenpäin kunnes löytyy vapaa paikka
   samalla tavalla kuin funktiossa rectangle_closest_i. */
/* If rectangle of size (rwidth,rheight) fits to position (i,j), return negative.
   Otherwise return the next i-coordinate (x) to try. */
static int rectangle_nexti(int j, int i, int rwidth, int rheight, const short (*right)[], const short (*down)[], int w, int h) {
	const short (*spaceright)[w] = right;
	const short (*spacedown)[w] = down;
	for (int jj=0; jj<rheight; jj++)
		if (spaceright[j+jj][i] < rwidth)
			return i + spaceright[j+jj][i] + 1;
	/* is this necessary? */
	for (int ii=0; ii<rwidth; ii++)
		if (spacedown[j][i+ii] < rheight)
			return i + ii + 1;
	return -1;
}

/* If rectangle of size (rwidth,rheight) fits to position (i,j), return negative.
   Otherwise return the next j-coordinate (y) to try. */
static int rectangle_nextj(int j, int i, int rwidth, int rheight, const short (*right)[], const short (*down)[], int w, int h) {
	const short (*spaceright)[w] = right;
	const short (*spacedown)[w] = down;
	for (int ii=0; ii<rwidth; ii++)
		if (spacedown[j][i+ii] < rheight)
			return j + spacedown[j][i+ii] + 1;
	/* is this necessary? */
	for (int jj=0; jj<rheight; jj++)
		if (spaceright[j+jj][i] < rwidth)
			return j + jj + 1;
	return -1;
}

/* Return the negative distance to closest non-free pixel in the i-direction (x) from the left side of the rectangle.
   If the rectangle does not fit, return the next i-coordinate to try. */
static int rectangle_closest_i(int j, int i, int rwidth, int rheight, const short (*right)[], int w) {
	const short (*spaceright)[w] = right;
	int space;
	int closest = w;
	int longest_jump = -1;
	for (int jj=0; jj<rheight; jj++)
		if ((space = spaceright[j+jj][i]) < rwidth) {
			int jump = space+1;
			for (; jump+i<w; jump+=space+1)
				if ((space=spaceright[j+jj][i+jump]) >= rwidth)
					goto jump_found;
			return w; // no more spots in this direction
jump_found:
			if (jump > longest_jump)
				longest_jump = jump;
		}
		else if (space < closest)
			closest = space;
	if (longest_jump >= 0)
		return i + longest_jump;
	return -closest;
}

/* Return the negative distance to closest non-free pixel in the j-direction (y) from the top of the rectangle.
   If the rectangle does not fit, return the next j-coordinate to try. */
static int rectangle_closest_j(int j, int i, int rwidth, int rheight, const short (*down)[], int w, int h) {
	const short (*spacedown)[w] = down;
	int space;
	int closest = h;
	int longest_jump = -1;
	for (int ii=0; ii<rwidth; ii++)
		if ((space = spacedown[j][i+ii]) < rheight) {
			int jump = space+1;
			for (; jump+j<h; jump+=space+1)
				if ((space=spacedown[j+jump][i+ii]) >= rheight)
					goto jump_found;
			return h; // no more spots in this direction
jump_found:
			if (jump > longest_jump)
				longest_jump = jump;
		}
		else if (space < closest)
			closest = space;
	if (longest_jump >= 0)
		return j + longest_jump;
	return -closest;
}

/* Return negative if does not fit to image.
   Return positive if no empty slot is available.
   Return zero on success. */
int kahto_find_empty_rectangle(struct kahto_figure *figure, int rwidth, int rheight, int *xout, int *yout, enum kahto_placement method) {
	int x0 = figure->ro_inner_xywh[0],
		y0 = figure->ro_inner_xywh[1],
		w = figure->ro_inner_xywh[2],
		h = figure->ro_inner_xywh[3];
	int x1 = x0 + w,
		y1 = y0 + h;
	if (w < rwidth || h < rheight)
		return -1;
	if (!method)
		return 0;
	int retval = 0;

	int width = figure->wh[0],
		height = figure->wh[1];
	unsigned (*image)[width] = calloc(width * height, sizeof(unsigned));
	if (!figure->ro_colors_set)
		kahto_set_colors(figure);
	unsigned background = figure->background;
	kahto_fill_u4((void*)image, background, width, height, width);
	for (int i=0; i<figure->ngraph; i++)
		kahto_draw_graph(figure->graph[i], (void*)image, width, figure, 0);

	/* including the pointed spot */
	short (*spaceright)[w] = malloc(w*h * sizeof(short));
	for (int j=h-1; j>=0; j--)
		spaceright[j][w-1] = image[j+y0][x1-1] == background;
	for (int j=h-1; j>=0; j--)
		for (int i=w-2; i>=0; i--)
			spaceright[j][i] = (spaceright[j][i+1] + 1) * (image[j+y0][i+x0] == background);

	short (*spacedown)[w] = malloc(w*h * sizeof(short));
	for (int i=w-1; i>=0; i--)
		spacedown[h-1][i] = image[y1-1][i+x0] == background;
	for (int j=h-2; j>=0; j--)
		for (int i=w-1; i>=0; i--)
			spacedown[j][i] = (spacedown[j+1][i] + 1) * (image[j+y0][i+x0] == background);

	*xout = x0, *yout = y0;
	int jpos, ipos, nextpos;

	/* Corners */
	for (int n=0; n<4; n++) {
		jpos = !!(n/2) * (h - rheight);
		ipos = !!(n%2) * (w - rwidth);
		if (rectangle_nexti(jpos, ipos, rwidth, rheight, spaceright, spacedown, w, h) < 0)
			goto found;
	}

	/* Edges */
	switch (method) {
		case kahto_placement_first:
			jpos=0;
			for (ipos=0; ipos<=w-rwidth; ipos=nextpos)
				if ((nextpos = rectangle_nexti(jpos, ipos, rwidth, rheight, spaceright, spacedown, w, h)) < 0)
					goto found;
			jpos = h - rheight;
			for (ipos=0; ipos<=w-rwidth; ipos=nextpos)
				if ((nextpos = rectangle_nexti(jpos, ipos, rwidth, rheight, spaceright, spacedown, w, h)) < 0)
					goto found;
			ipos = 0;
			for (jpos=0; jpos<=h-rheight; jpos=nextpos)
				if ((nextpos = rectangle_nextj(jpos, ipos, rwidth, rheight, spaceright, spacedown, w, h)) < 0)
					goto found;
			ipos = w - rwidth;
			for (jpos=0; jpos<=h-rheight; jpos=nextpos)
				if ((nextpos = rectangle_nextj(jpos, ipos, rwidth, rheight, spaceright, spacedown, w, h)) < 0)
					goto found;
			break;
		default:
		case kahto_placement_singlemaxdist:
			int save[4] = {-1}; // dist, i, j, which direction
			jpos = 0; // top row
			for (int itwice=0; itwice<2; itwice++, jpos=h-rheight/*bottom row*/)
				for (ipos=0; ipos<=w-rwidth; )
					if ((nextpos = rectangle_closest_i(jpos, ipos, rwidth, rheight, spaceright, w)) < 0) {
						nextpos = -nextpos;
						if (nextpos-rwidth > save[0])
							save[0]=nextpos-rwidth, save[1]=ipos, save[2]=jpos, save[3]=0;
						ipos += nextpos+1;
					}
					else
						ipos = nextpos;
			ipos=0; // left column
			for (int itwice=0; itwice<2; itwice++, ipos=w-rwidth/*right column*/)
				for (int jpos=0; jpos<=h-rheight; )
					if ((nextpos = rectangle_closest_j(jpos, ipos, rwidth, rheight, spacedown, w, h)) < 0) {
						nextpos = -nextpos;
						if (nextpos-rheight > save[0])
							save[0]=nextpos-rheight, save[1]=ipos, save[2]=jpos, save[3]=1;
						jpos += nextpos+1;
					}
					else
						jpos = nextpos;
			if (save[0] >= 0) {
				ipos = save[1] + (save[3]==0) * save[0] / 2;
				jpos = save[2] + (save[3]==1) * save[0] / 2;
				goto found;
			}
			break;
	}

	/* All positions */
	for (jpos=0; jpos<=h-rheight; jpos++)
		for (ipos=0; ipos<=w-rwidth; ipos=nextpos)
			if ((nextpos = rectangle_nexti(jpos, ipos, rwidth, rheight, spaceright, spacedown, w, h)) < 0)
				goto found;

	retval = 1;
	goto end;

found:
	*yout = jpos + y0;
	*xout = ipos + x0;
end:
	free(image);
	free(spacedown);
	free(spaceright);
	return retval;
}
