#include <waylandhelper.h>
#include <xkbcommon/xkbcommon.h>

enum cplot_show_mode {typing_none, typing_savename};

struct cplot_show_cookie {
	void *standalone;
	char input[256];
	int len_input;
	enum cplot_show_mode mode;
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

void* cplot_show_preserve(void *vplot) {
	struct cplot_axes *standalone = vplot;
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
	wlh.userdata = &cookie;
	wlh.title = standalone->name;

	wlh_init(&wlh);
	long updatecount = 0;
	double starttime = -1;
	while (!wlh.stop && wlh_roundtrip(&wlh) >= 0) {
		if (wlh_key_should_repeat(&wlh))
			wlh.key_callback(&wlh);

		if (wlh.can_redraw) {
			standalone->wh[0] = wlh.xres;
			standalone->wh[1] = wlh.yres;
			int should_commit = 0;
			if (wlh.redraw) {
				cplot_draw(standalone, wlh.data, standalone->wh[0]);
				should_commit++;
			}
			if (starttime < 0)
				starttime = get_time();
			if (standalone->update) {
				int ret = standalone->update(standalone, wlh.data, wlh.xres, updatecount++, get_time() - starttime);
				if (ret < 0)
					break;
				should_commit += ret;
			}
			if (should_commit)
				wlh_commit(&wlh);
		}
		usleep(10000);
	}
	wlh_destroy(&wlh);
	standalone->wlh = old_wlh;
	return vplot;
}

void* cplot_show(void *vplot) {
	return cplot_destroy(cplot_show_preserve(vplot)), NULL;
}

#undef cookie_t
