/* https://en.wikipedia.org/wiki/Bresenham's_line_algorithm */
/* This method is nice because it uses only integers. */
static void draw_line_bresenham(uint32_t *canvas, int ystride, const int *xy, uint32_t color) {
	int nosteep = Abs(xy[3] - xy[1]) < Abs(xy[2] - xy[0]);
	int backwards = xy[2+!nosteep] < xy[!nosteep]; // m1 < m0
	int m1=xy[2*!backwards+!nosteep], m0=xy[2*backwards+!nosteep],
		n1=xy[2*!backwards+nosteep],  n0=xy[2*backwards+nosteep]; // Δm ≥ Δn

	const int n_add = n1 > n0 ? 1 : -1;
	const int dm = m1 - m0;
	const int dn = n1 > n0 ? n1 - n0 : n0 - n1;
	const int D_add0 = 2 * dn;
	const int D_add1 = 2 * (dn - dm);
	int D = 2*dn - dm;
	if (nosteep) // (m,n) = (x,y)
		for (; m0<=m1; m0++) {
			canvas[n0*ystride + m0] = color;
			n0 += D > 0 ? n_add : 0;
			D  += D > 0 ? D_add1 : D_add0;
		}
	else // (m,n) = (y,x)
		for (; m0<=m1; m0++) {
			canvas[m0*ystride + n0] = color;
			n0 += D > 0 ? n_add : 0;
			D  += D > 0 ? D_add1 : D_add0;
		}
}

static void draw_datum_for_line(struct draw_data_args *ar) {
	int x0_ = ar->axis_xywh_outer[0],       y0_ = ar->axis_xywh_outer[1];
	int x1_ = x0_ + ar->axis_xywh_outer[2], y1_ = y0_ + ar->axis_xywh_outer[3];
	int x0  = ar->yxz[1] - ar->mapw/2,           y0 = ar->yxz[0] - ar->maph/2; // x0_ and x1_ has been already taken into account
	int j0  = max(0, y0_ - y0);
	int j1  = min(ar->maph, y1_ - y0);
	int i0  = max(0, x0_ - x0);
	int i1  = min(ar->mapw, x1_ - x0);
	uint32_t (*canvas)[ar->ystride] = (void*)ar->canvas;
	const unsigned char (*bmap)[ar->mapw] = (void*)ar->bmap;
	for (int j=j0, y=y0+j0; j<j1; j++, y++)
		for (int i=i0; i<i1; i++)
			tocanvas(canvas[y]+x0+i, bmap[j][i], ar->color);
}

/* like draw_line_bresenham, but instead of a dot, each pixel is used as a center for a circle */
static void _draw_line_circle_e(uint32_t *canvas, int ystride, const int *xy, uint32_t color, struct draw_data_args *args) {
	int nosteep = Abs(xy[3] - xy[1]) < Abs(xy[2] - xy[0]);
	int backwards = xy[2+!nosteep] < xy[!nosteep]; // m1 < m0
	int m1=xy[2*!backwards+!nosteep], m0=xy[2*backwards+!nosteep],
		n1=xy[2*!backwards+nosteep],  n0=xy[2*backwards+nosteep]; // Δm ≥ Δn

	const int n_add = n1 > n0 ? 1 : -1;
	const int dm = m1 - m0;
	const int dn = n1 > n0 ? n1 - n0 : n0 - n1;
	const int D_add0 = 2 * dn;
	const int D_add1 = 2 * (dn - dm);
	int D = 2*dn - dm;
	if (nosteep) // (m,n) = (x,y)
		for (; m0<=m1; m0++) {
			args->yxz[1] = m0;
			args->yxz[0] = n0;
			draw_datum_for_line(args);
			n0 += D > 0 ? n_add : 0;
			D  += D > 0 ? D_add1 : D_add0;
		}
	else // (m,n) = (y,x)
		for (; m0<=m1; m0++) {
			args->yxz[0] = m0;
			args->yxz[1] = n0;
			draw_datum_for_line(args);
			n0 += D > 0 ? n_add : 0;
			D  += D > 0 ? D_add1 : D_add0;
		}
}

struct _kahto_dashed_line_args {
	uint32_t *canvas, color, *colors;
	int ystride, *xy, ithickness, *axis_area, nosteep, patternlength;
	struct kahto_figure *fig;
	float *pattern;
};

struct _kahto_line_args {
	short *xypixels;
	long x0, len;
	uint32_t *canvas;
	const int ystride, *axis_xywh;
	struct kahto_figure *fig;
};

static uint32_t draw_line_bresenham_dashed(struct _kahto_dashed_line_args *args, uint32_t carry) {
	int nosteep		= args->nosteep,
		ystride		= args->ystride;
	struct kahto_figure *fig = args->fig;
	uint32_t *canvas	     = args->canvas,
			 color           = args->color,
			 *colors         = args->colors,
			 colornow;

	int backwards = args->xy[2+!nosteep] < args->xy[!nosteep]; // m1 < m0
	int m1=args->xy[2*!backwards+!nosteep], m0=args->xy[2*backwards+!nosteep],
		n1=args->xy[2*!backwards+nosteep],  n0=args->xy[2*backwards+nosteep]; // Δm ≥ Δn

	const int n_add = n1 > n0 ? 1 : -1;
	const int dm = m1 - m0;
	const int dn = n1 > n0 ? n1 - n0 : n0 - n1;
	const int D_add0 = 2 * dn;
	const int D_add1 = 2 * (dn - dm);
	int D = 2*dn - dm;
	const float coef = dm / sqrt(dm*dm + dn*dn); // m-yksikön pituus = coef * hypotenuusa
	unsigned short ipat = carry>>16, try = (carry & 0xffff) + m0;
	ipat++; // carry:ssä on ipat-1, jotta oletusarvona toimii 0

#define move_forward	\
	n0 += D > 0 ? n_add : 0,	\
	D  += D > 0 ? D_add1 : D_add0
	if (nosteep) { // (m,n) = (x,y)
		while (1) {
			if (colors) colornow = colors[ipat];
			else colornow = color * (ipat % 2 == 0);
			int end = min(m1+1, try);
			if (colornow)
				for (; m0<end; m0++) {
					canvas[n0*ystride + m0] = colornow; // only this line differs
					move_forward;
				}
			else
				for (; m0<end; m0++)
					move_forward;
			if (m0 > m1)
				break;
			ipat = (ipat + 1) % args->patternlength;
			try = m0 + topixels(args->pattern[ipat]*coef, fig);
		}
	}
	else { // (m,n) = (y,x)
		while (1) {
			if (colors) colornow = colors[ipat];
			else colornow = color * (ipat % 2 == 0);
			int end = min(m1+1, try);
			if (colornow)
				for (; m0<end; m0++) {
					canvas[m0*ystride + n0] = colornow; // only this line differs
					move_forward;
				}
			else
				for (; m0<end; m0++)
					move_forward;
			if (m0 > m1)
				break;
			ipat = (ipat + 1) % args->patternlength;
			try = m0 + topixels(args->pattern[ipat]*coef, fig);
		}
	}
	return (ipat-1)<<16 | (try-m0);
#undef move_forward
}

/* anti-aliased line */
static void draw_line_xiaolin(uint32_t *canvas_, int ystride, const int *xy, uint32_t color) {
	uint32_t (*canvas)[ystride] = (void*)canvas_;
	int nosteep = Abs(xy[3] - xy[1]) < Abs(xy[2] - xy[0]);
	int backwards = xy[2+!nosteep] < xy[!nosteep]; // m1 < m0
	int    m1=xy[2*!backwards+!nosteep], m0=xy[2*backwards+!nosteep];
	float fn1=xy[2*!backwards+nosteep], fn0=xy[2*backwards+nosteep];

	const float slope = (fn1 - fn0) / (m1 - m0);

	if (nosteep) { // (m,n) = (x,y)
		for (; m0<=m1; m0++) {
			int n0 = fn0;
			float off = fn0 - n0;
			if (off >= 0.5) {
				canvas [n0 + 1] [m0] = color;
				tocanvas(&canvas[n0][m0], iroundpos((1-off) * 255), color);
			}
			else {
				canvas[n0][m0] = color;
				tocanvas(&canvas[n0+1][m0], iroundpos(off*255), color);
			}
			fn0 += slope;
		}
	}
	else { // (m,n) = (y,x)
		for (; m0<=m1; m0++) {
			int n0 = fn0;
			float off = fn0 - n0;
			if (off >= 0.5) {
				canvas [m0] [n0 + 1] = color;
				tocanvas(&canvas[m0][n0], iroundpos((1-off) * 255), color);
			}
			else {
				canvas[m0][n0] = color;
				tocanvas(&canvas[m0][n0 + 1], iroundpos(off*255), color);
			}
			fn0 += slope;
		}
	}
}

static uint32_t draw_line_xiaolin_dashed(struct _kahto_dashed_line_args *args, uint32_t carry) {
	int nosteep = args->nosteep;
	struct kahto_figure *fig = args->fig;
	uint32_t (*canvas)[args->ystride] = (void*)args->canvas,
			 color = args->color, *colors = args->colors, colornow;

	int backwards = args->xy[2+!nosteep] < args->xy[!nosteep]; // m1 < m0
	int m1=args->xy[2*!backwards+!nosteep], m0=args->xy[2*backwards+!nosteep];
	float fn1=args->xy[2*!backwards+nosteep], fn0=args->xy[2*backwards+nosteep]; // Δm ≥ Δn

	if (m1 == m0)
		return carry;
	int dm = m1 - m0;
	const float dn = fn1 - fn0;
	const float slope = dn / dm;
	const float coef = dm / sqrt(dm*dm + dn*dn); // m-yksikön pituus = coef * hypotenuusa
	unsigned short ipat = carry>>16, m1dash = (carry & 0xffff) + m0;
	ipat++; // carry:ssä on ipat-1, jotta oletusarvona toimii 0

	if (nosteep) { // (m,n) = (x,y)
		while (1) {
			if (colors) colornow = colors[ipat];
			else colornow = color * (ipat % 2 == 0);
			int end = min(m1+1, m1dash);
			for (; m0<end; m0++) {
				int n0 = fn0;
				int level = iroundpos((fn0 - n0) * 255);
				if (colornow) {
					tocanvas(&canvas[n0][m0], 255-level, colornow);	// only these lines differ
					tocanvas(&canvas[n0+1][m0], level, colornow);	// only these lines differ
				}
				fn0 += slope;
			}
			if (m0 > m1)
				break;
			ipat = (ipat + 1) % args->patternlength;
			int add = topixels(args->pattern[ipat]*coef, fig);
			if (add == 0)
				break; // avoid halting with small figure size
			m1dash = m0 + add;
		}
	}
	else { // (m,n) = (y,x)
		while (1) {
			if (colors) colornow = colors[ipat];
			else colornow = color * (ipat % 2 == 0);
			int end = min(m1+1, m1dash);
			for (; m0<end; m0++) {
				int n0 = fn0;
				int level = iroundpos((fn0 - n0) * 255);
				if (colornow) {
					tocanvas(&canvas[m0][n0], 255-level, colornow);	// only these lines differ
					tocanvas(&canvas[m0][n0+1], level, colornow);	// only these lines differ
				}
				fn0 += slope;
			}
			if (m0 > m1)
				break;
			ipat = (ipat + 1) % args->patternlength;
			int add = topixels(args->pattern[ipat]*coef, fig);
			if (add == 0)
				break; // avoid halting with small figure size
			m1dash = m0 + add;
		}
	}
	return (ipat-1)<<16 | (m1dash-m0);
}

static int check_line(int *line, const int *area) {
	if (line[0] == line[2]) {
		if (line[0] < area[0] || line[0] >= area[2])
			return 1;
		update_max(line[0], area[0]);
		update_max(line[2], area[0]);
		update_max(line[1], area[1]);
		update_max(line[3], area[1]);

		update_min(line[0], area[2]-1);
		update_min(line[2], area[2]-1);
		update_min(line[1], area[3]-1);
		update_min(line[3], area[3]-1);
		return 0;
	}

	float slope = (float)(line[3] - line[1]) / (line[2] - line[0]);
	if (line[0] < area[0]) {
		line[0] = area[0];
		line[1] = iround(line[3] - ((line[2]-line[0]) * slope));
	}
	else if (line[0] >= area[2]) {
		line[0] = area[2]-1;
		line[1] = iround(line[3] - ((line[2]-line[0]) * slope));
	}

	if (line[1] < area[1]) {
		if (slope == 0)
			return 1;
		line[1] = area[1];
		if ((line[0] = iround(line[2] - ((line[3]-line[1]) / slope))) < area[0])
			return 1;
	}
	else if (line[1] >= area[3]) {
		if (slope == 0)
			return 1;
		line[1] = area[3]-1;
		if ((line[0] = iround(line[2] - ((line[3]-line[1]) / slope))) < area[0])
			return 1;
	}

	if (line[2] < area[0]) {
		line[2] = area[0];
		line[3] = iround(line[1] + ((line[2]-line[0]) * slope));
	}
	else if (line[2] >= area[2]) {
		line[2] = area[2]-1;
		line[3] = iround(line[1] + ((line[2]-line[0]) * slope));
	}

	if (line[3] < area[1]) {
		if (slope == 0)
			return 1;
		line[3] = area[1];
		if ((line[2] = iround(line[0] + ((line[3]-line[1]) / slope))) < area[2])
			return 1;
	}
	else if (line[3] >= area[3]) {
		if (slope == 0)
			return 1;
		line[3] = area[3]-1;
		if ((line[2] = iround(line[0] + ((line[3]-line[1]) / slope))) < area[2])
			return 1;
	}
	return 0;
}

static void _draw_thick_line_bresenham_xiaolin(uint32_t *canvas, int ystride,
	int xy[4], uint32_t color, int ithickness, int *axis_area, int nosteep)
{
	if (!check_line(xy, axis_area))
		draw_line_xiaolin(canvas, ystride, xy, color);
	xy[nosteep+0]++;
	xy[nosteep+2]++;
	int p = 1;
	for (; p<ithickness-1; p++) {
		if (!check_line(xy, axis_area))
			draw_line_bresenham(canvas, ystride, xy, color);
		xy[nosteep+0]++;
		xy[nosteep+2]++;
	}
	if (p < ithickness && !check_line(xy, axis_area))
		draw_line_xiaolin(canvas, ystride, xy, color);
}

static uint32_t _draw_thick_line_dashed(struct _kahto_dashed_line_args *args, uint32_t carry) {
	uint32_t new_carry = carry;
	if (!check_line(args->xy, args->axis_area))
		new_carry = draw_line_xiaolin_dashed(args, carry);
	args->xy[args->nosteep+0]++;
	args->xy[args->nosteep+2]++;
	int p = 1;
	for (; p<args->ithickness-1; p++) {
		if (!check_line(args->xy, args->axis_area))
			new_carry = draw_line_bresenham_dashed(args, carry);
		args->xy[args->nosteep+0]++;
		args->xy[args->nosteep+2]++;
	}
	if (p < args->ithickness && !check_line(args->xy, args->axis_area))
		new_carry = draw_line_xiaolin_dashed(args, carry);
	return new_carry;
}

static uint32_t draw_line(uint32_t *canvas, int ystride, const int *xy_c, int *area,
	struct kahto_linestyle *style, struct kahto_figure *fig, struct draw_data_args *dotargs, int32_t carry)
{
	if (style->style == kahto_line_future_e) {
		draw_line_kahto(canvas, ystride, xy_c, style->color, tofpixels(style->thickness, fig), area);
		return 0;
	}

	int xy[4];
	memcpy(xy, xy_c, sizeof(xy));
	float nthickness = topixels(style->thickness, fig); // initially just thickness,
	int n_ind = Abs(xy[3] - xy[1]) < Abs(xy[2] - xy[0]);
	// m is the direction which is always incremented (x on non-steep lines)
	// n is incremented only sometimes

	/* Vinon viivan leveys vaakasuunnassa on eri.
	   Yhtälö on johdettu kynällä ja paperilla yhdenmuotoisista kolmioista. */
	int dm = xy[2+!n_ind] - xy[!n_ind];
	int dn = xy[2+n_ind] - xy[n_ind];
	if (dm && dn) {
		float n_per_m = (float)dn / dm;
		nthickness *= sqrt(1 + (n_per_m * n_per_m));
	}

	if (nthickness < 1)
		nthickness = 1;
	int inthickness = iroundpos(nthickness);
	switch (style->align) {
		case 0:
			xy[n_ind+0] -= inthickness/2;
			xy[n_ind+2] -= inthickness/2;
			break;
		default:
		case 1:
			break;
		case -1:
			xy[n_ind+0] -= inthickness;
			xy[n_ind+2] -= inthickness;
			break;
	}

	static float __default_dashpattern[] = {1.0/60, 1.0/60};

	switch (style->style) {
		case kahto_line_dashed_e:
			if (!style->pattern) {
				style->pattern = __default_dashpattern;
				style->patternlen = 2;
			}
			if (!style->patternlen)
				style->patternlen = 2;
			struct _kahto_dashed_line_args args = {
				canvas, style->color, style->colors, ystride, xy, inthickness, area, n_ind,
				style->patternlen, fig, style->pattern };
			carry = _draw_thick_line_dashed(&args, carry);
			break;
		case kahto_line_circle_e:
			if (dotargs) {
				_draw_line_circle_e(canvas, ystride, xy, style->color, dotargs);
				break;
			}
			/* run through */
		default:
		case kahto_line_normal_e:
		case kahto_line_bresenham_xiaolin_e:
			_draw_thick_line_bresenham_xiaolin(canvas, ystride, xy, style->color, inthickness, area, n_ind);
		case kahto_line_none_e:
			break;
	}
	return carry;
}
