/* xy gives points A0 and A1 in both sides of hypotenusa.
   Point A2 at the rectangle has the same x (y) coordinate than A0 if whichside is 0 (1) */
static void draw_straight_triangle
(uint32_t *canvas, int ystride, const float *xy, uint32_t color, int whichside, int *xyminmax) {
	int dx = xy[2] - xy[0],
		dy = xy[3] - xy[1];

	int steep = Abs(xy[3] - xy[1]) >= Abs(xy[2] - xy[0]);
	float n_per_m = !steep ? (float)dy / dx : (float)dx / dy;

	float m0 = xy[0*2 + steep],
		  m1 = xy[1*2 + steep],
		  na = xy[0*2 + !steep],
		  /* if whichside == ndim (= !steep), nb (constant) is the same as n in the start,
			 otherwise it is the same as n in the end */
		  nb = xy[(whichside != !steep)*2 + !steep];

	if (m1 < m0) {
		m0 = m1;
		m1 = xy[0*2 + steep];
		na -= (m1-m0) * n_per_m;
	}

	int im0 = iceil(m0),
		im1 = iceil(m1);
	float mdiff0 = im0 - m0;

#define apu ((255<<16)-1)
	na += mdiff0 * n_per_m;
	int na_is_smaller = na < nb || (na == nb && n_per_m < 0);
	int ina, inb, ndiff;
	char invert_ndiff = 0;
	if (na_is_smaller) {
		ina = iceil(na);
		inb = iceil(nb);
		/* n is changed when ndiff == 1; with ina = 5 and na = 4.8, alpha = 0.2, but ndiff is 0.8 */
		ndiff = (na - ina + 1) * apu;
		invert_ndiff = 1;
	}
	else {
		ina = na;
		inb = nb;
		/* with ina = 5 and na = 5.8, alpha = 0.8 and ndiff = 0.8 */
		ndiff = (na - ina) * apu;
	}

	int i_n_per_m = n_per_m * apu;
	int add = n_per_m < 0 ? -1 : 1;

	if (n_per_m < 0) {
		i_n_per_m = -i_n_per_m; // always positive, ndiff goes from < 1 to 1
		/* with ina = 5 and na = 5.8, alpha is still 0.8 but ndiff = 0.2,
		   with ina = 5 and na = 4.8, alpha is still 0.2 but ndiff = 0.2 */
		ndiff = apu - ndiff;
		invert_ndiff = !invert_ndiff;
	}

#define alfa (invert_ndiff ? (apu-ndiff) >> 16 : ndiff>>16)
	uint32_t (*canvas2d)[ystride] = (void*)canvas;
	if (!steep) {
		for (; im0<im1; im0++) {
			tocanvas(&canvas2d[ina-na_is_smaller][im0], alfa, color);
			for (int nn=min(ina, inb); nn<max(ina, inb); nn++) canvas2d[nn][im0] = color;
			if ((ndiff += i_n_per_m) >= apu) ndiff -= apu, ina += add;
		}
	}
	else {
		for (; im0<im1; im0++) {
			tocanvas(&canvas2d[im0][ina-na_is_smaller], alfa, color);
			for (int nn=min(ina, inb); nn<max(ina, inb); nn++) canvas2d[im0][nn] = color;
			if ((ndiff += i_n_per_m) >= apu) ndiff -= apu, ina += add;
		}
	}
}

#undef apu
#undef alfa

static void argsort3_2gap(const float *arr, unsigned char *order) {
	if (arr[2] < arr[4]) {
		if (arr[0] < arr[2]) {
			order[0] = 0;
			order[1] = 2;
			order[2] = 4;
		}
		else {
			order[0] = 2;
			order[1] = arr[0] < arr[4] ? 0 : 4;
			order[2] = arr[0] < arr[4] ? 4 : 0;
		}
	}
	else {
		if (arr[0] < arr[4]) {
			order[0] = 0;
			order[1] = 4;
			order[2] = 2;
		}
		else {
			order[0] = 4;
			order[1] = arr[0] < arr[2] ? 0 : 2;
			order[2] = arr[0] < arr[2] ? 2 : 0;
		}
	}
}

void kahto_fill_triangle
(uint32_t *canvas, int ystride, const float *xycorners, uint32_t color, const int *xyminmax) {
	unsigned char yorder[3], xorder[3];
	argsort3_2gap(xycorners+1, yorder);
	argsort3_2gap(xycorners+0, xorder);
	const float *c0 = xycorners + yorder[0];
	const float *cleft = xycorners + (yorder[0] != xorder[0] ? xorder[0] : xorder[1]);
	const float *cright = xycorners;
	while (cright == cleft || cright == c0)
		cright += 2;
	float dy_per_dx[] = {
		(cleft [1] - c0[1]) / (cleft [0] - c0[0]),
		(cright[1] - c0[1]) / (cright[0] - c0[0]),
	};
	float intercept[] = {
		c0[1] - dy_per_dx[0] * c0[0],
		c0[1] - dy_per_dx[1] * c0[0],
	};

	float y = floorf(c0[1]) + 1;

	char line_above_partial[] = { // above means bigger y, below in figure
		dy_per_dx[0] < 0,
		dy_per_dx[1] > 0,
	};

	float xx[][2] = {{
		(c0[1] - intercept[0]) / dy_per_dx[0],
		(y - intercept[0]) / dy_per_dx[0],
	}, {
		(c0[1] - intercept[1]) / dy_per_dx[1],
		(y - intercept[1]) / dy_per_dx[1],
	}};
	int ithis = 1;

	float x0[] = {c0[0], c0[0]};

	float y1 = xycorners[yorder[1]+1],
		  y2 = xycorners[yorder[2]+1];
	uint32_t *canvasrow = canvas + (int)y*ystride;
	char y1passed = 0;
	while (1) {
		/* päällekkäisyys on käsittelemättä */
		/* partial pixels */
		int xlow[2], xhigh[2];
		for (int iside=0; iside<2; iside++) {
			float xnext = (y+1 - intercept[iside]) / dy_per_dx[iside];
			if (y < xyminmax[1] || y >= xyminmax[3]) {
				xx[iside][!ithis] = xnext;
				continue;
			}
			if (fabs(dy_per_dx[iside]) < 0.5) {
				if (line_above_partial[iside])
					xx[iside][!ithis] = xnext;
				int smaller = xx[iside][1] < xx[iside][0];
				xlow[iside] = floorf(xx[iside][smaller]) + 1;
				xhigh[iside] = iceil(xx[iside][!smaller]);
				int x0tmp = max(xlow[iside], xyminmax[0]);
				int x1tmp = min(xhigh[iside], xyminmax[2]);
				if (line_above_partial[iside])
					for (int i=x0tmp; i<x1tmp; i++) {
						float yf = dy_per_dx[iside] * i + intercept[iside];
						float frac = 1 - (yf - y);
						tocanvas(canvasrow+i, iroundpos(frac*255), color);
					}
				else
					for (int i=x0tmp; i<x1tmp; i++) {
						float yf = dy_per_dx[iside] * i + intercept[iside];
						float frac = 1 - (y - yf);
						tocanvas(canvasrow+i, iroundpos(frac*255), color);
					}
			}
			else if (!my_isnan(dy_per_dx[iside])){
				float x = xx[iside][ithis];
				int ix = floorf(x) + iside;
				if (xyminmax[0] <= ix && ix < xyminmax[2]) {
					float frac = 1 - (iside ? ix - x : x - ix);
					if (0 < frac && frac < 1)
						tocanvas(canvasrow+ix, iroundpos(frac*255), color);
				}
				xlow[iside] = floorf(xx[iside][ithis]) + 1;
				xhigh[iside] = iceil(xx[iside][ithis]);
			}
			else { // dx == 0
				xlow[iside] = floorf(x0[iside]) + 1;
				xhigh[iside] = iceil(x0[iside]);
			}

			xx[iside][!ithis] = xnext;
		}

		/* full pixels */
		if (y < xyminmax[1])
			continue;
		if (y >= xyminmax[3])
			break;
		int x0tmp = max(xhigh[0], xyminmax[0]);
		int x1tmp = min(xlow[1], xyminmax[2]);
		for (int i=x0tmp; i<x1tmp; i++)
			canvasrow[i] = color;

		canvasrow += ystride;
		y += 1;
		if (!y1passed) {
			if (y >= y1) {
				const float *c2 = xycorners+yorder[2];
				const float *c1 = xycorners+yorder[1];
				int y1side = c1 == cright;
				dy_per_dx[y1side] = (c2[1] - c1[1]) / (c2[0] - c1[0]);
				intercept[y1side] = c1[1] - dy_per_dx[y1side] * c1[0],
				line_above_partial[y1side] = (y1side==1) ^ (dy_per_dx[0]<0);
				xx[y1side][ithis] = (y-1 - intercept[y1side]) / dy_per_dx[y1side];
				xx[y1side][!ithis] = (y - intercept[y1side]) / dy_per_dx[y1side];
				y1passed = 1;
				x0[y1side] = c1[0];
			}
		}
		else if (y >= y2)
			break;
		ithis = !ithis;
	}
}
