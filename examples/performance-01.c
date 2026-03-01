/* Most of time in performance-00.c is consumed by libpng library.
   This is the same example, but without saving the figures.
   This compares better the performance in kahto library. */

#include <stdlib.h>
#include <kahto.h>
#include <time.h>
#include <stdio.h>

int main(){
#define ndata 40
#define length 50
	short data[ndata][length];
	for (int i=0; i<ndata; i++)
		for (int ii=0; ii<length; ii++)
			data[i][ii] = rand() % 3000 + i*10;
	clock_t start;
	double time;

	uint32_t *canvas = malloc(4 * kahto_default_width * kahto_default_height);

	/* Creating a new figure each time is simple but takes some time. */
	start = clock();
	for (int i=0; i<ndata; i++) {
		struct kahto_figure *fig = kahto_y(data[i], length);
		kahto_draw(fig, canvas, kahto_default_width);
		kahto_destroy(fig);
	}
	time = (double)(clock() - start) / CLOCKS_PER_SEC;
	printf("%f s without preserving the figure\n", time);

	/* It is faster to preserve the figure. */
	start = clock();
	struct kahto_figure *fig = kahto_figure_new();
	for (int i=0; i<ndata; i++) {
		kahto_y(data[i], length, .figure=fig);
		kahto_draw(fig, canvas, kahto_default_width);
		kahto_clean(fig);
	}
	kahto_destroy(fig);
	time = (double)(clock() - start) / CLOCKS_PER_SEC;
	printf("%f s preserving the figure\n", time);

	free(canvas);

	/* When I ran this code, I got:
	   0.356713 s without preserving the figure
	   0.058833 s preserving the figure
	   */
}
