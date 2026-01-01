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
