#include <png.h>
#include <string.h>
#include <err.h>
#include <stdlib.h>
#include "kahto_mkdir.c"

static void argb_to_bgr(void *vfrom, unsigned char *to, long size) {
	char *from = vfrom;
	for (int i=0; i<size; i++) {
		to[i*3+0] = from[i*4+2];
		to[i*3+1] = from[i*4+1];
		to[i*3+2] = from[i*4+0];
	}
}

static int write_png(uint32_t *argb, const char* name, int draw_w, int draw_h) {
	png_structp png_p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_p)
		return 1;
	png_infop info_p = png_create_info_struct(png_p);
	if (!info_p) {
		png_destroy_write_struct(&png_p, NULL);
		return 1;
	}

	png_set_IHDR(
		png_p, info_p,
		draw_w, draw_h, 8, PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	FILE* file = fopen(name, "w");
	if (!file) {
		warn("fopen %s (%s)", name, __func__);
		return 1;
	}
	png_init_io(png_p, file);
	png_write_info(png_p, info_p);

	png_byte* pngdata = malloc(draw_w * 3);
	for (int j=0; j<draw_h; j++) {
		argb_to_bgr(argb+j*draw_w, pngdata, draw_w);
		png_write_row(png_p, pngdata);
	}
	free(pngdata);

	png_write_end(png_p, info_p);
	fclose(file);
	png_destroy_write_struct(&png_p, &info_p);

	return 0;
}

struct kahto_figure* kahto_write_png_preserve_va(struct kahto_figure *fig, const char *name_, va_list va) {
	kahto_layout(fig); // might change fig->wh
	unsigned *argb = malloc(fig->wh[0] * fig->wh[1] * sizeof(unsigned));
	kahto_draw_figures(fig, argb, fig->wh[0]);

	char *name = NULL;
	char freename = 0;
	if (name_) {
		vasprintf(&name, name_, va);
		freename = 1;
	}

	char _name[80];
	if (!name) {
		if (fig->name)
			name = fig->name;
		else {
			sprintf(_name, "kahto_%li.png", (long)time(NULL));
			name = _name;
		}
	}

	mkdir_file(name);
	write_png(argb, name, fig->wh[0], fig->wh[1]);
	free(argb);
	if (freename)
		free(name);
	return fig;
}
