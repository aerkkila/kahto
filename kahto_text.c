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

	int ret = 0;
	if (iround(rot*100'000) % (400*100'000)) {
		struct ttra ttra0 = *ttra;
		unsigned *colors = malloc(wh[0]*wh[1] * 4);
		unsigned char *alpha = NULL;
		if (!colors) {
			warn("malloc %i * %i * 4 epäonnistui", wh[0], wh[1]);
			ret = 1;
			goto done;
		}
		ttra_set_xy0(ttra, 0, 0);
		ttra->ystride = ttra->x1 = wh[0];
		ttra->y1 = wh[1];
		ttra->clean_line = 1;
		ttra->fullcolormode = 1;
		ttra->canvas = colors;
		ttra_print(ttra, text);
		ttra->fullcolormode = 0;

		alpha = calloc(1, wh[0]*wh[1]);
		if (!alpha) {
			warn("malloc %i * %i epäonnistui", wh[0], wh[1]);
			*ttra = ttra0;
			ret = 1;
			goto done;
		}
		ttra_set_xy0(ttra, 0, 0);
		ttra->ystride = ttra->x1 = wh[0];
		ttra->y1 = wh[1];
		ttra->clean_line = 1;
		ttra->alphamode = 1;
		ttra->canvas = (void*)alpha;
		ttra_print(ttra, text);

		rotate(ttra0.canvas, ttra0.ystride, area_out[0], area_out[1],
			ttra0.x1-area_out[0], ttra0.y1-area_out[1], colors, alpha, wh[0], wh[1], rot);

done:
		*ttra = ttra0;
		free(colors);
		free(alpha);
		return ret;
	}

	ttra_set_xy0(ttra, area_out[0], area_out[1]);
	ttra_print(ttra, text);
	return 0;
}
