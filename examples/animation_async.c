#include <kahto.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define arrlen(a) (sizeof(a) / sizeof((a)[0]))

int main(int argc, char **argv) {
	float ydata[50];
	for (int i=0; i<arrlen(ydata); i++)
		ydata[i] = (double)rand() / (RAND_MAX+1L);
	struct kahto_figure *fig = kahto_y(ydata, arrlen(ydata));
	kahto_set_range(kahto_gly(fig), 0, 1);

	int write_video = argc > 1;

	struct kahto_async *async = NULL;
	if (write_video)
		async = kahto_async_write_mp4(fig, "async.mp4", 20);
	else
		async = kahto_async_show(fig);
	int ifig = 0;

	while (1) {
		if (!write_video)
			usleep(50'000);

		/* Before changing anything, we must call kahto_async_lock.
		   Usually it returns false.
		   It retuns true for example if user has interactively closed the figure.
		   In that case, we must exit.
		   The function will wait until the thread confirms that locking is ready. */
		if (kahto_async_lock(async))
			break;

		/* now we can update the figure */
		ydata[ifig % arrlen(ydata)] = (double)rand() / (RAND_MAX+1L);
		kahto_clear_data(async->figure, async->canvas, async->ystride);
		kahto_draw_figures(async->figure, async->canvas, async->ystride);

		/* unlock when changes are done */
		if (write_video)
			// For a video, unlock only one step:
			// After writing the frame, the lock turns automatically on
			// until unlock_step is called again
			// This ensures that each update corresponds to one frame in the video
			kahto_async_unlock_step(async);
		else
			kahto_async_unlock(async);

		/* exit automatically after a while */
		if (++ifig > 40)
			break;
	}

	kahto_async_destroy(async); // finally kill the threads and free all memory
}
