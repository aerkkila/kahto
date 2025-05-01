#include <waylandhelper.h>
#include <xkbcommon/xkbcommon.h>

enum cplot_show_mode {typing_none, typing_savename};

struct highlight {
	struct cplot_figure *fig;
	int idata;
	char update;
	uint32_t *canvascopy;
	int id_canvascopy, canvascopy_start;
	struct cplot_figure *fig_canvascopy;
};

struct cplot_show_cookie {
	struct cplot_figure *fig;
	char input[256];
	int len_input;
	enum cplot_show_mode mode;
	struct highlight highlight;
};
#define cookie_t struct cplot_show_cookie

static int omit_utf8(const char *str, int len) {
	if ((signed char)str[len-1] >= 0)
		return len;
	for (int back=0; back<4; back++) {
		switch (str[len-1-back] >> 6 & 3) {
			case 2:
				continue;
			case 3:
				return len-back-1;
			default:
				return len-back;
		}
	}
	return len;
}

void end_typingmode(cookie_t* cookie) {
	switch (cookie->mode) {
		case typing_savename:
			cplot_write_png_preserve(cookie->fig, cookie->input);
			break;
		case typing_none: break;
	}
	cookie->len_input = 0;
	cookie->mode = typing_none;
	printf("\n\r");
}

static void typing_input(cookie_t *cookie, const char *utf8input, int len) {
	for (int i=0; i<len; i++)
		switch (utf8input[i]) {
			case '\b':
				if (cookie->len_input)
					cookie->len_input = omit_utf8(cookie->input, cookie->len_input-1);
				break;
			case 033:  // abort with Escape
				cookie->mode = typing_none;
			case '\r':
			case '\n':
				cookie->input[cookie->len_input] = 0;
				end_typingmode(cookie);
				return;
			case 0:
				goto endfor;
			default:
				if (cookie->len_input >= sizeof(cookie->input)-1)
					goto endfor;
				cookie->input[cookie->len_input++] = utf8input[i];
				break;
		}
endfor:
	cookie->input[cookie->len_input] = 0;
	printf("\e[u%s\e[K", cookie->input), fflush(stdout);
	return;
}

static void keycallback(struct waylandhelper *wlh) {
	if (!wlh->keydown)
		return;
	cookie_t *cookie = wlh->userdata;

	if (cookie->mode == typing_savename) {
		char *ptr = cookie->input+cookie->len_input;
		int len = wlh_get_keytext(wlh, ptr, sizeof(cookie->input)-cookie->len_input);
		typing_input(cookie, ptr, len);
		return;
	}

	const xkb_keysym_t* syms;
	int nsyms = wlh_get_keysyms(wlh, &syms);

	for (int isym=0; isym<nsyms; isym++) {
		switch (syms[isym]) {
			case XKB_KEY_s:
				if (wlh->last_keymods & WLR_MODIFIER_CTRL)
					cplot_write_png_preserve(cookie->fig, NULL);
				else {
					printf("filename: \e[s"), fflush(stdout);
					cookie->mode = typing_savename;
				}
				break;
			case XKB_KEY_q:
				wlh->stop = 1;
				break;
		}
	}
}

static void motioncallback(struct waylandhelper *wlh, int xmove, int ymove) {
	cookie_t *cookie = wlh->userdata;
	struct cplot_figure *fig = cookie->fig;
	int ifig, x = wlh->mousex, y = wlh->mousey;
	struct highlight *hi = &cookie->highlight;

	goto fig_test;
	while ((ifig = cplot_next_ifigure_from_coords(fig, x, y)) >= 0) {
		if (!(fig = fig->children[ifig]))
			continue;
		/* convert coordinates to child coordinates */
		x -= fig->ro_corner[0];
		y -= fig->ro_corner[1];
fig_test:
		if (fig->legend.visible &&
			x >= fig->legend.ro_xywh[0] && x < intsum_02(fig->legend.ro_xywh+0) &&
			y >= fig->legend.ro_xywh[1] && y < intsum_02(fig->legend.ro_xywh+1))
			goto fig_found;
	}
	goto turn_off_highlight;

fig_found:
	for (int idata=0; idata<fig->ndata; idata++)
		if (fig->legend.ro_xywh[1] + fig->legend.ro_datay[idata+1] > y &&
			fig->legend.ro_xywh[1] + fig->legend.ro_datay[idata] <= y)
		{
			if (hi->fig == fig && hi->idata == idata)
				return; // nothing changed
			hi->fig = fig;
			hi->idata = idata;
			hi->update = 1;
			return;
		}

turn_off_highlight:
	if (hi->fig)
		hi->update = 1;
	hi->fig = NULL;
}

void highlight_data(struct waylandhelper *wlh) {
	cookie_t *cookie = wlh->userdata;
	struct highlight *hl = &cookie->highlight;
	hl->update = 0;
	if (hl->canvascopy && hl->id_canvascopy == hl->fig_canvascopy->draw_counter)
		/* The figure has not been redrawn since highlighting data last time.
		   Hence, there is an old highlight which must be erased.*/
		copy_canvas(
			wlh->data + hl->canvascopy_start, wlh->xres,
			hl->canvascopy, hl->fig_canvascopy->wh[0],
			hl->fig_canvascopy->wh);

	if (!hl->fig)
		return;

	if (hl->canvascopy &&
		(hl->fig_canvascopy != hl->fig || hl->id_canvascopy != hl->fig->draw_counter))
	{
		/* A new copy is needed because the highlighted figure has been changed or updated. */
		free(hl->canvascopy);
		goto new_copy;
	}

	if (!hl->canvascopy) {
new_copy:
		hl->canvascopy_start = cplot_get_startcanvas(hl->fig, cookie->fig, wlh->xres);
		hl->canvascopy = duplicate_canvas(wlh->data + hl->canvascopy_start, wlh->xres, hl->fig->wh);
		hl->fig_canvascopy = hl->fig;
		hl->id_canvascopy = hl->fig->draw_counter;
	}

	/* highlight the data */
	struct cplot_data *data = hl->fig->data[hl->idata];
	struct cplot_data copy = *data;
	copy.linestyle.thickness *= 3;
	copy.markerstyle.size *= 3;
	copy.errstyle.thickness *= 3;
	uint32_t *canvas = wlh->data + hl->canvascopy_start;//cplot_get_startcanvas(hl->fig, cookie->fig, wlh->xres);
	cplot_data_render(&copy, canvas, wlh->xres, hl->fig, 0);
}

struct cplot_figure* cplot_show_preserve_(struct cplot_figure *fig, char *name) {
	struct waylandhelper wlh = fig->wlh ? *fig->wlh : (struct waylandhelper){
		.xresmin = 20,
		.yresmin = 20,
	};
	struct waylandhelper *old_wlh = fig->wlh;
	fig->wlh = &wlh;

	cookie_t cookie = {
		.fig = fig,
	};
	wlh.xres = fig->wh[0];
	wlh.yres = fig->wh[1];
	wlh.key_callback = keycallback;
	wlh.motion_callback = motioncallback;
	wlh.userdata = &cookie;
	wlh.title = name ? name : fig->name;

	wlh_init(&wlh);
	long updatecount = 1;
	double starttime = -1;
	while (!wlh.stop && wlh_roundtrip(&wlh) >= 0) {
		if (wlh_key_should_repeat(&wlh))
			wlh.key_callback(&wlh);

		if (!wlh.can_redraw)
			goto next;
		fig->wh[0] = wlh.xres;
		fig->wh[1] = wlh.yres;
		int should_commit = 0;
		if (wlh.redraw) {
			cplot_draw(fig, wlh.data, fig->wh[0]);
			should_commit++;
		}
		if (fig->update) {
			double now = starttime < 0 ? 0 : get_time() - starttime;
			int ret = fig->update(fig, wlh.data, wlh.xres, updatecount++, now);
			if (ret < 0)
				break;
			should_commit += ret;
		}
		if (cookie.highlight.fig && cookie.highlight.id_canvascopy != fig->draw_counter)
			cookie.highlight.update = 1;
		if (cookie.highlight.update) {
			should_commit = 1;
			highlight_data(&wlh);
		}
		if (should_commit) {
			wlh_commit(&wlh);
			if (starttime < 0)
				starttime = get_time();
		}
next:
		usleep(9000);
	}
	wlh_destroy(&wlh);
	free(cookie.highlight.canvascopy);
	fig->wlh = old_wlh;
	return fig;
}

#undef cookie_t
