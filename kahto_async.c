static int async_response = 2,
		   async_request = 1,
		   async_step = 3;

static int async_update(struct kahto_async *async, uint32_t *canvas, int ystride) {
	if (!async)
		return 0;
	if (async->_exit == async_request)
		return -1;
	if (async->_lock != async_request)
		return 0;
	async->canvas = canvas;
	async->ystride = ystride;
	async->_lock = async_response;
	while (async->_lock == async_response)
		usleep(3000);
	if (async->_lock == async_step)
		async->_lock = async_request;
	return 1;
}

int kahto_async_lock(struct kahto_async *async) {
	async->_lock = async_request;
	while (async->_lock != async_response) {
		if (!kahto_async_running(async))
			return 1;
		usleep(10);
	}
	return 0;
}

void kahto_async_unlock(struct kahto_async *async) {
	async->_lock = 0;
}

void kahto_async_unlock_step(struct kahto_async *async) {
	async->_lock = async_step;
}

void kahto_async_stop(struct kahto_async *async) {
	async->_exit = async_request;
}

int kahto_async_running(struct kahto_async *async) {
	return async->_exit != async_response;
}

void kahto_async_destroy(struct kahto_async *async) {
	if (kahto_async_running(async))
		kahto_async_stop(async);
	pthread_join(async->_thread, NULL);
	kahto_destroy(async->figure);
	free(async);
}

static void* async_show(void *vargs) {
	struct kahto_async *h = vargs;
	kahto_show_preserve(h->figure);
	h->_exit = async_response;
	return NULL;
}

struct kahto_async* kahto_async_show(struct kahto_figure *fig) {
	struct kahto_async *h = calloc(1, sizeof(*h));
	h->figure = fig;
	fig->async = h;
	pthread_create(&h->_thread, NULL, async_show, h);
	return h;
}

static void* async_write_mp4(void *vargs) {
	struct kahto_async *h = vargs;
	kahto_async_unlock_step(h);
	kahto_write_mp4_preserve(h->figure, NULL, h->_fps);
	h->_exit = async_response;
	return NULL;
}

struct kahto_async* kahto_async_write_mp4(struct kahto_figure *fig, const char *name, float fps) {
	struct kahto_async *h = calloc(1, sizeof(*h));
	h->figure = fig;
	h->_fps = fps;
	if (name)
		fig->name = (void*)(intptr_t)name;
	fig->async = h;
	pthread_create(&h->_thread, NULL, async_write_mp4, h);
	return h;
}

void kahto_async_join(struct kahto_async **async, int n) {
	unsigned char *mask = malloc(n);
	memset(mask, 1, n);
	while (1) {
		usleep(50'000);
		int count = 0;
		for (int i=0; i<n; i++) {
			if (mask[i] && !kahto_async_running(async[i])) {
				kahto_async_destroy(async[i]);
				mask[i] = 0;
			}
			count += mask[i];
		}
		if (!count)
			break;
	}
	free(mask);
}
