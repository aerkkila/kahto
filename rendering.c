#include <math.h>
#include <stdio.h>
#include <ttra.h>

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

static void put_text(struct ttra *ttra, char *text, int x, int y, enum $alignment alignment, float rot) {
    if (rot != 0)
	printf("Pyöräytystä ei ole toteutettu\n");
    int y0;
    switch (alignment) {
	default:
	    y0 = y;
	    break;
	case east_e: case center_e: case west_e:
	    y0 = y - ttra->fontheight * 0.5;
	    break;
	case southeast_e: case south_e: case southwest_e:
	    y0 = y - ttra->fontheight;
	    break;
    }
    if (y0 < 0)
	return;
    ttra_set_y0(ttra, y0);

    int x0, width;
    switch (alignment) {
	default:
	    x0 = x;
	    break;
	case north_e: case center_e: case south_e:
	    width = ttra_get_width_pixels(ttra, text);
	    x0 = x - width*0.5;
	    break;
	case southeast_e: case east_e: case northeast_e:
	    width = ttra_get_width_pixels(ttra, text);
	    x0 = x - width;
	    break;
    }
    if (x0 < 0)
	x0 = 0;
    ttra_set_x0(ttra, x0);
    
    ttra_print(ttra, text);
}
