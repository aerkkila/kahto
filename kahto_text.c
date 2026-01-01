#include <err.h>

static int put_text(struct ttra *ttra, const char *text, int x, int y, float xalignment, float yalignment,
	float rot, int area_out[4], int area_only)
{
	int wh[2];
	float farea[4], fw, fh;

	ttra_get_textdims_pixels(ttra, text, wh+0, wh+1); // almost unnecessary when not rotated
	get_rotated_area(wh[0], wh[1], farea, rot); // almost unnecessary when not rotated
	fw = farea[2] - farea[0];
	fh = farea[3] - farea[1];

	area_out[0] = x + round(fw * xalignment);
	area_out[1] = y + round(fh * yalignment);
	area_out[2] = area_out[0] + round(fw);
	area_out[3] = area_out[1] + round(fh);

	if (area_only)
		return 0;

	if (area_out[0] < 0 || area_out[1] < 0)
		return -1;

	if (iround(rot*100'000) % (400*100'000)) {
		uint32_t *canvas0 = ttra->canvas;
		int ystride0 = ttra->ystride,
			x10 = ttra->x1,
			y10 = ttra->y1;

		if (!(ttra->canvas = malloc(wh[0]*wh[1] * sizeof(uint32_t)))) {
			warn("malloc %i * %i * %zu epäonnistui", wh[0], wh[1], sizeof(uint32_t));
			return 1;
		}
		ttra_set_xy0(ttra, 0, 0);
		ttra->ystride = ttra->x1 = wh[0];
		ttra->y1 = wh[1];
		ttra->clean_line = 1;
		ttra_print(ttra, text);

		rotate(canvas0, ystride0, area_out[0], area_out[1], x10-area_out[0], y10-area_out[1], ttra->canvas, wh[0], wh[1], rot);

		free(ttra->canvas);
		ttra->canvas = canvas0;
		ttra->ystride = ystride0;
		ttra->x1 = x10;
		ttra->y1 = y10;
		ttra->clean_line = 0;
		return 0;
	}

	ttra_set_xy0(ttra, area_out[0], area_out[1]);
	ttra_print(ttra, text);
	return 0;
}
