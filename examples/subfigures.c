#include <kahto.h>
#include <string.h>
#include <stdio.h>

void make_figures(struct kahto_figure *fig) {
	for (int i=0; i<fig->nsubfigures; i++) {
		fig->subfigures[i] = kahto_figure_new();
		fig->subfigures[i]->background = kahto_colorschemes[0][i%kahto_ncolors[0]];
		char title[30];
		sprintf(title, "figure %i", i);
		fig->subfigures[i]->title.text = strdup(title);
		fig->subfigures[i]->title.owner = 1;
	}
}

int main() {
	struct kahto_figure *fig;
	/* An even grid: (x,y) = (3,4) */
	fig = kahto_subfigures_new(3,4);
	make_figures(fig);
	fig->title.text = "suptitle";
	kahto_show(fig);

	/* An uneven grid with x-sizes = (0.5 a a), where a is such that sum(x-sizes) = 1
	   and y-sizes = (0.5 0.25 b b), where b is such that sum(y-sizes) = 1 */
	fig = kahto_subfigures_xyarr_new(3, (0.5), 4, (0.5, 0.25));
	make_figures(fig);
	fig->title.text = "suptitle";
	kahto_show(fig);
}
