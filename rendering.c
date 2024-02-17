#include <math.h>
#include <stdio.h>
#include <ttra.h>
#include <err.h>

#define Abs(a) ((a) < 0 ? -(a) : (a))

/* https://en.wikipedia.org/wiki/Bresenham's_line_algorithm */
/* This method is nice because it uses only integers. */
static void draw_thick_line_bresenham(unsigned *data, int win_w, const $i4si xy, unsigned väri, int leveys, int win_h) {
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

static void put_text(struct ttra *ttra, char *text, int x, int y, float xalignment, float yalignment, float rot) {
    int wh[2], x0, y0;

    if ((int)rot % 25)
	fprintf(stderr, "Tekstin pyöräytys on toteutettu vain suorakulmille.\n");

    ttra_get_textdims_pixels(ttra, text, wh+0, wh+1);

    y0 = y + wh[(int)rot%50 == 0] * yalignment; // height if not rotated
    x0 = x + wh[(int)rot%50 != 0] * xalignment; // height if rotated
    if (y0 < 0 || x0 < 0)
	return;

    if ((int)rot % 100) {
	unsigned *canvas = ttra->canvas;
	int width0 = ttra->realw;
	int height0 = ttra->realh;

	if (!(ttra->canvas = malloc(wh[0]*wh[1] * sizeof(unsigned)))) {
	    warn("malloc %i * %i * %zu epäonnistui", wh[0], wh[1], sizeof(unsigned));
	    return;
	}
	ttra->realw = wh[0];
	ttra->realh = wh[1];
	ttra_set_xy0(ttra, 0, 0);
	ttra_print(ttra, text);
	rotate(canvas+y0*width0 + x0, width0, height0, ttra->canvas, wh[0], wh[1], rot);

	free(ttra->canvas);
	ttra->canvas = canvas;
	ttra->realw = width0;
	ttra->realh = height0;
	return;
    }

    ttra_set_xy0(ttra, x0, y0);
    ttra_print(ttra, text);
}
