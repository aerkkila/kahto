#ifdef HAVE_PNG // wraps the whole file

#include <png.h>
#include <string.h>
#include <err.h>
#include <stdlib.h>

static int write_png(unsigned char* rgb, const char* name, int draw_w, int draw_h) {
	png_structp png_p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(!png_p)
		return 1;
	png_infop info_p = png_create_info_struct(png_p);
	if(!info_p) {
		png_destroy_write_struct(&png_p, (png_infopp)NULL);
		return 1;
	}

	png_set_IHDR(
		png_p, info_p,
		draw_w, draw_h, 8, PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	FILE* file = fopen(name, "w");
	if(!file) {
		warn("Couldn't open %s (%s)", name, __func__);
		return 1;
	}
	png_init_io(png_p, file);
	png_write_info(png_p, info_p);

	png_byte** pngdata = png_malloc(png_p, draw_h * sizeof(void*));
	for (int j=0; j<draw_h; j++) {
		pngdata[j] = png_malloc(png_p, 3*draw_w);
		memcpy(pngdata[j], rgb + 3*draw_w * j, 3*draw_w);
	}

	png_set_rows(png_p, info_p, pngdata);
	png_write_png(png_p, info_p, PNG_TRANSFORM_IDENTITY, NULL);

	for (int j=0; j<draw_h; j++)
		png_free(png_p, pngdata[j]);
	png_free(png_p, pngdata);

	fclose(file);
	png_destroy_write_struct(&png_p, &info_p);

	return 0;
}

static void argb_to_bgr(void *vfrom, unsigned char *to, long size) {
	char *from = vfrom;
	for (int i=0; i<size; i++) {
		to[i*3+0] = from[i*4+2];
		to[i*3+1] = from[i*4+1];
		to[i*3+2] = from[i*4+0];
	}
}

void* cplot_write_png_preserve(void *vstandalone, const char *name) {
	struct cplot_subplots *subplots = vstandalone;
	unsigned *argb = malloc(subplots->wh[0] * subplots->wh[1] * sizeof(unsigned));

	if (subplots->type == cplot_axes_e)
		cplot_axes_draw(vstandalone, argb, subplots->wh[0]);
	else {
		cplot_subplots_to_axes(subplots);
		cplot_fill_u4(argb, subplots->background, subplots->wh[0], subplots->wh[1], subplots->wh[0]);
		for (int i=0; i<subplots->naxes; i++)
			if (subplots->axes[i])
				cplot_axes_draw(subplots->axes[i], argb, subplots->wh[0]);
	}

	char _name[80];
	if (!name) {
		if (subplots->name)
			name = subplots->name;
		else {
			sprintf(_name, "cplot_%li.png", (long)time(NULL));
			name = _name;
		}
	}

	unsigned char *bgr  = malloc(subplots->wh[0] * subplots->wh[1] * 3);
	argb_to_bgr(argb, bgr, subplots->wh[0] * subplots->wh[1]);
	free(argb);
	write_png(bgr, name, subplots->wh[0], subplots->wh[1]);
	free(bgr);
	return vstandalone;
}

void *cplot_write_png(void *vstandalone, const char *name) {
	return cplot_destroy(cplot_write_png_preserve(vstandalone, name)), NULL;
}

#endif // HAVE_PNG
