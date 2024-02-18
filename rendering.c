#include <math.h>
#include <stdio.h>
#include <ttra.h>
#include <err.h>

#define Abs(a) ((a) < 0 ? -(a) : (a))

/* https://en.wikipedia.org/wiki/Bresenham's_line_algorithm */
/* This method is nice because it uses only integers. */
static void draw_thick_line_bresenham(unsigned *data, int win_w, const int xy[4], unsigned väri, int leveys, int win_h) {
    int nosteep = Abs(xy[3] - xy[1]) < Abs(xy[2] - xy[0]);
    int backwards = xy[2+!nosteep] < xy[!nosteep]; // m1 < m0

    /* Vinon viivan leveys vaakasuunnassa on eri.
       Yhtälö on johdettu kynällä ja paperilla yhdenmuotoisista kolmioista. */
    int dy = xy[2+!nosteep] - xy[!nosteep];
    int dx = xy[2+nosteep] - xy[nosteep];
    int lev = leveys;
    if (dx && dy) {
	float x_per_y = (float)dx / dy;
	lev = round(leveys * sqrt(1 + (x_per_y * x_per_y)));
    }

    for (int p=-(lev-1)/2; p<=lev/2; p++) {
	int m1=xy[2*!backwards+!nosteep], m0=xy[2*backwards+!nosteep],
	    n1=xy[2*!backwards+nosteep]+p,  n0=xy[2*backwards+nosteep]+p;
	if (n0 < 0 && n1 < 0) continue;
	if (n1 >= win_h && n0 >= win_h && nosteep) continue;
	if (n1 >= win_w && n0 >= win_w && !nosteep) continue;

	const int n_add = n1 > n0 ? 1 : -1;
	const int dm = m1 - m0;
	const int dn = n1 > n0 ? n1 - n0 : n0 - n1;
	const int D_add0 = 2 * dn;
	const int D_add1 = 2 * (dn - dm);
	int D = 2*dn - dm;

	/* ohitetaan negatiivinen n0 */
	for (; m0<=m1 && n0 < 0; m0++) {
	    n0 += D > 0 ? n_add : 0;
	    D += D > 0 ? D_add1 : D_add0;
	}

	/* Miten n0-ehdon saisi ujutettua m0:an? */
	if (nosteep) { // (m,n) = (x,y)
	    if (n1 >= win_h)
		n1 = win_h-1;
	    for (; m0<=m1 && n0 <= n1; m0++) {
		data[n0*win_w + m0] = väri;
		n0 += D > 0 ? n_add : 0;
		D  += D > 0 ? D_add1 : D_add0;
	    }
	}
	else { // (m,n) = (y,x)
	    if (n1 >= win_w)
		n1 = win_w-1;
	    for (; m0<=m1 && n0 <= n1; m0++) {
		data[m0*win_w + n0] = väri;
		n0 += D > 0 ? n_add : 0;
		D  += D > 0 ? D_add1 : D_add0;
	    }
	}
    }
}

static int put_text(struct ttra *ttra, char *text, int x, int y, float xalignment, float yalignment, float rot, int area_out[4]) {
    int wh[2], x0, y0;

    if ((int)rot % 25)
	fprintf(stderr, "Tekstin pyöräytys on toteutettu vain suorakulmille.\n");

    ttra_get_textdims_pixels(ttra, text, wh+0, wh+1);

    y0 = y + wh[(int)rot%50 == 0] * yalignment; // height if not rotated
    x0 = x + wh[(int)rot%50 != 0] * xalignment; // height if rotated
    if (y0 < 0 || x0 < 0)
	return -1;

    int quarter = (int)rot % 50 != 0;
    area_out[0] = x0;
    area_out[1] = y0;
    area_out[2] = x0+wh[quarter];
    area_out[3] = y0+wh[!quarter];

    if ((int)rot % 100) {
	unsigned *canvas = ttra->canvas;
	int width0 = ttra->realw;
	int height0 = ttra->realh;

	if (!(ttra->canvas = malloc(wh[0]*wh[1] * sizeof(unsigned)))) {
	    warn("malloc %i * %i * %zu epäonnistui", wh[0], wh[1], sizeof(unsigned));
	    return 1;
	}
	ttra->realw = wh[0];
	ttra->realh = wh[1];
	ttra->clean_line = 1;
	ttra_set_xy0(ttra, 0, 0);
	ttra_print(ttra, text);
	rotate(canvas+y0*width0 + x0, width0, height0, ttra->canvas, wh[0], wh[1], rot);

	free(ttra->canvas);
	ttra->canvas = canvas;
	ttra->realw = width0;
	ttra->realh = height0;
	return 0;
    }

    ttra_set_xy0(ttra, x0, y0);
    ttra_print(ttra, text);
    return 0;
}

static void draw_data_xy(const short *xypixels, int len, unsigned *canvas, int axeswidth, int axeheight, int ystride, int *axis_xywh) {
    for (int idata=0; idata<len; idata++) {
	if (xypixels[idata*2] < 0 || xypixels[idata*2] > axis_xywh[2]) continue;
	if (xypixels[idata*2+1] < 0 || xypixels[idata*2+1] > axis_xywh[3]) continue;
	canvas[(axis_xywh[1] + xypixels[idata*2+1]) * ystride + axis_xywh[0] + xypixels[idata*2]] = 0;
    }
}

static void draw_data_x(const short *xypixels, double xdiff, long x0, int len,
    unsigned *canvas, int axeswidth, int axeheight, int ystride, int *axis_xywh)
{
    int y0 = axis_xywh[1], y1 = axis_xywh[1] + axis_xywh[3];
    double xjump = axis_xywh[2] / (xdiff-1);

    for (int idata=0; idata<len; idata++) {
	if (xypixels[idata*2+1] < y0 || xypixels[idata*2+1] > y1) continue;
	double xd = (x0 + idata) * xjump;
	int x = iroundpos(xd);
	canvas[(axis_xywh[1] + xypixels[idata*2+1]) * ystride + axis_xywh[0] + x] = 0;
    }
}

static void cplot_data_draw(struct $data *data, unsigned *canvas, int axeswidth, int axesheight, int ystride, int *axis_xywh) {
    double yxmin[] = {data->yxaxis[0]->min, data->yxaxis[1]->min};
    double yxdiff[] = {data->yxaxis[0]->max - yxmin[0], data->yxaxis[1]->max - yxmin[1]};
    int yxlen[] = {axis_xywh[3], axis_xywh[2]};
    const long npoints = 1024;
    short xypixels[npoints*2];

    for (long istart=0; istart<data->length; ) {
	long iend = min(istart+npoints, data->length);
	if (data->yxzdata[1])
	    get_datapx[data->yxztype[1]](istart, iend, data->yxzdata[1], xypixels+0, yxmin[1], yxdiff[1], yxlen[1]);
	get_datapx_inv[data->yxztype[0]](istart, iend, data->yxzdata[0], xypixels+1, yxmin[0], yxdiff[0], yxlen[0]);
	if (data->yxzdata[1])
	    draw_data_xy(xypixels, iend-istart, canvas, axeswidth, axesheight, ystride, axis_xywh);
	else
	    draw_data_x(xypixels, yxdiff[1], istart, iend-istart, canvas, axeswidth, axesheight, ystride, axis_xywh);
	istart = iend;
    }
}
