static void kahto_draw_straight_line
(uint32_t *canvas, int ystride, const int *xy, uint32_t color, float thickness, int *yxminmax) {
	int i1, i0, j1, j0;
	int ithickness = iroundpos(thickness);
	if (xy[3] == xy[1]) {
		int smaller = xy[2] < xy[0];
		i1 = xy[2*!smaller];
		i0 = xy[2*smaller];
		j0 = xy[1] - ithickness/2;
		j1 = j0 + ithickness;
	}
	else {
		int smaller = xy[3] < xy[1];
		j1 = xy[2*!smaller+1];
		j0 = xy[2*smaller+1];
		i0 = xy[0] - ithickness/2;
		i1 = i0 + ithickness;
	}
	if (j0 < yxminmax[0]) j0 = yxminmax[0];
	if (i0 < yxminmax[1]) i0 = yxminmax[1];
	if (j1 > yxminmax[2]) j1 = yxminmax[2];
	if (i1 > yxminmax[3]) i1 = yxminmax[3];
	for (int j=j0; j<j1; j++)
		for (int i=i0; i<i1; i++)
			canvas[j*ystride+i] = color;
	return;
}

static void _kahto_draw_line_sort(const float corners[4][2], unsigned char *ind, int idim) {
	ind[0] = ind[3] = 0;
	for (int i=1; i<4; i++) {
		if (corners[i][idim] < corners[ind[0]][idim])
			ind[0] = i;
		else if (corners[i][idim] > corners[ind[3]][idim])
			ind[3] = i;
	}

	for (int i=0, j=1; j<=2; i++)
		if (ind[0] != i && ind[3] != i)
			ind[j++] = i;

	if (corners[ind[1]][idim] > corners[ind[2]][idim]) {
		unsigned char a = ind[1];
		ind[1] = ind[2];
		ind[2] = a;
	}
}

static void draw_line_xiaolin(uint32_t *canvas_, int ystride, const int *xy, uint32_t color);

#include "kahto_draw_triangle.c"

static void draw_line_kahto
(uint32_t *canvas, int ystride, const int *xy, uint32_t color, float thickness, int *yxminmax) {
	int dx = xy[2] - xy[0],
		dy = xy[3] - xy[1];
	if (!dy || !dx)
		return kahto_draw_straight_line(canvas, ystride, xy, color, thickness, yxminmax);

	int steep = Abs(xy[3] - xy[1]) >= Abs(xy[2] - xy[0]);
	float m_per_n = steep ? (float)dy / dx : (float)dx / dy; // m,n = (steep ? y,x : x,y)
	float n_per_m = 1.f / m_per_n;
	float nthickness = thickness * sqrt(1 + (n_per_m * n_per_m)); // saadaan yhdenmuotoisista kolmioista
	if (nthickness <= 1)
		return draw_line_xiaolin(canvas, ystride, xy, color);//, xmin, xmax, ymin, ymax);

	float corners[4][2];
	{
		float perp_n_per_m = -m_per_n;
#if 0 /* Both options result in the same outcome.
		 To me, this was easier to understand and useful in debugging. */
		float unitvec_mn[] = { 1, perp_n_per_m, }; // not a unitvector yet
		float len = sqrt(unitvec_mn[0]*unitvec_mn[0] + unitvec_mn[1]*unitvec_mn[1]);
		unitvec_mn[0] /= len;
		unitvec_mn[1] /= len;
		float dm = 0.5 * thickness * unitvec_mn[0];
		float dn = 0.5 * thickness * unitvec_mn[1];
#else
		float dm = 0.5 * sqrt(thickness*thickness / (1 + perp_n_per_m * perp_n_per_m));
		float dn = perp_n_per_m * dm;
#endif
		float dmn[] = {dm, dn};
		corners[0][0] = xy[0] + dmn[steep];
		corners[0][1] = xy[1] + dmn[!steep];
		corners[1][0] = xy[0] - dmn[steep];
		corners[1][1] = xy[1] - dmn[!steep];

		corners[2][0] = xy[2] + dmn[steep];
		corners[2][1] = xy[3] + dmn[!steep];
		corners[3][0] = xy[2] - dmn[steep];
		corners[3][1] = xy[3] - dmn[!steep];
	}

	/* draw the line without the end tringles */
	unsigned char ind[4];
	_kahto_draw_line_sort(corners, ind, steep);
	float m0 = corners[ind[1]][steep];
	float m1 = corners[ind[2]][steep];
	float na = corners[ind[1]][!steep];
	if (n_per_m < 0)
		na -= nthickness;

	int im0 = iceil(m0);
	const int im1 = m1;
	float mdiff0 = im0 - m0;

	na += mdiff0 * n_per_m;
	int ina = iceil(na);
#define apu ((255<<16)-1)
	int ndiff[2];
	ndiff[0] = (na - ina + 1) * apu;

	float nb = na + nthickness;
	int inb = nb; // ifloor would be needed, if negative
	ndiff[1] = (nb - inb) * apu;

	int i_n_per_m = n_per_m * apu;
	char invert_alpha[2];
	int add = n_per_m < 0 ? -1 : 1;

	if (n_per_m < 0) {
		ndiff[0] = apu - ndiff[0];
		i_n_per_m = -i_n_per_m; // always positive
		((short*)invert_alpha)[0] = 0x0100;
	}
	else {
		ndiff[1] = apu - ndiff[1];
		((short*)invert_alpha)[0] = 0x0001;
	}

#define alfa(which) (invert_alpha[which] ? (apu-ndiff[which]) >> 16 : ndiff[which]>>16)
	uint32_t (*canvas2d)[ystride] = (void*)canvas;
	const int im00 = im0;
	if (!steep) {
		for (; im0<im1; im0++) {
			tocanvas(&canvas2d[ina-1][im0], alfa(0), color);
			for (int nn=ina; nn<inb; nn++) canvas2d[nn][im0] = color;
			tocanvas(&canvas2d[inb][im0], alfa(1), color);
			if ((ndiff[0] += i_n_per_m) >= apu) ndiff[0] -= apu, ina += add;
			if ((ndiff[1] += i_n_per_m) >= apu) ndiff[1] -= apu, inb += add;
		}
	}
	else {
		for (; im0<im1; im0++) {
			tocanvas(&canvas2d[im0][ina-1], alfa(0), color);
			for (int nn=ina; nn<inb; nn++) canvas2d[im0][nn] = color;
			tocanvas(&canvas2d[im0][inb], alfa(1), color);
			if ((ndiff[0] += i_n_per_m) >= apu) ndiff[0] -= apu, ina += add;
			if ((ndiff[1] += i_n_per_m) >= apu) ndiff[1] -= apu, inb += add;
		}
	}

	/* Draw the end triangles. Both of them are divided into two smaller rectangular triangles
	   where only one side (theline) is not straight in x- or y-direction. */
	{
		float theline[4] = {corners[ind[0]][0], corners[ind[0]][1]};
		theline[2+steep] = im00;
		theline[2+!steep] = na;
		draw_straight_triangle(canvas, ystride, theline, color, !steep, yxminmax);
		theline[2+!steep] = nb;
		draw_straight_triangle(canvas, ystride, theline, color, !steep, yxminmax);
	} {
		float theline[4] = {corners[ind[3]][0], corners[ind[3]][1]};
		theline[2+steep] = im1-1;
		theline[2+!steep] = ina - alfa(0) / 256.f;
		draw_straight_triangle(canvas, ystride, theline, color, !steep, yxminmax);
		theline[2+!steep] = inb + alfa(1) / 256.f;
		draw_straight_triangle(canvas, ystride, theline, color, !steep, yxminmax);
	}
}

#undef apu
#undef alfa
