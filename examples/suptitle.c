#include <cplot.h>
#include <string.h>
#include <stdio.h>

int main() {
	struct cplot_figure *fig = cplot_subfigures_new(3,4);
	for (int i=0; i<12; i++) {
		fig->subfigures[i] = cplot_figure_void_new();
		fig->subfigures[i]->background = cplot_colorschemes[0][i%cplot_ncolors[0]];
		char title[30];
		sprintf(title, "figure %i", i);
		fig->subfigures[i]->title.text = strdup(title);
		fig->subfigures[i]->title.owner = 1;
	}
	fig->title.text = "suptitle";
	cplot_show(fig);
}
