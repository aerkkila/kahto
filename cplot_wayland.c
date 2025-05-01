#include <waylandhelper.h>
#include <xkbcommon/xkbcommon.h>

enum cplot_show_mode {typing_none, typing_savename};

struct highlight {
	int iaxes, idata;
	char update, used;
	uint32_t *canvascopy;
	int id_canvascopy;
};

struct cplot_show_cookie {
	void *standalone;
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
			cplot_write_png_preserve(cookie->standalone, cookie->input);
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
					cplot_write_png_preserve(cookie->standalone, NULL);
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
	struct cplot_standalone_common *standalone = cookie->standalone;
	struct cplot_axes **axess = (void*)&cookie->standalone;
	int naxes = 1;
	if (standalone->type == cplot_axes_e);
	else if (standalone->type == cplot_subplots_e) {
		naxes = ((struct cplot_subplots*)standalone)->naxes;
		axess = ((struct cplot_subplots*)standalone)->axes;
	}
	else
		goto turn_off_highlight;

/* The data is highlighted when cursor is on its legend item. */
	struct highlight *hi = &cookie->highlight;
	for (int iaxes=0; iaxes<naxes; iaxes++) {
		struct cplot_axes *axes = axess[iaxes];
		if (!axes->legend.visible)
			continue;
		const int *legpos = axes->legend.ro_xywh;
		int leg_x = axes->ro_corner[0]+legpos[0];
		int leg_y = axes->ro_corner[1]+legpos[1];
		if (wlh->mousex < leg_x || wlh->mousex > leg_x+legpos[2])
			continue;
		if (wlh->mousey < leg_y || wlh->mousey > leg_y+legpos[3])
			continue;
		for (int idata=0; idata<axes->ndata; idata++)
			if (axes->legend.ro_datay[idata+1] > wlh->mousey &&
				axes->legend.ro_datay[idata] <= wlh->mousey) {
				if (hi->used && hi->iaxes == iaxes && hi->idata == idata)
					return; // nothing changed
				hi->iaxes = iaxes;
				hi->idata = idata;
				hi->used = hi->update = 1;
				return;
			}
		break;
	}

turn_off_highlight:
	if (hi->used)
		hi->update = 1;
	hi->used = 0;
}

void highlight_data(struct waylandhelper *wlh) {
	cookie_t *cookie = wlh->userdata;
	struct highlight *hl = &cookie->highlight;
	struct cplot_standalone_common *common = cookie->standalone;
	hl->update = 0;
	if (hl->canvascopy && hl->id_canvascopy == common->draw_counter) {
		/* there is an old highlight which must be erased */
		copy_canvas(wlh->data, hl->canvascopy, wlh->xres, common);
	}
	else {
		/* there is not an old highlight */
		free(hl->canvascopy);
		hl->canvascopy = duplicate_canvas(wlh->data, wlh->xres, common);
		hl->id_canvascopy = common->draw_counter;
	}
	if (!hl->used)
		return;

	/* highlight the data */
	struct cplot_axes *axes = (void*)common;
	if (common->type == cplot_subplots_e)
		axes = ((struct cplot_subplots*)common)->axes[hl->iaxes];
	else if (common->type != cplot_axes_e)
		return;
	struct cplot_data *data = axes->data[hl->idata];
	struct cplot_data copy = *data;
	copy.linestyle.thickness *= 3;
	copy.markerstyle.size *= 3;
	copy.errstyle.thickness *= 3;
	cplot_data_render(&copy, wlh->data, wlh->xres, axes, 0);
}

void* cplot_show_preserve(void *vplot) {
	struct cplot_standalone_common *standalone = vplot;
	struct waylandhelper wlh = standalone->wlh ? *standalone->wlh : (struct waylandhelper){
		.xresmin = 20,
		.yresmin = 20,
	};
	struct waylandhelper *old_wlh = standalone->wlh;
	standalone->wlh = &wlh;

	cookie_t cookie = {
		.standalone = vplot,
	};
	wlh.xres = standalone->wh[0];
	wlh.yres = standalone->wh[1];
	wlh.key_callback = keycallback;
	wlh.motion_callback = motioncallback;
	wlh.userdata = &cookie;
	wlh.title = standalone->name;

	wlh_init(&wlh);
	long updatecount = 1;
	double starttime = -1;
	while (!wlh.stop && wlh_roundtrip(&wlh) >= 0) {
		if (wlh_key_should_repeat(&wlh))
			wlh.key_callback(&wlh);

		if (!wlh.can_redraw)
			goto next;
		standalone->wh[0] = wlh.xres;
		standalone->wh[1] = wlh.yres;
		int should_commit = 0;
		if (wlh.redraw) {
			cplot_draw(standalone, wlh.data, standalone->wh[0]);
			should_commit++;
		}
		if (standalone->update) {
			double now = starttime < 0 ? 0 : get_time() - starttime;
			int ret = standalone->update(vplot, wlh.data, wlh.xres, updatecount++, now);
			if (ret < 0)
				break;
			should_commit += ret;
		}
		if (cookie.highlight.used && cookie.highlight.id_canvascopy != standalone->draw_counter)
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
	standalone->wlh = old_wlh;
	return vplot;
}

#undef cookie_t
