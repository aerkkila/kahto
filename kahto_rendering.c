#include <math.h>
#include <ttra.h>
#include <err.h>
#ifndef KAHTO_NO_VERSION_CHECK
#define KAHTO_NO_VERSION_CHECK
#endif
#include "kahto.h"
#include "definitions.h"

static inline void tocanvas(uint32_t *ptr, int value, uint32_t color) {
	int eulav = 255 - value;
	int fg2 = color >> 16 & 0xff,
		fg1 = color >> 8 & 0xff,
		fg0 = color >> 0 & 0xff,
		bg2 = *ptr >> 16 & 0xff,
		bg1 = *ptr >> 8 & 0xff,
		bg0 = *ptr >> 0 & 0xff;
	int c2 = (fg2 * value + bg2 * eulav) / 255,
		c1 = (fg1 * value + bg1 * eulav) / 255,
		c0 = (fg0 * value + bg0 * eulav) / 255;
	*ptr = (color & 0xff<<24) | c2 << 16 | c1 << 8 | c0 << 0;
}

static inline uint32_t from_cmap(const unsigned char *ptr) {
	return
		(ptr[0] << 16 ) |
		(ptr[1] << 8 ) |
		(ptr[2] << 0) |
		(0xff << 24);
}

#include "kahto_colormesh.c"
#include "kahto_init_markers.c"

struct draw_data_args {
	uint32_t *canvas;
	unsigned *canvascount;
	int ystride;
	const unsigned char *bmap;
	int mapw, maph, x, y;
	unsigned char alpha;
	const int *axis_xywh_outer;
	uint32_t color;

	short *xypixels;
	long x0;
	int len;
	const unsigned char *zlevels, *cmap;
	int reverse_cmap;
};

static void draw_datum_count(struct draw_data_args *ar) {
	unsigned (*count)[ar->axis_xywh_outer[2]] = (void*)ar->canvascount;
	int x = ar->x, y = ar->y;
	float alpha = ar->alpha;
	if (!ar->bmap) {
		if (0 <= x && x < ar->axis_xywh_outer[2] && 0 <= y && y < ar->axis_xywh_outer[3])
			count[y][x] += alpha;
		return;
	}
	int j0 = max(0, y-ar->maph/2);
	int j1 = min(ar->axis_xywh_outer[3]-1, y+(ar->maph-ar->maph/2));
	int i0 = max(0, x-ar->mapw/2);
	int i1 = min(ar->axis_xywh_outer[2]-1, x+(ar->mapw-ar->mapw/2));
	int bj0 = j0 - (y-ar->maph/2);
	int bi0 = i0 - (x-ar->mapw/2);
	const unsigned char (*bmap)[ar->mapw] = (void*)ar->bmap;
	alpha /= 255;
	for (int j=j0; j<j1; j++, bj0++)
		for (int i=i0, bi=bi0; i<i1; i++, bi++)
			count[j][i] += bmap[bj0][bi] * alpha;
}

static inline void draw_datum(struct draw_data_args *ar) {
	if (ar->canvascount)
		draw_datum_count(ar);
	if (!ar->bmap) {
		if (0 <= ar->x && ar->x < ar->axis_xywh_outer[2] && 0 <= ar->y && ar->y < ar->axis_xywh_outer[3])
			ar->canvas[(ar->axis_xywh_outer[1] + ar->y) * ar->ystride + ar->axis_xywh_outer[0] + ar->x] = ar->color;
		/* alphaa ei ole toteutettu tähän */
		return;
	}
	int x0_ = ar->axis_xywh_outer[0],       y0_ = ar->axis_xywh_outer[1];
	int x1_ = x0_ + ar->axis_xywh_outer[2], y1_ = y0_ + ar->axis_xywh_outer[3];
	int x0  = x0_ + ar->x - ar->mapw/2,      y0 = y0_ + ar->y - ar->maph/2;
	int j0  = max(0, y0_ - y0);
	int j1  = min(ar->maph, y1_ - y0);
	int i0  = max(0, x0_ - x0);
	int i1  = min(ar->mapw, x1_ - x0);
	uint32_t (*canvas)[ar->ystride] = (void*)ar->canvas;
	const unsigned char (*bmap)[ar->mapw] = (void*)ar->bmap;
	int alpha = ar->alpha;
	for (int j=j0, y=y0+j0; j<j1; j++, y++)
		for (int i=i0; i<i1; i++)
			tocanvas(canvas[y]+x0+i, bmap[j][i]*alpha/255, ar->color);
}

static inline void draw_datum_for_line(struct draw_data_args *ar) {
	int x0_ = ar->axis_xywh_outer[0],       y0_ = ar->axis_xywh_outer[1];
	int x1_ = x0_ + ar->axis_xywh_outer[2], y1_ = y0_ + ar->axis_xywh_outer[3];
	int x0  = ar->x - ar->mapw/2,           y0 = ar->y - ar->maph/2; // x0_ and x1_ has been already taken into account
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

static inline void draw_datum_cmap(struct draw_data_args *restrict ar) {
	if (!ar->bmap) {
		if (0 <= ar->x && ar->x < ar->axis_xywh_outer[2] && 0 <= ar->y && ar->y < ar->axis_xywh_outer[3])
			ar->canvas[(ar->axis_xywh_outer[1] + ar->y) * ar->ystride + ar->axis_xywh_outer[0] + ar->x] = from_cmap(ar->cmap + 128*3);
		return;
	}
	float colors_per_y = 256.0/ar->maph,
		  colors_per_x = 256.0/ar->mapw;
	for (int j=0; j<ar->maph; j++) {
		int jaxis = ar->y - ar->maph/2 + j;
		if (jaxis < 0 || jaxis >= ar->axis_xywh_outer[3]) continue;
		float ylevel = colors_per_y * j;
		for (int i=0; i<ar->mapw; i++) {
			int iaxis = ar->x - ar->mapw/2 + i;
			if (iaxis < 0 || iaxis >= ar->axis_xywh_outer[2]) continue;
			int value = ar->bmap[j*ar->mapw + i];
			uint32_t *ptr = &ar->canvas[(ar->axis_xywh_outer[1]+jaxis) * ar->ystride + ar->axis_xywh_outer[0] + iaxis];
			int colorind = iroundpos(0.5 * (ylevel + colors_per_x * i));
			uint32_t color = from_cmap(ar->cmap + colorind*3);
			tocanvas(ptr, value, color);
		}
	}
}

#include "kahto_draw_line.c" // future default method
#include "kahto_draw_line_more.c" // other methods

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

void init_literal(unsigned char *bmap, int w, int h, struct kahto_graph *graph) {
	memset(bmap, 0, w*h);
	struct ttra *ttra = graph->yxaxis[0]->figure->ttra;
	struct ttra memttra = *ttra;
	ttra_set_xy0(ttra, 0, 0);
	ttra_set_fontheight(ttra, h);
	ttra->canvas = (void*)bmap;
	ttra->ystride = ttra->x1 = w;
	ttra->y1 = h;
	ttra->alphamode = 1;
	ttra_print(ttra, graph->markerstyle.marker);
	*ttra = memttra;
}

static void draw_data_xy(struct draw_data_args *restrict ar) {
	for (int idata=0; idata<ar->len; idata++) {
		ar->x = ar->xypixels[idata*2];
		ar->y = ar->xypixels[idata*2+1];
		draw_datum(ar);
	}
}

static void draw_data_xya(struct draw_data_args *restrict ar) {
	for (int idata=0; idata<ar->len; idata++) {
		ar->x = ar->xypixels[idata*2];
		ar->y = ar->xypixels[idata*2+1];
		ar->alpha = ar->zlevels[idata];
		draw_datum(ar);
	}
}

static void draw_data_xyc(struct draw_data_args *restrict ar) {
	if (ar->reverse_cmap)
		for (int idata=0; idata<ar->len; idata++) {
			ar->x = ar->xypixels[idata*2];
			ar->y = ar->xypixels[idata*2+1];
			ar->color = from_cmap(ar->cmap + (255-ar->zlevels[idata])*3);
			draw_datum(ar);
		}
	else
		for (int idata=0; idata<ar->len; idata++) {
			ar->x = ar->xypixels[idata*2];
			ar->y = ar->xypixels[idata*2+1];
			ar->color = from_cmap(ar->cmap + ar->zlevels[idata]*3);
			draw_datum(ar);
		}
}

static void draw_data_xyc_list(struct draw_data_args *restrict ar, const uint32_t *colors, int ncolors) {
	long idata0 = ar->x0;
	for (int idata=0; idata<ar->len; idata++) {
		ar->x = ar->xypixels[idata*2];
		ar->y = ar->xypixels[idata*2+1];
		ar->color = colors[(idata0+idata) % ncolors];
		draw_datum(ar);
	}
}

static void make_datapx(short *xypixels, int stride, long istart, long iend,
	double xpix_per_unit, double axis_x0, int addthis, struct kahto_data *xdata)
{
	double xstep = xdata->length > 1 ? (xdata->minmax[1] - xdata->minmax[0]) / (xdata->length-1) : 0;
	double x0 = xdata->minmax[0] - axis_x0;
	for (long idata=istart, ipix=0; idata<iend; idata++, ipix++)
		xypixels[ipix*stride] = iroundpos((x0 + idata*xstep) * xpix_per_unit) + addthis;
}

/* huomio: datan liittäminen kahden tämän funktion kutsun välillä on toteuttamatta */
static void connect_data_xy(struct _kahto_line_args *restrict args, struct kahto_linestyle *linestyle, struct draw_data_args *dataargs) {
	int axis_area[] = xywh_to_area(args->axis_xywh);

	uint32_t carry = 0;
	for (int i=0; i<args->len-1; i++, args->xypixels += 2) {
		if (args->xypixels[0] == NOT_A_PIXEL) continue;
		if (args->xypixels[1] == NOT_A_PIXEL) continue;
		if (args->xypixels[2] == NOT_A_PIXEL) continue;
		if (args->xypixels[3] == NOT_A_PIXEL) continue;
		int xy[] = {
			args->xypixels[0] + args->axis_xywh[0],
			args->xypixels[1] + args->axis_xywh[1],
			args->xypixels[2] + args->axis_xywh[0],
			args->xypixels[3] + args->axis_xywh[1],
		};
		carry = draw_line(args->canvas, args->ystride, xy, axis_area, linestyle, args->fig, dataargs, carry);
	}
}

static int kahto_visible_marker(const char *str) {
	return str && (unsigned char)*str > ' ';
}

static int kahto_visible_graph(struct kahto_graph *graph) {
	return kahto_visible_marker(graph->markerstyle.marker) || graph->linestyle.style || graph->errstyle.style;
}

/* width and height wouldn't have to be pointers */
static unsigned char* kahto_data_marker_bmap(struct kahto_graph *graph, unsigned char *bmap, int *has_marker, int *width, int *height) {
	if (!graph->markerstyle.marker) {
		*has_marker = 0;
		return NULL;
	}
	typeof(init_literal) *initfun = init_literal;
	*has_marker = 1;
	if (!graph->markerstyle.literal)
		switch (*graph->markerstyle.marker) {
			case ' ':
			case  0 : *has_marker = 0;
			case '.': return NULL;
			case '+': initfun = (void*)init_plus; break;
			case '^': initfun = (void*)init_triangle; break;
			case '*':
			case '4': initfun = (void*)init_4star; break;
			case 'o': initfun = (void*)init_circle; break;
			default: break;
		}

	initfun(bmap, *width, *height, graph);

	if (graph->markerstyle.nofill) {
		int W = *width, H = *height;
		int w = W * graph->markerstyle.nofill,
			h = *height * graph->markerstyle.nofill;
		int x0 = (W - w) / 2,
			y0 = (H - h) / 2;
		unsigned char *bmap1 = malloc(w * h);
		initfun(bmap1, w, h, graph);
		for (int j=0; j<h; j++)
			for (int i=0; i<w; i++)
				bmap[(j+y0)*W + i+x0] -= bmap1[j*w+i];
		free(bmap1);
	}
	return bmap;
}

void kahto_graph_render(struct kahto_graph *graph, uint32_t *canvas, int ystride, struct kahto_figure *fig, long start) {
	if (is_colormesh(graph))
		return kahto_colormesh_render(graph, canvas, ystride, fig, start);
	double yxmin[] = {
		graph->yxaxis[0]->min,
		graph->yxaxis[1]->min,
	};
	double yxdiff[] = {
		graph->yxaxis[0]->max - yxmin[0],
		graph->yxaxis[1]->max - yxmin[1],
	};
	const int *margin = graph->yxaxis[0]->figure->ro_inner_margin;
	const int *xywh0 = graph->yxaxis[0]->figure->ro_inner_xywh;
	int yxlen[] = {xywh0[3]-margin[1]-margin[3], xywh0[2]-margin[0]-margin[2]};
	const long npoints = 1024;
	short xypixels[npoints*2];
	unsigned char zlevels[npoints];

	int width, height, marker;
	width = height = topixels(graph->markerstyle.size, fig);
	unsigned char bmap_buff[width*height];
	unsigned char *bmap = kahto_data_marker_bmap(graph, bmap_buff, &marker, &width, &height);
	/* bmap points to bmap_buff or is NULL or a malloced pointer */

	unsigned char *linepen_bmap = NULL;
	int linepen_width, linepen_height, helper;
	linepen_width = linepen_height = topixels(graph->linestyle.thickness, fig);
	unsigned char linepen_buff[linepen_width*linepen_height];
	if (graph->linestyle.style == kahto_line_circle_e) {
		struct kahto_graph copy = *graph;
		copy.markerstyle.marker = "o";
		copy.markerstyle.size = graph->linestyle.thickness;
		copy.markerstyle.literal = copy.markerstyle.nofill = 0;
		linepen_bmap = kahto_data_marker_bmap(&copy, linepen_buff, &helper, &linepen_width, &linepen_height);
	}

	int line_thickness = topixels(graph->linestyle.thickness, fig);
	if (line_thickness < 1) line_thickness = 1;

	struct kahto_axis *caxis = graph->yxaxis[2];

	struct draw_data_args data_args = {
		.canvas = canvas,
		.ystride = ystride,
		.bmap = bmap,
		.mapw = width,
		.maph = height,
		.axis_xywh_outer = xywh0,
		.cmap = caxis ? caxis->cmap : NULL,
		.reverse_cmap = caxis ? caxis->reverse_cmap : 0,
		.color = graph->color,
		.alpha = graph->alpha,
		/*
		 * xypixels, x0, len
		 * zlevels
		 * (x, y), not set in this function
		 */
	};

	struct draw_data_args linepen_dataargs = data_args;
	linepen_dataargs.bmap = linepen_bmap;
	linepen_dataargs.mapw = linepen_width;
	linepen_dataargs.maph = linepen_height;

	struct kahto_data
		*xdata = graph->data.list.xdata,
		*ydata = graph->data.list.ydata,
		*zdata = graph->data.list.zdata;

	if (graph->markerstyle.count)
		data_args.canvascount = calloc(xywh0[2] * xywh0[3], sizeof(unsigned));

	long length = graph->data.list.ydata->length;
	for (long istart=start; istart<length; ) {
		long iend = min(istart+npoints, length);
		long num = iend - istart;
		if (xdata->data)
			get_datapx[xdata->type](istart, iend, xdata->data, xypixels+0,
				yxmin[1], yxdiff[1], yxlen[1], xdata->stride, 2, margin[0]);
		else {
			double xstep = xdata->length > 1 ? (xdata->minmax[1] - xdata->minmax[0]) / (xdata->length-1) : 0;
			double xpix_per_unit = yxlen[1] / yxdiff[1] * xstep;
			make_datapx(xypixels+0, 2, istart, iend, xpix_per_unit, yxmin[1], margin[0], xdata);
		}
		if (zdata) {
			if (my_isnan(caxis->center))
				get_datalevels[zdata->type](istart, iend, zdata->data, zlevels,
					caxis->min, caxis->max, 255, zdata->stride);
			else
				get_datalevels_with_center[zdata->type](istart, iend, zdata->data, zlevels,
					caxis->min, caxis->center, caxis->max, 255, zdata->stride);
		}
		get_datapx_inv[ydata->type](istart, iend, ydata->data, xypixels+1, yxmin[0], yxdiff[0], yxlen[0], ydata->stride, 2, margin[1]);
		if (graph->linestyle.style) {
			struct _kahto_line_args args = {
				.xypixels = xypixels,
				.x0 = istart,
				.len = num,
				.canvas = canvas,
				.ystride = ystride,
				.axis_xywh = xywh0,
				.fig = fig,
			};
			connect_data_xy(&args, &graph->linestyle, &linepen_dataargs);
		}

		if (marker) {
			data_args.xypixels = xypixels;
			data_args.x0 = istart;
			data_args.len = num;
			data_args.zlevels = zlevels;
			if (caxis) {
				if (caxis->feature == kahto_color_e)
					draw_data_xyc(&data_args);
				else if (caxis->feature == kahto_alpha_e)
					draw_data_xya(&data_args);
			}
			else if (graph->colors)
				draw_data_xyc_list(&data_args, graph->colors, graph->ncolors);
			else
				draw_data_xy(&data_args);
		}

		/* error bars */
		for (int icoord=0; icoord<2; icoord++) {
			struct kahto_data *edata = graph->data.arr[arrlen(graph->data.arr)-2+icoord];
			if (!edata)
				continue;
			const int ixcoord = 0;
			short errb[npoints];
			get_datapx_inv[edata->type](istart, iend, edata->data, errb, yxmin[0], yxdiff[0], yxlen[0], edata->stride, 1, margin[1]);
			int area[] = xywh_to_area(xywh0);
			struct kahto_linestyle style = graph->errstyle;
			for (int i=0; i<iend-istart; i++) {
				int line[] = {xypixels[i*2+ixcoord], errb[i], xypixels[i*2+ixcoord], xypixels[i*2+!ixcoord]};
				if (line[0] == NOT_A_PIXEL) continue;
				if (line[1] == NOT_A_PIXEL) continue;
				if (line[2] == NOT_A_PIXEL) continue;
				if (line[3] == NOT_A_PIXEL) continue;
				line[0] += xywh0[0];
				line[2] += xywh0[0];
				line[1] += xywh0[1];
				line[3] += xywh0[1];
				if (graph->colors)
					style.color = graph->colors[(i+istart) % graph->ncolors];
				draw_line(canvas, ystride, line, area, &style, fig, NULL, 0);
				/*if (!check_line(line, area))
				  draw_line_y(canvas, ystride, line, data->errstyle.color);*/
			}
		}
		istart = iend;
	}

	if (!data_args.canvascount)
		return;

	unsigned (*count)[xywh0[2]] = (void*)data_args.canvascount;
	float min, max;
	struct kahto_axis *countaxis = graph->countaxis;
	if (!countaxis || countaxis->range_isset != kahto_range_isset) {
		min = max = count[0][0];
		for (int j=0; j<xywh0[3]; j++)
			for (int i=0; i<xywh0[2]; i++)
				if (count[j][i] > max)
					max = count[j][i];
				else if (count[j][i] < min)
					min = count[j][i];
		min /= 255;
		max /= 255;
		if (countaxis) {
			countaxis->min = min;
			countaxis->max = max;
		}
		fprintf(stderr, "Warning: countaxis range must be given by user\n");
		fprintf(stderr, "current range is [%f, %f]\n", min, max);
	}
	else
		min = countaxis->min,
			max = countaxis->max;
	float range = max - min;

	const unsigned char *cmap = countaxis ? countaxis->cmap : cmh_colormaps[default_colormap].map;
	uint32_t (*canvasview)[ystride] = (void*)(canvas + xywh0[1]*ystride + xywh0[0]);
	for (int j=0; j<xywh0[3]; j++)
		for (int i=0; i<xywh0[2]; i++) {
			if (!count[j][i])
				continue;
			float level = (count[j][i]/255 - min) / range;
			if (level < 0)
				level = 0;
			else if (level > 1)
				level = 1;
			int ilevel = iroundpos(level * 255);
			if (count[j][i] < 255)
				tocanvas(&canvasview[j][i], count[j][i], from_cmap(cmap+ilevel*3));
			else
				canvasview[j][i] = from_cmap(cmap+ilevel*3);
		}
	free(data_args.canvascount);
}

static void legend_draw_marker(struct kahto_figure *fig, struct kahto_graph *graph,
	uint32_t *canvas, int ystride, int x0, int y0, int text_left)
{
	int width, height, marker;
	width = height = topixels(graph->markerstyle.size, fig);
	unsigned char bmap_buff[width*height];
	unsigned char *bmap = kahto_data_marker_bmap(graph, bmap_buff, &marker, &width, &height);
	int *xywh = graph->yxaxis[0]->figure->ro_inner_xywh;
	x0 -= xywh[0];
	y0 -= xywh[1];
	struct kahto_axis *caxis = graph->yxaxis[2];
	if (graph->linestyle.style) {
		short xypixels[] = {x0 - (text_left+1)/3, y0, x0 + (text_left+1)/3, y0};
		struct _kahto_line_args args = {
			.xypixels = xypixels,
			.x0 = 0, .len = 2,
			.canvas = canvas,
			.ystride = ystride,
			.axis_xywh = xywh,
			.fig = fig,
		};
		connect_data_xy(&args, &graph->linestyle, NULL);
	}
	if (marker) {
		struct draw_data_args args = {
			.x = x0,
			.y = y0,
			.canvas = canvas,
			.ystride = ystride,
			.axis_xywh_outer = xywh,
			.bmap = bmap,
			.mapw = width,
			.maph = height,
			.color = graph->color,
			.cmap = caxis ? caxis->cmap : NULL,
			.reverse_cmap = caxis ? caxis->reverse_cmap : 0,
			.alpha = graph->alpha,
		};
		if (graph->data.list.zdata)
			draw_datum_cmap(&args);
		else
			draw_datum(&args);
	}

	if (bmap != bmap_buff)
		free(bmap);
}

void kahto_draw_box(uint32_t *canvas, int ystride, struct kahto_figure *fig, int *area, struct kahto_linestyle *linestyle) {
	int linewidth = topixels(linestyle->thickness, fig);
	struct kahto_linestyle lstyle = *linestyle;
	lstyle.thickness = -1; // becomes 1 for all fig sizes

	{
		int xy[] = {area[0], area[1], area[0], area[3]};
		for (int i=0; i<linewidth; i++) {
			draw_line(canvas, ystride, xy, area, &lstyle, fig, NULL, 0);
			xy[0]++; xy[2]++;
		}
	} {
		int x1 = area[2] - linewidth;
		int xy[] = {x1, area[1], x1, area[3]};
		for (int i=0; i<linewidth; i++) {
			draw_line(canvas, ystride, xy, area, &lstyle, fig, NULL, 0);
			xy[0]++; xy[2]++;
		}
	} {
		int xy[] = {area[0], area[1], area[2], area[1]};
		for (int i=0; i<linewidth; i++) {
			draw_line(canvas, ystride, xy, area, &lstyle, fig, NULL, 0);
			xy[1]++; xy[3]++;
		}
	} {
		int y1 = area[3] - linewidth;
		int xy[] = {area[0], y1, area[2], y1};
		for (int i=0; i<linewidth; i++) {
			draw_line(canvas, ystride, xy, area, &lstyle, fig, NULL, 0);
			xy[1]++; xy[3]++;
		}
	}
}

void kahto_fill_box(uint32_t *canvas, int ystride, const int *restrict area, uint32_t color) {
	for (int j=area[1]; j<area[3]; j++)
		for (int i=area[0]; i<area[2]; i++)
			canvas[j*ystride+i] = color;
}

void kahto_draw_box_xywh(uint32_t *canvas, int ystride, struct kahto_figure *fig, int *xywh, struct kahto_linestyle *linestyle) {
	int area[] = xywh_to_area(xywh);
	kahto_draw_box(canvas, ystride, fig, area, linestyle);
}

void kahto_fill_box_xywh(uint32_t *canvas, int ystride, int *xywh, uint32_t color) {
	int area[] = xywh_to_area(xywh);
	kahto_fill_box(canvas, ystride, area, color);
}

void kahto_legend_draw(struct kahto_figure *fig, uint32_t *canvas, int ystride) {
	if (!fig->legend.visible || no_room_for_legend(fig) || (fig->legend.visible < 0 && fig->legend.ro_place_err))
		return;
	uint32_t fillcolor = fig->background;
	switch (fig->legend.fill) {
		case kahto_fill_color_e:
			fillcolor = fig->legend.fillcolor;
			/* run through */
		case kahto_fill_bg_e:
			kahto_fill_box_xywh(canvas, ystride, fig->legend.ro_xywh, fillcolor);
			/* run through */
		case kahto_no_fill_e:
			kahto_draw_box_xywh(canvas, ystride, fig, fig->legend.ro_xywh, &fig->legend.borderstyle);
			break;
	}

	int leg_x0 = fig->legend.ro_xywh[0];
	int leg_y0 = fig->legend.ro_xywh[1];
	int linewidth = topixels(fig->legend.borderstyle.thickness, fig);
	/* lisään y:hyn +1:n, että kirjaimen ja viivan väliin jää tyhjä pikseli */
	ttra_set_xy0(fig->ttra, leg_x0 + fig->legend.ro_text_left + linewidth, leg_y0 + linewidth + 1);
	int text_left = fig->legend.ro_text_left;
	for (int i=0; i<fig->ngraph; i++) {
		if (!fig->graph[i]->label)
			continue;
		legend_draw_marker(
			fig, fig->graph[i], canvas, ystride,
			leg_x0 + text_left/2,
			fig->legend.ro_xywh[1] + (fig->legend.ro_datay[i] + fig->legend.ro_datay[i+1]) / 2 + linewidth + 1,
			text_left);
		/* drawing a literal marker changes fontheight */
		if (fig->graph[i]->label) {
			set_fontheight(fig, fig->legend.rowheight);
			uint32_t mem = fig->ttra->bg_default;
			fig->ttra->bg_default = fillcolor;
			ttra_printf(fig->ttra, "\033[0m%s\n", fig->graph[i]->label);
			fig->ttra->bg_default = mem;
		}
	}
}
