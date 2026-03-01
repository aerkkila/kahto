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
	char name[30];
	clock_t start;
	double time;

	/* Creating a new figure each time is simple but takes some time. */
	start = clock();
	for (int i=0; i<ndata; i++) {
		struct kahto_figure *fig = kahto_y(data[i], length);
		sprintf(name, "tmpdir/a_%03i.png", i);
		kahto_write_png(fig, name);
	}
	time = (double)(clock() - start) / CLOCKS_PER_SEC;
	printf("%f s without preserving the figure\n", time);

	/* It is faster to preserve the figure. */
	start = clock();
	struct kahto_figure *fig = kahto_figure_new();
	for (int i=0; i<ndata; i++) {
		kahto_y(data[i], length, .figure=fig);
		sprintf(name, "tmpdir/b_%03i.png", i);
		kahto_clean(kahto_write_png_preserve(fig, name));
	}
	kahto_destroy(fig);
	time = (double)(clock() - start) / CLOCKS_PER_SEC;
	printf("%f s preserving the figure\n", time);

	/* When I ran this code, I got:
	   2.992073 s without preserving the figure
	   1.903621 s preserving the figure

	   When running through valgrind, the difference was even clearer:
	   66.681877 s without preserving the figure
	   30.403787 s preserving the figure
	   */
}
