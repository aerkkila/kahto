#include <xkbcommon/xkbcommon.h>

static void keycallback(struct waylandhelper *wlh) {
	if (!wlh->keydown)
		return;
	const xkb_keysym_t* syms;
	int nsyms = wlh_get_keysyms(wlh, &syms);

	for (int isym=0; isym<nsyms; isym++) {
		switch (syms[isym]) {
			case XKB_KEY_s:
				if (wlh->last_keymods & WLR_MODIFIER_CTRL)
					cplot_write_png(wlh->userdata, NULL);
				else {
					char name[256];
					scanf("%256s", name);
					cplot_write_png(wlh->userdata, name);
				}
				break;
			case XKB_KEY_q:
				wlh->stop = 1;
				break;
		}
	}
}
